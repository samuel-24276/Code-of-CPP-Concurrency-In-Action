#include <iostream>
#include <thread>
#include<numeric>
#include<vector>

/*
本程序实现了并行版的std::accumulate。代码将整体工作拆分成小任务，交给每个线程去做，
并设置最小任务数，避免产生太多的线程，程序会在操作数量为0时抛出异常。
比如，std::thread无法启动线程，就会抛出异常。
*/

template<typename Iterator,typename T>
struct accumulate_block
{
    void operator()(Iterator first,Iterator last,T& result)
    {
        //accumulate定义在#include<numeric>中，作用有两个，一个是累加求和，
        //另一个是自定义类型数据的处理
        result=std::accumulate(first,last,result);
    }
};

template<typename Iterator,typename T>
T parallel_accumulate(Iterator first,Iterator last,T init)
{
    unsigned long const length=std::distance(first,last);//distance确定两指针的距离，也即范围内包含的元素个数

    if(!length) // 1,如果输入的范围为空，就会得到init的值。
        return init;

    unsigned long const min_per_thread=25;
// 2,如果范围内的元素多于一个时，需要用范围内元素的总数量除以线程(块)中最小任务数，
//从而确定启动线程的最大数量
    unsigned long const max_threads=
        (length+min_per_thread-1)/min_per_thread;

//std::thread::hardware_concurrency()在新版C++中非常有用，其会返回并发线程的数量。
//例如，多核系统中，返回值可以是CPU核芯的数量。返回值也仅仅是一个标识，当无法获取时，函数返回0。
    unsigned long const hardware_threads=
        std::thread::hardware_concurrency();
    std::cout<<"hardware_threads="<<hardware_threads<<std::endl;

//因为上下文频繁切换会降低线程的性能，所以计算量的最大值和硬件支持线程数，
//较小的值为启动线程的数量
    unsigned long const num_threads=  // 3
        std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    std::cout<<"num_threads="<<num_threads<<std::endl;

//每个线程中处理的元素数量，是范围中元素的总量除以线程的个数得出的
    unsigned long const block_size=length/num_threads; // 4
    std::cout<<"block_size="<<block_size<<std::endl;

    std::vector<T> results(num_threads);//存放中间结果
    std::vector<std::thread> threads(num_threads-1);  // 5,创建一个std::vector<std::thread>容器

    Iterator block_start=first;
//因为在启动之前已经有了一个线程(主线程)，所以启动的线程数必须比num_threads少1
    for(unsigned long i=0; i < (num_threads-1); ++i)
    {
        Iterator block_end=block_start;
        //使用循环来启动线程：block_end迭代器指向当前块的末尾
        std::advance(block_end,block_size);  // 6,advance()用于指针移位
        threads[i]=std::thread(     // 7，启动一个新线程为当前块累加结果
                       accumulate_block<Iterator,T>(),
                       block_start,block_end,std::ref(results[i]));
        block_start=block_end;  // 8，当迭代器指向当前块的末尾时，启动下一个块
    }
    accumulate_block<Iterator,T>()(
        block_start,last,results[num_threads-1]); //9，启动所有线程后，线程会处理最终块的结果。
        //因为知道最终块是哪一个，所以最终块中有多少个元素就无所谓了。

    for (auto& entry : threads)
        entry.join();  // 10，累加最终块的结果后，可等待std::for_each创建线程

    return std::accumulate(results.begin(),results.end(),init); // 11，将所有结果进行累加
}

int main()
{
    std::vector<int> data{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17};
    int ret = 0;
    ret = parallel_accumulate(data.begin(), data.end(), ret);
    std::cout<<ret<<std::endl;
    return 0;
}
