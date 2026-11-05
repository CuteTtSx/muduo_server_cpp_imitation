#include <iostream>
#include <string>
#include <cassert>
#include <typeinfo>

class Any
{
    class _Holder
    {
    public:
        virtual ~_Holder() {}
        virtual _Holder *clone() = 0; // clone, 用于实现Any的拷贝构造
        virtual const std::type_info &type() = 0; // 返回内部维护的类型
    };

    template <class T>
    class _Placeholder : public _Holder
    {
    public:
        virtual ~_Placeholder() {}
        _Placeholder(const T &val) : _val(val) {} // 构造函数, Any 构造函数 Any(const T &val) 的基础 
        virtual _Holder *clone() { return new _Placeholder<T>(_val); } // clone就是创建一个新对象
        virtual const std::type_info &type() { return typeid(T); } // 返回typeid(T)即可

    public:
        T _val;
    };

public:
    // 默认构造
    Any() : _content(nullptr) {} 
    // 构造, 单参数的构造支持显示类型转换: Any any1 = 10; Any any2 = std::string("hello"); 
    template <class T>
    Any(const T &val) : _content(new _Placeholder<T>(val)) {} 
    // 拷贝构造, 不为空就调用clone拷过来
    Any(const Any &other) : _content(other._content ? other._content->clone() : nullptr) {}
    // 返回内部维护的类型
    const std::type_info &type() { return _content ? _content->type() : typeid(void); }
    // 析构函数, 释放维护的对象
    ~Any()
    {
        if (_content)
            delete _content;
    }
    // 辅助函数, 用于实现赋值重载
    Any &swap(Any &other)
    {
        std::swap(_content, other._content);
        return *this;
    }
    //赋值重载, 就是构造一个临时Any对象, 然后swap, 出作用域临时对象会自动销毁, 释放空间
    template <class T>
    Any &operator=(const T &val)
    {
        Any(val).swap(*this);
        return *this;
    }
    //另一种赋值重载, 利用Any对象, 也是构造一个临时Any对象, 然后swap, 出作用域临时对象会自动销毁, 释放空间
    Any &operator=(const Any &other)
    {
        if (this != &other)
            Any(other).swap(*this);
        return *this;
    }
    // 获取内部维护的具体对象, 但是可能出现const int 和 int识别为一种类型, 注释部分解决了这个问题, 但暂没采用
    template <class T>
    T *get()
    {
        // using U = typename std::remove_cv<T>::type;
        // if (!_content || typeid(U) != _content->type())
        //     throw std::bad_cast();
        // return &static_cast<_Placeholder<U> *>(_content)->_val;
        if (!_content || typeid(T) != _content->type())
            throw std::bad_cast();
        return &static_cast<_Placeholder<T> *>(_content)->_val;
    }

private:
    _Holder *_content;
};

// class Test
// {
// public:
//     Test() { std::cout << "Test()" << std::endl; }
//     Test(const Test &other) { std::cout << "Test(const Test &other)" << std::endl; }
//     ~Test() { std::cout << "~Test()" << std::endl; }
// };
// int main()
// {
//     // 构造
//     Any a = 10;
//     Any b = std::string("hello");
//     Any c = Test();
//     std::cout << *a.get<int>() << std::endl;
//     std::cout << *b.get<std::string>() << std::endl;
//     // 赋值重载
//     a = 20;
//     c = b;
//     std::cout << *a.get<int>() << std::endl;
//     std::cout << *c.get<std::string>() << std::endl;
//     // 拷贝构造
//     Any d(b);
//     std::cout << *d.get<std::string>() << std::endl;

//     try{
//         Any empty;
//         empty.get<int>(); // 抛异常, 空指针
//         Any e = 10;
//         e.get<std::string>(); // 抛异常, 类型错误
//     }
//     catch (const std::bad_cast& e)
//     {
//         std::cout << e.what() << '\n';
//     }

//     std::cout << "----------------------------------------" << '\n';
//     // Any f = "hello world"; // 编译不通过, 因为字符串字面量通过引用传参, T被识别为char[12]而不是char*, 而数组不支持普通的构造
//     //下面是解释
//     int arr[5] = {1, 2, 3, 4, 5};
//     auto p1 = arr;  // int *p1 = arr;
//     auto &p2 = arr; // int (&p2)[5] = arr;
//     std::cout << typeid(p1).name() << std::endl;
//     std::cout << typeid(p2).name() << std::endl;

//     int arr2[5] = {1, 2, 3, 4, 6};
//     int *p3(arr2); // 正常, Pi
//     int (&p4)[5](arr2); // 正常, A5_i
//     std::cout << (p4 == arr2) << std::endl; // 1
//     // int p5[5](p4); // "int *" 类型的值不能用于初始化 "int[5]" 类型的实体

//     return 0;
// }