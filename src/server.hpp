#pragma once
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cassert>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include "signal.h"
#include "../knowledges/Any.hpp"

#define INF 0
#define DBG 1
#define ERR 2
#define DEFAULT_LOG_LEVEL INF
#define LOG(level, format, ...)                                                                                               \
    {                                                                                                                         \
        if (level >= DEFAULT_LOG_LEVEL)                                                                                       \
        {                                                                                                                     \
            std::stringstream oss;                                                                                            \
            std::thread::id thread_id = std::this_thread::get_id();                                                           \
            oss << thread_id;                                                                                                 \
            std::string log_thread_id_text = oss.str();                                                                       \
            time_t t = time(NULL);                                                                                            \
            struct tm *m = localtime(&t);                                                                                     \
            char ts[32] = {0};                                                                                                \
            strftime(ts, 31, "%H:%M:%S", m);                                                                                  \
            fprintf(stdout, "[%s %s %s:%d] " format "\n", log_thread_id_text.c_str(), ts, __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                                                                                     \
    }
#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__);
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__);
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__);

class Buffer
{
private:
    uint64_t _read_idx;
    uint64_t _write_idx;
    std::vector<char> _buffer;

private:
    inline char *Begin() { return _buffer.data(); }
    inline const char *Begin() const { return _buffer.data(); }

public:
    // 构造
    Buffer() : _read_idx(0), _write_idx(0) {}

    Buffer(const std::string &data)
        : _read_idx(0), _write_idx(0), _buffer(data.size())
    {
        Write(data, true);
    }

    Buffer(const Buffer &other)
        : _read_idx(0), _write_idx(0), _buffer(other.ReadableSize())
    {
        Write(other, true);
    }

    // 返回可读大小
    size_t ReadableSize() const { return _write_idx - _read_idx; }
    // 获取可读可写空间
    size_t TailSpace() const { return _buffer.size() - _write_idx; }
    size_t HeadSpace() const { return _read_idx; }
    // 获取可读和可写位置(真实地址)
    inline char *GetReadPos() { return Begin() + _read_idx; }
    inline char *GetWritePos() { return Begin() + _write_idx; }
    inline const char *GetReadPos() const { return Begin() + _read_idx; }
    inline const char *GetWritePos() const { return Begin() + _write_idx; }

    // 空间预留
    void Reserve(size_t len)
    {
        // 如果后面的可写空间足够, 什么都不用做
        if (len <= TailSpace())
            return;
        else
        {
            size_t space = HeadSpace() + TailSpace();
            // 如果整体预留空间足够, 进行龙空间整理
            if (len <= space)
            {
                size_t read_size = ReadableSize();
                std::copy(GetReadPos(), GetReadPos() + read_size, Begin());
                _read_idx = 0;
                _write_idx = read_size;
            }
            // 如果整体空间不够, 原数据位置不动, 向后扩容len个字节
            else
            {
                _buffer.resize(_buffer.size() + len);
            }
        }
    }

    bool Read(void *out, size_t len, bool is_move)
    {
        if (len == 0)
            return true;
        // 超过可读范围什么都不做
        if (len > ReadableSize())
            return false;

        std::copy(GetReadPos(), GetReadPos() + len, (char *)out);
        if (is_move)
            MoveReadIdx(len);
        return true;
    }

    bool Read(std::string *out, size_t len, bool is_move)
    {
        if (len == 0)
            return true;
        // 超过可读范围什么都不做
        if (len > ReadableSize())
            return false;

        // 把当前_buffer的可读内容赋给out
        out->assign(GetReadPos(), len);
        if (is_move)
            MoveReadIdx(len);
        return true;

        // std::string tmp(len, '\0');
        // if(!Read(&tmp[0], len, is_move))
        //      return false;
        // out->swap(tmp);
        // return true;
    }

    const char *FindCRLF()
    {
        if (ReadableSize() == 0)
            return nullptr;
        const char *pos = static_cast<const char *>(std::memchr(GetReadPos(), '\n', ReadableSize()));
        return pos;
    }

    bool GetLine(std::string *out, bool is_move)
    {
        const char *pos = FindCRLF();
        if (pos == nullptr)
            return false;
        return Read(out, static_cast<size_t>(pos - GetReadPos()) + 1, is_move);
    }

    void Write(const void *in, size_t len, bool is_move)
    {
        if (len == 0)
            return;
        // 确保空间足够
        Reserve(len);
        // 拷贝数据到缓冲区
        const char *data = (const char *)in;
        std::copy(data, data + len, GetWritePos());
        if (is_move)
            MoveWriteIdx(len);
    }

    void Write(const std::string &in, bool is_move)
    {
        Write(in.c_str(), in.size(), is_move);
    }

    void Write(const Buffer &buffer, bool is_move) // todo: 参数不能设置为const 怎么办?
    {
        size_t len = buffer.ReadableSize();
        if (len == 0)
            return;
        // 确保空间足够
        Reserve(len);
        // 拷贝数据到缓冲区
        std::copy(buffer.GetReadPos(), buffer.GetWritePos(), GetWritePos());
        if (is_move)
            MoveWriteIdx(len);
    }

    // 移动读写指针
    void MoveReadIdx(size_t len)
    {
        assert(len <= ReadableSize());
        _read_idx += len;
    }
    void MoveWriteIdx(size_t len)
    {
        assert(len <= TailSpace());
        _write_idx += len;
    }
};

const static int defaultBackLog = 10;
class NetWork
{
public:
    NetWork()
    {
        if(::signal(SIGPIPE, SIG_IGN))
            DBG_LOG("忽略SIGPIPE信号");
    }
};
/*避免服务器因为给断开连接的客⼾端进⾏send触发异常导致程序崩溃，因此忽略SIGPIPE信号*/
/*定义静态全局是为了保证构造函数中的信号忽略处理能够在程序启动阶段就被直接执⾏*/
static NetWork nw;

class Socket
{
private:
    int _sockfd;

public:
    Socket(int sockfd = -1) : _sockfd(sockfd) {}
    ~Socket() { Close(); }
    // 防止一个文件描述被多个Socket使用, Socket b = a;
    // 比如 b关闭fd=3后, a还维护着这个已被关闭的fd=3, 下次有新文件到来, fd=3, a调用close会误关闭
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;

