#ifndef CLIENT_H
#define CLIENT_H

#include <vector>
#include <string>

#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <netinet/tcp.h>
using namespace std;

#include "net.h"
#include "thread.h"
#include "protocol.h"


class fd_guard
{
public:
    fd_guard(int _fd)
    {
        fd = _fd;
    }
    ~fd_guard()
    {
        if (fd > 3)
        {
            close(fd);
            fd = -1;
        }
    }
private:
    fd_guard();
    int fd;
};


class client
{
public:
    class client_connection : public thread_runner
    {
    public:
        static const int state_connecting = 1;
        static const int state_sending = 2;
        static const int state_reading = 3;
        static const int state_done = 4;
        
        struct sockaddr_in addr;
        string ip;
        unsigned short port;
        
        int sock;
        int state;
        int last_errno;
        int cmd;
        const char* to_send_data;
        unsigned send_bytes;
        io_vector<unsigned char> recv_buf;
        int timeout_msec;
        bool bad_response;
        bool timeout;
        int elapse_msec;
        int max_retry_times;
        
        client_connection():sock(-1)
        {
            reset();
        }
        
        bool is_done()
        {
            return state == state_done;
        }
        
        string dump_status()
        {
            char buf[1024];
            sprintf(buf, "[%s:%d] state:%d errno:%d bad_response:%s timeout:%s", 
                            ip.c_str(), 
                            port,
                            state,
                            last_errno,
                            bad_response?"true":"false",
                            timeout?"true":"false");
            return buf;
        }
        
        void reset()
        {
            reset_infrastructure();
            
            to_send_data = 0;
            send_bytes = 0;
            cmd = -1;
            timeout_msec = -1;
        }
        void reset_infrastructure()
        {
            if (sock >= 0) close(sock);
            sock = -1;
            state = state_connecting;
            recv_buf.clear();
            last_errno = 0;
            bad_response = false;
            timeout = false;
            elapse_msec = 0;
        }
        
        int check_connect()
        {
            char c = 0;
            while (1)
            {
                struct tcp_info tcpinfo;
                socklen_t len = sizeof(tcpinfo);
                getsockopt(sock, IPPROTO_TCP, TCP_INFO, (char*)&tcpinfo, &len);
                if (TCP_SYN_SENT == tcpinfo.tcpi_state) return 1;
                if (TCP_ESTABLISHED == tcpinfo.tcpi_state) return 0;
                last_errno = EINPROGRESS;
                return -1;
                
                int ret = recv(sock, &c, 1, MSG_PEEK);
                last_errno = errno;
                if (ret == 0) return -1;
                if (ret > 0) return 0;
                if (errno == EINTR) continue;
                if (errno == EINPROGRESS) return 1;
                if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
                return -1;
            }
        }
        
        int send_request()
        {
            char buf[1024];
            request_header* rh = (request_header*)buf;
            rh->length = send_bytes + 8;
            rh->cmd = cmd;
            
            if (assure_write(sock, buf, 8) < 0)
            {
                last_errno = errno;
                return -1;
            }
            if (to_send_data > 0)
            {
                if (assure_write(sock, to_send_data, send_bytes) < 0)
                {
                    last_errno = errno;
                    return -1;
                }
            }
            return 0;
        }
        
        int recv_response()
        {
            unsigned char buf[8192];
            while (1)
            {
                int ret = recv(sock, buf, sizeof(buf), 0);
                if (ret == 0) return 0;
                if (ret < 0)
                {
                    if (errno == EINTR) continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
                    return -1;
                }
                
                recv_buf.append(buf, ret);
                if (recv_buf.size() > 12)
                {
                    response_header* rh = (response_header*)recv_buf.data();
                    if (rh->length <= recv_buf.size())
                            return 0;
                }
            }
            return 0;
        }
        
        virtual void run()
        {
            int try_times = 1;
            while (try_times < 3)
            {
                reset_infrastructure();
                
                run_core();
                if (state == state_done) return;
                try_times += 1;
            }
        }
        
        virtual void run_core()
        {
            //printf("begin to run!\n");
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0)
            {
                last_errno = errno;
                return;
            }
            
            set_nonblock(sock);
            set_sndbuf_size(sock, 256*1024);
            
            connect(sock, (struct sockaddr*)&addr, sizeof(addr));
            
