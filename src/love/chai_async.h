#ifndef SRC_LOVE_CHAI_ASYNC_H_
#define SRC_LOVE_CHAI_ASYNC_H_

#include <vector>
#include <functional>
#include <atomic>

#ifdef __HAVE_CHAISCRIPT__
#include <chaiscript/chaiscript.hpp>
#endif

namespace love {

// Simple task structure
struct AsyncTask {
    std::function<chaiscript::Boxed_Value()> func;
    int id;
    bool completed;
    chaiscript::Boxed_Value result;
    
    AsyncTask(std::function<chaiscript::Boxed_Value()> f, int taskId) 
        : func(f), id(taskId), completed(false) {}
};

class chai_async {
private:
    std::vector<AsyncTask> m_tasks;
    std::atomic<int> m_taskCounter{0};
    std::atomic<int> m_currentTask{0};

public:
    chai_async();
    ~chai_async();

    // Async execution methods - execute one task per frame
    int executeAsync(const std::function<chaiscript::Boxed_Value()>& func);
    int executeAsync(const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value)>& func, 
                     chaiscript::Boxed_Value param);
    int executeAsync(const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value, chaiscript::Boxed_Value)>& func, 
                     chaiscript::Boxed_Value param1, chaiscript::Boxed_Value param2);

    // Update method - call this every frame to process tasks
    void update();
    
    // Check if task is completed
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