    int GetFd() { return _sockfd; }

    void Close()
    {
        if (_sockfd != -1)
        {
            ::close(_sockfd);
            _sockfd = -1;
        }
    }

    // 创建套接字
    bool Create()
    {
        _sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (_sockfd < 0)
        {
            ERR_LOG("create socket fail: %s", strerror(errno));
            return false;
        }
        return true;
    }

    // 绑定地址信息
    bool Bind(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);

        int ret = ::bind(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("bind socket fail: %s", strerror(errno));
            return false;
        }
        return true;
    }

    // 开始监听
    bool Listen(int backlog = defaultBackLog)
    {
        int ret = ::listen(_sockfd, backlog);
        if (ret < 0)
        {
            ERR_LOG("listen socket fail: %s", strerror(errno));
            return false;
        }
        return true;
    }

    // 向服务器发起连接
    int Connect(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);

        int ret = ::connect(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("connect socket fail: %s", strerror(errno));
            return false;
        }
        return true;
    }

    // 获取新连接
    int Accept()
    {
        // 暂时不需要客户端的ip和port信息
        int newfd = ::accept(_sockfd, nullptr, nullptr);
        if (newfd < 0)
        {
            ERR_LOG("listen socket fail: %s", strerror(errno));
            return -1;
        }
        return newfd;
    }

    // 接收数据
    // >0: bytes received; 0: retry later; -1: EOF or fatal error.
    ssize_t Recv(void *buf, size_t n, int flag = 0)
    {
        // ssize_t recv(int sockfd, void *buf, size_t len, int flag);
        ssize_t ret = ::recv(_sockfd, buf, n, flag);
        if (ret == 0)
            return -1;
        if (ret < 0)
        {
            // 这种错误是可以接受的
            // EAGAIN 当前socket的接收缓冲区中没有数据了，在⾮阻塞的情况下才会有这个错误;
            // EINTR 表⽰当前socket的阻塞等待，被信号打断了，
            if (errno == EAGAIN || errno == EINTR)
                return 0; // 表示这次没有接收到数据
            // 其它情况是recv出错了
            ERR_LOG("socket recv fail: %s", strerror(errno));
            return -1;
        }
        return ret; // 实际接收的数据⻓度
    }

    ssize_t NonBlockRecv(void *buf, size_t len)
    {
        return Recv(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表⽰当前接收为非阻塞。
    }

    // 发送数据
    ssize_t Send(const void *buf, size_t n, int flag = 0)
    {
        // ssize_t send(int sockfd, void *data, size_t len, int flag);
        ssize_t ret = ::send(_sockfd, buf, n, flag);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                return 0; // 表示这次没有发送数据
            ERR_LOG("socket send fail: %s", strerror(errno));
            return -1;
        }
        return ret;
    }

    // 非阻塞发送
    ssize_t NonBlockSend(void *buf, size_t len)
    {
        return Send(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表⽰当前发送为⾮阻塞
    }
    // 创建⼀个服务端连接
    bool CreateListenSocket(uint16_t port, const std::string &ip = "0.0.0.0", bool is_nonblock = false)
    {
        // 1. 创建套接字，
        if (!Create())
            return false;
        if (
            !ReuseAddress()                 // 2. 启动地址重⽤
            || (is_nonblock && !NonBlock()) // 3. 启用非阻塞
            || !Bind(ip, port)              // 4. 绑定地址
            || !Listen()                    // 5. 开始监听
        )
        {
            Close();
            return false;
        }
        return true;
    }

    // 创建⼀个客⼾端连接
    bool CreateClientSocket(uint16_t port, const std::string &ip)
    {
        // 1. 创建套接字，
        if (!Create())
            return false;
        // 2.指向连接服务器
        if (!Connect(ip, port))
            return false;
        return true;
    }

    // 设置套接字选项---开启地址端⼝重⽤
    bool ReuseAddress()
    {
        int val = 1;
        socklen_t len = sizeof(int);
        // 地址复用
        if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &val, len) < 0)
        {
            ERR_LOG("setsockopt fail: %s", strerror(errno));
            return false;
        }
        // 端口复用
        val = 1;
        if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &val, len) < 0)
        {
            ERR_LOG("setsockopt fail: %s", strerror(errno));
            return false;
        }
        return true;
    }

    // 设置套接字阻塞属性-- 设置为⾮阻塞
    bool NonBlock()
    {
        // int fcntl(int fd, int cmd, ... /* arg */ );
        int flag = ::fcntl(_sockfd, F_GETFL);
        if (flag < 0)
        {
            ERR_LOG("fcntl F_GETFL fail: %s", strerror(errno));
            return false;
        }
        int ret = ::fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
        if (ret < 0)
        {
            ERR_LOG("fcntl F_SETFL fail: %s", strerror(errno));
            return false;
        }
        return true;
    }
};

class Poller;
class EventLoop;
class Channel
{
    using EpollCallback_t = std::function<void()>;

private:
    // 事件回调
    EpollCallback_t _read_func;
    EpollCallback_t _write_func;
    EpollCallback_t _error_func;
    EpollCallback_t _close_func;
    EpollCallback_t _normal_func;

    int _fd;           // sockfd
    uint32_t _events;  // 需要被监听的事件
    uint32_t _revents; // 已经就绪的事件
    // Poller *_poller;   // 多路复用
    EventLoop *_loop; // 多路复用

    bool _closed; // 是否被关闭
public:
    // Channel(int fd, Poller *poller) : _fd(fd), _events(0), _revents(0), _poller(poller), _closed(false) {}
    Channel(int fd, EventLoop *loop) : _fd(fd), _events(0), _revents(0), _loop(loop), _closed(false) {}

