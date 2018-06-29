/**
 * @brief: OS 67808, Ex.2
 * @authors Keren Meron, Eldan Chodorov
 * @date 2017
 */

#include "uthreads.h"
#include <sys/time.h>
#include <setjmp.h>
#include <cstdio>
#include <signal.h>
#include <deque>
#include <iostream>
#include <stdlib.h>

#define JB_SP 6
#define JB_PC 7
#define SELF_SWITCH -30
#define TERMINATE_SWITCH -40
#define NO_THREAD -10

typedef unsigned long address_t;
typedef void (*fp)(void);
typedef class thread thread;

/** timer struct and array for environments of all threads*/
struct itimerval timer;
sigjmp_buf env[MAX_THREAD_NUM];

/** Pointers to all threads, and array with True/False for thread existence */
bool threadIds[MAX_THREAD_NUM];
thread* threadPtrs[MAX_THREAD_NUM];

/** Queues of thread ids which are in ready state, queue of blocked state and index of thread
 * which is in running state. */
std::deque<int> ready;
std::deque<int> blocked;
int runningIndex;

/** Total quatums in running state since beginning of run. */
int totalQuantum;

/** Thread class, represents a user level thread. Holds account of thread's state and id. */
class thread{
public:
    int quatums;
    std::deque<int> syncedWith;     // thread ids waiting for this thread to run
    int synced_t;                   // current thread is waiting for synced_t to run
    bool blocked;
    char* stackPtr;
    thread(){
        quatums = 0;
        blocked = false;
        synced_t = NO_THREAD;
    }
    ~thread(){
        delete[] stackPtr;
    }
};

/**
 * Handles switching between threads (upon timer end).
 */
void contextSwitch(int sig){

    struct sigaction set;
    set.sa_handler = SIG_IGN;
    sigaction(SIGVTALRM, &set, NULL);

    // update quantums
    threadPtrs[runningIndex]->quatums += 1;
    totalQuantum += 1;

    int ret_val = sigsetjmp(env[runningIndex],1);

    if (ret_val == 3){

        set.sa_handler = &contextSwitch;
        sigaction(SIGVTALRM, &set, NULL);

        // initialize timer
        if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
            std::cerr << "thread library error: setitimer error.\n" << std::endl;
            // no error thrown, program will continue running with timing error
            // in order to not disrupt flow
        }
        return;
    }
    // insert back into ready queue
    if (sig == SIGVTALRM){
        ready.push_back(runningIndex);
    }
    // update threads which were synced with running thread
    if (sig != TERMINATE_SWITCH){
        for (int i : threadPtrs[runningIndex]->syncedWith){
            threadPtrs[i]->synced_t = NO_THREAD;
            threadPtrs[runningIndex]->syncedWith.pop_front();
            if (!threadPtrs[i]->blocked){
                ready.push_back(i);
            }
        }
    }
    // remove those synced threads from blocked state
    int j = 0;
    for (int i : blocked)
    {
        if (!threadPtrs[i]->blocked and threadPtrs[i]->synced_t == NO_THREAD){
            blocked.erase(blocked.begin()+ j);
        }
        j++;
    }
    if (sig == TERMINATE_SWITCH){
        delete threadPtrs[runningIndex];
        threadPtrs[runningIndex] = nullptr;
    }

    // update running state
    runningIndex = ready.front();
    ready.pop_front();

    set.sa_handler = &contextSwitch;
    sigaction(SIGVTALRM, &set, NULL);
    if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
        std::cerr << "thread library error: setitimer error.\n" << std::endl;
        // no error thrown, program will continue running with timing error
        // in order to not disrupt flow
    }
    siglongjmp(env[runningIndex], 3);
}



