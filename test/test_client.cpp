#include "../src/server.hpp"
#include <sstream>

int main()
{
    Socket client1;
    client1.CreateClientSocket(8080, "127.0.0.1");

    for (int i = 0; i < 5; ++i)
    {
        // 获取id的hash
        std::thread::id thread_id = std::this_thread::get_id();
        auto id = std::hash<std::thread::id>()(thread_id);

        std::stringstream oss;
        oss << "hello world, im client[" << id << "]";
        std::string msg = oss.str();

        client1.Send(msg.c_str(), msg.size());

        char buffer[1024];
        int n = client1.Recv(buffer, 1023);
        if (n <= 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                return 0;
            return -1;
        }
        buffer[n] = '\0';
        std::cout << buffer << std::endl;

        sleep(1);
    }
    
    while (true)
    {
    }

    return 0;
}