            struct timeval t_start, t_end;
            gettimeofday(&t_start, NULL);
            unsigned long long msec_start = t_start.tv_sec*1000+t_start.tv_usec/1000;
            unsigned long long msec_end = msec_start;
            while (msec_end - msec_start < timeout_msec)
            {
                if (state == state_connecting)
                {
                    int ret = check_connect();
                    if (ret < 0) return;
                    if (ret == 0)
                    {
                        last_errno = 0;
                        state = state_sending;
                    }
                    // ret > 0 : do nothing
                }
                
                if (state == state_sending)
                {
                    int ret = send_request();
                    if (ret < 0) return;
                    if (ret == 0)
                    {
                        last_errno = 0;
                        state = state_reading;
                    }
                }
                
                if (state == state_reading)
                {
                    int ret = recv_response();
                    if (ret < 0) return;
                    if (ret == 0)
                    {
                        last_errno = 0;
                        if (recv_buf.size() < 12) bad_response = true;
                        else
                        {
                            response_header* rh = (response_header*)recv_buf.data();
                            if (rh->length != recv_buf.size()) bad_response = true;
                            else
                            {
                                state = state_done;
                                gettimeofday(&t_end, NULL);
                                msec_end = t_end.tv_sec*1000+t_end.tv_usec/1000;
                                elapse_msec = msec_end - msec_start;
                            }
                        }
                        return;
                    }
                }
                gettimeofday(&t_end, NULL);
                msec_end = t_end.tv_sec*1000+t_end.tv_usec/1000;
                
                usleep(1000);
            }
            //printf("connection timeout: %s:%d\n", ip.c_str(), port);
            timeout = true;
            elapse_msec = msec_end - msec_start;
        }
    };

    client():running_cnt(0) {}
    
    void add_remote_server(vector<string>& ips, vector<unsigned short>& ports)
    {
        if (cs.size() > 0) return;
        
        for (int i = 0; i < ips.size(); ++i)
        {
            client_connection* c = new client_connection;
            c->ip = ips[i];
            c->port = ports[i];
            c->addr.sin_family = AF_INET;
            c->addr.sin_port = htons(c->port);
            c->addr.sin_addr.s_addr = inet_addr(c->ip.c_str()); 
            
            cs.push_back(c);
            ts.push_back(new worker_thread);
        }
        assure_start_worker_thread(ts);
    }

    unsigned server_count()
    {
        return cs.size();
    }

    void request(int cmd, char* data, unsigned bytes, unsigned msec, int64_t routeid=-1)
    {
        assert(running_cnt == 0);
        if (running_cnt > 0) return;
        
        int mod = -1;
        if(routeid >= 0) mod = routeid%server_count();
        
        running_cnt = 0;
        for (int i = 0; i < cs.size(); ++i)
        {
            if (mod>=0 && mod != i) continue;
            
            client_connection* c = cs[i];
            c->reset();
            c->to_send_data = data;
            c->send_bytes = bytes;
            c->timeout_msec = msec;
            c->cmd = cmd;
            
            ts[i]->add_task(c);
            running_cnt += 1;
        }
    }

    void request_group(int cmd, vector<char*>& data, vector<unsigned>& bytes, unsigned msec)
    {
        assert(running_cnt == 0);
        if (running_cnt > 0) return;
        
        running_cnt = 0;
        for (int i = 0; i < cs.size(); ++i)
        {
            if (i >= data.size() || data[i] == NULL || bytes[i] == 0) continue;
            
            client_connection* c = cs[i];
            c->reset();
            c->to_send_data = data[i];
            c->send_bytes = bytes[i];
            c->timeout_msec = msec;
            c->cmd = cmd;
            ts[i]->add_task(c);
            running_cnt += 1;
        }
    }
    
    client_connection* get_connection()
    {
        if (running_cnt == 0) return 0;
        
        while (1)
        {
            for (int i = 0; i < ts.size(); ++i)
            {
                client_connection* c = (client_connection*)ts[i]->get_task();
                if (c)
                {
                    running_cnt -= 1;
                    return c;
                }
            }
            usleep(1000);
        }
    }

    bool request_finished()
    {
        return running_cnt == 0;
    }

    vector<client_connection*> cs;
    vector<worker_thread*> ts;
    int running_cnt;
};

#endif
