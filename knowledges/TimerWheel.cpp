// #pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unistd.h>

using TaskFunc_t = std::function<void()>;
using ReleaseFunc_t = std::function<void()>;

class TimerTask
{
private:
    uint64_t _id;           // 定时器任务对象ID
    uint32_t _delay;        // 定时任务的超时时间
    bool _canceled;         // 任务是否被取消, false-表示没有被取消， true-表示被取消
    TaskFunc_t _task;       // 定时器对象要执行的定时任务
    ReleaseFunc_t _release; // 用于删除TimerWheel中保存的定时器对象信息

public:
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc_t &task)
        : _id(id), _delay(delay), _task(task), _canceled(false)
    {
    }

    ~TimerTask()
    {
        if(!_canceled)
            _task();

        _release();
    }

    void SetRelease(ReleaseFunc_t release)
    {
        _release = release;
    }

    void Cancel() { _canceled = true; }

    uint32_t getDelay() { return _delay; }
};

class TimerWheel
{
private:
    const static int _default_capacity = 60;
    using Task_Sharedptr = std::shared_ptr<TimerTask>;
    using Task_Weakptr = std::weak_ptr<TimerTask>;

    int _tick;                                          // 当前的秒针，走到哪里释放哪里，释放哪里，就相当于执行哪里的任务
    int _capacity;                                      // 表盘最大数量---其实就是最大延迟时间
    std::vector<std::vector<Task_Sharedptr>> _wheel;    // 时间轮
    std::unordered_map<uint64_t, Task_Weakptr> _timers; // 用于复制shared_ptr

public:
    TimerWheel(int capacity = _default_capacity)
        : _tick(0), _capacity(capacity), _wheel(_capacity)
    {
    }

    ~TimerWheel() {}

    void DelTimerTask(int id)
    {
        auto iter = _timers.find(id);
        if (iter == _timers.end())
            return;

        _timers.erase(iter);
    }

    void AddTimerTask(int id, int delay, const TaskFunc_t &task)
    {
        // 0. 不能重复添加id相同的任务
        auto iter = _timers.find(id);
        if (iter != _timers.end())
        {
            std::cout << "不能重复添加id相同的任务: " << id << std::endl;
            return;
        }
        // 1. 创建一个任务, 并设置Release
        Task_Sharedptr task_ptr = std::make_shared<TimerTask>(id, delay, task);
        task_ptr->SetRelease(std::bind(&TimerWheel::DelTimerTask, this, id));
        // 2. 插入时间轮
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(task_ptr);
        // 3. 插入timers
        _timers[id] = Task_Weakptr(task_ptr);
    }

    // 刷新/延迟定时任务
    void RefreshTimerTask(int id)
    {
        auto iter = _timers.find(id);
        if (iter == _timers.end())
        {
            std::cout << "没有" << id << "任务, " << "无法刷新" << std::endl;
            return;
        }

        Task_Sharedptr task_ptr = iter->second.lock(); // lock获取weak_ptr管理的对象对应的shared_ptr
        int pos = (_tick + task_ptr->getDelay()) % _capacity;
        _wheel[pos].push_back(task_ptr);
    }

    // 取消定时任务
    void CancelTimerTask(int id)
    {
        auto iter = _timers.find(id);
        if (iter == _timers.end())
        {
            std::cout << "没有" << id << "任务, " << "无法删除" << std::endl;
            return;
        }
        Task_Sharedptr task_ptr = iter->second.lock();
        task_ptr->Cancel();
    }

    // 这个函数应该每秒钟被执行一次，相当于秒针向后走了一步
    void Step()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空指定位置的数组，就会把数组中保存的所有管理定时器对象的shared_ptr释放掉
    }
};

void Print()
{
    std::cout << "定时任务被执行" << std::endl;
}

int main()
{
    int id = 666;
    int delay = 5;

    TimerWheel wheel;
    wheel.AddTimerTask(id, delay, Print);

    time_t start = time(NULL);
    int cnt = 0;
    while (true)
    {
        sleep(1);
        cnt++;

        if (cnt == 3)
        {
            wheel.RefreshTimerTask(id);
        }
        // if (cnt == 7)
        // {
        //     wheel.CancelTimerTask(id);
        // }
        std::cout << "已经过去了:" << time(NULL) - start << "秒" << std::endl;
        wheel.Step();
    }

    return 0;
}