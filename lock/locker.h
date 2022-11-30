#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

/**
 * sem
 * Sem(int = 0) init sem with value, default = 0
 * ~Sem() destory sem
 * wait() value-1 or wait
 * post() value+1
*/
class Sem{
public:
    Sem(int num = 0);
    ~Sem();
    bool wait();
    bool post();
private:
    sem_t s_id;
};


/**
 * phtread_mutex
 * Locker() init mutex
 * ~Locker() destory mutex
 * lock() mutex lock
 * unlock() mutex unlock
 * get() return mutex_id
*/
class Locker{
public:
    Locker();
    ~Locker();
    bool lock();
    bool unlock();
    pthread_mutex_t* get();
private:
    pthread_mutex_t mutex_id;
};


/**
 * pthread_cond
 * Cond() init cond
 * ~Cond() destory cond
 * wait(pthread_mutex_t) must be called
 * timewait(pthread_mutex_t) if timeout, called automatically
 * singal() wake up one of blocked thread
 * broadcast() wake up all blocked thread
*/
class Cond{
public:
    Cond();
    ~Cond();
    bool wait(pthread_mutex_t* m_id);
    bool timewait(pthread_mutex_t* m_id, struct timespec t);
    bool singal();
    bool broadcast();
private:
    pthread_cond_t c_id;
};


#endif