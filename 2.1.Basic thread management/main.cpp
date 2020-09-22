#include <iostream>
#include<thread>
#include<assert.h>

void do_something()
{
    std::cout<<"do something"<<std::endl;
}

void do_something(int i)
{
    std::cout<<"do something"<<i<<std::endl;
}

void do_something_else()
{
    std::cout<<"do something else"<<std::endl;
}

class background_task
{
public:
  void operator()() const
  {
    do_something();
    do_something_else();
  }
};

struct func
{
  int& i;
  func(int& i_) : i(i_) {}
  void operator() ()
  {
    for (unsigned j=0 ; j<1000000 ; ++j)
    {
//      do_something(i);           //潜在访问隐患：空引用
    }
  }
};

void oops()
{
  int some_local_state=0;
  func my_func(some_local_state);
  std::cout<<"oops"<<std::endl;
  std::thread my_thread(my_func);
  my_thread.detach();          //分离当前线程，主线程不等待当前线程结束后结束，会导致当前线程在后台运行时访问主线程已经销毁的局部变量
}

void f()
{
  int some_local_state=0;
  func my_func(some_local_state);
  std::thread t(my_func);
  //避免应用被抛出的异常所终止。通常，在无异常的情况下使用join()时，需要在异常处理过程中调用join()，从而避免生命周期的问题。
  try
  {
    std::cout<<"正常:"<<std::endl;
  }
  catch(...)
  {
    t.join();  // 1
    throw  "f error";;
  }
  t.join();  // 2
}

class thread_guard
{
  std::thread& t;
public:
  explicit thread_guard(std::thread& t_):
    t(t_)
  {}
  ~thread_guard()
  {
    std::cout<<"开始析构"<<std::endl;
//    if(t.joinable()) // 1
//    {
    assert(t.joinable());
      t.join();      // 2
//    }
  }
  thread_guard(thread_guard const&)=delete;   // 3
  thread_guard& operator=(thread_guard const&)=delete;
};

void ff()
{
  int some_local_state=0;
  func my_func(some_local_state);
  std::thread t(my_func);
  std::cout<<"ff"<<std::endl;
  thread_guard g(t);
}    // 4

int main()
{
//    std::thread myThread(do_something);
//    myThread.join();
    background_task f;
//    std::thread my_thread(f);
//    std::thread my_thread((background_task()));//多加一组括号声明函数对象，缺少括号传递的是临时变量，编译器会将其解析为函数声明
    std::thread my_thread{background_task()};//统一的初始化语法
    my_thread.join();
//    oops();
//    f();
    ff();   //若要显示出析构函数里的内容，须把func结构体里的do_something()函数注释掉
    static_assert(true);        //静态断言，适用于常量
    return 0;
}