/**
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){

    if (threadIds[0]){
        std::cerr << "thread library error: libray already initialized.\n" << std::endl;
        return -1;
    }
    if (quantum_usecs <= 0){
        std::cerr << "thread library error: quantum_usecs must be positive.\n" << std::endl;
        return -1;
    }

    // Configure the timer to expire after initial and every quantum_usecs */
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = quantum_usecs;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = quantum_usecs;

    struct sigaction sa;
    sa.sa_handler = &contextSwitch;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
        std::cerr << "system error: error setting timer.\n" << std::endl;
        exit(1);
    }
    if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
        std::cerr << "system error: setitimer error.\n" << std::endl;
        exit(1);
    }
    threadIds[0] = true;
    runningIndex = 0;
    thread* newThread = nullptr;
    try
    {
        newThread = new thread();
    }
    catch (std::bad_alloc)
    {
        std::cerr << "system error: memory allocation failed.\n" << std::endl;
        exit(1);
    }
    threadPtrs[runningIndex] = newThread;
    return 0;
}

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
            "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

/** Memory cleanup before exit. */
void cleanUp()
{
    for (int i = 1; i < MAX_THREAD_NUM; ++i){
        if (threadIds[i]){
            threadIds[i] = false;
            delete threadPtrs[i];
            threadPtrs[i] = nullptr;
        }
    }
}


/**
 * Terminate all threads including main thread. Accessed with mask on SIGVTALARM.
 */
void terminateAll(){
    cleanUp();
    threadIds[0] = false;
    threadPtrs[0] = nullptr;
    delete threadPtrs[0];
}

/**
 * Setup of context for a new thread created.
 */
void setup_thread(int tid, fp func){
    thread* newThread = nullptr;
    try
    {
        newThread = new thread();
    }
    catch (std::bad_alloc)
    {
        std::cerr<< "system error: memory allocation failed." << std::endl;
        terminateAll();
        exit(1);
    }
    address_t sp, pc;
    char* stack = new char[STACK_SIZE];
    newThread->stackPtr = stack;
    sp = (address_t)stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t)func;

    // store thread's context and return for first context switch
    int ret_val = sigsetjmp(env[tid], 1);
    if (ret_val != 0){
        struct sigaction set;
        set.sa_handler = &contextSwitch;
        sigaction(SIGVTALRM, &set, NULL);
        if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
            std::cerr << "thread library error: setitimer error.\n" << std::endl;
            // no error thrown, program will continue running with timing error
            // in order to not disrupt flow
        }
        return;
    }
    (env[tid]->__jmpbuf)[JB_SP] = translate_address(sp);
    (env[tid]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env[tid]->__saved_mask);
    threadPtrs[tid] = newThread;
}



/**
 * Get first available id for thread.
 * @return: if no id available, return -1.
 */
int availableId(){
    for (int i=0; i < MAX_THREAD_NUM; ++i){
        if (!threadIds[i]){
            return i;
        }
    }
    return -1;
}

