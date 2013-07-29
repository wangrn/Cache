#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <stdio.h>

#include "net.h"
#include "thread.h"
#include "common.h"
#include "protocol.h"
#include "uin_index.h"
#include "min_heap.h"
#include "hash.h"



int mt_cache_run(int port, int thread_count)
{
    main_thread* mt = new main_thread;
    mt->thread_count = thread_count;
    mt->run();
    return 0;
}
