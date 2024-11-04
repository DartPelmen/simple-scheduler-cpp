#include <functional>
#include <chrono>
#include <future>
#include <queue>
#include <thread>
#include <memory>
#include <sstream>
#include <assert.h>
#include <iostream>

/**
 * Заглушка пула потоков.
 * В реальных задачах нужно что-то более работоспособное.
 */
class ThreadPool
{
public:
    /**
     * Выполнить задачу в пуле потоков.
     * @param task задача на исполнение
     */
    virtual void Execute(std::function<void()> task) = 0;
    virtual ~ThreadPool()
    {
    }
};
/**
 * Заглушка пула потоков. Просто запускает задачу в отдельном потоке.
 * В реальных задачах нужно что-то более работоспособное.
 */
class PseudoThreadPool : public ThreadPool
{
public:
    /**
     * Выполнить задачу в пуле потоков.
     * Здесь - просто в другом потоке.
     * @param task задача на исполнение
     */
    void Execute(std::function<void()> task) override
    {
        std::thread *t = new std::thread(std::move(task));
        t->join();
    }
};
/**
 * Запланированная задача.
 * Описывается, собственно, задачей, и временем ее выполнения.
 */
struct ScheduledTask
{
    std::function<void()> task;
    std::chrono::_V2::system_clock::time_point time;
    ScheduledTask(std::function<void()> task,std::time_t time) : task(task),
                                time(std::chrono::_V2::system_clock::from_time_t(time))
    {
       
       
    }
    /**
     * Сравнивает две задачи по времени выполнения.
     */
    bool operator<(const ScheduledTask &rhs) const
    {
        return time > rhs.time;
    }
};
/**
 * Планировщик задач.
 * Смотрим очередь задач (задачи отсортированы по времени) и запускает те, которые пора запускать.
 */
class Scheduler
{
private:
    /**
     * флаг, указывающий на то, нужно ли смотреть очередь задач
     */
    bool need_to_watch;
    /**
     * очередь задач
     */
    std::priority_queue<ScheduledTask> tasks;
    /**
     * поток, который смотрит задачи
     * ПРИМЕЧАНИЕ: вообще говоря, можно и на корутинах это все сделать, но нас это здесь не волнует.
     */
    std::unique_ptr<std::thread> task_watcher;
    /**
     * мьютекс для работы с очередью
     */
    std::mutex mtx;
    std::condition_variable cond;
    /**
     * пул, который выполняет задачи
     */
    std::unique_ptr<ThreadPool> pool;
    /**
     * смотреть очередь задач и запускать те, которые пока выполнить
     */
    void WatchTasks()
    {
        while (need_to_watch)
        {

            auto now = std::chrono::system_clock::now();
            std::unique_lock<std::mutex> lock(this->mtx);
            this->cond.wait(lock, [=]
                            {
                    while (!tasks.empty() && tasks.top().time <= now)
                    {
                        auto task = tasks.top();
                        tasks.pop();
                        pool -> Execute(task.task);
                    
                    }
                    return true; });
        //
            auto delay = tasks.empty()? std::chrono::nanoseconds(100000000) : tasks.top().time - now;

            std::this_thread::sleep_for(delay);
        }
    }

public:
    Scheduler() : tasks(std::priority_queue<ScheduledTask>()),
                  task_watcher(new std::thread([this]()
                                               { WatchTasks(); })),
                  pool(new PseudoThreadPool()),
                  need_to_watch(true)
    {
    }
    /**
     * Добавить задачу в очередь.
     * @param task задача на выполнение
     * @param time время выполнения задачи
     * @return true если задача добавлена успешно, иначе false
     */
    bool Add(std::function<void()> &task, std::time_t time)
    {   //примечание: да, chrono удобнее, чем time_t, но в задании именно здесь нужно time_t
        // здесь могла быть сложная проверка на то, имеет ли смысл добавлять задачу,
        // но здесь мы ограничимся тем, что не будем добавлять задачи, время которых уже прошло
        std::time_t now = std::time(nullptr);
        if (now > time)
        {
            return false;
        }

        std::unique_lock<std::mutex> lock(mtx);
        tasks.emplace(task, time);
        this->cond.notify_one();

        return true;
    }

    ~Scheduler()
    {

        need_to_watch = false;
        task_watcher->join();
    }
};
int main()
{

    Scheduler scheduler;
    std::function<void()> task = []()
    {
        std::cout << "work in " << std::this_thread::get_id() << std::endl;
    };
    std::function<void()> task2 = []()
    {
        std::cout << "second work in " << std::this_thread::get_id() << std::endl;
    };
    auto pool = new PseudoThreadPool();
    scheduler.Add(task, time(nullptr) + 1);
    scheduler.Add(task2, time(nullptr) + 2);
    scheduler.Add(task, time(nullptr) + 3);
    scheduler.Add(task2, time(nullptr) + 4);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}