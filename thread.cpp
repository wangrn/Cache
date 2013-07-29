#include <stdlib.h>
#include <assert.h>

#include "thread.h"


void* thread_entrance(void* arg)
{
    thread_base *tb = (thread_base*)arg;
    tb->run();
}


void thread_base::start()
{
    stop = false;
    pthread_create(&pid, NULL, thread_entrance, this);
}

void thread_base::add_task(thread_runner *tr)
{
    inq_lock.lock();
    inq.push(tr);
    inq_lock.unlock();
}

/**
 *  After stopping the thread, some tasks might still stay in the input queue.
 *  In this case, we should pop the input tasks which have not executed.
 */
thread_runner* thread_base::cancel_task()
{
    thread_runner *ret = 0;
    inq_lock.lock();
    if (inq.size() > 0)
    {
        ret = inq.front();
        inq.pop();
    }
    inq_lock.unlock();
    return ret;
}

thread_runner* thread_base::get_task()
{
    thread_runner *tr = 0;
    outq_lock.lock();
    if (outq.size() > 0)
    {
        ret = outq.front();
        outq.pop();
    }
    outq_lock.unlock();
    return ret;
}



virtual void worker_thread::run()
{
    i_am_stopped = false;
    while (!is_stop())
    {
        thread_runner *tr = 0;
        
        inq_lock.lock();
        if (inq.size() > 0)
        {
            tr = inq.front();
            inq.pop();
        }
        inq_lock.unlock();
        if (tr)
        {
            tr->run();
            
            outq_lock.lock();
            outq.push(tr);
            outq_lock.unlock();
            continue;
        }
        usleep(1000);
    }
    i_am_stopped = true;
}

virtual void master_thread::run()
{
    for (int i = 0; i < worker_count; i ++)
    {
        worker_thread *wt = new worker_thread();
        wt_set.push_back(wt);
        wt->start();
    }
    
    i_am_stopped = false;
    while (!is_stopped())
    {
        thread_runner *tr = NULL;
        inq_lock.lock();
        if (inq.size() > 0)
        {
            tr = inq.front();
            inq.pop();
        }
        inq_lock.unlock();
        
        if (tr)
        {
            int mod = rand() % worker_count;
            wt_set[mod]->add_task(tr);
            continue;
        }
        for (int i = 0; i < wt_set.size(); i ++)
        {
            thread_runner *tr = wt_set[i]->get_task();
            if (tr)
            {
                outq_lock.lock();
                outq.push_back(tr);
                outq_lock.unlock();
            }
        }
        usleep(1000);
    }
}



void assure_start_worker_thread(vector<worker_thread*>& wtl)
{
    unsigned int sz = wtl.size();
    for (unsigned int i = 0; i < sz; i ++)
        wtl[i]->start();
    
    while (true)
    {
        bool has_dead_thread = false;
        for (unsigned int i = 0; i < sz && !has_dead_thread; i ++)
            if (wtl[i]->is_i_am_stopped())
                has_dead_thread = true;
        if (has_dead_thread) usleep(1000);
        else break;
    }
}

void assure_stop_worker_thread(vector<worker_thread*>& wtl)
{
    unsigned int sz = wtl.size();
    for (unsigned int i = 0; i < sz; i ++)
        wtl[i]->stop();
    
    while (true)
    {
        bool has_running_thread = false;
        for (unsigned int i = 0; i < sz && !has_running_thread; i ++)
            if (wtl[i]->is_i_am_stopped())
                has_running_thread = true;
        if (has_running_thread) usleep(1000);
        else break;
    }
    
    for (unsigned int i = 0; i < sz; i ++)
        pthread_join(wtl[i]->pid, NULL);
}

void assure_resume_worker_thread(vector<worker_thread*>& wtl)
{
    unsigned int sz = wtl.size();
    for (unsigned int i = 0; i < sz; i ++)
        wtl[i]->set_running();
    
    while (true)
    {
        bool has_dead_thread = false;
        for (unsigned int i = 0; i < sz && !has_dead_thread; i ++)
            if (wtl[i]->is_i_am_stopped())
                has_dead_thread = true;
        if (has_dead_thread) usleep(1000);
        else break;
    }
}

void assure_pause_worker_thread(vector<worker_thread*>& wtl)
{
    unsigned int sz = wtl.size();
    for (unsigned int i = 0; i < sz; i ++)
        wtl[i]->set_running();
    
    while (true)
    {
        bool has_dead_thread = false;
        for (unsigned int i = 0; i < sz && !has_dead_thread; i ++)
            if (wtl[i]->is_i_am_stopped())
                has_dead_thread = true;
        if (has_dead_thread) usleep(1000);
        else break;
    }
}
