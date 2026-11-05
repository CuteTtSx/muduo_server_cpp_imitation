// #include "../src/server.hpp"

// void test_buffer()
// {
//     Buffer buffer1;
//     std::string s = "hello\r\n";
//     buffer1.Write(s, true);

//     std::string result1;
//     bool ret = buffer1.Read(&result1, s.size(), true);
//     std::cout << result1 << " " << buffer1.ReadableSize() << std::endl;

//     Buffer buffer2;
//     std::string msg = "hello world\r\n";
//     buffer2.Write(msg, true);

//     std::string result2;
//     bool ret2 = buffer2.GetLine(&result2, true);
//     std::cout << result2 << " " << buffer2.ReadableSize() << std::endl;
// }

// void HandlerClose(Channel *channel)
// {
//     if (channel->IsClosed())
//         return;
//     if (!channel->Remove())
//     {
//         ERR_LOG("failed to unregister channel: %d", channel->GetFd());
//         return;
//     }
//     channel->SetClosed();
//     ::close(channel->GetFd());
//     DBG_LOG("client quit: %d", channel->GetFd());
//     // delete channel; //其实不应该delete, 应该放入待关闭队列中, 等待HandlerEvents调用完之后, 再回收
// }

// void HandlerWrite(Channel *channel)
// {
//     const char *msg = "hello world, im server";
//     int ret = ::send(channel->GetFd(), msg, strlen(msg), 0);
//     if (ret < 0)
//     {
//         HandlerClose(channel);
//         return;
//     }
//     channel->DisableWrite();
// }

// void HandlerRead(Channel *channel)
// {
//     char buffer[1024];
//     int ret = ::recv(channel->GetFd(), buffer, sizeof(buffer) - 1, 0);
//     if (ret < 0)
//     {
//         const int err = errno;
//         if (err == EAGAIN || err == EWOULDBLOCK || err == EINTR)
//             return;
//         HandlerClose(channel);
//         return;
//     }
//     if (ret == 0)
//     {
//         HandlerClose(channel);
//         return;
//     }

//     buffer[ret] = '\0';
//     DBG_LOG("client said: %s", buffer);
//     channel->EnableWrite();
// }

// void HandlerError(Channel *channel)
// {
//     HandlerClose(channel);
// }

// void HandlerNormal(Channel *channel, EventLoop *loop, uint64_t id)
// {
//     // std::cout << "发生一个事件" << std::endl;

//     // 刷新事件
//     loop->RefreshTimerTaskInLoop(id);
// }

// void Acceptor(Channel *channel, EventLoop *poller)
// {
//     uint64_t id = rand() % 10000; // 任务id

//     int client_fd = ::accept(channel->GetFd(), nullptr, nullptr);
//     if (client_fd < 0)
//         return;
//     Channel *client_channel = new Channel(client_fd, poller);
//     client_channel->SetReadFunc(std::bind(HandlerRead, client_channel));
//     client_channel->SetWriteFunc(std::bind(HandlerWrite, client_channel));
//     client_channel->SetErrorFunc(std::bind(HandlerError, client_channel));
//     client_channel->SetCloseFunc(std::bind(HandlerClose, client_channel));
//     client_channel->SetNormalFunc(std::bind(HandlerNormal, client_channel, poller, id));

//     // 需要先添加定时任务, 再启动可读,
//     // 因为可能启动可读了之后就立即有事件发生, 但是此时定时任务还没添加进去, 导致刷新定时任务的时候根本找不到这个任务
//     poller->AddTimerTask(id, 10, std::bind(HandlerClose, client_channel)); // 内存泄漏
//     client_channel->EnableRead();
// }

// void test_socket()
// {
//     uint16_t port = 8080;
//     Socket listen_socket;
//     listen_socket.CreateListenSocket(port);

//     Poller poller;
//     EventLoop eventloop(&poller);
//     Channel *listen_channel = new Channel(listen_socket.GetFd(), &eventloop);
//     listen_channel->SetReadFunc(std::bind(Acceptor, listen_channel, &eventloop)); // 设置读回调
//     listen_channel->EnableRead();
//     while(true)
//     {
//         eventloop.Loop();
//     }                                        // 监听可读

//     // while (true)
//     // {
//     //     std::vector<Channel *> actives;
//     //     int ret = poller->Poll(&actives);
//     //     for (int i = 0; i < ret; ++i)
//     //     {
//     //         actives[i]->HandlerEvents();
//     //         if(actives[i]->IsClosed())
//     //             delete actives[i];
//     //     }
//     //     // int client_fd = listen_socket.Accept();
//     //     // if(client_fd < 0)
//     //     //     continue;
//     //     // Socket client_socket(client_fd);
//     //     // char buffer[1024];
//     //     // int n = client_socket.Recv(buffer, 1023);
//     //     // if (n <= 0)
//     //     //     continue;
//     //     // buffer[n] = '\0';
//     //     // INF_LOG("clietn said: %s", buffer);

