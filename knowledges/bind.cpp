#include <functional>
#include <string>
#include <iostream>
#include <vector>

using func_t = std::function<void(const std::string &)>;

class A
{
public:
    A()
    {
        std::cout << "A()" << std::endl;
    }
    ~A()
    {
        std::cout << "~A()" << std::endl;
    }

    void Print(int num)
    {
        std::cout << _id << ":" << num << std::endl;
    }

private:
    int _id = 0;
};

void Del(A *a, const std::string &msg, int num)
{
    std::cout << "我要关闭了:" << msg << " " << num << std::endl;
    delete a;
}

int main()
{
    A *a = new A();

    // auto func2 = std::bind(A::Print, a, 1000);
    // bind.cpp:42:31: error: invalid use of non-static member function ‘void A::Print(int)’
    //     42 |     auto func2 = std::bind(A::Print, a, 1000);
    auto func2 = std::bind(&A::Print, a, 1000);
    func2();

    func_t func = std::bind(Del, a, std::placeholders::_1, 100);
    func("A1");

    return 0;
}