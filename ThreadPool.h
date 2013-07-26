#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_


#include <vector>
#include <queue>
#include <pthread.h>

using namespace std;

class mutex
{
public:
    pthread_mutex_t mtx;
    mutex() {
        pthread_mutex_init(&mtx, NULL);
    }
    ~mutex() {
        pthread_mutex_destroy(&mtx);
    }
    void lock() {
        pthread_mutex_lock(&mtx);
    }
    void unlock() {
        pthread_mutex_unlock(&mtx);
    }
};

class thread_pool
{
public:
    class thread_runner
    {
    public:
        virtual ~thread_runner() {}
        virtual void run() = 0;
    };

    class thread_base
    {
    public:
        thread_base() {
      stop = false;
	}
        virtual ~thread_base(){}
	void start();
	virtual void run() = 0;

	void set_stop();
	bool is_stop();
        void add_task(thread_runner* tr);
	thread_runner* get_task();

	queue<thread_runner*> inq;
	queue<thread_runner*> outq;

	mutex inq_lock;
	mutex outq_lock;

	pthread_t pid;

	bool stop;
    };

    class worker_thread : public thread_base {
    public:
	virtual void run();
    };

    class master_thread : public thread_base {
    public:
        virtual void run();

	master_thread() {
	    worker_count = 10;
	}
	int worker_count;

	vector<worker_thread*> wt_set;
    };

    void start(int worker_count);
    void add_task(thread_runner* tr);
    thread_runner* get_task();

    thread_pool() {
        mt = NULL;
    }
private:
    master_thread* mt;
};


#endif

