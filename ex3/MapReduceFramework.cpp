
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <deque>
#include "MapReduceFramework.h"
#include <algorithm>
#include <map>
#include <semaphore.h>
#include <cstring>
#include "ctime"
#include <sys/time.h>

#define CHUNK_SIZE 10
#define ERROR_MSG1 "MapReduceFramework Failure: "
#define ERROR_MSG2 " failed."
#define SEC_TO_MICRO 1000000
#define MICRO_TO_NANO 1000

std::string map_Op = "ExecMap";
std::string reduce_Op = "ExecReduce";
std::string shuffle_Op = "Shuffle";
std::string created = "created";
std::string terminated = "terminated";

/** Flag determining whether to release key2, val2 */
bool delete_memory_flag;

/** timer values*/
std::ofstream log_file_stream;
time_t timer;
struct timeval *start;
struct timeval *end;


/** Container for pool of threads. */
std::vector<pthread_t> *map_threads;
std::vector<pthread_t> *reduce_threads;
pthread_t shuffle_thread;

/** Container for (key1,value1) elements, and an index for accessing it. */
IN_ITEMS_VEC keys_values_1;
int access_index = 0;

/** Container for queues into which MapExec will insert <key2, value2> */
typedef std::pair<k2Base*, v2Base*> kv2;
typedef std::deque<kv2> kv2_queue;
std::vector<kv2_queue*>* kv2_queues_container;

/** Container for queues into which ReduceExec will insert <key3, value3> */
typedef std::pair<k3Base*, v3Base*> kv3;
typedef std::deque<kv3> kv3_queue;
std::vector<kv3_queue*>* kv3_queues_container;

/** Mutexes to use in framework. */
pthread_mutex_t shuffle_end_mutex; // blocks reduce from running until shuffle has finished
pthread_mutex_t init_mutex; // blocking MapExec threads during their creation
pthread_mutex_t index_mutex; // allowing one thread at a time to access the access_index
pthread_mutex_t log_File_mutex;
sem_t shuffle_semaphore; // semaphore counting the pairs kv2 ready for shuffle
sem_t map_complete_semaphore; // semaphore for telling shuffle when all Map threads are done
std::vector<pthread_mutex_t> kv2_output_queues_mutexes; // mutex for each Mapthread's output queue

/** map object that maps between pthread_t to its output queue. */
typedef std::pair <pthread_t, int> mapping_pair;
std::vector<mapping_pair> *pthreadToContainer;


/** Container that holds the output keys and values of shuffle. */
typedef std::pair<k2Base*, std::vector<v2Base*>> kv2_pair;
typedef std::vector<kv2_pair> shuffle_vec;
shuffle_vec *shuffle_output;


/** function definitions */
void initializeQueuesKV2(int multiThreadLevel);
void initializeQueuesKV3(int multiThreadLevel);
void* MapExec (void* some_func);
void* ReduceExec (void* some_func);
void* shuffle (void* something);
void Emit2 (k2Base*, v2Base*);

/**
 * Write times of operations to log file.
 * @param operation what was performed
 * @param func by which function
 */
