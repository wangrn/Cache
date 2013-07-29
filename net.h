#ifndef NET_H
#define NET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <queue>
#include <algorithm>
#include <string>
using namespace std;


int set_nonblock(int fd);
int set_sndbuf_size(int sock, int size);
int set_reuse(int sock, int reuse);
int set_sock_linger(int sock, struct linger ling);

int create_tcp_server(const char * ip, unsigned short port);
int create_tcp_client(const char* ip, unsigned short port);

int assure_write(int sock, const void* data, int len, const int64_t timeout_msec = -1);
int assure_write(int sock, vector<iovec>& data, vector<unsigned>& cumulate_size, const int64_t timeout_msec = -1);

int assure_read_package_length_msgpeek(int sock, unsigned& bytes, int64_t timeout_msec);
int assure_read_package(int sock, const unsigned expect_bytes, char* data, const int64_t timeout_msec);

template <typename T>
class io_vector
{
public:
    io_vector():a(0),cap(0),sz(0) {}

    T* data() {return a;}
    unsigned capacity() { return cap; }
    unsigned size() { return sz; }

    void reserve(int nc) {
        recapacity(nc);  
    }

    void recapacity(int nc) {
        if (nc <= cap) return;
        if (cap == 0) cap = 1;
        while (nc > cap) { cap <<= 1; }
        a = (T*)realloc(a,cap*sizeof(T));
    }

    void append(T* _a, unsigned len) {
        if (len+sz > cap) {
            recapacity(len+sz);
        }
        memcpy(a+sz,_a,sizeof(T)*len);
        sz += len;
    }

    void clear() {
        sz = 0;
    }

    void zero() {
        if (a) {
            memset(a,0,sizeof(T)*cap);
        }
    }
   
private:
    T* a;
    unsigned cap;
    unsigned sz;
};

struct ev_io_arg{
    //ev_io body;
    void* arg;
};
struct ev_timer_arg{
    //ev_timer body;
    void* arg;
};



class connection;
class connection_manager
{
public:
    virtual ~connection_manager() {}
    virtual void collect_connection(connection* c) = 0;
    virtual connection* apply_connection() = 0;
    virtual int idle_connection_count() = 0;
};

class simple_connection_manager : public connection_manager
{
public:
    queue<connection*> conns;
    
    virtual ~simple_connection_manager() {}
    void add_connection(connection* c)
    {
        conns.push(c);
    }
    virtual void collect_connection(connection* c)
    {
        conns.push(c);
    }
    virtual connection* apply_connection()
    {
        if (conns.size() == 0)
            return NULL;
        connection* ret = conns.front();
        conns.pop();
        return ret;
    }
    virtual int idle_connection_count()
    {
        return conns.size();
    }
};


class connection
{
public:
    int sock;
    int last_errno;
    sockaddr_in addr;
    io_vector<unsigned char> rb;
    ev_io_arg watcher;
    ev_timer_arg timer;
    connection_manager* cm;
    struct ev_loop * loop;
    bool free_buff_after_send;

    connection()
    {
        sock = -1;
        last_errno = 0;
        memset(&addr, 0 ,sizeof(addr));
        rb.reserve(1024*1024*1);
        rb.zero();
        cm = NULL;
        loop = NULL;
        free_buff_after_send = true;
    }
    
    virtual void init(int _sock, float timeout, struct ev_loop* _loop, connection_manager* _cm)
    {
        sock = _sock;
        rb.clear();
        loop = _loop;
        cm = _cm;
        
        //ev_io_init((ev_io*)&watcher, sockin_cb,  sock, EV_READ);
        //ev_io_start(loop, (ev_io*)&watcher);
        //watcher.arg = this;
        
        //ev_timer_init((ev_timer*)&timer, timeout_cb, timeout, 0.);
        //ev_timer_start(loop, (ev_timer*)&timer);
        timer.arg = this;
    }

    virtual void onevent(int e);
    virtual void onwrite();
    //virtual void ontimeout(ev_timer*);
    virtual void onclose(int r); //r=0:peer close,r=-1:read error,r=-2:time out
    virtual void reset();

    virtual int oncommand() { return 0 ;}
    virtual void response(int cmd, int err, unsigned char* data, unsigned len);
    
protected:
    class send_state
    {
    public:
        bool sending;
        bool send_head;
        unsigned char* data;
        unsigned send_cnt;
        unsigned len;
        int cmd;
        int err;
        unsigned char head_buff[sizeof(unsigned)*3];
        
        send_state()
        {
            reset();
        }
        void reset()
        {
            sending = false;
            send_head = false;
            cmd = -1;
            data = 0;
            len = 0;
            send_cnt = 0;
            err = -1;
        }
    };
    send_state ss;
};

class listen_connection : public connection
{
public:
    virtual void onevent(int e);

    virtual void init(int _sock, float timeout, struct ev_loop* _loop, connection_manager* _cm)
    {
        sock = _sock;
        rb.clear();
        loop = _loop;
        cm = _cm;
        //ev_io_init((ev_io*)&watcher, sockin_cb,  sock, EV_READ);
        //ev_io_start(loop, (ev_io*)&watcher);
        watcher.arg = this;
    }
};
#endif
