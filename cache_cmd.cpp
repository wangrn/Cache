#include <stdio.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

#include "protocol.h"
#include "client.h"
#include "helper.h"

#ifdef __VERSION_ID__
char __version_id__[] = __VERSION_ID__;
#else
char __version_id__[] = "unknown";
#endif 

#if defined(__DATE__) && defined(__TIME__)
char __built_date__[] = __DATE__ " " __TIME__;
#else
char __built_date__[] = "unknown";
#endif

void print_version()
{
    cout << "   Version : " << __version_id__ << endl;
    cout << "Built date : " << __built_date__ << endl;
}
void print_usage()
{
    cout << "Usage: cache_cmd -hvurs"<< endl;
    cout << "  -h       Print this help message." << endl;
    cout << "  -v       Print version info." << endl;
    cout << "  -g       Get statues about remote server." << endl;
    cout << "  -u       Set state of remote server to be updating." << endl;
    cout << "  -e       Set state of remote server to be established." << endl;
    cout << "  -a       Set state of remote server to be appending." << endl;
    cout << "  -s       Shut down remote server" << endl;
}

extern const char* iplist[];

//TODO define callback
typedef string (*CALLBACK)(unsigned char *buf);

int do_work(int cmd, int msec, CALLBACK cb)
{
    vector<string> remote_ip;
    vector<unsigned short> remote_port;

    client c;
    for (int i = 0; iplist[i]; i ++)
    {
        remote_ip.push_back(iplist[i]);
        remote_port.push_back(9527);
    }
    
    c.add_remote_server(remote_ip, remote_port);
    c.request(cmd, NULL, 0, msec);

    client::client_connection *cc = c.get_connection();
    while (cc)
    {
        if (!cc->is_done())
        {
            string status = cc->dump_status();
            FILE *fp = fopen("err.log", "a+");
            if (fp)
            {
                fprintf(fp, "[ERROR] %s\n", status.c_str());
                fclose(fp);
            }
            _exit(1);
        }
        else
        {
            response_header *rh = (response_header*)cc->recv_buf.data();
            if (rh->err != 0)
                printf("request done but remote error! response_header={length=%u, err=%d, cmd=%d}\n", rh->length, rh->err, rh->cmd);
            else
                printf("request done! response_header={length=%u, err=%d, cmd=%d} %s [%s:%d]\n", rh->length, rh->err, rh->cmd, (cb(cc->recv_buf.data()+sizeof(response_header))).c_str(), cc->ip.c_str(), cc->port);
        }
        cc = c.get_connection();
    }
    return 0;
}


string get_status_callback(unsigned char *data)
{
    static char buf[128];
    unsigned* user_count = (unsigned*)(data);
    unsigned* max_simultaneous_read = (unsigned*)(data+sizeof(unsigned));
    unsigned* max_simultaneous_write = (unsigned*)(data+sizeof(unsigned)*2);

    sprintf(buf, "usercnt=%u ms_read=%u ms_write=%u", *user_count, *max_simultaneous_read, *max_simultaneous_write);
    return buf;
}
string shut_down_callback(unsigned char *data)
{
    return "";
}
string append_callback(unsigned char *data)
{
    return "";
}
string update_callback(unsigned char *data)
{
    return "";
}
string established_callback(unsigned char *data)
{
    return "";
}

void test()
{
    cout << __FUNCTION__ << endl;
    do_work(CMD_CACHE_GET_STATUS, 1000, &get_status_callback);
}

int main(int argc, char *argv[])
{
    char c = 0;
    while (-1 != (c = getopt(argc, argv, "hvgueas")))
    {

#define CASE(arg, cmd, msec, cb)             \
        case arg:                    \
            return do_work(cmd, msec, cb)

        switch(c)
        {
        case 'h':
            print_usage();
            return 0;
        case 'v':
            print_version();
            return 0;
        CASE('g', CMD_CACHE_GET_STATUS, 10000, &get_status_callback);
        CASE('e', CMD_CACHE_UPDATE_STATE_ESTABLISHED, 1200000, &established_callback);
        CASE('u', CMD_CACHE_UPDATE_STATE_UPDATING, 600000, &update_callback);
        CASE('a', CMD_CACHE_UPDATE_STATE_APPENDING, 600000, &append_callback);
        CASE('s', CMD_CACHE_SHUTDOWN, 1000, &shut_down_callback);
        default:
            print_usage();
            return -1;
        }

    }
    return 0;
}
