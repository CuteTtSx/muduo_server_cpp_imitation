#include <sys/eventfd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cerrno>

void event_start(int efd)
{
    uint64_t val = 1;
    write(efd, &val, sizeof(val));
}

int main()
{
    time_t t;
    /* 初始化随机数发生器 */
    srand((unsigned)time(&t));

    int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (efd < 0)
    {
        perror("eventfd fail\n");
        return -1;
    }

    while (true)
    {
        sleep(1);
        if (rand() % 2)
        {
            event_start(efd);
            if(rand() % 3)
                event_start(efd);
        }

        uint64_t result = 0;
        int ret = read(efd, &result, sizeof(result));
        if(ret <= 0)
        {
            if(errno == EAGAIN || errno == EINTR)
            {
                printf("无事件发生\n");
                continue;
            }
            perror("read fail\n");
            return -1;
        }
        else
        {
            printf("有%ld个事件发生\n", result);
        }
    }

    close(efd);
    return 0;
}