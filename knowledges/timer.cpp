#include <unistd.h>
#include <sys/timerfd.h>
#include <stdio.h>
#include <iostream>
#include <stdint.h>
#include <string>

int main()
{
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd < 0)
    {
        std::cerr << "timerfd_create fail" << std::endl;
        exit(errno);
    }

    struct itimerspec itm;
    // 设置第一次超时时间
    itm.it_value.tv_sec = 3;
    itm.it_value.tv_nsec = 0;
    // 设置周期超时时间
    itm.it_interval.tv_sec = 3;
    itm.it_interval.tv_nsec = 0;

    int ret = timerfd_settime(timerfd, 0, &itm, nullptr); // 设置
    if (ret < 0)
    {
        std::cerr << "timerfd_settime fail" << std::endl;
        exit(errno);
    }

    time_t start = time(NULL);
    while (true)
    {
        uint64_t num = 0;
        int ret = read(timerfd, &num, sizeof(num));
        if (ret < 0)
        {
            std::cerr << "read fail" << std::endl;
            exit(errno);
        }
        printf("发生%ld次超时: 已经过去了%ld秒\n", num, time(NULL)-start);
    }
    close(timerfd);
    
    return 0;
}