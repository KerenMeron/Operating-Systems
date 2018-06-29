/**
 * @brief: OS 67808, Ex.1
 * @authors Keren Meron, Eldan Chodorov
 * @date 2017
 */

#include <sys/time.h>
#include <unistd.h>
#include "osm.h"
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <cfenv>
#include <malloc.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <cstring>

#define UNROLLING_FACTOR 10
#define MICRO_TO_NANO 1000
#define SEC_TO_MICRO 1000000
#define MACHINE_NAME_BUF_SIZE 1024
#define DEFAULT_ITERATIONS 1000

char* machine_name_ptr;
timeMeasurmentStructure* measureStruct;

/* Initialization function that the user must call
 * before running any other library function.
 * The function may, for example, allocate memory or
 * create/open files.
 * Pay attention: this function may be empty for some designs. It's fine.
 * Returns 0 upon success and -1 on failure
 */
int osm_init() {
    machine_name_ptr = new char[MACHINE_NAME_BUF_SIZE];
    return 0;
}


/* finalizer function that the user must call
 * after running any other library function.
 * The function may, for example, free memory or
 * close/delete files.
 * Returns 0 uppon success and -1 on failure
 */
int osm_finalizer(){
    delete [] machine_name_ptr;
    delete measureStruct;
    return 0;
}

/* Time measurement function for a simple assignment operation.
   returns time in milli-seconds upon success,
   and -1 upon failure.
   */
double osm_assignment_time(unsigned int iterations=DEFAULT_ITERATIONS){
    iterations = (iterations <= 0) ? DEFAULT_ITERATIONS : iterations;
    try {
        struct timeval *begin = new struct timeval;
        struct timeval *end = new struct timeval;
        int a = 1;int b = 2;int c = 3;int d = 4;int e = 5;int f = 6;int g = 7;int h = 8;
        for (unsigned int i = 0; i < DEFAULT_ITERATIONS; i++) {
            a = i;
            b = i;
            c = i;
            d = i;
            e = i;
            f = i;
            g = i;
            h = i;
        }
        int res = gettimeofday(begin, NULL);
        if (res == -1) { return -1;}
        for (unsigned int i = 0; i < iterations; i++) {
            a = i;
            b = i;
            c = i;
            d = i;
            e = i;
            f = i;
            g = i;
            h = i;
        }
        res = gettimeofday(end, NULL);
        if (res == -1) { return -1;}
        int r = a + b + c + d + e +f + g + h;
        double endTotal = (end->tv_sec * (SEC_TO_MICRO) + end->tv_usec);
        double startTotal = (begin->tv_sec * (SEC_TO_MICRO) + begin->tv_usec) +(a-a);
        double smallTime = ((endTotal - startTotal) *MICRO_TO_NANO + (r - r)) / (iterations*8);

        delete begin;
        delete end;
        return smallTime;
    } catch (int exc){
        return -1;
    }
}


/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time(unsigned int iterations=DEFAULT_ITERATIONS){
    iterations = (iterations <= 0) ? DEFAULT_ITERATIONS : iterations;
    try {
        struct timeval *start = new struct timeval;
        struct timeval *end = new struct timeval;

        unsigned int realRepetitions = iterations * UNROLLING_FACTOR;
        int res1 = 0, res2 = 0, res3 = 0, res4 = 0, res5 = 0, res6 = 0, res7 = 0, res8 = 0, res9
                = 0, res10 = 0;
        for (unsigned int i = 0; i < DEFAULT_ITERATIONS; i++) {
            res1 = 1 + i;
            res2 = 1 + i;
            res3 = 1 + i;
            res4 = 1 + i;
            res5 = 1 + i;
            res6 = 1 + i;
            res7 = 1 + i;
            res8 = 1 + i;
            res9 = 1 + i;
            res10 = 1 + i;
        }
        int res = gettimeofday(start, NULL);
        if (res == -1) {return -1;}
        for (unsigned int i = 0; i < iterations; i++) {
            res1 = 1 + i;
            res2 = 1 + i;
            res3 = 1 + i;
            res4 = 1 + i;
            res5 = 1 + i;
            res6 = 1 + i;
            res7 = 1 + i;
            res8 = 1 + i;
            res9 = 1 + i;
            res10 = 1 + i;
        }
        res = gettimeofday(end, NULL);
        if (res == -1) { return -1;}
        int a = res1 + res2 + res3 + res4 + res5 + res6 + res7 + res8 + res9 + res10;
        double endTotal = (end->tv_sec * (SEC_TO_MICRO) + end->tv_usec);
        double startTotal = (start->tv_sec * (SEC_TO_MICRO) + start->tv_usec) +(a-a);
        double opTime = ((endTotal - startTotal) * MICRO_TO_NANO)/realRepetitions;
        delete start;
        delete end;
        return opTime;
    } catch (int exp){
        return -1;
    }
}

void empty_function(){}


