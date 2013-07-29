#ifndef THREAD_H
#define THREAD_H

#include <vector>
#include <queue>
#include <pthread.h>
using namespace std;


class mutex
{
public:
    pthread_mutex_t mtx;
    
    mutex()
    {
        pthread_mutex_init(&mtx, NULL);
    }
    ~mutex()
    {
        pthread_mutex_destroy($mtx);
    }
    void lock()
    {
        pthread_mutex_lock(&mtx);
    }
    void unlock()
    {
        pthread_mutex_unlock(&mtx);
    }
};

class fast_lock
{
public:
    fast_lock()
    {
        pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE);
    }
    ~fast_lock()
    {
        pthread_spin_destroy(&spin);
    }
    void lock()
    {
        pthread_spin_lock(&spin);
    }
    void unlock()
    {
        pthread_spin_unlock(&spin);
    }

private:
    pthread_spinlock_t spin;
};

/**
class thread_runner
{
public:
    virtual ~thread_runner() {}
    virtual void run() = 0;
};

void* thread_entrance(void *arg);
**/



#endif
