/*
 * CacheFS.h
 *
 *  Author: Netanel Zakay, Operating Systems course 2016-2017, HUJI
 */

#include <cstring>
#include <fcntl.h>
#include "CacheFS.h"
#include "Cache.h"
#include <vector>
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <fcntl.h>
#include <limits.h>
#include <zconf.h>
#include <sys/stat.h>
#include <malloc.h>
#include <cmath>

#define MAX_OPEN_FILES 75000
#define AVAILABLE 0
#define TAKEN 1


/** File descriptors table holding fd num and path. */
typedef int os_fd;
typedef int lib_fd;

/** Table holding mapping between library fd and os fd with pathname. */
typedef std::unordered_map<lib_fd, std::pair<std::string*, os_fd>*> files_table;
files_table *fd_table;
char available_ids[MAX_OPEN_FILES] = {AVAILABLE};

/** Class cache */
Cache *cache;

/** Number of hits and misses within usage of CacheFS library. */
int hits;
int misses;

/**
 Initializes the CacheFS.
 Assumptions:
	1. CacheFS_init will be called before any other function.
	2. CacheFS_init might be called multiple times, but only with CacheFS_destroy
  	   between them.

 Parameters:
	blocks_num   - the number of blocks in the buffer cache
	cache_algo   - the cache algorithm that will be used
    f_old        - the percentage of blocks in the old partition (rounding down)
				   relevant in FBR algorithm only
    f_new        - the percentage of blocks in the new partition (rounding down)
				   relevant in FBR algorithm only
 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. system call or library function fails (e.g. new).
		2. invalid parameters.
	Invalid parameters are:
		1. blocks_num is invalid if it's not a positive number (zero is invalid too).
		2. f_old is invalid if it is not a number between 0 to 1 or
		   if the size of the partition of the old blocks is not positive.
		3. fNew is invalid if it is not a number between 0 to 1 or
		   if the size of the partition of the new blocks is not positive.
		4. Also, fOld and fNew are invalid if the fOld+fNew is bigger than 1.

		Pay attention: bullets 2-4 are relevant (and should be checked)
		only if cache_algo is FBR.

 For example:
 CacheFS_init(100, FBR, 0.3333, 0.5)
 Initializes a CacheFS that uses FBR to manage the cache.
 The cache contains 100 blocks, 33 blocks in the old partition,
 50 in the new partition, and the remaining 17 are in the middle partition.
 */
int CacheFS_init(int blocks_num, cache_algo_t cache_algo,
                 double f_old , double f_new  ){

    if (blocks_num <= 0)
    {
        return -1;
    }
    hits = 0, misses = 0;
    switch (cache_algo){
        case LRU:
            f_old = (double)1 / blocks_num;
            f_new = (double) 1 - f_old;
            break;
        case LFU:
            f_old = 1;
            f_new = 0;
            break;
        case FBR:
            if ((f_old < 0) or (f_old > 1) or (0 > f_new) or (f_new > 1) or (f_old + f_new) > 1)
            {
                return -1;
            }
            break;
    }try{
        fd_table = new files_table();
        cache = new Cache(blocks_num, f_old, f_new);
    }catch (std::bad_alloc){
        return -1;
    }

    return 0;
}


/**
 Destroys the CacheFS.
 This function releases all the allocated resources by the library.

 Assumptions:
	1. CacheFS_destroy will be called only after CacheFS_init (one destroy per one init).
	2. After CacheFS_destroy is called,
	   the next CacheFS's function that will be called is CacheFS_init.
	3. CacheFS_destroy is called only after all the open files already closed.
	   In other words, it's the user responsibility to close the files before destroying
	   the CacheFS.

 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail if a system call or a library function fails.
*/
int CacheFS_destroy(){
    for (auto it = fd_table->begin(); it != fd_table->end(); ++it)
    {
        delete (*it).second->first;
        delete (*it).second;
    }
    delete fd_table;
    delete cache;
    return 0;
}


int availableId(){
    for (int i = 0; i < MAX_OPEN_FILES; ++i)
    {
        if (!available_ids[i])
        {
            available_ids[i] = TAKEN;
            return i;
        }
    }

    std::cerr << "Too many open files." <<std::endl;
    return -1;
}



/**
 File open operation.
 Receives a path for a file, opens it, and returns an id
 for accessing the file later

 Notes:
	1. You must open the file with the following flags: O_RDONLY | O_DIRECT | O_SYNC
	2. The same file might be opened multiple times.
	   Like in POISX, it's valid.
	3. The pathname is not unique per file, because:
		a. relative paths are not unique: "myFolder/../tmp" and "tmp".
		b. we might open a link ("short-cut") to the file

 Parameters:
    pathname - the path to the file that will be opened

 Returned value:
    In case of success:
		Non negative value represents the id of the file.
		This may be the file descriptor, or any id number that you wish to create.
		This id will be used later to read from the file and to close it.

 	In case of failure:
		Negative number.
		A failure will occur if:
			1. System call or library function fails (e.g. open).
			2. Invalid pathname. Pay attention that we support only files under
			   "/tmp" due to the use of NFS in the Aquarium.
 */