    // 获得文件描述符, 事件
    int GetFd() { return _fd; }
    // 是否可读/写
    bool Readable() { return _events & EPOLLIN; }
    bool Writable() { return _events & EPOLLOUT; }
    // 获取事件
    uint32_t GetEvents() { return _events; }
    uint32_t GetREvents() { return _revents; }
    void SetREvents(uint32_t events) { _revents = events; } // 设置实际就绪的事件
    // 设置回调
    void SetReadFunc(const EpollCallback_t &cb) { _read_func = cb; }
    void SetWriteFunc(const EpollCallback_t &cb) { _write_func = cb; }
    void SetErrorFunc(const EpollCallback_t &cb) { _error_func = cb; }
    void SetCloseFunc(const EpollCallback_t &cb) { _close_func = cb; }
    void SetNormalFunc(const EpollCallback_t &cb) { _normal_func = cb; }

    void SetClosed() { _closed = true; }
    bool IsClosed() { return _closed; }
    // 设置读/写监听事件
    bool EnableRead()
    {
        _events |= EPOLLIN;
        return Update();
    }
    bool EnableWrite()
    {
        _events |= EPOLLOUT;
        return Update();
    }
    bool DisableRead()
    {
        _events &= ~EPOLLIN;
        return Update();
    }
    bool DisableWrite()
    {
        _events &= ~EPOLLOUT;
        return Update();
    }
    bool DisableAll()
    {
        _events = 0;
        return Update();
    }

    bool Update();
    bool Remove();
    void HandlerEvents()
    {
        if (_closed)
            return;

        // Callbacks may mark this Channel closed, but must not delete it.
        auto dispatch = [this](const EpollCallback_t &callback)
        {
            if (_closed)
                return;

            // 先刷新定时任务, 再执行回调.
            // 如果先执行回调, 可能执行时间很长, 到时候执行结束就超时了
            if (_normal_func)
                _normal_func();
            if (!_closed && callback)
                callback();
        };

        if (_revents & (EPOLLIN | EPOLLRDHUP | EPOLLPRI))
            dispatch(_read_func);
        if (_revents & EPOLLOUT)
            dispatch(_write_func);
        if (_revents & EPOLLERR)
            dispatch(_error_func);
        if (_revents & EPOLLHUP)
            dispatch(_close_func);
    }
    // void HandlerEvents()
    // {
    //     if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
    //     {
    //         if (_normal_func)
    //             _normal_func();
    //         if (_read_func)
    //             _read_func();
    //     }
    //     if (_revents & EPOLLOUT)
    //     {
    //         if (_normal_func)
    //             _normal_func();
    //         if (_write_func)
    //             _write_func();
    //     }
    //     if (_revents & EPOLLERR)
    //     {
    //         if (_normal_func)
    //             _normal_func();
    //         if (_error_func)
    //             _error_func();
    //     }
    //     if (_revents & EPOLLHUP)
    //     {
    //         if (_normal_func)
    //             _normal_func();
    //         if (_close_func)
    //             _close_func();
    //     }
    // }
};

#define MAX_EPOLL_EVENT 1024
class Poller
{
private:
    int _epfd;
    std::unordered_map<int, Channel *> _channels;
    struct epoll_event _evs[MAX_EPOLL_EVENT];

public:
    Poller()
    {
        _epfd = ::epoll_create(MAX_EPOLL_EVENT);
        if (_epfd < 0)
        {
            ERR_LOG("epoll_create fail: %s", strerror(errno));
            abort();
        }
    }
    ~Poller() { ::close(_epfd); }
    // 和Socket一样, 防止重复关闭
    Poller(const Poller &) = delete;
    Poller &operator=(const Poller &) = delete;

    bool Update(Channel *channel, int op)
    {
        int fd = channel->GetFd();
        struct epoll_event event;
        event.data.fd = fd;
        event.events = channel->GetEvents();

        int n = ::epoll_ctl(_epfd, op, fd, &event);
        if (n < 0)
        {
            ERR_LOG("epoll_ctl fail: %s", strerror(errno));
            return false;
        }
        return true;
    }

    bool HasChannel(Channel *channel)
    {
        auto it = _channels.find(channel->GetFd());
        if (it == _channels.end())
            return false;
        return true;
    }

    bool Update_Events(Channel *channel)
    {
        if (!HasChannel(channel))
        {
            if (!Update(channel, EPOLL_CTL_ADD))
                return false;
            _channels[channel->GetFd()] = channel;
            return true;
        }
        else
        {
            if (!Update(channel, EPOLL_CTL_MOD))
                return false;
            return true;
        }
    }

    bool Del_Events(Channel *channel)
    {
        if (HasChannel(channel))
        {
            if (!Update(channel, EPOLL_CTL_DEL))
                return false;
            _channels.erase(channel->GetFd());
            return true;
        }
        ERR_LOG("不存在该channel, 无法删除:%d", channel->GetFd());
        return false;
    }

    int Poll(std::vector<Channel *> *ready_channels)
    {
        int nfds = epoll_wait(_epfd, _evs, MAX_EPOLL_EVENT, -1);
        if (nfds < 0)
        {
            // 允许被信号打断
            if (errno == EINTR)
                return -1;

            ERR_LOG("epoll_wait fail: %s", strerror(errno));
            abort();
        }

        ready_channels->clear();
        for (int i = 0; i < nfds; i++)
        {
            int fd = _evs[i].data.fd;
            uint32_t events = _evs[i].events;

            auto it = _channels.find(fd);
            assert(it != _channels.end()); // 活跃的连接一定会被管理起来, 找不到说明管理出现问题

            it->second->SetREvents(events);
            ready_channels->push_back(it->second);
        }
        return nfds;
    }
};

