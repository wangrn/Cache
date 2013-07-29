#include <signal.h>

#include "thread.h"
#include "net.h"

struct settings
{
    int port;
    int udpport;
    int verbose;
    int thread_count;
};

struct settings g_settings;

static void settings_init()
{
    g_settings.port = 9527;
    g_settings.verbose = 0;
    g_settings.thread_count = 10;
}

static void usage()
{
    printf("-p <num>  TCP port number to listen on (default 9527)\n"
           "-h        print help\n"
           "-v        print errors or warnings\n"
           "-d        run as a daemon\n"
           "-t <num>  thread count\n");
}

static void sig_handler(const int sig)
{
    printf("SIGINT handled.\n");
    exit(EXIT_SUCCESS);
}

static int sig_ignore(int sig)
{
    sturct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    
    if (sigemptyset(&sa.sa_mask) == -1 || sigaction(sig, &sa, 0) == -1)
        return -1;
    return 0;
}

static bool do_daemonize = false;
int daemonize(int noclose)
{
    int fd;
    swtich(fork())
    {
    case -1:
        return -1;
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }
    
    if (setsid() == -1) return -2;
    if (noclose == 0 &&  (fd = open("/dev/null", O_RDWR, 0)) != -1)
    {
        if (dup2(fd, STDIN_FILENO) < 0) return -3;
        if (dup2(fd, STDOUT_FILENO) < 0) return -4;
        if (dup2(fd, STDERR_FILENO) < 0) return -5;
        if (fd > STDERR_FILENO && close(fd) < 0) return -6;
    }
    return 0;
}

/**
 * cache的运行调用
 */
extern int mt_cache_run(int port, int thread_count);

int main(int argc, char *argv[])
{
    settings_init();
    signal(SIGINT, sig_handler);
    setbuf(stderr, NULL);
    
    char c = 0;
    char *p0 = NULL, *p1 = NULL;
    while (-1 != (c = getopt(argc, argv, "p:hvdt:")))
    {
        switch(c)
        {
        case 'p':
            g_settings.port = atoi(optarg);
            break;
        case 't':
            g_settings.thread_count = atoi(optarg);
            break;
        case 'h':
            useage();
            exit(EXIT_SUCCESS);
        case 'v':
            g_settings.verbose ++;
            break;
        case 'd':
            do_daemonize = true;
            break;
        default:
            fprintf(stderr, "Illegal argument \"%c\"\n", c);
            return -1;
        }
    }
    
    fprintf(stderr, "settings.verbose=%d\n", g_settings.verbose);
    if (do_daemonize)
    {
        if (sig_ignore(SIGHUP) == -1)
            perror("Failed to ignore SIGHUP");
        if (daemonize(g_settings.verbose) == -1)
        {
            fprintf(stderr, "Failed to daemon() in order to daemonize\n");
            exit(EXIT_FAILURE);
        }
    }
    
    if (sigignore(SIGPIPE) == -1)
    {
        fprintf(stderr, "Failed to ignore SIGPIPE\n");
        exit(EXIT_FAILURE);
    }
    
    mt_cache_run(g_settings.port, g_settings.thread_count);
    return 0;
}
