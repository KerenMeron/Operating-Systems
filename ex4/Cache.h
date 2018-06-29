//
// Created by keren.meron on 6/1/17.
//

#ifndef CACHE_H
#define CACHE_H

#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <malloc.h>


/** Class fileBlock represents a block in cache. */
typedef class FileBlock{
public:
    int blockIndex;
    int freq;
    char* buf; // points to content of block
    int numberBytes; // actual content in block, without garbage
    std::string path;
    FileBlock(std::string path, int block_idx, char* content, int bytes) : blockIndex(block_idx),
                                                                           freq(1), buf(content),
                                                                           numberBytes(bytes),
                                                                           path(path){};
    FileBlock() : blockIndex(1), freq(0), buf(nullptr), numberBytes(0), path(NULL){};

    ~FileBlock(){
        free (buf);
    }


} FileBlock;



typedef class Cache{

private:
    int block_size;
    int num_blocks;
    int middle_begin;
    int old_begin;

    /** Map container holding blocks currently in cache, for quick look up. */
    std::unordered_map<std::string, std::unordered_set<int>*>* cacheMap;

    /** Cache container holding blocks from files. */
    std::vector<FileBlock*>* cache;

/** Insert into a given buffer the requested content which is in the given fileBlock. */
    int fillBuffer(void* buf, size_t count, off_t offset, FileBlock* block);

public:
    /** default Constructor. */
    Cache();
    /** Constructor. */
    Cache(int num_blocks, double f_old , double f_new);
    /** Destructor. */
    ~Cache();
    /** print the content of the cache in order of the alg */
    int print_cache(int file_id);

    /**
     * Find whether wanted block is in cache, hit or miss
     * @param file_id file descriptor id
     * @param blockIndex index of block in file
     * @return True (hit) or False (miss)
     */
    bool findInCache(std::string path, int blockIndex);

    /**
     * Retrieve content from file.
     * @param path of file
     * @param buf pointer to where to save content
     * @param count amount to read
     * @param blockIndex index of block in file
     * @param offset offset of content in certain blockIndex
     * @return -1 on failure, number of bytes written to buf
     */
    int getContent(std::string path, void *buf, size_t count, int blockIndex, off_t offset);

    size_t get_block_size();

    /**
     * Insert given block into the cache
     * @param block block of bytes from a file
     * @return negative number upon failure, otherwise 0
     */
    int insert(FileBlock** block);


} Cache;

#endif //CACHE_H
