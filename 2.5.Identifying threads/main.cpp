#include <iostream>
#include <thread>

/*
线程标识为std::thread::id类型，可以通过两种方式进行检索。第一种，
可以通过调用std::thread对象的成员函数get_id()来直接获取。如果std::thread对象
没有与任何执行线程相关联，get_id()将返回std::thread::type默认构造值，这个值表示“无线程”。
第二种，当前线程中调用std::this_thread::get_id()
(这个函数定义在<thread>头文件中)也可以获得线程标识。

std::thread::id对象可以自由的拷贝和对比，因为标识符可以复用。
如果两个对象的std::thread::id相等，那就是同一个线程，或者都“无线程”。
如果不等，那么就代表了两个不同线程，或者一个有线程，另一没有线程。

C++线程库不会限制你去检查线程标识是否一样，std::thread::id类型对象提供了相当丰富的对比操作。
比如，为不同的值进行排序。这意味着开发者可以将其当做为容器的键值做排序，或做其他比较。
按默认顺序比较不同的std::thread::id：当a<b，b<c时，得a<c，等等。标准库也提供std::hash<std::thread::id>容器，
std::thread::id也可以作为无序容器的键值。

std::thread::id实例常用作检测线程是否需要进行一些操作。比如：当用线程来分割一项工作(如项目2.4)，
主线程可能要做一些与其他线程不同的工作，启动其他线程前，可以通过std::this_thread::get_id()得到自己的线程ID。
每个线程都要检查一下，其拥有的线程ID是否与初始线程的ID相同。
*/

std::thread::id master_thread;

/**验证了some_core_part_of_algorithm()和main()不是一个线程*/
void some_core_part_of_algorithm()
{
  if(std::this_thread::get_id()==master_thread)
  {
    std::cout<<"master_thread_work"<<std::endl;
  }
  std::cout<<"do_common_work"<<std::endl;
}

/*
另外，当前线程的std::thread::id将存储到数据结构中。之后这个结构体对当前线程的ID与存储的线程ID做对比，
来决定操作是“允许”，还是“需要”(permitted/required)。

同样，作为线程和本地存储不适配的替代方案，线程ID在容器中可作为键值。例如，
容器可以存储其掌控下每个线程的信息，或在多个线程中互传信息。
*/

int main()
{
    some_core_part_of_algorithm();
    return 0;
}