/*定时器*/
using TaskFunc_t = std::function<void()>;
using ReleaseFunc_t = std::function<void()>;
class TimerTask
{
private:
    uint64_t _id;           // 定时器任务对象ID
    uint32_t _delay;        // 定时任务的超时时间
    bool _canceled;         // 任务是否被取消, false-表示没有被取消， true-表示被取消
    TaskFunc_t _task;       // 定时器对象要执行的定时任务
    ReleaseFunc_t _release; // 用于删除TimerWheel中保存的定时器对象信息(_timers维护的)

public:
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc_t &task)
        : _id(id), _delay(delay), _task(task), _canceled(false)
    {
    }

    ~TimerTask()
    {
        if (!_canceled)
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

    EventLoop *_eventloop;
    int _timer_fd; // 定时器描述符--可读回调就是读取定时器, 执行定时任务
    std::unique_ptr<Channel> _timer_channel;

    int InitTimerFd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            ERR_LOG("timerfd_create fail: %s", strerror(errno));
            abort();
        }

        struct itimerspec itm;
        // 设置第一次超时时间
        itm.it_value.tv_sec = 1;
        itm.it_value.tv_nsec = 0;
        // 设置周期超时时间
        itm.it_interval.tv_sec = 1;
        itm.it_interval.tv_nsec = 0;

        int ret = timerfd_settime(timerfd, 0, &itm, nullptr); // 设置
        if (ret < 0)
        {
            ERR_LOG("timerfd_settime fail: %s", strerror(errno));
            abort();
        }
        return timerfd;
    }

    uint64_t ReadTimerFd()
    {
        uint64_t res;
        int ret = ::read(_timer_fd, &res, sizeof(res));
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("read _event_fd fail: %s", strerror(errno));
            abort();
        }
        return res;
    }

    void DelTimerTask(uint64_t id)
    {
        auto iter = _timers.find(id);
        if (iter == _timers.end())
            return;

        _timers.erase(iter);
    }

    void TimerFdReadFunc()
    {
        // 1. 看看超时几次, 因为可能有其它IO任务执行了不止一秒
        uint64_t res = ReadTimerFd();
        // 2. 超时几次, 时针转动几次
        for (uint64_t i = 0; i < res; ++i)
            Step();
    }

    void AddTimerTaskInLoop(uint64_t id, int delay, const TaskFunc_t &task)
    {
        // 0. 不能重复添加id相同的任务
        auto iter = _timers.find(id);
        if (iter != _timers.end())
        {
            INF_LOG("不能重复添加id相同的任务: %ld", id);
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
    void RefreshTimerTaskInLoop(uint64_t id)
    {
        auto iter = _timers.find(id);
        if (iter == _timers.end())
        {
            INF_LOG("没有 %llu 任务, 无法刷新", static_cast<unsigned long long>(id));
            return;
        }

        Task_Sharedptr task_ptr = iter->second.lock(); // lock获取weak_ptr管理的对象对应的shared_ptr
        int pos = (_tick + task_ptr->getDelay()) % _capacity;
        _wheel[pos].push_back(task_ptr);
    }

    // 取消定时任务
    void CancelTimerTaskInLoop(uint64_t id)
    {
        auto iter = _timers.find(id);
        if (iter == _timers.end())
        {
            INF_LOG("没有 %ld 任务, 无法删除", id);
            return;
        }

        // 走到说明_timers里肯定有这个任务. 不过不确定它引用计数为 0 还是 1
        Task_Sharedptr task_ptr = iter->second.lock();
        // 大部分情况这里面的引用计数都至少是1, 能拿到一个shared_ptr
        // 但如果 TimerTask 的析构中定时任务已经被执行了, 但是还没执行 _release, 此时_timers维护着这个引用计数为0的weak_ptr
        // 而那个任务恰好是清理定时任务, 所以会调用到这里使用这个shared_ptr, 但是这个shared_ptr的引用计数为0, 也就是task_ptr为空;
        // 所以要判断一下task_ptr是否为空, 如果为空就不需要取消了, 因为这个定时任务正在被执行.
        if (task_ptr)
            task_ptr->Cancel();
    }

public:
    TimerWheel(EventLoop *loop, int capacity = _default_capacity)
        : _tick(0), _capacity(capacity), _wheel(_capacity),
          _eventloop(loop),
          _timer_fd(InitTimerFd()),
          _timer_channel(new Channel(_timer_fd, _eventloop))
    {
        _timer_channel->SetReadFunc(std::bind(&TimerWheel::TimerFdReadFunc, this));
        _timer_channel->EnableRead();
    }

    ~TimerWheel() { ::close(_timer_fd); }

    // 这个函数应该每秒钟被执行一次，相当于秒针向后走了一步
    void Step()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空指定位置的数组，就会把数组中保存的所有管理定时器对象的shared_ptr释放掉
    }

    // 因为很多定时任务都考虑到连接的操作(不确定对不对)
    // 因为定时器中_timer成员, wheel成员, 定时器信息的操作有可能在多线程中进行, 因此需要考虑线程安全问题, 所以不能直接运行
    // 如果不想加锁, 那就把对定期的所有操作, 放在同一个线程中去执行(放到timerwheel依赖的那个eventloop线程)
    void AddTimerTask(uint64_t id, int delay, const TaskFunc_t &task);
    void RefreshTimerTask(uint64_t id);
    void CancelTimerTask(uint64_t id);
    /*这个接口存在线程安全问题--这个接口实际上不能被外界使用者调用, 只能在模块内，在
对应的EventLoop线程内执⾏*/
    bool HasTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return false;
        }
        return true;
    }
};

class EventLoop
{
    using event_task_t = std::function<void()>;

private:
    Poller _poller;

    std::thread::id _thread_id; // 该 eventloop 线程绑定的 id
    int _event_fd;
    std::unique_ptr<Channel> _event_channel;

    std::vector<event_task_t> _tasks;
    std::mutex _mutex;

    TimerWheel _timer_wheel;

private:
    int CreateEventFd()
    {
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd < 0)
        {
            ERR_LOG("eventfd fail: %s", strerror(errno));
            abort();
        }
        return efd;
    }

    void EventFdRead()
    {
        uint64_t res;
        int ret = read(_event_fd, &res, sizeof(res));
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                // 无事件发生
                return;
            }
            ERR_LOG("read _event_fd fail: %s", strerror(errno));
            abort();
        }
        return;
    }

    void WakeUpEventFd()
    {
        uint64_t val = 1;
        int n = ::write(_event_fd, &val, sizeof(val));
        if (n < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("write _event_fd fail: %s", strerror(errno));
            abort();
        }
        return;
    }

    void RunAllTasks()
    {
        std::vector<event_task_t> tasks;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            tasks.swap(_tasks);
        }
        for (auto &f : tasks)
            f();
        return;
    }

