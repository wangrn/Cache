all : test_ThreadPool



test_ThreadPool: ThreadPool.h ThreadPool.cpp test_ThreadPool.cpp
  g++ ThreadPool.cpp test_ThreadPool.cpp -o test_ThreadPool -lpthread
