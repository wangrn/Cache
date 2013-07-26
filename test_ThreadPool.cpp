#include <stdlib.h>
#include <stdio.h>

#include "thread_pool.h"

class thread_runner_test : public thread_pool::thread_runner
{
public:
    virtual ~thread_runner_test() {}
    virtual void run() {
        printf("trt: %d\n", isn);
    }
    int isn;
};


int main(int argc, char* argv[])
{
    thread_pool* tp = new thread_pool();
    tp->start(10);
    for (int i = 0; i < 100; ++i) {
    	thread_runner_test* trt = new thread_runner_test;
    	trt->isn = i;
    	tp->add_task(trt);
    }

    int recv_count = 0;

    while (1) {
        thread_runner_test* trt = (thread_runner_test*)tp->get_task();
	if (trt) {
	    recv_count += 1;
	    if (recv_count == 100)
	        break;
	    else
	        continue;
	}
        usleep(1000);
    }

    printf ("recv_count:%d\n", recv_count);
    return 0;
}