int CacheFS_open(const char *pathname){
    char* full_path = (char*) malloc(PATH_MAX + 1);

    if (realpath(pathname, full_path) == nullptr)
    {
        return -1;
    }
    std::string full_path_str = (std::string) full_path;
    free (full_path);

    unsigned long found_idx = full_path_str.find("/tmp/");
    if (found_idx == std::string::npos || found_idx != 0)
    {
        return -1;
    }

    for (auto it = fd_table->begin(); it != fd_table->end(); ++it)
    {
        if (*(*it).second->first == full_path_str)
        {
            lib_fd our_fd = availableId();
            if (our_fd == - 1){
                return -1;
            }
            os_fd real_fd = (*it).second->second;
            std::pair<std::string*, os_fd>* pair = nullptr;
            std::string* path_ptr = new std::string(full_path_str);
            try {
                pair = new std::pair<std::string*, os_fd>(path_ptr, real_fd);
            } catch (std::bad_alloc) {
                return -1;
            }
            fd_table->insert(std::make_pair(our_fd, pair));
            return our_fd;
        }
    }
    os_fd real_fd = open(pathname, O_RDONLY|O_DIRECT|O_SYNC);
    if (real_fd < 0)
    {
        return -1;
    }
    lib_fd our_fd = availableId();
    if (our_fd == - 1){
        return -1;
    }
    std::pair<std::string*, os_fd>* pair = nullptr;
    std::string* path_ptr = new std::string(full_path_str);
    try {
        pair = new std::pair<std::string*, os_fd>(path_ptr, real_fd);
    } catch (std::bad_alloc) {
        return -1;
    }

    fd_table->insert(std::make_pair(our_fd, pair));
    return our_fd;
}


/**
 File close operation.
 Receives id of a file, and closes it.

 Returned value:
	0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. a system call or a library function fails (e.g. close).
		2. invalid file_id. file_id is valid if"f it was returned by
		CacheFS_open, and it is not already closed.
 */
int CacheFS_close(int file_id){
    files_table::iterator pair = (*fd_table).find(file_id);
    if (pair == fd_table->end())
    {
        return -1;
    }
    os_fd real_fd = (*pair).second->second;

    // don't close file if another instance of it exists in the library
    bool do_close = true;
    for (auto it = fd_table->begin(); it != fd_table->end(); ++it)
    {
        if (it->second->second == real_fd && it->first != file_id)
        {
            do_close = false;
        }
    }
    if (do_close && close(real_fd) < 0)
    {
        return -1;
    }
    available_ids[file_id] = AVAILABLE;
    delete (*(*fd_table)[file_id]).first;
    delete (*fd_table)[file_id];
    fd_table->erase(file_id);
    return 0;
}



/**
   Read data from an open file.

   Read should return exactly the number of bytes requested except
   on EOF or error. For example, if you receive size=100, offset=0,
   but the size of the file is 10, you will initialize only the first
   ten bytes in the buff and return the number 10.

   In order to read the content of a file in CacheFS,
   We decided to implement a function similar to POSIX's pread, with
   the same parameters.

 Returned value:
    In case of success:
		Non negative value represents the number of bytes read.
		See more details above.

 	In case of failure:
		Negative number.
		A failure will occur if:
			1. a system call or a library function fails (e.g. pread).
			2. invalid parameters
				a. file_id is valid if"f it was returned by
			       CacheFS_open, and it wasn't already closed.
				b. buf is invalid if it is NULL.
				c. offset is invalid if it's negative
				   [Note: offset after the end of the file is valid.
				    In this case, you need to return zero,
				    like posix's pread does.]
				[Note: any value of count is valid.]
 */