public:
    EventLoop() : _thread_id(std::this_thread::get_id()),
                  _event_fd(CreateEventFd()),
                  _event_channel(new Channel(_event_fd, this)),
                  _timer_wheel(this)
    {
        _event_channel->EnableRead();
        _event_channel->SetReadFunc(std::bind(&EventLoop::EventFdRead, this));
    }

    // 判断当前线程是否是EventLoop对应的线程
    bool IsInLoop()
    {
        return _thread_id == std::this_thread::get_id();
    }

    void QueueInLoop(const event_task_t &task)
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.push_back(task);
        }
        // 任务队列中有任务, 但如果没有事件就绪, 程序一直卡在Loop, 无法执行任务; 所以有任务来了就触发一下读就绪
        WakeUpEventFd();
    }

    void RunInLoop(const event_task_t &task)
    {
        // 如果任务是当前线程的任务, 直接执行就行
        if (IsInLoop())
            return task();
        // 否则需要把任务压入任务队列
        return QueueInLoop(task);
    }

    void Loop()
    {
        while (true)
        {
            std::vector<Channel *> actives;
            // 1. 事件监控
            _poller.Poll(&actives);
            // 2. 处理就绪事件
            for (auto &channel : actives)
            {
                channel->HandlerEvents();
            }
            // 3. 处理任务队列中的任务
            RunAllTasks();
        }
    }

    bool Del_Events(Channel *channel)
    {
        return _poller.Del_Events(channel);
    }
    bool Update_Events(Channel *channel)
    {
        return _poller.Update_Events(channel);
    }

    /*定时器*/
    void AddTimerTask(uint64_t id, int delay, const TaskFunc_t &task)
    {
        _timer_wheel.AddTimerTask(id, delay, task);
    }

    void RefreshTimerTaskInLoop(uint64_t id)
    {
        _timer_wheel.RefreshTimerTask(id);
    }

    void CancelTimerTask(uint64_t id)
    {
        _timer_wheel.CancelTimerTask(id);
    }

    bool HasTimer(uint64_t id)
    {
        return _timer_wheel.HasTimer(id);
    }
};

class Connection;
// 连接的四种状态: 已断开连接, 已连接, 半断开连接, 半连接
typedef enum
{
    DISCONNECTED,
    CONNECTED,
    DISCONNECTING,
    CONNECTING
} ConnStatus;
/* 内部回调中对于连接的操作都用Connection_Shared. 如果用Connection*, 外部已经释放连接,
但一些收尾工作还没有做完, 导致使用野指针. 使用智能指针的话, 外部就算释放也只是引用计数-1,
能保证函数内这个指针是有效的, 收尾工作做完, 指针计数清零也就释放了, 相当于延迟释放  */
using Connection_Shared = std::shared_ptr<Connection>;
class Connection : public std::enable_shared_from_this<Connection>
{
private:
    uint64_t _conn_id;
    uint64_t _timer_id;            // 定时任务id, 使用和_conn_id一样的数值, 因为都是唯一的
    bool _enable_inactive_release; // 是否启用非活跃连接释放功能, 默认false
    int _sock_fd;
    EventLoop *_loop;
    ConnStatus _status;
    Socket _socket;
    Channel _channel;
    Buffer _in_buffer;
    Buffer _out_buffer;
    Any _context;

    using ConnectedCallback = std::function<void(const Connection_Shared &)>;
    using MessageCallback = std::function<void(const Connection_Shared &, Buffer *)>;
    using ClosedCallback = std::function<void(const Connection_Shared &)>;
    using AnyEventCallback = std::function<void(const Connection_Shared &)>;

    ConnectedCallback _connected_cb;
    MessageCallback _message_cb;
    ClosedCallback _closed_cb;
    AnyEventCallback _anyevent_cb;
    ClosedCallback _server_closed_cb; // 服务器连接关闭回调函数, 用于内部把连接从服务器的连接管理中删除
private:
    /*Channel的事件处理函数*/
    // 读事件处理回调:
    // 1. 从Socket中读取数据到 in_buffer 中;
    // 2. 调用用户注册的消息回调函数
    void HandleRead()
    {
        // 1. 从Socket中读取数据到 in_buffer 中
        char buf[65536];
        ssize_t n = _socket.NonBlockRecv(buf, sizeof(buf));
        if (n < 0)
        {
            // 读取出错就关闭连接
            ShutDownInLoop();
            return;
        }
        // 读取成功, 存进输入缓冲区
        _in_buffer.Write(buf, n, true);
        // 2. 调用用户注册的消息回调函数
        if (_message_cb)
            _message_cb(shared_from_this(), &_in_buffer);
    }
    // 写事件处理回调:
    // 将发送缓冲区中的数据写入Socket中, 如果发送缓冲区为空, 就关闭写事件监控
    void HandleWrite()
    {
        ssize_t n = _socket.NonBlockSend(_out_buffer.GetReadPos(), _out_buffer.ReadableSize());
        if (n < 0)
        {
            // 如果发送错误, 检查输入缓冲区是否有数据待处理, 处理完关闭连接.
            // (这里不能再ShutdownInLoop, 防止死循环)
            if (_in_buffer.ReadableSize() > 0)
                _message_cb(shared_from_this(), &_in_buffer);
            return RealeseConnectionInLoop();
        }
        // 发送成功, 移动发送缓冲区的读指针
        _out_buffer.MoveReadIdx(n);
        if (_out_buffer.ReadableSize() == 0)
        {
            // 发送缓冲区为空, 关闭写事件监控
            if (_channel.Writable())
                _channel.DisableWrite();
            // 如果是半关闭状态, 发送缓冲区还空了, 直接释放连接
            if (_status == DISCONNECTING)
            {
                // 真正释放连接
                RealeseConnectionInLoop();
            }
        }
    }
    // 描述符触发挂断事件
    void HandleClose()
    {
        /*⼀旦连接挂断了，套接字就什么都干不了了，因此有数据待处理就处理⼀下，完毕关闭连接*/
        if (_in_buffer.ReadableSize() > 0)
            _message_cb(shared_from_this(), &_in_buffer);
        return RealeseConnectionInLoop();
    }
    // 描述符触发出错事件
    void HandleError()
    {
        return HandleClose();
    }
    // 描述符触发任意事件: 1. 刷新连接的活跃度--延迟定时销毁任务; 2. 调用组件使用者的任意事件回调
    void HandleNormal()
    {
        if (_enable_inactive_release)
            _loop->RefreshTimerTaskInLoop(_timer_id);
        if (_anyevent_cb)
            _anyevent_cb(shared_from_this());
    }

