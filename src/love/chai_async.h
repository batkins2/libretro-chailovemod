#ifndef SRC_LOVE_CHAI_ASYNC_H_
#define SRC_LOVE_CHAI_ASYNC_H_

#include <vector>
#include <functional>
#include <atomic>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>

#ifdef __HAVE_CHAISCRIPT__
#include <chaiscript/chaiscript.hpp>
#endif

namespace love {

// AsyncTask for true async execution (using std::future)
struct AsyncTask {
    int id;
    std::future<chaiscript::Boxed_Value> future;
    AsyncTask(int id_, std::future<chaiscript::Boxed_Value>&& fut) : id(id_), future(std::move(fut)) {}
};

class chai_async {
public:
    static constexpr int THREAD_POOL_SIZE = 16;
private:
    std::vector<AsyncTask> m_tasks;
    std::atomic<int> m_taskCounter{0};
    // Thread pool members
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> taskQueue;
    mutable std::mutex queueMutex;
    std::condition_variable condition;
    bool stopPool = false;

public:
    chai_async();
    ~chai_async();

    // Singleton accessor
    static chai_async* getInstance();

    // Generic enqueue for any callable (returns std::future)
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using return_type = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (stopPool)
                throw std::runtime_error("enqueue on stopped chai_async");
            taskQueue.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    // Async execution methods - launch tasks in background threads
    int executeAsync(const std::function<chaiscript::Boxed_Value()>& func);
    int executeAsync(const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value)>& func, 
                     chaiscript::Boxed_Value param);
    int executeAsync(const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value, chaiscript::Boxed_Value)>& func, 
                     chaiscript::Boxed_Value param1, chaiscript::Boxed_Value param2);

    // No update needed for true async; tasks run in background
    void update();
    
    // Check if task is completed (future ready)
    bool isTaskComplete(int taskId);
    chaiscript::Boxed_Value getTaskResult(int taskId);

    // Utility methods
    int getTaskCount() const;
    int getTotalTasksCreated() const;
    void sleep(double milliseconds);
    
    // ChaiScript binding method
    void bindToChaiScript(chaiscript::ChaiScript& chai);
};

} // namespace love

#endif