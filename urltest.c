

#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int p = -1;

int ex = 0;

void waitPipe()
{
    struct timespec ts;
    int rn = 0;
    char buff[128];
    int e = 0;

    ts.tv_sec = 0;
    ts.tv_nsec = 1000000;

    while (!ex)
    {
        nanosleep(&ts, NULL);
        rn = read(p, buff, 128);
        if (rn < 0)
        {
            e = errno;
            if (e != EAGAIN)
            {
                printf("Err: %d\n", e);
                break;
            }
        }
        else if (rn == 0)
        {
            printf("End of pipe\n");
            buff[127] = 0;
            printf("%s", buff);
            break;
        }
    }
    if (ex)
    {
        printf("Stopped\n");
    }
}

void handle_sig(int sig)
{
    if (sig == SIGINT)
    {
        ex = 1;
    }
}

int main(int argc, char ** argv)
{
    struct stat st;

    int pe = mkfifo("/tmp/skymodurl", 0666);
    if (pe == -1)
    {
        printf("Failed to create pipe\n");
    }

    p = open("/tmp/skymodurl", O_RDONLY);

    if (p == -1)
    {
        printf("Failed to open pipe\n");
        unlink("/tmp/skymodurl");
        return 1;
    }

    if (fstat(p, &st) == -1)
    {
        printf("Failed to stat pipe\n");
        unlink("/tmp/skymodurl");
        return 1;
    }

    if (!S_ISFIFO(st.st_mode))
    {
        printf("opened fd is not a fifo\n");
        close(p);
        return 1;
    }

    // not working but whatever
    signal(SIGINT, handle_sig);
    signal(SIGQUIT, handle_sig);

    waitPipe();

    close(p);
    unlink("/tmp/skymodurl");

    return 0;
}




