#include "MapReduceClient.h"
#include "MapReduceFramework.h"
#include <string>
#include <vector>
#include <dirent.h>
#include <iostream>
#include <cstring>
#include <stdlib.h>
#include <fstream>

#define ERROR_MSG1 "Search Failure: "
#define ERROR_MSG2 " failed."


#define SEC_TO_MICRO 1000000
#define MICRO_TO_NANO 1000

// ============= Keys & Values =============

/**
 * Class k1SearchTerm represents a name of directory
 */
class k1DirName: public k1Base {
public:
    std::string dirName;
    k1DirName(std::string word) : dirName(word){};
    ~k1DirName(){};
    bool operator<(const k1Base &other) const
    {
        k1DirName o = (const k1DirName&) other;
        return strcmp(dirName.c_str(), o.dirName.c_str()) < 0;
    };
};

/**
 * Class v1fileName represents a sub string to find
 */
class v1SearchTerm : public v1Base {
public:
    std::string term;
    v1SearchTerm(std::string name) : term(name){};
    ~v1SearchTerm(){};
};

/**
 * Class k2SearchTerm represents the occurance of the substring term a filename
 * 0 if no occurance, otherwise 1/2/etc...
 */
class k2FileName: public k2Base {
public:
    std::string fileName;
    k2FileName(std::string word) : fileName(word){};
    k2FileName(const k1DirName* other) : fileName(other->dirName){};
    ~k2FileName(){};
    bool operator<(const k2Base &other) const
    {
        k2FileName o = (const k2FileName&) other;
        return  strcmp(fileName.c_str(), o.fileName.c_str()) < 0;};
};

/**
 * Class v2occur if the string occurred in the filename or not (0 or 1).
 */
class v2occur : public v2Base {
public:
    int occurrence;
    v2occur(int num) : occurrence(num){};
    ~v2occur(){occurrence = 0;};
};

/**
 * Class k3FileName represents a name of file
 */
class k3FileName: public k3Base {
public:
    std::string fileName;
    k3FileName(std::string name) : fileName(name){};
    k3FileName(k3Base* other)
    {
        k3FileName* k = (k3FileName*) other;
        fileName = k->fileName;
    }
    ~k3FileName(){};
    bool operator<(const k3Base &other) const
    {
        k3FileName o = (const k3FileName&) other;
        return  strcmp(fileName.c_str(), o.fileName.c_str())<0;};
};

/**
 * Class v3occur represents the number of occurrences of the substring term a filename
 * 1/2/3...
 */
class v3occur : public v3Base {
public:
    int occurrence;
    v3occur(int num) : occurrence(num){};
    ~v3occur(){occurrence = 0;};
};

class MapReduce : public MapReduceBase {
public:

    void Map(const k1Base *const key, const v1Base *const val) const{
        k1DirName *key1 = (k1DirName*) key;
        v1SearchTerm * val1 = (v1SearchTerm*)val;
        k2FileName *key2 = nullptr;
        v2occur *value2 = nullptr;

        DIR* dirp = nullptr;
        dirp = opendir(key1->dirName.c_str());


        // if given dirName is not directory
        if (errno == 2 or errno == 20) {
            return;
        }
        if (dirp == NULL)
        {
            std::cerr << "Error in opendir " <<errno << std::endl;
            exit(1);
        }
        struct dirent* dp;
        while ((dp = readdir(dirp)) != NULL)
        {
			try {
				key2 = new k2FileName(dp->d_name);
				if ((key2->fileName).find(val1->term) != std::string::npos) {
					value2 = new v2occur(1);
				} else {
					value2 = new v2occur(0);
				}
			}catch (std::bad_alloc)
			{
				std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
				exit(1);
			}

			Emit2(key2, value2);
        }
        closedir(dirp);
    };

    void Reduce(const k2Base *const key, const V2_VEC &vals) const{


        k2FileName *key2 = (k2FileName *) key;
        k3FileName *key3 = nullptr;
		try {
			key3 =  new k3FileName(key2->fileName);
		}catch (std::bad_alloc)
		{
			std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
			exit(1);
		}

		v3occur *value3 = nullptr;
        size_t length = vals.size();
        if (length == 0 or ((v2occur*)vals[0])->occurrence == 0){
            delete key3;
            return;
        }
		try
		{
			value3 = new v3occur(length);
		}catch (std::bad_alloc)
		{
			std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
			exit(1);
		}

        Emit3(key3, value3);
    };
    virtual ~MapReduce(){};
};



IN_ITEMS_VEC initializeKeysValues(std::vector<std::string> directories, std::string search_word)
{
    IN_ITEMS_VEC keys_values1;
	try {
		for (std::vector<std::string>::iterator it = directories.begin(); it !=directories.end(); ++it)
		{
			k1DirName *k1 = new k1DirName(*it);
			v1SearchTerm *v1 = new v1SearchTerm(search_word);
			IN_ITEM k_v = std::pair<k1DirName*, v1SearchTerm*>(k1,v1);
			keys_values1.push_back(k_v);
		}
	}catch (std::bad_alloc)
	{
		std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
		exit(1);
	}

	return keys_values1;
}



/**
 * @param argv: [searchTerm[String], [directories[String]]
 *              searchTerm is the sub string to seach for in given directories
 *              directories is a list of names of dirs
 *              main will find all files in the directories which contain searchTerm in their name
 * */
int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::cout << "Program arguments invalid." << std::endl;
		exit(0);
	}

	int multiThreadLevel = 3;
	std::string searchTerm = argv[1];
	std::vector<std::string> directories;
	directories.assign(argv + 2, argv + argc);
	IN_ITEMS_VEC kv_1 = initializeKeysValues(directories, searchTerm);
	MapReduce * myMapReduce= nullptr;
	try
	{
		myMapReduce = new MapReduce();
	}
	catch(std::bad_alloc)
	{
		std::cerr << ERROR_MSG1 << "new" << ERROR_MSG2 << std::endl;
		exit(1);
	}
	OUT_ITEMS_VEC kv_out3 = RunMapReduceFramework(*myMapReduce, kv_1, multiThreadLevel, true);

	for (auto it = kv_out3.begin(); it != kv_out3.end(); ++it)
	{
		for (int i = 0; i < ((v3occur*)it->second)->occurrence; ++i)
		{
			std::cout << ((k3FileName*)it->first)->fileName;
			if ((i+1 < ((v3occur*)it->second)->occurrence) || (it != --kv_out3.end()))
			{
				std::cout << " ";
			}
		}
		delete it->first;
		delete it->second;
	};
	std::cout << std::endl;

	for (IN_ITEMS_VEC::iterator it = kv_1.begin(); it != kv_1.end(); ++it)
	{
		delete (*it).first;
		delete (*it).second;
	}
	delete myMapReduce;

}