    /*Connection建立和释放*/
    // 连接获取之后, 所处状态下要进行各种设置(启动读监控, 调用回调)
    void EstablishConnectionInLoop()
    {
        assert(_status == CONNECTING);
        _status = CONNECTED;
        // 1. 启动读事件监控
        // (这一步不能放在构造里, 因为启动了读监控可能立即触发读事件, 如果这时候启动了非活跃连接销毁规则,
        // 那就要刷新活跃度, 到时候去定时器中找不到这个连接的定时任务. 所以启动读监控应该是在启动活跃连接销毁规则之后设置的)
        // todo: 我感觉放构造里也可以啊?
        _channel.EnableRead();
        // 2. 调用组件使用者的连接建立回调函数
        if (_connected_cb)
            _connected_cb(shared_from_this());
    }

    // 真正进行连接的释放
    // 1. 把 channel 从 poller 中删除;
    // 2. 关闭socket;
    // 3. 如果有定时任务, 取消定时任务(防止时间到了之后又重复释放连接);
    // 4. 调用组件使用者的连接关闭回调函数
    // 5. 调用服务器连接关闭回调函数
    void RealeseConnectionInLoop()
    {
        if (_status == DISCONNECTED)
            return;
        _status = DISCONNECTED;
        // 1. 把 channel 从 poller 中删除;
        _loop->Del_Events(&_channel);
        // 2. 关闭socket;
        _socket.Close();
        // 3. 如果有定时任务, 取消定时任务(防止时间到了之后又重复释放连接);
        if (_loop->HasTimer(_timer_id))
            DisableInactiveReleaseInLoop();
        // 4. 调用组件使用者的连接关闭回调函数
        if (_closed_cb)
            _closed_cb(shared_from_this());
        // 5. 调用服务器连接关闭回调函数
        if (_server_closed_cb)
            _server_closed_cb(shared_from_this());
    }

    /*Connection功能函数*/
    // 发送数据, 不是真的发送, 而是: 1. 把数据放到发送缓冲区; 2. 启动写事件监控
    void SendInLoop(const void *buff, size_t len)
    {
        if (_status == DISCONNECTED)
            return;
        // 1. 把数据放到发送缓冲区
        _out_buffer.Write(buff, len, true);
        // 2. 启动写事件监控
        if (!_channel.Writable())
            _channel.EnableWrite();
    }

    // 关闭连接:
    // 1. 检查输入缓冲区是否有数据待处理, 如果有, 先处理;
    // 2. 检查发送缓冲区是否有数据待发送, 如果有, 先发送;
    void ShutDownInLoop()
    {
        if (_status == DISCONNECTED)
            return;
        _status = DISCONNECTING;
        // 1. 检查输入缓冲区是否有数据待处理, 如果有, 先处理; (用户层, 不涉及Socket)
        while (_in_buffer.ReadableSize() > 0)
            _message_cb(shared_from_this(), &_in_buffer);
        // 2. 检查发送缓冲区是否有数据待发送: (用户层和内核层, 涉及Socket)
        // 2.1 如果没有数据待发送, 直接释放连接;
        if (_out_buffer.ReadableSize() == 0)
            RealeseConnectionInLoop();
        // 2.2 如果有, 先发送, 发送完自动调用ReleaseConnectionInLoop释放连接;
        else
        {
            if (!_channel.Writable())
                _channel.EnableWrite();
        }
    }

    // 启动非活跃连接释放功能
    void EnableInactiveReleaseInLoop(int sec)
    {
        // 1. 将判断标志位置为true
        _enable_inactive_release = true;
        // 2. 如果有定时任务, 刷新一下
        if (_loop->HasTimer(_timer_id))
            _loop->RefreshTimerTaskInLoop(_timer_id);
        // 3. 如果没有定时任务, 添加一个定时任务
        else
            // todo: 为什么不传ShutDownInLoop?
            _loop->AddTimerTask(_timer_id, sec, std::bind(&Connection::RealeseConnectionInLoop, this));
    }

    // 关闭非活跃连接释放功能
    void DisableInactiveReleaseInLoop()
    {
        // 1. 将判断标志位置为false
        _enable_inactive_release = false;
        // 2. 取消定时任务
        _loop->CancelTimerTask(_timer_id);
    }

