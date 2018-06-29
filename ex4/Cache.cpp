//
// Created by keren.meron on 6/1/17.
//

#include <vector>
#include <X11/Xlib.h>
#include <sys/stat.h>
#include <cmath>
#include <stdlib.h>
#include <cstring>
#include "Cache.h"
#include <unistd.h>
#include <algorithm>

typedef std::unordered_map<std::string, std::unordered_set<int>*> cache_map_type;
typedef std::vector<FileBlock*> cache_container;


// ===================== FileBlock Class ===================



bool compare_fileBlock(FileBlock* first ,FileBlock* second){
    return first->freq > second->freq;

}

// ===================== Cache Class =======================

/** Constructor. */
Cache::Cache(int num_blocks, double f_old , double f_new):num_blocks(num_blocks){
    try {
        cacheMap = new cache_map_type();
        cache = new cache_container();
    } catch (std::bad_alloc) {
        exit(1);
    }
    middle_begin = std::floor(num_blocks * f_new);
    old_begin = num_blocks - std::floor(num_blocks * f_old);
    if (old_begin == num_blocks)
    {
        old_begin -= 1;
    }
    struct stat fi;
    stat("/tmp", &fi);
    block_size = fi.st_blksize;
};


Cache::Cache():num_blocks(0){

}
/** Destructor. */
Cache::~Cache(){
    for (auto it = cache->begin(); it != cache->end(); ++it)
    {
        delete *it;
    }
    delete cache;
    for (auto it = cacheMap->begin(); it != cacheMap->end(); ++it)
    {
        delete (*it).second;
    }
    delete cacheMap;
}


/** Insert into a given buffer the requested content which is in the given fileBlock. */
int Cache::fillBuffer(void* buf, size_t count, off_t offset, FileBlock* block){
    int upper_bound = (int) (offset + count);
    upper_bound = std::min(upper_bound, block_size);
    upper_bound = std::min(upper_bound, block->numberBytes);
    try {
        (memcpy(buf,(block->buf + offset), upper_bound - (unsigned int)offset));
    } catch (std::bad_alloc)
    {
        return -1;
    }
// TODO hold flag for where there is trash (not part of file content)? flag in fileBlock class
    return (int) (upper_bound - offset);
}

/**
 * Find whether wanted block is in cache, hit or miss
 * @param path of file to find
 * @param blockIndex index of block in file
 * @return True (hit) or False (miss)
 */
bool Cache::findInCache(std::string path, int blockIndex){
    cache_map_type::iterator idInCache = cacheMap->find(path);
    if (idInCache == cacheMap->end())
    {
        return False;
    }
    if (idInCache->second->find(blockIndex) == idInCache->second->end())
    {
        return False;
    }
    return True;
}

/**
 * Retrieve content from file in cache. Assumes block is already in cache.
 * moves the fileBlock to the head of the cache
 * @param path of file
 * @param buf pointer to where to save content
 * @param count amount to read. will not read outside than block boundaries
 * @param blockIndex index of block in file
 * @param offset offset of content in certain blockIndex
 * @return -1 on failure, number of bytes written to buf
 */
int Cache::getContent(std::string path, void *buf, size_t count, int blockIndex, off_t offset){
    if (not findInCache(path, blockIndex))
    {
        return -1;
    }
    for (auto it = cache->begin(); it != cache->end(); ++it)
    {
        if ((*it)->path == path && (*it)->blockIndex == blockIndex)
        {
            FileBlock* block = &(**it);

            if (it >= cache->begin() + middle_begin)
            {
                block->freq += 1;
            }
            cache->erase(it);
            cache->insert(cache->begin(), block);
            return fillBuffer(buf, count, offset, block);;
        }
    }
    return -1;

}


/**
 * Insert given block into the cache and remove a block according to miss policy.
 * @param block of bytes from a file
 * @return negative number upon failure, otherwise 0
 */
int Cache::insert(FileBlock** block) {
    // perform eviction policy only if cache is full
    if (cache->size() == (unsigned int)num_blocks) {

        int lowest_freq = (*cache)[old_begin]->freq;
        cache_container::const_iterator removeBlock = (*cache).begin() + old_begin;

        for (unsigned int i = old_begin; i < cache->size(); ++i) {
            if ((*cache)[i]->freq <= lowest_freq) {
                removeBlock = (*cache).begin() + i;
                lowest_freq = (*cache)[i]->freq;
            }
        }
        (*cacheMap)[(*removeBlock)->path]->erase((*removeBlock)->blockIndex);
        FileBlock* deleteBlock = &(**removeBlock);
        cache->erase(removeBlock);
        delete deleteBlock;
    }

    // insert block into cache
    if ((*cacheMap).find((*block)->path) == cacheMap->end())
    {
        std::unordered_set<int>* new_blocks_set = nullptr;
        try
        {
            new_blocks_set = new std::unordered_set<int>();
        } catch (std::bad_alloc)
        {
            return -1;
        }
        new_blocks_set->insert((*block)->blockIndex);
        (*cacheMap)[(*block)->path] = new_blocks_set;
    } else
    {
        (*cacheMap)[(*block)->path]->insert((*block)->blockIndex);
    }
    cache->insert(cache->begin(), *block);
    return 0;

}

/** print the content of the cache in order of the alg */
int Cache::print_cache(int file_id)
{
    lseek(file_id, 0, SEEK_END);
    cache_container* curr_container = new cache_container(*cache);

    // LFU
    if (old_begin == 0){
        std::sort(curr_container->begin(),curr_container->end(),compare_fileBlock);
    }
    // LRU and FBR
    for (unsigned int i  = 0; i < cache->size(); ++i)
    {
        int bytes_writen = 0;
        std::string new_line = (*curr_container)[i]->path + " " + std::to_string
                                     ((*curr_container)[i]->blockIndex) + "\n";
        const char * line_to_write = new_line.c_str();
        int size_line = strlen(line_to_write);
        while (bytes_writen < size_line) {
            bytes_writen += write(file_id,line_to_write + bytes_writen,size_line - bytes_writen);
        }
    }
    if (close(file_id) < 0)
    {
        delete curr_container;
        return -1;
    }
    delete curr_container;
    return 0;
}


size_t Cache::get_block_size(){
    return (size_t) block_size;
}