/**
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void)){

    // block alarm signal
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);

    int id = availableId();
    if (id == -1){
        sigprocmask(SIG_UNBLOCK, &set, NULL);
        std::cerr << "thread library error: No available thread id.\n" << std::endl;
        return -1;
    }
    threadIds[id] = true;

    setup_thread(id, f);
    ready.push_back(id);
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    return id;
}

/**
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered as an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){

    // block alarm signal
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);

    if (tid < 0 or tid >= MAX_THREAD_NUM or !threadIds[tid])
    {
        std::cerr << "thread library error: invalid thread id for terminate" << std::endl;
        return -1;
    }
    if (tid == 0)
    {
        terminateAll();
        exit(0);
    }

    threadIds[tid] = false;

    // remove from ready
    for (int i = (int)ready.size(); i >= 0; --i)
    {
        if (ready[i] == tid)
        {
            ready.erase(ready.begin() + i);
            break;
        }
    }
    // clear all syncs
    for (int syncedThread : threadPtrs[tid]->syncedWith)
    {
        threadPtrs[syncedThread]->synced_t = NO_THREAD;
        if (!threadPtrs[syncedThread]->blocked){
            for (int i = (int)blocked.size(); i >= 0; --i)
            {
                if (blocked[i] == syncedThread)
                {
                    blocked.erase(blocked.begin() + i);
                    break;
                }
            }
            ready.push_back(syncedThread);
        }
    }
    // remove tid from blocked
    for (unsigned int i = 0; i < blocked.size(); ++i){
        if (blocked[i] == tid){
            blocked.erase(blocked.begin() + i);
            break;
        }
    }
    if (runningIndex == tid){
        sigprocmask(SIG_UNBLOCK, &set, NULL);
        contextSwitch(TERMINATE_SWITCH);
        return 0;
    } else{
        delete threadPtrs[tid];
        threadPtrs[tid] = nullptr;
    }

    sigprocmask(SIG_UNBLOCK, &set, NULL);
    return 0;
}


/**
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered as an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
    // block alarm signal
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);

    if (!threadIds[tid] or tid < 0 or tid >= MAX_THREAD_NUM or tid == 0)
    {
        std::cerr << "thread library error: invalid thread id.\n" << std::endl;
        return -1;
    }

    // do not reblock threads already blocked
    for (int i : blocked)
    {
        if (i == tid and threadPtrs[tid]->blocked)
        {
            sigprocmask(SIG_UNBLOCK, &set, NULL);
            return 0;
        }
    }
    if (runningIndex == tid)
    {
        threadPtrs[runningIndex]->blocked = true;
        if (threadPtrs[tid]->synced_t == NO_THREAD){
            blocked.push_back(tid);
        }
        contextSwitch(SELF_SWITCH);
        sigprocmask(SIG_UNBLOCK, &set, NULL);
        return 0;
    }
    else {
        for (unsigned int j = 0; j < ready.size(); ++j) {
            if (ready[j] == tid) {
                ready.erase(ready.begin() + j);
                break;
            }
        }
    }
    threadPtrs[tid]->blocked = true;
    if (threadPtrs[tid]->synced_t == NO_THREAD){
        blocked.push_back(tid);
    }
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    return 0;
}


/**
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered as an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
    if (tid < 0 or tid >= MAX_THREAD_NUM or !threadIds[tid]){
        std::cerr << "thread library error: thread does not exist for resume." << std::endl;
        return -1;
    }
    if (runningIndex == tid or tid == 0){
        return 0;
    }
    for (int i : ready){
        if (i == tid){
            return 0;
        }
    }
    for (unsigned int i = 0; i < blocked.size(); ++i){
        if (blocked[i] == tid){
            if (threadPtrs[tid]->synced_t == NO_THREAD){
                blocked.erase(blocked.begin() + i);
                ready.push_back(tid);
            }
            threadPtrs[tid]->blocked = false;
            break;
        }
    }
    return 0;
}


/**
 * Description: This function blocks the RUNNING thread until thread with
 * ID tid will move to RUNNING state (i.e.right after the next time that
 * thread tid will stop running, the calling thread will be resumed
 * automatically). If thread with ID tid will be terminated before RUNNING
 * again, the calling thread should move to READY state right after thread
 * tid is terminated (i.e. it won’t be blocked forever). It is considered
 * as an error if no thread with ID tid exists or if the main thread (tid==0)
 * calls this function. Immediately after the RUNNING thread transitions to
 * the BLOCKED state a scheduling decision should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sync(int tid){

    // block alarm signal

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);
    if (tid < 0 or tid >= MAX_THREAD_NUM or !threadIds[tid]){
        std::cerr << "thread library error: invalid thread id.\n" << std::endl;
        return -1;
    }
    if (runningIndex == 0) {
        std::cerr << "thread library error: main thread cannot call sync.\n" << std::endl;
        return -1;
    }
    threadPtrs[runningIndex]->synced_t = tid;
    threadPtrs[tid]->syncedWith.push_back(runningIndex);
    blocked.push_back(runningIndex);
    contextSwitch(SELF_SWITCH);
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    return 0;
}


/**
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid(){
    return runningIndex;
}


/**
 * Description: This function returns the total number of quantums that were
 * started since the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums(){
    return totalQuantum + 1;
}



/**
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered as an error.
 * Return value: On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){
    if (tid < 0 or tid >= MAX_THREAD_NUM or !threadIds[tid]){
        std::cerr << "thread library error: thread id not available for get quantums" << std::endl;
        return -1;
    }
    if (tid == runningIndex){
        return threadPtrs[tid]->quatums + 1;
    }
    return threadPtrs[tid]->quatums;
}