int CacheFS_pread(int file_id, void *buf, size_t count, off_t offset){

    int this_round_bytes = 0;
    if (fd_table->find(file_id) == fd_table->end())
    {
        return -1;
    }
    if (count == 0)
    {
        return 0;
    }
    os_fd real_fd = (*fd_table)[file_id]->second;
    size_t block_size = cache->get_block_size();
    int bytes_read = 0;
    char* local_buf = (char *) buf;
    std::string path = *(*fd_table)[file_id]->first;
    int first_block_idx = (int) std::floor(offset / block_size);
    int last_block_idx = (int) std::floor((offset + count) / block_size);
    if ((offset + count) % block_size == 0)
    {
        last_block_idx -= 1;
    }
    for (int i = first_block_idx; i <= last_block_idx; ++i) {
        // MISS
        if (!cache->findInCache(path, i)) {
            char *new_buf = (char *) aligned_alloc(block_size, block_size);
            unsigned int bytes = 0;
            bytes = pread(real_fd, new_buf, block_size, i*block_size);
            // EOF - no bytes requested were read / existed
            if (bytes == 0 or (i == first_block_idx && (offset % block_size >= bytes)))
            {
                free (new_buf);
                break;
            }
            misses += 1;
            FileBlock *new_block = nullptr;
            try {
                new_block = new FileBlock(path, i, new_buf, bytes);
            } catch (std::bad_alloc) {
                return -1;
            }
            cache->insert(&new_block);
        }
        else{
            hits += 1;
        }
        // READ FROM CACHE
        off_t local_offset = 0;
        size_t local_count = block_size;
        if (i == first_block_idx) {
            local_offset = offset % block_size;
        }
        if (i == last_block_idx && ((offset + count) % block_size) != 0) {
            local_count = (offset + count) % block_size;
        }
        if (first_block_idx == last_block_idx) {
            local_count = count;
        }
        this_round_bytes = cache->getContent(path, local_buf, local_count, i, local_offset);

        bytes_read += this_round_bytes;
        local_buf += this_round_bytes;
    }
    return bytes_read;
}


/**
This function writes the current state of the cache to a file.
The function writes a line for every block that was used in the cache
(meaning, each block with at least one access).
Each line contains the following values separated by a single space.
	1. Full path of the file
	2. The number of the block. Pay attention: this is not the number in the cache,
	   but the enumeration within the file itself, starting with 0 for the first
	   block in each file.
For LRU and LFU The order of the entries is from the last block that will be evicted from the cache
to the first (next) block that will be evicted.
For FBR use the LRU order (the order of the stack).

Notes:
	1. If log_path is a path to existed file - the function will append the cache
	   state (described above) to the cache.
	   Otherwise, if the path is valid, but the file doesn't exist -
	   a new file will be created.
	   For example, if "/tmp" contains a single folder named "folder", then
			"/tmp/folder/myLog" is valid, while "/tmp/not_a_folder/myLog" is invalid.
	2. Of course, this operation doesn't change the cache at all.
	3. log_path doesn't have to be under "/tmp".
	3. This function might be useful for debugging purpose as well as auto-tests.
	   Make sure to follow the syntax and order as explained above.

 Parameter:
	log_path - a path of the log file. A valid path is either: a path to an existing
			   log file or a path to a new file (under existing directory).

 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. system call or library function fails (e.g. open, write).
		2. log_path is invalid.
 */
int CacheFS_print_cache (const char *log_path){
    if (log_path == nullptr)
    {
        return -1;
    }
    chmod(log_path, S_IRWXU);
    os_fd real_fd = open(log_path, O_WRONLY|O_CREAT);
    if (real_fd < 0)
    {
        return -1;
    }
    if(cache->print_cache(real_fd) < 0){

        return -1;

    }
    return 0;
}


/**
This function writes the statistics of the CacheFS to a file.
This function writes exactly the following lines:
Hits number: HITS_NUM.
Misses number: MISS_NUM.

Where HITS_NUM is the number of cache-hits, and MISS_NUM is the number of cache-misses.
A cache miss counts the number of fetched blocks from the disk.
A cache hit counts the number of required blocks that were already stored
in the cache (and therefore we didn't fetch them from the disk again).

Notes:
	1. If log_path is a path to existed file - the function will append the cache
	   state (described above) to the cache.
	   Otherwise, if the path is valid, but the file doesn't exist -
	   a new file will be created.
	   For example, if "/tmp" contains a single folder named "folder", then
			"/tmp/folder/myLog" is valid, while "/tmp/not_a_folder/myLog" is invalid.
	2. Of course, this operation doesn't change the cache at all.
	3. log_path doesn't have to be under "/tmp".

 Parameter:
	log_path - a path of the log file. A valid path is either: a path to an existing
			   log file or a path to a new file (under existing directory).

 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. system call or library function fails (e.g. open, write).
		2. log_path is invalid.
 */
int CacheFS_print_stat (const char *log_path){
    if (log_path == nullptr)
    {
        return -1;
    }
    os_fd real_fd = open(log_path, O_WRONLY|O_CREAT);
    if (real_fd < 0)
    {

        return -1;
    }

    lseek(real_fd, 0, SEEK_END);
    std::string new_line1 = "Hits number: " + std::to_string(hits) + "\n";
    std::string new_line2 = "Misses number: " + std::to_string(misses) + "\n";
    std::string lines[2] = {new_line1, new_line2};
    int bytes_writen = 0, size_line = 0;

    for (std::string line : lines){
        const char * line_to_write = line.c_str();
        size_line = strlen(line_to_write);

        while (bytes_writen < size_line) {
            bytes_writen += write(real_fd, line_to_write + bytes_writen, size_line - bytes_writen);
        }
        bytes_writen = 0;
    }
    if (close(real_fd) < 0)
    {
        return -1;
    }
    return 0;
}