//     //     // int n2 = client_socket.Send(buffer, sizeof(buffer));
//     //     // if (n2 <= 0)
//     //     //     continue;
//     // }
//     listen_socket.Close();
// }

// int main()
// {
//     srand(time(NULL));
//     test_socket();
//     return 0;
// }

/*写完Connection*/
// #include "../src/server.hpp"
// std::unordered_map<uint64_t, Connection_Shared> g_connections;
// EventLoop eventloop;
// uint64_t conn_id = rand() % 10000; // 连接id

// void OnMessage(const Connection_Shared &conn, Buffer *buf)
// {
//     std::string msg;
//     buf->Read(&msg, buf->ReadableSize(), true);
//     INF_LOG("收到消息: %s", msg.c_str());
//     std::string reply = "server reply: " + msg;
//     conn->Send(reply.c_str(), reply.size());
// }

// void OnConnected(const Connection_Shared &conn)
// {
//     INF_LOG("新连接建立: %ld", conn->GetConnId());
// }

// void OnClosed(const Connection_Shared &conn)
// {
//     INF_LOG("连接关闭: %ld", conn->GetConnId());
// }

// void OnServerClosed(const Connection_Shared &conn)
// {
//     INF_LOG("服务器关闭连接: %ld", conn->GetConnId());
//     g_connections.erase(conn->GetConnId());
// }

// // void OnAnyEvent(const Connection_Shared &conn)
// // {
// //     INF_LOG("连接发生事件: %ld", conn->GetConnId());
// // }

// void NewConnection(int client_fd)
// {
//     conn_id++;

//     Connection_Shared client_conn = std::make_shared<Connection>(conn_id, client_fd, &eventloop);
//     client_conn->SetMessageCallback(std::bind(OnMessage, std::placeholders::_1, std::placeholders::_2));
//     client_conn->SetConnectedCallback(std::bind(OnConnected, std::placeholders::_1));
//     client_conn->SetClosedCallback(std::bind(OnClosed, std::placeholders::_1));
//     client_conn->SetServerClosedCallback(std::bind(OnServerClosed, std::placeholders::_1));
//     // client_conn->SetAnyEventCallback(std::bind(OnAnyEvent, std::placeholders::_1));
//     client_conn->EnableInactiveRelease(2);
//     client_conn->EstablishConnection();

//     g_connections[conn_id] = client_conn;
// }

// void test_socket()
// {
//     uint16_t port = 8080;
//     Acceptor acceptor(port, &eventloop);
//     acceptor.SetAcceptorCallback(NewConnection);
//     acceptor.Listen();
//     while (true)
//     {
//         eventloop.Loop();
//     } // 监听可读
// }

// int main()
// {
//     srand(time(NULL));
//     test_socket();
//     return 0;
// }

/*写完LoopThread*/
// #include "../src/server.hpp"
// std::unordered_map<uint64_t, Connection_Shared> g_connections;
// std::vector<LoopThread> g_loop_threads(2); // 2个客户端

// uint64_t conn_id = rand() % 10000; // 连接id

// void OnMessage(const Connection_Shared &conn, Buffer *buf)
// {
//     std::string msg;
//     buf->Read(&msg, buf->ReadableSize(), true);
//     INF_LOG("收到消息: %s", msg.c_str());
//     std::string reply = "server reply: " + msg;
//     conn->Send(reply.c_str(), reply.size());
// }

// void OnConnected(const Connection_Shared &conn)
// {
//     INF_LOG("新连接建立: %ld", conn->GetConnId());
// }

// void OnClosed(const Connection_Shared &conn)
// {
//     INF_LOG("连接关闭: %ld", conn->GetConnId());
// }

// void OnServerClosed(const Connection_Shared &conn)
// {
//     INF_LOG("服务器关闭连接: %ld", conn->GetConnId());
//     g_connections.erase(conn->GetConnId());
// }

// // void OnAnyEvent(const Connection_Shared &conn)
// // {
// //     INF_LOG("连接发生事件: %ld", conn->GetConnId());
// // }

// void NewConnection(int client_fd)
// {
//     conn_id++;
//     EventLoop *eventloop = g_loop_threads[conn_id % g_loop_threads.size()].GetLoop();
//     Connection_Shared client_conn = std::make_shared<Connection>(conn_id, client_fd, eventloop);
//     client_conn->SetMessageCallback(std::bind(OnMessage, std::placeholders::_1, std::placeholders::_2));
//     client_conn->SetConnectedCallback(std::bind(OnConnected, std::placeholders::_1));
//     client_conn->SetClosedCallback(std::bind(OnClosed, std::placeholders::_1));
//     client_conn->SetServerClosedCallback(std::bind(OnServerClosed, std::placeholders::_1));
//     // client_conn->SetAnyEventCallback(std::bind(OnAnyEvent, std::placeholders::_1));
//     client_conn->EnableInactiveRelease(2); // 先添加定时任务
//     client_conn->EstablishConnection(); // 再启动读事件监控

