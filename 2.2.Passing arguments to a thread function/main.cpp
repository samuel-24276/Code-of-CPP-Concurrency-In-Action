#include <iostream>
#include <thread>

/*
向可调用对象或函数传递参数很简单，只需要将这些参数作为
std::thread构造函数的附加参数即可。需要注意的是，这些参数
会拷贝至新线程的内存空间中(同临时变量一样)。即使函数中的参数是引用的形式，
拷贝操作也会执行
*/
void f(int i, std::string const& s)
{
    std::cout<<"f"<<i<<s<<std::endl;
}
void ff(int& i)
{
    std::cout<<"ff"<<i<<std::endl;
}

void oops(int some_param)
{
  char buffer[1024]; // 1
  sprintf(buffer, "%i",some_param);
  std::thread t(f,3,buffer); // 2
  t.detach();
}

/*
buffer①是一个指针变量，指向局部变量，然后此局部变量通过buffer传递到新线程中②。
此时，函数oops可能会在buffer转换成std::string之前结束，从而导致未定义的行为。
因为，无法保证隐式转换的操作和std::thread构造函数的拷贝操作的顺序，
有可能std::thread的构造函数拷贝的是转换前的变量(buffer指针)。解决方案就是在
传递到std::thread构造函数之前，就将字面值转化为std::string
*/
void not_oops(int some_param)
{
  char buffer[1024];
  sprintf(buffer,"%i",some_param);
  std::thread t(f,3,std::string(buffer));  // 使用std::string，避免悬空指针
  t.detach();
}

class X
{
public:
  void do_lengthy_work()
  {
      std::cout<<"do_lengthy_work"<<std::endl;
  }
  void do_lengthy_work2(int i)
  {
      std::cout<<"do_lengthy_work"<<i<<std::endl;
  }
};

int main()
{
    std::thread t1(f, 3, "hello");
    t1.join();
//    oops(112);
    not_oops(112);

    X my_x;
    std::thread t2(&X::do_lengthy_work, &my_x);
    t2.join();

    /*
    ff()期待传入一个引用，但std::thread的构造函数②并不知晓，
    构造函数无视函数参数类型，盲目地拷贝已提供的变量。不过，内部代码会将拷贝
    的参数以右值的方式进行传递，这是为了那些只支持移动的类型，而后会尝试以
    右值为实参调用update_data_for_widget。但因为函数期望的是一个非常量引用
    作为参数(而非右值)，所以会在编译时出错。
    */
    int i = 1;
    int& num = i;
    std::thread t3(ff, std::ref(num));
    t3.join();

    /*
    使用“移动”转移对象所有权后，就会留下一个空指针。使用移动操作可以将对象
    转换成函数可接受的实参类型，或满足函数返回值类型要求。当原对象是临时变量时，
    则自动进行移动操作，但当原对象是一个命名变量，转移的时候就需要使用std::move()
    进行显示移动。
    */
    int num2(0);
    std::thread t4(&X::do_lengthy_work2, &my_x, num2);
//    std::thread t4(&X::do_lengthy_work2, &my_x, std::move(num2));
    t4.join();
    return 0;
}
