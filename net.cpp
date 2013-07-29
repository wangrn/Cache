#include <errno.h>
#include <sys/time.h>

#include "net.h"
#include "protocol.h"

int set_nonblock(int sock)
{
    int flags;
    flags = fcntl(sock, F_GETFL);
    fcntl(sock, F_SETFL, flags|O_NONBLOCK);
}

int set_sndbuf_size(int sock, int size)
{
    return setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size));
}

int set_reuse(int sock, int reuse)
{
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}

int set_sock_linger(int sock, struct linger ling)
{
    return setsockopt(sock, SOL_SOCKET, SO_LINGER, (void*)&ling, sizeof(ling));
}

int create_tcp_server(const char *ip, unsigned short port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    set_reuse(sock, 1);
    set_nonblock(sock);
    set_sndbuf_size(sock, 1024*1024*64);
    
    struct linger ling = {0, 0};
    set_sock_linger(sock, ling);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        return -2;
    }
    
    if (listen(sock, 20) < 20)
    {
        close(sock);
        return -3;
    }
    return sock;
}

int create_tcp_client(const char *ip, unsigned short port)
{
    //TODO
}

int assure_read_package_length_msgpeek(int sock, unsigned int &bytes, unsigned long long timeout_msec)
{
    char buf[8];
    int ret = 0;
    timeval t_start, t_end;
    gettimeofday(&t_start, NULL);
    t_end = t_start;
    unsigned long long m_start = (unsigned long long)(t_start.tv_sec)*1000+(unsigned long long)(t_start.tv_usec)/1000;
    
    while (true)
    {
        ret = recv(sock, buf, sizeof(buf), MSG_PEEK);
        if (ret > 0)
        {
            if (ret > sizeof(unsigned int))
            {
                memcpy(&bytes, buf, sizeof(bytes));
                return 0;
            }
        }
        else if (ret == 0)
            return -2;
        else
        {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                usleep(10);
            }
            else
                return -1;
        }
        
        if (timeout_msec > 0)
        {
            gettimeofday(&t_end, NULL);
            unsigned long long m_end = (unsigned long long)(t_end.tv_sec)*1000+(unsigned long long)(t_end.tv_usec)/1000;
            if (m_end - m_start >= timeout_msec)
                break;
        }
    }
    
    return -3;
}

#define RECV_BUF_LIMIT 8192
int assure_read_package(int sock, const unsigned int expect_bytes, char *data, const unsigned long long timeout_msec)
{
    int ret = 0;
    unsigned int offset = 0;
    timeval t_start, t_end;
    gettimeofday(&t_start, NULL);
    t_end = t_start;
    unsigned long long m_start = (unsigned long long)(t_start.tv_sec)*1000+(unsigned long long)(t_start.tv_usec)/1000;
    
    while (true)
    {
        if (offset >= expect_bytes) return 0;
        
        unsigned int left = expect_bytes - offset;
        if (left > RECV_BUF_LIMIT) left = RECV_BUF_LIMIT;
        
        ret = recv(sock, data+offset, left, 0);
        if (ret > 0)
        {
            offset += ret;
            if (offset >= expect_bytes) return 0;
        }
        else if (ret == 0)
        {
            if (offset >= expect_bytes) return 0;
            return -2; // connection closed by peer
        }
        else
        {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) usleep(10);
            else return -1;
        }
        
        if (timeout_msec > 0)
        {
            gettimeofday(&t_end, NULL);
            unsigned long long m_end = (unsigned long long)(t_end.tv_sec)*1000+(unsigned long long)(t_end.tv_usec)/1000;
            if (m_end - m_start >= timeout_msec)
                break;
        }
    }
    return -3; // timeout
}

