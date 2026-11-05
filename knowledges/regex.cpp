#include <iostream>
#include <string>
#include <regex>

int main()
{
    std::vector<std::string> urls;
    urls.push_back("GET /home/zzy?a=b&c=d HTTP/1.1\r\n");
    urls.push_back("GET /home]zzy?a=b&c=d HTTP/1.1\r\n");
    urls.push_back("GET / HTTP/1.1\r\n");

    std::smatch matches;
    // std::regex re(
    //     "(GET|HEAD|POST|DELETE|PUT) ([^?]*)(?:\\?([^\s]*))? (HTTP/1\\.[01])(?:\r\n|\n)"
    // );
    std::regex re(
        R"((GET|HEAD|POST|DELETE|PUT) (/[a-zA-Z0-9_/-]*)(?:\?([^\s]*))? (HTTP/1\.[01])(?:\r\n|\n))");

    for (const std::string &str : urls)
    {
        bool ret = std::regex_match(str, matches, re);
        if (!ret)
        {
            std::cout << "匹配失败\n";
            std::cout << "-----------------------------------" << std::endl;
            continue;
        }

        for (const auto &e : matches)
            std::cout << e << std::endl;
        std::cout << "-----------------------------------" << std::endl;
    }

    return 0;
}