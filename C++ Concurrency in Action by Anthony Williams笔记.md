# 第4章 同步操作

## 4.1 等待事件或条件

>假设你正在一辆在夜间运行的火车上，在夜间如何在正确的站点下车呢？有一种方法是整晚都要醒着，每停一站都能知道，这样就不会错过你要到达的站点，但会很疲倦。另外，可以看一下时间表，估计一下火车到达目的地的时间，然后在一个稍早的时间点上设置闹铃，然后安心的睡会。这个方法听起来也很不错，也没有错过你要下车的站点，但是当火车晚点时，就要被过早的叫醒了。当然，闹钟的电池也可能会没电了，并导致你睡过站。理想的方式是，无论是早或晚，只要当火车到站的时候，有人或其他东西能把你叫醒就好了。

这和线程有什么关系呢？当一个线程等待另一个线程完成时，可以持续的检查共享数据标志(用于做保护工作的互斥量)，直到另一线程完成工作时对这个标识进行重置。不过，这种方式会消耗线程的执行时间检查标识，并且当互斥量上锁后，其他线程就没有办法获取锁，就会持续等待。因为对等待线程资源的限制，并且在任务完成时阻碍对标识的设置。类似于保持清醒状态和列车驾驶员聊了一晚上：驾驶员不得不缓慢驾驶，因为你分散了他的注意力，所以火车需要更长的时间，才能到站。同样，等待的线程会等待更长的时间，也会消耗更多的系统资源。

所以我们可以在 等待线程在检查间隙，使用`std::this_thread::sleep_for()`进行周期性的间歇 ：

```c++
bool flag;
std::mutex m;

void wait_for_flag()
{
  std::unique_lock<std::mutex> lk(m);
  while(!flag)
  {
    lk.unlock();  // 1 解锁互斥量
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 2 休眠100ms
    lk.lock();   // 3 再锁互斥量
  }
}
////先解锁再休眠，这样其他线程就能继续检查标识或者工作，该线程占用的系统资源会被释放，休眠完成后上锁，若该锁用于同步，则可能休眠完成前其他线程就会对该互斥量上锁。
```

循环中，休眠前②函数对互斥量进行解锁①，并且在休眠结束后再对互斥量上锁，所以另外的线程就有机会获取锁并设置标识。

这个实现就进步很多，当线程休眠时没有浪费执行时间，但很难确定正确的休眠时间。太短的休眠和没有一样，都会浪费执行时间。太长的休眠时间，可能会让任务等待时间过久。休眠时间过长比较少见，这会影响到程序的行为，在高节奏的游戏中，就意味着丢帧或错过了一个时间片。

第三个选择(也是**优先选择**的)，**使用C++标准库提供的工具去等待事件的发生**。通过另一线程触发等待事件的机制是最基本的唤醒方式(例如：流水线上存在额外的任务时)，这种机制就称为“条件变量”。从概念上来说，条件变量会与多个事件或其他条件相关，并且一个或多个线程会等待条件的达成。当某些线程被终止时，为了唤醒等待线程(允许等待线程继续执行)，终止线程将会向等待着的线程广播“条件达成”的信息。

### 4.1.1 等待条件达成

C++标准库对条件变量有两套实现：**`std::condition_variable`和`std::condition_variable_any`**，这两个实现都包含在` <condition_variable> `头文件的声明中。**两者都需要与互斥量一起才能工作**(互斥量是为了同步)，**前者仅能与`std::mutex`一起工作**，而后者可以和合适的互斥量一起工作，从而加上了`_any`的后缀。因为` std::condition_variable_any`更加通用，不过在性能和系统资源的使用方面会有更多的开销，所以**通常会将`std::condition_variable`作为首选类型**。当对灵活性有要求时，才会考虑`std::condition_variable_any`。

所以，使用`std::condition_variable`去处理之前提到的情况——当有数据需要处理时，如何唤醒休眠中的线程？以下代码展示了使用条件变量唤醒线程的方式。

```c++
std::mutex mut;
std::queue<data_chunk> data_queue;  // 1
std::condition_variable data_cond;

void data_preparation_thread()//数据准备
{
  while(more_data_to_prepare())
  {
    data_chunk const data=prepare_data();
    std::lock_guard<std::mutex> lk(mut);
    data_queue.push(data);  // 2
    data_cond.notify_one();  // 3
  }
}

void data_processing_thread()//数据处理
{
  while(true)
  {
    std::unique_lock<std::mutex> lk(mut);  // 4
    data_cond.wait(
         lk,[]{return !data_queue.empty();});  // 5
    data_chunk data=data_queue.front();
    data_queue.pop();
    lk.unlock();  // 6
    process(data);
    if(is_last_chunk(data))
      break;
  }
}
```

