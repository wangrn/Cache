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


class thread_runner
{
public:
    virtual ~thread_runner() {}
    virtual void run() = 0;
};

void* thread_entrance(void *arg);

class thread_base
{
public:
    thread_base()
    {
        stop = false;
        i_am_stoped = true;
    }
    virtual ~thread_base() {}
    
    void start() {}
    virtual void run() = 0;

    void set_stop() { stop = true; }
    bool is_stop() { return stop; }
    bool is_i_am_stoped() { return i_am_stoped; }
    
    void add_task(thread_runner* tr);
    thread_runner* cancel_task();
    thread_runner* get_task();
    
    queue<thread_runner*> inq;
    queue<thread_runner*> outq;
    
    mutex inq_lock;
    mutex outq_lock;
    
    pthread_t pid;
    
    bool stop;
    bool i_am_stoped;
};

class worker_thread : public thread_base
{
public:
    virtual void run();

    volatile bool running;

    worker_thread() { running = false; }   
        
    bool is_running() { return running; }       
    void set_running() { running = true; }       

    void wait_until_running()
    { 
        while (!is_running())
            usleep(1000);
    }
};

class master_thread : public thread_base
{
public:
    virtual void run() ; 
    master_thread()
    {
        worker_count = 10;
    }
    int worker_count;

    vector<worker_thread*> wt_set;
};

void assure_start_worker_thread(vector<worker_thread*>& wtl);
void assure_stop_worker_thread(vector<worker_thread*>& wtl);

void assure_resume_worker_thread(vector<worker_thread*>& wtl);
void assure_pause_worker_thread(vector<worker_thread*>& wtl);



#endif