    void ProtocolExchangeInLoop(const Any &context,
                                const ConnectedCallback &conn_cb,
                                const MessageCallback &mess_cb,
                                const ClosedCallback &closed_cb,
                                const AnyEventCallback &anyevent_cb)
    {
        _context = context;
        _connected_cb = conn_cb;
        _message_cb = mess_cb;
        _closed_cb = closed_cb;
        _anyevent_cb = anyevent_cb;
    }

public:
    Connection(uint64_t conn_id, int sock_fd, EventLoop *loop) : 
    _conn_id(conn_id), _timer_id(conn_id), _enable_inactive_release(false),                                                        
    _sock_fd(sock_fd), _loop(loop), _status(CONNECTING), _socket(sock_fd), _channel(sock_fd, loop)
    {
        _channel.SetReadFunc(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteFunc(std::bind(&Connection::HandleWrite, this));
        _channel.SetCloseFunc(std::bind(&Connection::HandleClose, this));
        _channel.SetErrorFunc(std::bind(&Connection::HandleError, this));
        _channel.SetNormalFunc(std::bind(&Connection::HandleNormal, this));
    }
    ~Connection() { DBG_LOG("RELEASE CONNECTION:%p", this); }

    uint64_t GetConnId() { return _conn_id; }
    int GetFd() { return _sock_fd; }
    bool IsConnected() { return _status == CONNECTED; }
    void SetContext(const Any &context) { _context = context; }
    Any *GetContext() { return &_context; }

    // 不是真正发送数据, 而是先把数据放到发送缓冲区, 然后启动写事件监控
    void Send(const void *buff, size_t len)
    {
        _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, buff, len));
    }
    // 不是真正的关闭连接, 而应该在实际释放连接之前, 看看输入输出缓冲区内是否有数据待处理. 都处理完了再关闭连接
    void ShutDown()
    {
        _loop->RunInLoop(std::bind(&Connection::ShutDownInLoop, this));
    }
    // 启用非活跃连接释放功能
    void EnableInactiveRelease(int sec)
    {
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
    }
    // 关闭非活跃连接释放功能
    void DisableInactiveRelease()
    {
        _loop->RunInLoop(std::bind(&Connection::DisableInactiveReleaseInLoop, this));
    }
    // 建立连接
    void EstablishConnection()
    {
        _loop->RunInLoop(std::bind(&Connection::EstablishConnectionInLoop, this));
    }

    /* 协议切换.
    注意: 这个函数只能在线程内部去调用, 如果其它线程调用, 导致该函数压入任务队列,
    而此时有新事件就绪, 要执行事件就绪的处理函数, 而此时协议切换还在队列里,
    导致事件仍然以旧的协议去解析.  所以该函数需要在线程内调用, 使其立即得到执行, 不能被压入任务队列.
    */
    void ProtocolExchange(const Any &context,
                          const ConnectedCallback &conn_cb,
                          const MessageCallback &mess_cb,
                          const ClosedCallback &closed_cb,
                          const AnyEventCallback &anyevent_cb)
    {
        assert(_loop->IsInLoop());
        ProtocolExchangeInLoop(context, conn_cb, mess_cb, closed_cb, anyevent_cb);
    }
    void SetConnectedCallback(const ConnectedCallback &cb) { _connected_cb = cb; }
    void SetMessageCallback(const MessageCallback &cb) { _message_cb = cb; }
    void SetClosedCallback(const ClosedCallback &cb) { _closed_cb = cb; }
    void SetAnyEventCallback(const AnyEventCallback &cb) { _anyevent_cb = cb; }
    void SetServerClosedCallback(const ClosedCallback &cb) { _server_closed_cb = cb; }

};

class Acceptor
{
private:
    Socket _listen_socket;
    EventLoop *_loop;
    Channel _channel;

    using AcceptorCallback = std::function<void(int)>;
    AcceptorCallback _accept_cb;

private:
    int CreateSocket(int port)
    {
        bool ret = _listen_socket.CreateListenSocket(port);
        if (!ret)
        {
            ERR_LOG("服务器套接字创建失败");
            abort();
        }
        return _listen_socket.GetFd();
    }
    // 读事件回调
    // 1. 获取新连接
    // 2. 调用 _accept_callback 函数进行新连接处理
    void HandlerRead()
    {
        // 1. 获取新连接
        int client_fd = ::accept(_channel.GetFd(), nullptr, nullptr);
        if (client_fd < 0)
        {
            ERR_LOG("accpet failed, %s", strerror(errno));
            return;
        }
        // 2. 调用 _accept_callback 函数进行新连接处理
        if (_accept_cb)
            _accept_cb(client_fd);
    }

public:
    Acceptor(uint16_t port, EventLoop *loop)
        : _listen_socket(CreateSocket(port)), _loop(loop), _channel(_listen_socket.GetFd(), _loop)
    {
        _channel.SetReadFunc(std::bind(&Acceptor::HandlerRead, this));
    }

    void SetAcceptorCallback(const AcceptorCallback &cb)
    {
        _accept_cb = cb;
    }

    // 启动读事件监听不能放在构造函数里
    // 因为如果立马有事件就绪, 但是还没设置accept_cb, 会导致这个连接没人处理, 资源泄露;
    // 所以外部调用应该先SetAcceptorCallback, 再EnableRead
    void EnableRead()
    {
        _channel.EnableRead();
    }

    void Listen()
    {
        _loop->Loop();
    }
};

class LoopThread
{
private:
    bool _is_free;
    std::mutex _mutex;
    std::condition_variable _cond;
    EventLoop *_loop;
    std::thread _thread;

private:
    void ThreadEntry()
    {
        // 1. 在线程内部初始化EventLoop, 此时loop内的thread_id一定是当前线程的id
        // 2. EventLoop的生命周期和线程一样, 线程任务执行完之后, 线程结束, EventLoop也跟着销毁.
        EventLoop loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _loop = &loop;
            _cond.notify_one();
        }
        loop.Loop();
    }

public:
    LoopThread() : _is_free(true), _loop(nullptr), _thread(std::bind(&LoopThread::ThreadEntry, this))
    {
    }

    EventLoop *GetLoop()
    {
        // 外部调用的时候有可能_loop还没有被赋值, 所以需要保证同步: 先初始化_loop, 再返回_loop
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [this]()
                       { return _loop != nullptr; });
            return _loop;
        }
    }

    bool IsFree()
    {
        return _is_free;
    }

    void SetFree()
    {
        _is_free = true;
    }

    void SetBusy()
    {
        _is_free = false;
    }
};

class LoopThreadPool
{
private:
    int _next_id;
    int _thread_count;
    EventLoop *_base_loop;              // 主Reactor线程的EventLoop
    std::vector<LoopThread *> _threads; // 从reactor线程池
    std::vector<EventLoop *> _loops;    // 从reactor对应的eventloop

public:
    LoopThreadPool(EventLoop *base_loop, int thread_count = 0)
        : _next_id(0), _thread_count(thread_count), _base_loop(base_loop)
    {
    }

    ~LoopThreadPool()
    {
        for (auto &thread : _threads)
            delete thread;
    }

    void SetThreadNum(int thread_count)
    {
        _thread_count = thread_count;
    }

    void SetThreadFree(int thread_pos)
    {
        _threads[thread_pos]->SetFree();
    }
    
    bool IsFull()
    {
        for(int i = 0; i < _thread_count; ++i)
        {
            if(_threads[i]->IsFree())
                return false;
        }
        return true;
    }

    void Init()
    {
        if (_thread_count > 0)
        {
            _threads.resize(_thread_count);
            _loops.resize(_thread_count);

            for (int i = 0; i < _thread_count; ++i)
            {
                _threads[i] = new LoopThread();
                _loops[i] = _threads[i]->GetLoop();
            }
        }
        return;
    }