//     g_connections[conn_id] = client_conn;
// }

// void test_socket()
// {
//     uint16_t port = 8080;
//     EventLoop base_loop;
//     Acceptor acceptor(port, &base_loop);
//     acceptor.SetAcceptorCallback(NewConnection); // 设置新连接处理回调
//     acceptor.Listen(); // 启动读就绪

//     base_loop.Loop();
// }

// int main()
// {
//     srand(time(NULL));
//     test_socket();
//     return 0;
// }

// #include "../src/server.hpp"
// std::unordered_map<uint64_t, Connection_Shared> g_connections;
// EventLoop base_loop;
// LoopThreadPool thread_pool(&base_loop, 2);
// uint64_t conn_id = rand() % 10000; // 连接id

// void OnMessage(const Connection_Shared &conn, Buffer *buf)
// {
//     std::string msg;
//     buf->Read(&msg, buf->ReadableSize(), true);
//     INF_LOG("收到消息: %s", msg.c_str());
//     std::string reply = "server reply: " + msg;
//     conn->Send(reply.c_str(), reply.size());
// }

// void OnConnected(const Connection_Shared &conn)
// {
//     INF_LOG("新连接建立: %ld", conn->GetConnId());
// }

// void OnClosed(const Connection_Shared &conn)
// {
//     INF_LOG("连接关闭: %ld", conn->GetConnId());
// }

// void OnServerClosed(const Connection_Shared &conn)
// {
//     INF_LOG("服务器关闭连接: %ld", conn->GetConnId());
//     g_connections.erase(conn->GetConnId());
// }

// // void OnAnyEvent(const Connection_Shared &conn)
// // {
// //     INF_LOG("连接发生事件: %ld", conn->GetConnId());
// // }

// void NewConnection(int client_fd)
// {
//     conn_id++;
//     EventLoop *eventloop = thread_pool.NextLoop();
//     Connection_Shared client_conn = std::make_shared<Connection>(conn_id, client_fd, eventloop);
//     client_conn->SetMessageCallback(std::bind(OnMessage, std::placeholders::_1, std::placeholders::_2));
//     client_conn->SetConnectedCallback(std::bind(OnConnected, std::placeholders::_1));
//     client_conn->SetClosedCallback(std::bind(OnClosed, std::placeholders::_1));
//     client_conn->SetServerClosedCallback(std::bind(OnServerClosed, std::placeholders::_1));
//     // client_conn->SetAnyEventCallback(std::bind(OnAnyEvent, std::placeholders::_1));
//     client_conn->EnableInactiveRelease(2); // 先添加定时任务
//     client_conn->EstablishConnection(); // 再启动读事件监控

//     g_connections[conn_id] = client_conn;
// }

// void test_socket()
// {
//     uint16_t port = 8080;
//     thread_pool.Init(); 
    
//     Acceptor acceptor(port, &base_loop);
//     acceptor.SetAcceptorCallback(NewConnection); // 设置新连接处理回调
//     acceptor.Listen(); // 启动读就绪

//     base_loop.Loop();
// }

// int main()
// {
//     srand(time(NULL));
//     test_socket();
//     return 0;
// }

#include "../src/server.hpp"

void OnMessage(const Connection_Shared &conn, Buffer *buf)
{
    std::string msg;
    buf->Read(&msg, buf->ReadableSize(), true);
    INF_LOG("收到消息: %s", msg.c_str());
    std::string reply = "server reply: " + msg;
    conn->Send(reply.c_str(), reply.size());
}

void OnConnected(const Connection_Shared &conn)
{
    INF_LOG("新连接建立: %ld", conn->GetConnId());
}

void OnClosed(const Connection_Shared &conn)
{
    INF_LOG("连接关闭: %ld", conn->GetConnId());
}

// void OnAnyEvent(const Connection_Shared &conn)
// {
//     INF_LOG("连接发生事件: %ld", conn->GetConnId());
// }

int main()
{
    uint16_t port = 8080;
    TcpServer server(port, 2);
    // server.SetThreadCount(2);
    
    server.SetConnectedCallback(OnConnected);
    server.SetMessageCallback(OnMessage);
    server.SetClosedCallback(OnClosed);

    server.EnableInactiveRelease(3);

    server.Start();

    return 0;
}