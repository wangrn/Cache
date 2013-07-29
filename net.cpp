#include <errno.h>
#include <sys/time.h>

#include "net.h"
#include "protocol.h"

int set_nonblock(int sock)
{
//TODO
}

int set_sndbuf_size(int sock, int size)
{
    //TODO
}

int create_tcp_server(const char *ip, unsigned short port)
{
    //TODO
}

int create_tcp_client(const char *ip, unsigned short port)
{
    //TODO
}

int assure_read_package_length_msgpeek(int sock, unsigned int &bytes, unsigned long long timeout_msec)
{
    //TODO
}

int assure_read_package(int sock, const unsigned int expect_bytes, char *data, const unsigned long long timeout_msec)
{
    //TODO
}

int assure_write(int sock, const viud *data, int len, unsigned long long timeout_msec)
{
    //TODO
}

int assure_write(int sock, vecotr<iovec>& data, vector<unsigned int>& cumulate_size, const unsigned long long timeout_msec)
{
    //TODO
}


void connection::onevent(int e)
{
    //TODO
}

void connection::onwrite()
{
    //TODO
}

void connection::reset()
{
    //TODO
}

void connection::onclose(int r)
{
    //TODO
}

void connection::response(Command cmd, int err, unsigned char* data, unsinged int len)
{
    //TODO
}

void listen_connection::onevent(int e)
{
    //TODO
}
