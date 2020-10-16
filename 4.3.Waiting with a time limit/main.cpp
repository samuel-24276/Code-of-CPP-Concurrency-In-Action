///4.3 限时等待

/*阻塞调用会将线程挂起一段(不确定的)时间，直到相应的事件发生。通常情况下，这样
的方式很不错，但是在一些情况下，需要限定线程等待的时间。可以发送一些类似“我还存
活”的信息，无论是对交互式用户，或是其他进程，亦或当用户放弃等待，也可以按下“取消”
键终止等待。

这里介绍两种指定超时方式：一种是“时间段”，另一种是“时间点”。第一种方式，需要指定
一段时间(例如，30毫秒)。第二种方式，就是指定一个时间点(例如，世界标准时间
[UTC]17:30:15.045987023，2011年11月30日)。多数等待函数提供变量，对两种超时方式
进行处理。处理持续时间的变量以_for作为后缀，处理绝对时间的变量以_until作为后缀。

所以，std::condition_variable的两个成员函数wait_for()和wait_until()成员函数分别
有两个重载，这两个重载都与wait()成员函数的重载相关——其中一个只是等待信号触发，
或超期，亦或伪唤醒，并且醒来时会使用谓词检查锁，并且只有在校验为true时才会返回
(这时条件变量的条件达成)，或直接超时。

观察使用超时函数的细节前，我们来检查一下在C++中指定时间的方式，就从“时钟”开始吧！*/

///4.3.1 时钟
/*对于C++标准库来说，时钟就是时间信息源。并且，时钟是一个类，提供了四种不同的信息：

当前时间The time now

时间类型The type of the value used to represent the times obtained from the clock

时钟节拍The tick period of the clock

稳定时钟Whether or not the clock ticks at a uniform rate and is therefore
considered to be a steady clock

当前时间可以通过静态成员函数now()从获取。例如，std::chrono::system_clock::now()
会返回系统的当前时间。特定的时间点可以通过time_point的typedef成员来指定，所以
some_clock::now()的类型就是some_clock::time_point。

时钟节拍被指定为1/x(x在不同硬件上有不同的值)秒，这是由时间周期所决定——一个时钟
一秒有25个节拍，因此一个周期为std::ratio<1, 25>，当一个时钟的时钟节拍每2.5秒一次，
周期就可以表示为std::ratio<5, 2>。当时钟节拍在运行时获取时，可以使用给定的应用
程序运行多次，用执行的平均时间求出，其中最短的时间可能就是时钟节拍，或者是写在
手册当中，这就不保证在给定应用中观察到的节拍周期与指定的时钟周期是否相匹配。

当时钟节拍均匀分布(无论是否与周期匹配)，并且不可修改，这种时钟就称为稳定时钟。
is_steady静态数据成员为true时，也表明这个时钟就是稳定的。通常情况下，因为
std::chrono::system_clock可调，所以是不稳定的。这可调可能造成首次调用now()返回
的时间要早于上次调用now()所返回的时间，这就违反了节拍频率的均匀分布。稳定闹钟
对于计算超时很重要，所以C++标准库提供一个稳定时钟std::chrono::steady_clock。
C++标准库提供的其他时钟可表示为std::chrono::system_clock，代表了系统时钟的“实际时间”，
并且提供了函数，可将时间点转化为time_t类型的值。
std::chrono::high_resolution_clock 可能是标准库中提供的具有最小节拍周期(因此具有
最高的精度)的时钟。它实际上是typedef的另一种时钟，这些时钟和与时间相关的工具，
都在<chrono>库头文件中定义。

我们先看一下时间段是怎么表示的。*/

///4.3.2 时间段

/*时间部分最简单的就是时间段，std::chrono::duration<>函数模板能够对时间段进行处理
(线程库使用到的所有C++时间处理工具，都在std::chrono命名空间内)。第一个模板参数是一个
类型表示(比如，int，long或double)，第二个模板参数是定制部分，表示每一个单元所用
秒数。例如，当几分钟的时间要存在short类型中时，可以写成
std::chrono::duration<short, std::ratio<60, 1>>，因为60秒是才是1分钟，所以第二个
参数写成std::ratio<60, 1>。当需要将毫秒级计数存在double类型中时，可以写成
std::chrono::duration<double, std::ratio<1, 1000>>，因为1秒等于1000毫秒。

标准库在std::chrono命名空间内为时间段变量提供一系列预定义类型：nanoseconds[纳秒] ,
 microseconds[微秒] , milliseconds[毫秒] , seconds[秒] , minutes[分]和hours[时]。
 比如，你要在一个合适的单元表示一段超过500年的时延，预定义类型可充分利用了大整型，
 来表示所要表示的时间类型。当然，这里也定义了一些国际单位制(SI, [法]le Système international d'unités)分数，
 可从std::atto(10^(-18))到std::exa(10^(18))(题外话：当你的平台支持128位整型)，也
 可以指定自定义时延类型。例如：std::duration<double, std::centi>，
 就可以使用一个double类型的变量表示1/100。

方便起见，C++14中std::chrono_literals命名空间中有许多预定义的后缀操作符用来表示
时长。下面的代码就是使用硬编码的方式赋予变量具体的时长：*/

using namespace std::chrono_literals;
auto one_day=24h;
auto half_an_hour=30min;
auto max_time_between_messages=30ms;

/*使用整型字面符时，15ns和std::chrono::nanoseconds(15)就是等价的。不过，当使用
浮点字面量时，且未指明表示类型时，数值上会对浮点时长进行适当的缩放。因此，
2.5min会被表示为std::chrono::duration<some-floating-point-type,std::ratio<60,1>>。
如果非常关心所选的浮点类型表示的范围或精度，就需要构造相应的对象来保证表示范围
或精度，而不是去苛求字面值来对范围或精度进行表达。

当不要求截断值的情况下(时转换成秒是没问题，但是秒转换成时就不行)时间段的转换是
隐式的，显示转换可以由std::chrono::duration_cast<>来完成。
*/
std::chrono::milliseconds ms(54802);
std::chrono::seconds s=
       std::chrono::duration_cast<std::chrono::seconds>(ms);
/*这里的结果就是截断的，而不是进行了舍入，所以s最后的值为54。

时间值支持四则运算，所以能够对两个时间段进行加减，或者是对一个时间段乘除一个常数
(模板的第一个参数)来获得一个新时间段变量。例如，5*seconds(1)与seconds(5)或
minutes(1)-seconds(55)是一样。在时间段中可以通过count()成员函数获得单位时间的
数量。例如，std::chrono::milliseconds(1234).count()就是1234。

基于时间段的等待可由std::chrono::duration<>来完成。例如：等待future状态变为就绪
需要35毫秒：
*/
std::future<int> f=std::async(some_task);
if(f.wait_for(std::chrono::milliseconds(35))==std::future_status::ready)
  do_something_with(f.get());

/*等待函数会返回状态值，表示是等待超时，还是继续等待。等待future时，超时会返回
std::future_status::timeout。当future状态改变，则会返回std::future_status::ready。
当与future相关的任务延迟了，则会返回std::future_status::deferred。
基于时间段的等待使用稳定时钟来计时，所以这里的35毫秒不受任何影响。当然，系统调度
的不确定性和不同操作系统的时钟精度意味着：线程调用和返回的实际时间间隔可能要比
35毫秒长。

现在，来看看“时间点”如何工作。*/

///4.3.3 时间点

