int assure_write(int sock, const viud *data, int len, unsigned long long timeout_msec)
{
    timeval t_start, t_end;
    gettimeofday(&t_start, NULL);
    t_end = t_start;
    unsigned long long m_start = (unsigned long long)(t_start.tv_sec)*1000+(unsigned long long)(t_start.tv_usec)/1000;
    
    int offset = 0;
    while (true)
    {
        int send_cnt = send(sock, (const char*)data+offset, len-offset, 0);
        if (send_cnt < 0)
        {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) usleep(10);
            else return -1;
        }
        else if (send_cnt == 0)
            return -2; // close by peer
        else 
        {
            if (send_cnt + offset == len) return 0;
            offset += send_cnt;
            continue;
        }
        
        if (timeout_msec > 0)
        {
            gettimeofday(&t_end, NULL);
            unsigned long long m_end = (unsigned long long)(t_end.tv_sec)*1000+(unsigned long long)(t_end.tv_usec)/1000;
            if (m_end - m_start >= timeout_msec)
                break;
        }
    }
    
    return -3;
}

int assure_write(int sock, vecotr<iovec>& data, vector<unsigned int>& cumulate_size, const unsigned long long timeout_msec)
{
    timeval t_start, t_end;
    gettimeofday(&t_start, NULL);
    t_end = t_start;
    unsigned long long m_start = (unsigned long long)(t_start.tv_sec)*1000+(unsigned long long)(t_start.tv_usec)/1000;
    
    int offset = 0;
    while (true)
    {
        int send_cnt = writev(sock, data.data(), data.size());
        if (send_cnt < 0)
        {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) usleep(10);
            else return -1;
        }
        else if (send_cnt == 0)
            return -2; // close by peer
        else 
        {
            offset += send_cnt;
            int i = 0;
            for (; i < cumulate_size; i ++) 
                if (cumulate_size[i] > offset)
                    break;
            if (i >= cumulate_size.size())
                return 0;
                
            unsigned int block_offset = cumulate_size[i] - offset;
            data[i].iov_base = (char*)(data[i].iov_base)+(data[i].iov_len-block_offset);
            data[i].iov_len = block_offset;
            if (i > 0) 
            {
                data.erase(data.begin(), data.begin() + i);
                cumulate_size.erase(cumulate_size.begin(), cumulate_size.begin()+i);
            }
            continue;
        }
        
        if (timeout_msec > 0)
        {
            gettimeofday(&t_end, NULL);
            unsigned long long m_end = (unsigned long long)(t_end.tv_sec)*1000+(unsigned long long)(t_end.tv_usec)/1000;
            if (m_end - m_start >= timeout_msec)
                break;
        }
    }
    
    return -3;
}


void connection::onevent(int e)
{
    //TODO
}

void connection::onwrite()
{
    if (!ss.sending) return;
    if (!ss.send_head)
    {
        int ret = assure_write(sock, ss.head_buff, sizeof(ss.head_buff));
        if (ret < 0)
        {
            reset();
            return ;
        }
        ss.send_head = true;
        if (ss.len == 0)
        {
            reset();
            return ;
        }
    }
    
    while (true)
    {
        int send_cnt = send(sock, ss.data + ss.send_cnt, ss.len-ss.send_cnt, 0);
        if (send_cnt < 0)
        {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            reset();
            return;
        }
        else if (send_cnt == 0)
        {
            reset();
            return;
        }
        
        ss.send_cnt += send_cnt;
        if (ss.send_cnt >= ss.len)
        {
            if (free_buff_after_send)
                free(ss.data);
        }
        reset();
        return;
    }
}

void connection::reset()
{
    //TODO
}

void connection::onclose(int r)
{
    reset();
}

void connection::response(Command cmd, int err, unsigned char* data, unsinged int len)
{
    if (ss.sending) return;
    ss.reset();
    ss.sending = true;
    ss.cmd = cmd;
    ss.err = err;
    ss.data = data;
    ss.len = len;
    
    response_header *rh = (response_header*)ss.head_buff;
    rh->length = len+12;
    rh->cmd = cmd;
    rh->err = err;
    
    watcher.arg = this;
}

void listen_connection::onevent(int e)
{
    struct sockaddr_in client_addr;
    socklen_t al = sizeof(client_addr);
    int client_sock;
    
    client_sock = accept(sock, (struct sockaddr*)&client_addr, &al);
    set_nonblock(client_sock);
    connection *c = cm->apply_connection();
    c->init(client_sock, 10.0, loop, cm);
    
    if (cm->idle_connection_count() == 0)
    {
    }
}