首先，队列中中有两个线程，两个线程之间会对数据进行传递①。数据准备好时，使用`std::lock_guard`锁定队列，将准备好的数据压入队列②之后，线程会对队列中的数据上锁，并**调用`std::condition_variable`的notify_one()成员函数，对等待的线程(如果有等待线程)进行通知**③。

另外的一个线程正在处理数据，线程首先对互斥量上锁(这里使用`std::unique_lock`要比`std::lock_guard`④更加合适)。之后会**调用`std::condition_variable`的成员函数wait()，传递一个锁和一个Lambda表达式**(作为等待的条件⑤)。Lambda函数是C++11添加的新特性，可以让一个匿名函数作为其他表达式的一部分，并且非常合适作为标准函数的谓词。例子中，简单的Lambda函数`[]{return !data_queue.empty();}`会去检查data_queue是否为空，当data_queue不为空，就说明数据已经准备好了。附录A的A.5节有Lambda函数更多的信息。

wait()会去检查这些条件(通过Lambda函数)，当条件满足(Lambda函数返回true)时返回。**如果条件不满足(Lambda函数返回false)，wait()将解锁互斥量，并且将线程(处理数据的线程)置于阻塞或等待状态**。当准备数据的线程调用notify_one()通知条件变量时，处理数据的线程从睡眠中苏醒，重新获取互斥锁，并且再次进行条件检查。在条件满足的情况下，从wait()返回并继续持有锁。当条件不满足时，线程将对互斥量解锁，并重新等待。**这就是为什么用`std::unique_lock`而不使用`std::lock_guard`的原因——等待中的线程必须在等待期间解锁互斥量，并对互斥量再次上锁，而`std::lock_guard`没有这么灵活**。如果互斥量在线程休眠期间保持锁住状态，准备数据的线程将无法锁住互斥量，也无法添加数据到队列中。同样，等待线程也永远不会知道条件何时满足。

代码4.1使用了简单的Lambda函数用于等待⑤(用于检查队列何时不为空)，不过**任意的函数和可调用对象都可以传入wait()。当写好函数做为检查条件时，不一定非要放在一个Lambda表达式中，也可以直接将这个函数传入wait()**。调用wait()的过程中，在互斥量锁定时，可能会去检查条件变量若干次，当提供测试条件的函数返回true就会立即返回（退出）。**当等待线程重新获取互斥量并检查条件变量时，并非直接响应另一个线程的通知，就是所谓的*伪唤醒*(spurious wakeup)**。因为任何伪唤醒的数量和频率都是不确定的，所以不建议使用有副作用的函数做条件检查。

本质上，` std::condition_variable::wait`是“忙碌-等待”的优化。下面用简单的循环实现了一个“忙碌-等待”：

```c++
template<typename Predicate>
void minimal_wait(std::unique_lock<std::mutex>& lk, Predicate pred){
  while(!pred()){
    lk.unlock();
    lk.lock();
  }
}
```

为wait()准备一个最小化实现，只需要notify_one()或notify_all()。

`std::unique_lock`的灵活性，不仅适用于对wait()的调用，还可以用于待处理的数据⑥。处理数据可能是耗时的操作，并且长时间持有锁是个糟糕的主意。

使用队列在多个线程中转移数据(如代码4.1)很常见。做得好的话，同步操作可以在队列内部完成，这样同步问题和条件竞争出现的概率也会降低。鉴于这些好处，需要从代码4.1中提取出一个通用线程安全的队列。



- 1.`std::this_thread::sleep_for()`线程可以进行周期性的休眠 
- 2.条件变量的两套实现：**`std::condition_variable`和`std::condition_variable_any`**，包含在` <condition_variable> `头文件的声明中。**两者都需要与互斥量一起才能工作**(互斥量是为了同步)，**前者仅能与`std::mutex`一起工作**，而后者可以和合适的互斥量一起工作，从而加上了`_any`的后缀。因为` std::condition_variable_any`更加通用，不过在性能和系统资源的使用方面会有更多的开销，所以**通常会将`std::condition_variable`作为首选类型**。当对灵活性有要求时，才会考虑`std::condition_variable_any`。
- 3.**调用`std::condition_variable`的notify_one()成员函数，对等待的线程(如果有等待线程)进行通知**
- 4.**调用`std::condition_variable`的成员函数wait()，传递一个锁和一个Lambda表达式**（作为等待的条件）
- 5.用`std::unique_lock`而不使用`std::lock_guard`的原因——等待中的线程必须在等待期间解锁互斥量，并对互斥量再次上锁，而`std::lock_guard`没有这么灵活。如果互斥量在线程休眠期间保持锁住状态，准备数据的线程将无法锁住互斥量，也无法添加数据到队列中。同样，等待线程也永远不会知道条件何时满足。 

# Bottom