    EventLoop *NextLoop(int *p_thread_pos)
    {
        if (_thread_count == 0)
            return _base_loop;
        // 判断线程是否都被分配完?
        if(IsFull())
        {
            ERR_LOG("没有多余的从Reactor线程可以分配");
            return nullptr;
        }
        // 找一个空闲的
        while(!_threads[_next_id]->IsFree())
            _next_id = (_next_id + 1) % _thread_count;

        _threads[_next_id]->SetBusy();
        *p_thread_pos = _next_id;

        return _loops[_next_id];
    }
};


class TcpServer
{
    struct Connection_Item
    {
        Connection_Item()
        :_thread_pos(-1), _connection(nullptr)
        {}

        Connection_Item(const Connection_Shared &conn, int thread_pos = -1)
        :_thread_pos(thread_pos), _connection(conn)
        {}

        int _thread_pos;
        Connection_Shared _connection;
    };
private:
    uint64_t _conn_id;
    uint64_t _timer_id;
    uint16_t _port;
    int _timeout;
    bool _enable_inactive_release;
    int _thread_num;
    EventLoop _base_loop;
    Acceptor _acceptor;
    LoopThreadPool _pool;
    std::unordered_map<uint64_t, Connection_Item> _conns;

    using ConnectedCallback = std::function<void(const Connection_Shared &)>;
    using MessageCallback = std::function<void(const Connection_Shared &, Buffer *)>;
    using ClosedCallback = std::function<void(const Connection_Shared &)>;
    using AnyEventCallback = std::function<void(const Connection_Shared &)>;

    ConnectedCallback _connected_cb;
    MessageCallback _message_cb;
    ClosedCallback _closed_cb;
    AnyEventCallback _anyevent_cb;

private:
    void NewConnection(int client_fd)
    {
        int thread_pos = -1;
        EventLoop *eventloop = _pool.NextLoop(&thread_pos);
        if (!eventloop)
        {
            ERR_LOG("为新连接分配EventLoop失败");
            ::close(client_fd);
            return;
        }

        ++_conn_id;
        Connection_Shared client_conn = std::make_shared<Connection>(_conn_id, client_fd, eventloop);
        client_conn->SetMessageCallback(_message_cb);
        client_conn->SetConnectedCallback(_connected_cb);
        client_conn->SetClosedCallback(_closed_cb);
        client_conn->SetAnyEventCallback(_anyevent_cb);
        // BUG: 不要重复包装, 这样即使_anyevent_cb是空, Connection里设置的仍然是个bind的function对象, 到时候判断为非空
        // client_conn->SetAnyEventCallback(std::bind(_anyevent_cb, std::placeholders::_1));  
        client_conn->SetServerClosedCallback(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
        
        if(_enable_inactive_release)
            client_conn->EnableInactiveRelease(_timeout); // 先添加定时任务
        
        client_conn->EstablishConnection();    // 再启动读事件监控

        Connection_Item conn_item(client_conn, thread_pos); 
        _conns[_conn_id] = conn_item;
    }

    void RemoveConnectionInLoop(const Connection_Shared &conn)
    {
        // 1. 将对应的从reactor线程设置为空闲
        int thread_pos = _conns[conn->GetConnId()]._thread_pos;
        // 单reactor不需要, 即thread_pos = -1
        if(thread_pos >= 0)
            _pool.SetThreadFree(thread_pos); // 该线程设置为空闲

        // 2. 释放被管理的连接对象
        int conn_id = conn->GetConnId();
        auto it = _conns.find(conn_id);
        if (it == _conns.end())
        {
            ERR_LOG("未被管理的连接: %d", conn_id);
            return;
        }
        _conns.erase(conn_id); // 意味着该连接即将被释放 
    }

    void RemoveConnection(const Connection_Shared &conn)
    {
        _base_loop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
    }

    void RunAfterInLoop(const TaskFunc_t &task, int delay)
    {
        _base_loop.AddTimerTask(++_timer_id, delay, task);
    }
public:
    TcpServer(int port, int thread_num = 0)
        : _conn_id(0), _timer_id(0), _port(port), _timeout(-1), _enable_inactive_release(false),  
        _thread_num(thread_num), _acceptor(port, &_base_loop), _pool(&_base_loop, _thread_num)
    {
        // 1. 初始化线程池, 准备好子线程及EventLoop
        _pool.Init();
        // 2. 设置Accetor的回调, 监听listen套接字的读事件
        _acceptor.SetAcceptorCallback(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
        _acceptor.EnableRead();
    }

    void SetConnectedCallback(const ConnectedCallback &cb) { _connected_cb = cb; }
    void SetMessageCallback(const MessageCallback &cb) { _message_cb = cb; }
    void SetClosedCallback(const ClosedCallback &cb) { _closed_cb = cb; }
    void SetAnyEventCallback(const AnyEventCallback &cb) { _anyevent_cb = cb; }

    void SetThreadCount(int count) { return _pool.SetThreadNum(count); }

    void EnableInactiveRelease(int timeout)
    {
        if(timeout >= 0)
        {
            _timeout = timeout;
            _enable_inactive_release = true;
        }
    }

    // 提供的额外接口, 允许使用者运行定时任务
    void RunAfter(const TaskFunc_t &task, int delay) 
    {
        _base_loop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop, this, task, delay));
    }

    void Start()
    {
        // 3. 开始监听
        _acceptor.Listen();
    }
};

/* Channel */
inline bool Channel::Remove() { return _loop->Del_Events(this); }
inline bool Channel::Update() { return _loop->Update_Events(this); }

/* TimerWheel */
void TimerWheel::AddTimerTask(uint64_t id, int delay, const TaskFunc_t &task)
{
    _eventloop->RunInLoop(std::bind(&TimerWheel::AddTimerTaskInLoop, this, id, delay, task));
    return;
}
void TimerWheel::RefreshTimerTask(uint64_t id)
{
    _eventloop->RunInLoop(std::bind(&TimerWheel::RefreshTimerTaskInLoop, this, id));
    return;
}
void TimerWheel::CancelTimerTask(uint64_t id)
{
    _eventloop->RunInLoop(std::bind(&TimerWheel::CancelTimerTaskInLoop, this, id));
    return;
}

