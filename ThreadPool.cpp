#include <stdlib.h>
#include <assert.h>
#include "thread_pool.h"

static void* thread_entrance(void* arg)
{
    thread_pool::thread_base* tb = (thread_pool::thread_base*)arg;
    tb->run();
}
void thread_pool::thread_base::set_stop() {
    stop = false;
}
bool thread_pool::thread_base::is_stop() {
    return stop;
}

void thread_pool::thread_base::start()
{
    pthread_create(&pid, NULL, thread_entrance, this);
}

void thread_pool::thread_base::add_task(thread_pool::thread_runner* tr)
{
    inq_lock.lock();
    inq.push(tr);
    inq_lock.unlock();
}

thread_pool::thread_runner* thread_pool::thread_base::get_task()
{
    thread_runner* ret = 0;

    outq_lock.lock();
    if (outq.size() > 0) {
        ret = outq.front();
  outq.pop();
    }
    outq_lock.unlock();
    return ret;
}

void thread_pool::master_thread::run()
{
    //create children thread
    for (int i = 0; i < worker_count; ++i) {
        worker_thread* wt = new worker_thread;
	wt_set.push_back(wt);
	wt->start();
    }

    while (!is_stop()) {
        //check the child output channel
        thread_runner* tr = NULL;
        inq_lock.lock();
	if (inq.size() > 0) {
	    tr = inq.front();
	    inq.pop();
	}
	inq_lock.unlock();
	if (tr) {
	    int mod = rand()%worker_count;
	    wt_set[mod]->add_task(tr);
	    continue;
	}
	for (int i = 0 ; i < wt_set.size(); ++i) {
	    thread_runner* tr = wt_set[i]->get_task();
	    if (tr) {
	        outq_lock.lock();
		outq.push(tr);
		outq_lock.unlock();
	    }
	}
	usleep(1000);
    }
}

void thread_pool::worker_thread::run()
{
    while (!is_stop()) {
        thread_runner* tr = NULL;

        inq_lock.lock();
        if (inq.size() > 0) {
            tr = inq.front();
            inq.pop();
        }
	inq_lock.unlock();
	if (tr) {
	    tr->run();

	    outq_lock.lock();
	    outq.push(tr);
	    outq_lock.unlock();
	    continue;
	}

        usleep(1000);
    }
}

void thread_pool::start(int worker_count) 
{
    assert(mt == NULL);
    mt = new master_thread;
    mt->worker_count = worker_count;
    mt->start();
}

void thread_pool::add_task(thread_runner* tr) {
    mt->add_task(tr);
}

thread_pool::thread_runner* thread_pool::get_task() {
    return mt->get_task();
}
