#include <iostream>
#include <mutex>
#include<thread>
#include <list>
#include <algorithm>
#include <deque>
#include <exception>
#include <memory>
#include <stack>

/*
通过实例化std::mutex创建互斥量实例，成员函数lock()可对互斥量上锁，unlock()为解锁。
不过，不推荐直接去调用成员函数，调用成员函数就意味着，必须在每个函数出口都要去调用
unlock()(包括异常的情况)。C++标准库为互斥量提供了RAII模板类std::lock_guard，
在构造时就能提供已锁的互斥量，并在析构时进行解锁，从而保证了互斥量能被正确解锁
*/

std::list<int> some_list;    // 1
std::mutex some_mutex;    // 2，全局的互斥量，保护全局变量some_list

void add_to_list(int new_value)
{
  std::lock_guard<std::mutex> guard(some_mutex);    // 3
  some_list.push_back(new_value);
}

//list_contains()不可能看到正在被add_to_list()修改的列表，因为它们都使用了
//std::lock_guard<std::mutex>，所以二者对于数据的访问是互斥的
bool list_contains(int value_to_find)
{
//  std::lock_guard<std::mutex> guard(some_mutex);    // 4
  std::lock_guard guard(some_mutex);//c++17新增了模板类参数推导，上一行代码可以简化为本行代码
  return std::find(some_list.begin(),some_list.end(),value_to_find) != some_list.end();
}

/*
某些情况下使用全局变量没问题，但大多数情况下，互斥量通常会与需要保护的数据放在同一类中，而不是定义成全局变量。
这是面向对象设计的准则：将其放在一个类中，就可让他们联系在一起，也可对类的功能进行封装，并进行数据保护。
这种情况下，函数add_to_list和list_contains可以作为这个类的成员函数。互斥量和需要保护的数据，在类中都定义为private成员，
这会让代码更清晰，并且方便了解什么时候对互斥量上锁。所有成员函数都会在调用时对数据上锁，结束时对数据解锁，
这就保证了访问时数据不变量的状态稳定。

当然，也不是总能那么理想：当其中一个成员函数返回的是保护数据的指针或引用时，也会破坏数据。
具有访问能力的指针或引用可以访问(并可能修改)保护数据，而不会被互斥锁限制。
这就需要对接口谨慎设计，要确保互斥量能锁住数据访问，并且不留后门。
*/

/*
使用互斥量来保护数据，并不是在每一个成员函数中加入一个std::lock_guard对象那么简单。
一个指针或引用，也会让这种保护形同虚设。不过，检查指针或引用很容易，只要没有成员函数
通过返回值或者输出参数的形式，向其调用者返回指向受保护数据的指针或引用，数据就是安全的。
确保成员函数不会传出指针或引用的同时，检查成员函数是否通过指针或引用的方式来调用也是很重要的
(尤其是这个操作不在你的控制下时)。函数可能没在互斥量保护的区域内存储指针或引用，这样就很危险。
更危险的是：将保护数据作为一个运行时参数，如同下面代码中所示。
*/
class some_data
{
  int a;
  std::string b;
public:
  void do_something()
  {
      std::cout<<a<<b<<std::endl;
  }
};

class data_wrapper
{
private:
  some_data data;
  std::mutex m;
public:
  template<typename Function>
  void process_data(Function func)
  {
    std::lock_guard<std::mutex> l(m);
    func(data);    // 1 传递“保护”数据给用户函数
  }
};

some_data* unprotected;

void malicious_function(some_data& protected_data)
{
  unprotected=&protected_data;
}

data_wrapper x;
void foo()
{
  x.process_data(malicious_function);    // 2 传递一个恶意函数
  unprotected->do_something();    // 3 在无保护的情况下访问保护数据
}
/*
process_data看起来没有问题，std::lock_guard对数据做了很好的保护，但调用用户提供的函数func①，
就意味着foo能够绕过保护机制将函数malicious_function传递进去②，可以在没有锁定互斥量的情况下调用do_something()

这段代码的问题在于根本没有保护，只是将所有可访问的数据结构代码标记为互斥。函数foo()中调用unprotected->do_something()
的代码未能被标记为互斥。这种情况下，C++无法提供任何帮助，只能由开发者使用正确的互斥锁来保护数据。
从乐观的角度上看，还是有方法的：切勿将受保护数据的指针或引用传递到互斥锁作用域之外。
*/

