all : test_ThreadPool

all: tools svr test
svr: mt_cache
tools: cache_cmd 
test: test_ThreadPool

INCL=-I./
LIB=-L./

cache_cmd: cache_cmd.cpp svrip.cpp net.cpp thread.cpp
  g++ -g $? -o $@ ${INCL} ${LIB} -lpthread


mt_cache: mt_server.cpp net.cpp net.h mt_cache.cpp min_heap.h protocol.h uin_index.h helper.h thread.cpp thread.h
	g++ -g $? -o $@ ${INCL} ${LIB} -lpthread
  

test_ThreadPool: ThreadPool.h ThreadPool.cpp test_ThreadPool.cpp
  g++ ThreadPool.cpp test_ThreadPool.cpp -o test_ThreadPool -lpthread

clean:
  rm -rf *.o mt_cache cache_cmd
