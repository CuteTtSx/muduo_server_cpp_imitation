#include <any>
#include <iostream>

int main()
{
    std::any a1;
    a1 = 10;
    int *pa1 = std::any_cast<int>(&a1);
    std::cout << *pa1 << std::endl;

    std::any a2;
    a2 = std::string("hello");
    std::string pa2 = std::any_cast<std::string>(a2);
    std::cout << pa2 << std::endl;

    return 0;
}