void log_printer(std::string operation, std::string func)
{
	timer = std::time(0);
	char date[50];
	std::strftime(date,sizeof(date),"[%d.%m.%Y %X]", std::localtime(&timer));
	if (pthread_mutex_lock(&log_File_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	log_file_stream<<(std::string)"Thread "<<func<<(std::string)" "<<operation<<(std::string)" "
			""<<date << "\n";
	if (pthread_mutex_unlock(&log_File_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
}

/**
 * Initialize different containers and mutexes used by the Framework.
 * @param multiThreadLevel number of map/reduce threads.
 */
void init_containers_mutexes(int multiThreadLevel)
{
	if (sem_init(&map_complete_semaphore, 0, multiThreadLevel) < 0)
	{
		std::cerr << ERROR_MSG1 << "sem_init" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if (sem_init(&shuffle_semaphore, 0, 0) < 0)
	{
		std::cerr << ERROR_MSG1 << "sem_init" << ERROR_MSG2 << std::endl;
		exit(1);
	}

	if (pthread_mutex_init(&index_mutex, NULL) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_init" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	try
	{
		pthreadToContainer = new std::vector<mapping_pair>();
		map_threads = new std::vector<pthread_t>(multiThreadLevel);
		shuffle_output = new std::vector<kv2_pair>();
		reduce_threads = new std::vector<pthread_t>(multiThreadLevel);


	}catch(std::bad_alloc)
	{
		std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
		exit(1);
	}

	if ((pthread_mutex_init(&shuffle_end_mutex, NULL) != 0))
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_init" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if (pthread_mutex_lock(&shuffle_end_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
		exit(1);
	}

}


/**
 * Create framework threads for Map, Reduce and Shuffle methods.
 * @param mapReduce
 * @param multiThreadLevel
 */
void createMapShuffleThreads(MapReduceBase& mapReduce, int multiThreadLevel)
{
	if (pthread_mutex_init(&init_mutex, NULL) != 0){
		std::cerr << ERROR_MSG1 << "pthread_mutex_init" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if (pthread_mutex_lock(&init_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	init_containers_mutexes(multiThreadLevel);

	// create MapExec threads
	for (int i = 0; i < multiThreadLevel; ++i)
	{
		if (pthread_create(&((*map_threads)[i]), NULL, MapExec, (void*) &mapReduce) != 0)
		{
			std::cerr << ERROR_MSG1 << "pthread_create" << ERROR_MSG2 << std::endl;
			exit(1);
		}
		log_printer(map_Op, created);
		if (pthread_detach(((*map_threads)[i])) != 0)
		{
			std::cerr << ERROR_MSG1 << "pthread_detach" << ERROR_MSG2 << std::endl;
			exit(1);
		};
		pthreadToContainer->push_back(mapping_pair((*map_threads)[i], i));
		// initialize mutex for output queue
		pthread_mutex_t output_queue_mutex;
		kv2_output_queues_mutexes.push_back(output_queue_mutex);
		if (pthread_mutex_init(&(kv2_output_queues_mutexes[i]), NULL))
		{
			std::cerr << ERROR_MSG1 << "pthread_create" << ERROR_MSG2 << std::endl;
			exit(1);
		}
	}
	// create shuffle thread
	if (pthread_create(&shuffle_thread, NULL, shuffle, NULL))
	{
		std::cerr << ERROR_MSG1 << "pthread_create" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	log_printer(shuffle_Op, created);
	if (pthread_detach(shuffle_thread) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_detach" << ERROR_MSG2 << std::endl;
		exit(1);
	};

	// initialize queue for each thread and unlock mutex to allow threads to start running
	initializeQueuesKV2(multiThreadLevel);

	if (pthread_mutex_unlock(&init_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	};
}

/**
 * Initialize output queues for storing key2, value2
 * @param multiThreadLevel number of queues required
 */
void initializeQueuesKV2(int multiThreadLevel)
{
	try
	{
		kv2_queues_container = new std::vector<kv2_queue*>(multiThreadLevel);
		for (int i = 0; i < multiThreadLevel; ++i)
		{
			(*kv2_queues_container)[i] = new kv2_queue();
		}
	} catch (std::bad_alloc){
		std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
		exit(1);
	}

}

/**
 * Initialize output queues for Reduce threads.
 * @param multiThreadLevel number of queues needed.
 */
void initializeQueuesKV3(int multiThreadLevel)
{
	try {
		kv3_queues_container = new std::vector<kv3_queue*>(multiThreadLevel);
		for (int i = 0; i < multiThreadLevel; ++i)
		{
			(*kv3_queues_container)[i] = new kv3_queue();
		}
	}catch (std::bad_alloc){
		std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
		exit(1);
	}

}

/**
 * Execute user's reduce method.
 */
void* ReduceExec (void * some_func)
{
	if (pthread_mutex_lock(&init_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if (pthread_mutex_unlock(&init_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	MapReduceBase* mapReduce = (MapReduceBase*) some_func;

	int container_index = 0;

	while (true) {
		// read and change index
		if (pthread_mutex_lock(&index_mutex) != 0)
		{
			std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
			exit(1);
		}
		container_index = access_index;
		int curr_chunk_size = std::min(CHUNK_SIZE, (int) shuffle_output->size() - container_index);
		access_index += curr_chunk_size;

		if (pthread_mutex_unlock(&index_mutex) != 0)
		{
			std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
			exit(1);
		}

		// if all <key,value>s have been read, stop
		if ((unsigned int) container_index == shuffle_output->size())
		{
			log_printer(reduce_Op, terminated);
			pthread_exit(NULL);
		}

		int stop_index = container_index + curr_chunk_size;
		for (; container_index < stop_index; ++container_index) {
			*(((*shuffle_output)[container_index]).first) <
			*(((*shuffle_output)[container_index]).first);
			mapReduce->Reduce(((*shuffle_output)[container_index]).first,
							  (const V2_VEC) ((*shuffle_output)[container_index]).second);

			if (delete_memory_flag) {
				delete (*shuffle_output)[container_index].first;
				auto v2_vec = ((*shuffle_output)[container_index]).second;
				for (auto it = v2_vec.begin(); it != v2_vec.end(); ++it) {
					delete *it;
				}
			}
		}
	}
}

/**
 *
 * Execute user's Map method.
 */
void* MapExec (void* func_class)
{

	MapReduceBase* mapReduce = (MapReduceBase*) func_class;
	if (pthread_mutex_lock(&init_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if (pthread_mutex_unlock(&init_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	}

	int container_index = 0;

	while (true)
	{

		// read and change index
		if (pthread_mutex_lock(&index_mutex) != 0)
		{
			std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
			exit(1);
		}
		container_index = access_index;
		int curr_chunk_size = std::min(CHUNK_SIZE, (int)keys_values_1.size() - container_index);

		access_index += curr_chunk_size;
		if (pthread_mutex_unlock(&index_mutex) != 0)
		{
			std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
			exit(1);
		}

		// if all <key,value>s have been read, stop
		if ((unsigned int) container_index == keys_values_1.size())
		{
			if (sem_wait(&map_complete_semaphore) != 0){
				std::cerr << ERROR_MSG1 << "sem_wait" << ERROR_MSG2 << std::endl;
				exit(1);
			};

			try
			{
				int* sem_val = new int(0);
				if (sem_getvalue(&map_complete_semaphore, sem_val) != 0)
				{
					std::cerr << ERROR_MSG1 << "sem_getvalue" << ERROR_MSG2 << std::endl;
					exit(1);
				};
				log_printer(map_Op, terminated);
				if (*sem_val == 0)
				{
					if (sem_post(&shuffle_semaphore) != 0)
					{
						std::cerr << ERROR_MSG1 << "shuffle_semaphore" << ERROR_MSG2 << std::endl;
						exit(1);
					};
				}
				delete sem_val;
				pthread_exit(NULL);
			}
			catch (std::bad_alloc)
			{
				std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
				exit(1);
			}
		}
		int stop_index = container_index + curr_chunk_size;
		for (; container_index < stop_index; ++container_index)
		{
			mapReduce->Map(keys_values_1[container_index].first, keys_values_1[container_index].second);

		}
	}
}

/**
 * Insert key k2 and value v2 into containers.
 */
void Emit2 (k2Base* k2, v2Base* v2)
{
	// find thread's output queue
	kv2_queue* output_queue;
	auto it = pthreadToContainer->begin();
	for (; it != pthreadToContainer->end(); ++it)
	{
		if (pthread_equal(it->first, pthread_self()))
		{
			output_queue = (*kv2_queues_container)[it->second];
			break;
		}
	}
	if (it == pthreadToContainer->end())
	{
		std::cerr << ERROR_MSG1 << "Emit2" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if (pthread_mutex_lock(&(kv2_output_queues_mutexes[it->second])) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	output_queue->push_back(kv2(k2, v2));

	if(sem_post(&shuffle_semaphore))
	{
		std::cerr << ERROR_MSG1 << "sem_post" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if (pthread_mutex_unlock(&(kv2_output_queues_mutexes[it->second])) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	}

}

/**
 * Insert key k3 and value v3 into relevant containers.
 */
void Emit3 (k3Base* k3, v3Base* v3)
{
	// find thread's output queue
	kv3_queue* output_queue;
	auto it = pthreadToContainer->begin();
	for (; it != pthreadToContainer->end(); ++it)
	{
		if (pthread_equal(it->first, pthread_self()))
		{
			output_queue = (*kv3_queues_container)[it->second];
			break;
		}
	}
	if (it == pthreadToContainer->end())
	{
		std::cerr << ERROR_MSG1 << "Emit3" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	output_queue->push_back(kv3(k3, v3));
}

/**
 * Go over outputs of Map and combine k,v1 and k,v2 into k,[v1,v2]
 * @param last_run whether this is the last call to this function.
 */
void perform_shuffle(bool last_run=false)
{
	int* sem_finish_val = nullptr;
	try
	{
		sem_finish_val = new int(0);
	}
	catch (std::bad_alloc)
	{
		std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	for (unsigned int i = 0; i < kv2_queues_container->size(); ++i)
	{
		while ((*kv2_queues_container)[i]->size())
		{

			// extract pair from queue
			if (pthread_mutex_lock(&(kv2_output_queues_mutexes[i])) != 0)
			{
				std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
				exit(1);
			}
			kv2 input_pair = (*kv2_queues_container)[i]->front();
			(*kv2_queues_container)[i]->pop_front();
			if (pthread_mutex_unlock(&(kv2_output_queues_mutexes[i])) != 0)
			{
				std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
				exit(1);
			}
			if (sem_getvalue(&map_complete_semaphore, sem_finish_val) != 0)
			{
				std::cerr << ERROR_MSG1 << "sem_getvalue" << ERROR_MSG2 << std::endl;
				exit(1);
			}
			if (!last_run && *sem_finish_val != 0)
			{
				if (sem_wait(&shuffle_semaphore) != 0)
				{
					std::cerr << ERROR_MSG1 << "sem_wait" << ERROR_MSG2 << std::endl;
					exit(1);
				}
			}

			// see if key already exists
			bool found_pair = false;
			auto it = shuffle_output->begin();
			for (; it != shuffle_output->end(); ++it)
			{
				if (!(*(it->first) < *(input_pair.first)) && !(*(input_pair.first) < *(it->first)))
				{
					found_pair = true;
					break;
				}
			}
			if (!found_pair)
			{
				// insert pair into shuffle output map
				std::vector<v2Base*> v2_vec = std::vector<v2Base*>();
				v2_vec.push_back(input_pair.second);
				kv2_pair new_input_pair = kv2_pair(input_pair.first, v2_vec);
				shuffle_output->push_back(new_input_pair);
			}
			else
			{
				it->second.push_back(input_pair.second);
				if (delete_memory_flag)
				{
					delete input_pair.first;
				}
			}
		}
	}
	delete sem_finish_val;
}

/**
 * Code for shuffle thread which combines all pairs of k2,v2 with same k2.
 */
void* shuffle (void* something)
{
	if (pthread_mutex_lock(&init_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if (pthread_mutex_unlock(&init_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	}

	// read from queues and perform shuffle, as long as queues still have pairs
	int* sem_finish_val = nullptr;
	try
	{
		sem_finish_val = new int(0);
	} catch (std::bad_alloc) {
		std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
		exit(1);
	}

	if (sem_getvalue(&map_complete_semaphore, sem_finish_val))
	{
		std::cerr << ERROR_MSG1 << "sem_getvalue" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	while (*sem_finish_val != 0)
	{
		if (sem_wait(&shuffle_semaphore) == 0)
		{
			perform_shuffle();
		} else
		{
			std::cerr << ERROR_MSG1 << "sem_wait" << ERROR_MSG2 << std::endl;
			exit(1);
		}
		if (sem_getvalue(&map_complete_semaphore, sem_finish_val))
		{
			std::cerr << ERROR_MSG1 << "sem_getvalue" << ERROR_MSG2 << std::endl;
			exit(1);
		}
	}

	perform_shuffle(true);

	if (pthread_mutex_destroy(&index_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_destroy" << ERROR_MSG2 << std::endl;
		exit(1);
	};
	log_printer(shuffle_Op, terminated);

	// allow reduce to run
	if (pthread_mutex_unlock(&shuffle_end_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	delete sem_finish_val;
	pthread_exit(NULL);
}

/**
 * Create threads for Reduce method.
 * @param multiThreadLevel number of threads
 * @param mapReduce class containing reduce method.
 */
void createReduceThreads(int multiThreadLevel, MapReduceBase& mapReduce)
{
	access_index = 0;
	log_printer(reduce_Op, created);
	delete pthreadToContainer;
	try
	{
		pthreadToContainer = new std::vector<mapping_pair>();
	}
	catch (std::bad_alloc)
	{
		std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
		exit(1);
	}

	for (int i = 0; i < multiThreadLevel; ++i){
		if (pthread_create(&((*reduce_threads)[i]), NULL, ReduceExec, (void*) &mapReduce) != 0)
		{
			std::cerr << ERROR_MSG1 << "pthread_create" << ERROR_MSG2 << std::endl;
			exit(1);
		}
		try
		{
			log_printer(reduce_Op, created);
			pthreadToContainer->push_back(mapping_pair((*reduce_threads)[i], i));
		}
		catch (std::bad_alloc)
		{
			std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
			exit(1);
		}
	}

	// reinitialize queue for each thread
	initializeQueuesKV3(multiThreadLevel);


	if (pthread_mutex_init(&index_mutex, NULL))
	{
		std::cerr << ERROR_MSG1 << "pthread_create" << ERROR_MSG2 << std::endl;
		exit(1);
	}
}


/**
 * Comparator function between pairs of key3,value3
 * @return true if pair1 < pair2
 */
bool key3SortFunc(kv3 pair1, kv3 pair2)
{
	return *(pair1.first) < *(pair2.first);
}

/**
 * Combines all items in Reduces' output containers into one container.
 */
OUT_ITEMS_VEC MergeFinalOutput()
{
	try{
		OUT_ITEMS_VEC mergedPairs = OUT_ITEMS_VEC();

		for (auto it = kv3_queues_container->begin(); it != kv3_queues_container->end(); ++it)
		{
			for (auto it2 = (*it)->begin(); it2 != (*it)->end(); ++it2)
			{
				mergedPairs.push_back(*it2);
			}
		}
		return mergedPairs;
	}
	catch (std::bad_alloc)
	{
		std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
		exit(1);
	}

}


/**
 * Release resources allocated during the program.
 */
void destroy()
{
	delete map_threads;
	delete reduce_threads;

	while (!kv2_queues_container->empty()) delete kv2_queues_container->back(), kv2_queues_container->pop_back();
	delete kv2_queues_container;

	while (!kv3_queues_container->empty()) delete kv3_queues_container->back(), kv3_queues_container->pop_back();
	delete kv3_queues_container;

	if (pthread_mutex_destroy(&log_File_mutex) or pthread_mutex_destroy(&init_mutex)
			or pthread_mutex_destroy(&index_mutex) or pthread_mutex_destroy(&shuffle_end_mutex))
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_destroy" << ERROR_MSG2 << std::endl;
		exit(1);
	}


	if (sem_destroy(&shuffle_semaphore) != 0 or sem_destroy(&map_complete_semaphore))
	{
		std::cerr << ERROR_MSG1 << "sem_destroy" << ERROR_MSG2 << std::endl;
		exit(1);
	};

	while (!kv2_output_queues_mutexes.empty()) pthread_mutex_destroy(&(kv2_output_queues_mutexes.back())),
				kv2_output_queues_mutexes.pop_back();
	delete pthreadToContainer;
	delete shuffle_output;
}

/**
 * Run Framework which performs parralel logic of Map and Reduce.
 * @param mapReduce class holding Map and Reduce functions.
 * @param itemsVec input items
 * @param multiThreadLevel number of threads to use in parralel.
 * @param autoDeleteV2K2 whether to release resources of key2, value2
 * @return output items
 */
OUT_ITEMS_VEC RunMapReduceFramework(MapReduceBase& mapReduce, IN_ITEMS_VEC& itemsVec,
									int multiThreadLevel, bool autoDeleteV2K2)
{
	if (pthread_mutex_init(&log_File_mutex,NULL) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_create" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	timer = std::time(0);
	log_file_stream.open(".MapReduceFramework.log", std::ofstream::out);
	if (log_file_stream.is_open())
	{
		log_file_stream<<(std::string)"RunMapReduceFramework started with " <<
					   multiThreadLevel<< " threads\n";
	} else
	{
		std::cerr << ERROR_MSG1 << "std::ofstream.open" << ERROR_MSG2 << std::endl;
	}
	keys_values_1 = itemsVec;
	delete_memory_flag = autoDeleteV2K2;
	try
	{
		start = new struct timeval;
		end = new struct timeval;
	}catch (std::bad_alloc)
	{
		std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if (gettimeofday(start,NULL) == -1)
	{
		std::cerr << ERROR_MSG1 << "gettimeofday" << ERROR_MSG2 << std::endl;
	}

	createMapShuffleThreads(mapReduce, multiThreadLevel);

	// ======= do not proceed until shuffle has finished
	if (pthread_mutex_lock(&shuffle_end_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	}

	// log time
	if(gettimeofday(end,NULL) == -1)
	{
		std::cerr << ERROR_MSG1 << "gettimeofday" << ERROR_MSG2 << std::endl;
	}
	double end_time = (end->tv_sec*SEC_TO_MICRO + end->tv_usec);
	double start_time = (start->tv_sec*SEC_TO_MICRO +start->tv_usec);
	double final_time = (end_time-start_time)*MICRO_TO_NANO;
	if (pthread_mutex_lock(&log_File_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	log_file_stream<<(std::string)"Map and Shuffle took "<<final_time<<(std::string)" ns\n";
	if (pthread_mutex_unlock(&log_File_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if (pthread_mutex_lock(&init_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_lock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	if(gettimeofday(start,NULL) == -1)
	{
		std::cerr << ERROR_MSG1 << "gettimeofday" << ERROR_MSG2 << std::endl;
	}
	// ==================== create Reduce threads ======================
	createReduceThreads(multiThreadLevel, mapReduce);
	if (pthread_mutex_unlock(&init_mutex) != 0)
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	for (auto it = reduce_threads->begin(); it != reduce_threads->end(); ++it)
	{
		pthread_join(*it, NULL);
	}

	if (pthread_mutex_unlock(&shuffle_end_mutex))
	{
		std::cerr << ERROR_MSG1 << "pthread_mutex_unlock" << ERROR_MSG2 << std::endl;
		exit(1);
	}

	// ====================== do final output ==========================
	OUT_ITEMS_VEC final_output = MergeFinalOutput();
	std::sort(final_output.begin(), final_output.end(), key3SortFunc);

	if(gettimeofday(end,NULL) == -1)
	{
		std::cerr << ERROR_MSG1 << "gettimeofday" << ERROR_MSG2 << std::endl;
	}
	end_time = (end->tv_sec*SEC_TO_MICRO + end->tv_usec);
	start_time = (start->tv_sec*SEC_TO_MICRO +start->tv_usec);
	final_time = (end_time-start_time)*MICRO_TO_NANO;
	log_file_stream<<(std::string)"Reduce took "<<final_time<<" ns\n";
	log_file_stream<<(std::string)"RunMapReduceFramework finished\n";

	delete start;
	delete end;

	// release all remaining resources
	destroy();
	return final_output;
}