/*
使用了互斥量或其他机制保护了共享数据，就不必再为条件竞争所担忧吗？并不是，依旧需要确定数据是否受到了保护。
回想之前双链表的例子，为了能让线程安全地删除一个节点，需要确保防止对这三个节点(待删除的节点及其前后相邻的节点)的并发访问。
如果只对指向每个节点的指针进行访问保护，那就和没有使用互斥量一样，条件竞争仍会发生——除了指针，
整个数据结构和整个删除操作需要保护。这种情况下最简单的解决方案就是使用互斥量来保护整个链表，如add_to_list()所示。

尽管链表的个别操作是安全的，但依旧可能遇到条件竞争。例如，构建一个类似于std::stack的栈(如下面的代码)，
除了构造函数和swap()以外，需要对std::stack提供五个操作：push()一个新元素进栈，pop()一个元素出栈，
top()查看栈顶元素，empty()判断栈是否是空栈，size()了解栈中有多少个元素。即使修改了top()，
返回一个拷贝而非引用(即遵循了3.2.2节的准则)，这个接口仍存在条件竞争。这个问题不仅存在于互斥量实现接口中，
在无锁实现接口中，也会产生条件竞争。这是接口的问题，与实现方式无关。
*/
template<typename T,typename Container=std::deque<T> >
class stack
{
public:
  explicit stack(const Container&);
  explicit stack(Container&& = Container());
  template <class Alloc> explicit stack(const Alloc&);
  template <class Alloc> stack(const Container&, const Alloc&);
  template <class Alloc> stack(Container&&, const Alloc&);
  template <class Alloc> stack(stack&&, const Alloc&);

  bool empty() const;
  size_t size() const;
  T& top();
  T const& top() const;
  void push(T const&);
  void push(T&&);
  void pop();
  void swap(stack&&);
  template <class... Args> void emplace(Args&&... args); // C++14的新特性
};
/*
虽然empty()和size()可能在返回时是正确的，但结果不可靠。当返回后，其他线程就可以自由地访问栈，
并且可能push()多个新元素到栈中，也可能pop()一些已在栈中的元素。这样的话，
之前从empty()和size()得到的数值就有问题了。
*/


struct empty_stack: std::exception
{
  const char* what() const throw() {
	return "empty stack!";
  };
};

/**线程安全堆栈*/
template<typename T>
class threadsafe_stack
{
private:
  std::stack<T> data;
  mutable std::mutex m;

public:
  threadsafe_stack()
	: data(std::stack<T>()){}

  threadsafe_stack(const threadsafe_stack& other)
  {
    std::lock_guard<std::mutex> lock(other.m);
    data = other.data; // 1 在构造函数体中的执行拷贝
  }

  threadsafe_stack& operator=(const threadsafe_stack&) = delete;

  void push(T new_value)
  {
    std::lock_guard<std::mutex> lock(m);
    data.push(new_value);
  }

  std::shared_ptr<T> pop()
  {
    std::lock_guard<std::mutex> lock(m);
    if(data.empty()) throw empty_stack(); // 在调用pop前，检查栈是否为空

    std::shared_ptr<T> const res(std::make_shared<T>(data.top())); // 在修改堆栈前，分配出返回值
    data.pop();
    return res;
  }

  void pop(T& value)
  {
    std::lock_guard<std::mutex> lock(m);
    if(data.empty()) throw empty_stack();

    value=data.top();
    data.pop();
  }

  bool empty() const
  {
    std::lock_guard<std::mutex> lock(m);
    return data.empty();
  }
};
/*
堆栈可以拷贝——拷贝构造函数对互斥量上锁，再拷贝堆栈。构造函数体中①的拷贝
使用互斥量来确保复制结果的正确性，这样的方式比成员初始化列表好。

之前对top()和pop()函数的讨论中，因为锁的粒度太小，恶性条件竞争已经出现，需要保护的操作并未全覆盖到。
不过，锁的颗粒度过大同样会有问题。还有一个问题，一个全局互斥量要去保护全部共享数据，
在一个系统中存在有大量的共享数据时，线程可以强制运行，甚至可以访问不同位置的数据，
抵消了并发带来的性能提升。第一版为多处理器系统设计Linux内核中，就使用了一个全局内核锁。
这个锁能正常工作，但在双核处理系统的上的性能要比两个单核系统的性能差很多，四核系统就更不能提了。
太多请求去竞争占用内核，使得依赖于处理器运行的线程没有办法很好的工作。随后修正的Linux内核加入了一个
细粒度锁方案，因为少了很多内核竞争，这时四核处理系统的性能就和单核处理的四倍差不多了。

使用多个互斥量保护所有的数据，细粒度锁也有问题。如前所述，当增大互斥量覆盖数据的粒度时，只需要锁住一个互斥量。
但这种方案并非放之四海皆准，互斥量保护一个独立类的实例，锁的状态的下一个阶段，不是离开锁定区域将锁定区域还给用户，
就是有独立的互斥量去保护这个类的全部实例，两种方式都不怎么好。

一个给定操作需要两个或两个以上的互斥量时，另一个潜在的问题将出现：死锁。
与条件竞争完全相反——不同的两个线程会互相等待，从而什么都没做。
*/

int main()
{
    if(list_contains(42))
        std::cout << "42 included" << std::endl;
    add_to_list(42);
    if(list_contains(42))
        std::cout << "42 included" << std::endl;
    foo();
    std::cout << "" << std::endl;
    return 0;
}