/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time(unsigned int iterations=DEFAULT_ITERATIONS) {
    iterations = (iterations <= 0) ? DEFAULT_ITERATIONS : iterations;
    try {
        struct timeval *start = new struct timeval;
        struct timeval *end = new struct timeval;

        unsigned int realRepetitions = iterations * UNROLLING_FACTOR;
        for (unsigned int i = 0; i < DEFAULT_ITERATIONS; i++) {
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
        }
        int res = gettimeofday(start, NULL);
        if (res == -1) { return -1;}
        for (unsigned int i = 0; i < iterations; i++) {
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
            empty_function();
        }
        res = gettimeofday(end, NULL);
        if (res == -1) { return -1;}
        double endTotal = (end->tv_sec * (SEC_TO_MICRO) + end->tv_usec);
        double startTotal = (start->tv_sec * (SEC_TO_MICRO) + start->tv_usec);
        double opTime = ((endTotal - startTotal) * MICRO_TO_NANO)/realRepetitions;
        delete start;
        delete end;
        return opTime;
    } catch (int exp) {
        return -1;
    }
}




/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time(unsigned int iterations=DEFAULT_ITERATIONS){
    iterations = (iterations <= 0) ? DEFAULT_ITERATIONS : iterations;
    try {
        struct timeval *start = new struct timeval;
        struct timeval *end = new struct timeval;

        unsigned int realRepetitions = iterations * UNROLLING_FACTOR;
        for (unsigned int i = 0; i < DEFAULT_ITERATIONS; i++) {
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
        }
        int res = gettimeofday(start, NULL);
        if (res == -1) { return -1;}
        for (unsigned int i = 0; i < iterations; i++) {
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
            OSM_NULLSYSCALL;
        }
        res = gettimeofday(end, NULL);
        if (res == -1) { return -1;}
        double endTotal = (end->tv_sec * (SEC_TO_MICRO) + end->tv_usec);
        double startTotal = (start->tv_sec * (SEC_TO_MICRO) + start->tv_usec);
        double opTime = ((endTotal - startTotal) * MICRO_TO_NANO)/realRepetitions;
        delete start;
        delete end;
        return opTime;
    } catch (int exp) {
        return -1;
    }
}

/* Time measurement function for accessing the disk.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_disk_time(unsigned int iterations=DEFAULT_ITERATIONS) {
    iterations = (iterations <= 0) ? DEFAULT_ITERATIONS : iterations;
    try {
        int fd = open("/tmp/our_os_file.txt", O_WRONLY|O_DIRECT|O_SYNC|O_CREAT ,S_IRWXU);
        if (fd == -1) {
            std::cout << "Opening file error: " << strerror(errno) << std::endl;
            return -1; }

        struct stat fi;
        int res = stat("/tmp", &fi);
        if (res == -1) { return -1; }

        size_t blockSize = (size_t) fi.st_blksize;
        size_t length = blockSize * 10;

        char *myText = (char *) aligned_alloc(blockSize, blockSize);
        if (myText == NULL) { return -1; }
        for (unsigned int i = 0; i < blockSize; ++i) {
            myText[i] = 'a';
        }
        struct timeval *start = new struct timeval;
        struct timeval *end = new struct timeval;

        res = gettimeofday(start, NULL);
        if (res == -1) { return -1; }

        for (unsigned int j = 0; j < iterations; ++j) {
            write(fd, myText, blockSize);
        }
        res = gettimeofday(end, NULL);
        if (res == -1) { return -1; }

        double endTotal = (end->tv_sec * (SEC_TO_MICRO) + end->tv_usec);
        double startTotal = (start->tv_sec * (SEC_TO_MICRO) + start->tv_usec);
        double opTime = (endTotal - startTotal) * MICRO_TO_NANO;
        opTime = opTime / iterations;
        delete start;
        delete end;
        free(myText);
        close(fd);
        remove("/tmp/our_os_file.txt");
        return opTime;
    } catch (int exp) {
        return -1;
    }
}


/*
 * Create timeMeasurmentStructure to contain all time checks.
 * Returns struct timeMeasurmentStructure.
 */
timeMeasurmentStructure measureTimes (unsigned int operation_iterations,
                                      unsigned int function_iterations,
                                      unsigned int syscall_iterations,
                                      unsigned int disk_iterations){
    timeMeasurmentStructure *measureData = new timeMeasurmentStructure() ;
    measureStruct = measureData;
    int res = gethostname(machine_name_ptr, MACHINE_NAME_BUF_SIZE);
    if (res == -1) {strcpy(machine_name_ptr, "");}

    measureData->machineName = machine_name_ptr;
    measureData->instructionTimeNanoSecond = osm_operation_time(operation_iterations);
    measureData->functionTimeNanoSecond = osm_function_time(function_iterations);
    measureData->trapTimeNanoSecond = osm_syscall_time(syscall_iterations);
    measureData->diskTimeNanoSecond = osm_disk_time(disk_iterations);
    measureData->functionInstructionRatio = measureData->functionInstructionRatio /
            measureData->instructionTimeNanoSecond;
    measureData->trapInstructionRatio = measureData->trapTimeNanoSecond /
            measureData->instructionTimeNanoSecond;
    measureData->diskInstructionRatio = measureData->diskTimeNanoSecond /
            measureData->instructionTimeNanoSecond;
    return *measureData;
}
