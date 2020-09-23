#include <iostream>
#include <thread>

void some_function()
{
    std::cout<<"some_function"<<std::endl;
}
void some_other_function()
{
    std::cout<<"some_other_function"<<std::endl;
}

void some_other_function_int(int i)
{
    std::cout<<"some_other_function"<<i<<std::endl;
}

std::thread f()
{
  return std::thread(some_function);
}

std::thread g()
{
  std::thread t(some_other_function_int,42);
  return t;
}


void ff(std::thread t)
{
    //如果线程运行过程中发生异常，之后调用的join会被忽略，
    //为此需要捕获异常并在处理异常时调用join
    try
    {

    }
    catch(...)
    {
        t.join();
    }
    std::cout<<"ff"<<std::endl;
//    std::cout<<t.get_id();
    t.join();
}
void gg()
{
    //必须使用move()拷贝构造的左值，常左值，常右值在thread内均为 = delete
//    ff(std::thread(some_function));
    std::thread a(some_function);
    ff(std::move(a));
    std::cout<<"gg thread id:"<<std::this_thread::get_id()<<std::endl;
}

/*
std::thread支持移动可以创建thread_guard类的实例(定义见清单2.3)，
并且拥有线程所有权。当引用thread_guard对象所持有的线程时，移动操作就可以
避免很多不必要的麻烦。当某个对象转移了线程的所有权，就不能对线程进行汇入或
分离。为了确保线程在程序退出前完成，定义了scoped_thread类。现在，我们来看一下这个类型：
*/
class scoped_thread
{
  std::thread t;
public:
  explicit scoped_thread(std::thread t_): // 1
    t(std::move(t_))
  {
    if(!t.joinable())  // 2
      throw std::logic_error("No thread");
  }

  ~scoped_thread()
  {
    std::cout<<"scope join"<<std::endl;
    t.join(); // 3
  }
  scoped_thread(scoped_thread const&)=delete;
  scoped_thread& operator=(scoped_thread const&)=delete;
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

void scope()
{
  int some_local_state;
  scoped_thread t(std::thread(func(some_local_state)));    // 4
  std::cout<<"scope"<<std::endl;
} // 5

int main()
{
    std::thread t1(some_function);            // 1
    std::thread t2=std::move(t1);            // 2
    t1=std::thread(some_other_function);    // 3
    t1.join();
    std::thread t3;                            // 4
    t3=std::move(t2);                        // 5
    t3.join();
//    t1=std::move(t3);                        // 6 赋值操作将使程序崩溃
//    t1.join();
/*
首先，新线程与t1相关联①。当显式使用std::move()创建t2后②，t1的所有权就转移给了t2。
之后，t1和执行线程已经没有关联了，执行some_function的函数线程与t2关联。

然后，临时std::thread对象相关的线程启动了③。为什么不显式调用std::move()转移
所有权呢？因为，所有者是一个临时对象——移动操作将会隐式的调用。

t3使用默认构造方式创建④，没有与任何线程进行关联。调用std::move()将t2关联线程
的所有权转移到t3中⑤。因为t2是一个命名对象，需要显式的调用std::move()。
移动操作⑤完成后，t1与执行some_other_function的线程相关联，t2与任何线程都无关联，
t3与执行some_function的线程相关联。

最后一个移动操作，将some_function线程的所有权转移⑥给t1。不过，t1已经有了一个
关联的线程(执行some_other_function的线程)，所以这里系统直接调用std::terminate()
终止程序继续运行。这样做(不抛出异常，std::terminate()是noexcept函数)是为了保证
与std::thread的析构函数的行为一致。2.1.1节中，需要在线程对象析构前，显式的等待
线程完成，或者分离它，进行赋值时也需要满足这些条件(说明：不能通过赋新值给
std::thread对象的方式来"丢弃"一个线程)。
*/


    //std::thread支持移动，线程的所有权可以在函数外进行转移
    t2 = f();
    t2.join();
    std::thread t4 = std::move(g());
    std::cout<<"t4 id:"<<t4.get_id()<<std::endl;//线程一旦join，将不再拥有ID
    t4.join();


    //当所有权可以在函数内部传递，就允许std::thread实例作为参数进行传递
    gg();

    /*
    新线程会直接传递到scoped_thread中④，而非创建一个独立变量。当主线程到达f()末尾时⑤，
    scoped_thread对象就会销毁，然后在构造函数中完成汇入③。
    代码2.3中的thread_guard类，需要在析构中检查线程是否“可汇入”。
    这里把检查放在了构造函数中②，并且当线程不可汇入时抛出异常。
    */
    scope();
    return 0;
}
