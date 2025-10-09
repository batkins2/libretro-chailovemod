#include "chai_async.h"
#include "../LibretroLog.h"
#include <chrono>
#include <algorithm>

#ifdef __HAVE_CHAISCRIPT__
using namespace chaiscript;
#endif

namespace love {

chai_async::chai_async() {
    LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [chai_async] Initialized (thread-free mode)" << std::endl;
}

chai_async::~chai_async() {
    LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [chai_async] Destroyed" << std::endl;
}

// Async execution - no parameters (queues task for later execution)
int chai_async::executeAsync(const std::function<chaiscript::Boxed_Value()>& func) {
    if (!func) {
        LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [chai_async] Invalid function provided" << std::endl;
        return -1;
    }
    
    int taskId = ++m_taskCounter;
    m_tasks.emplace_back(func, taskId);
    
    LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [chai_async] Task " << taskId << " queued (async)" << std::endl;
    return taskId; // Return immediately
}

// Async execution - single parameter
int chai_async::executeAsync(const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value)>& func, 
                            chaiscript::Boxed_Value param) {
    if (!func) {
        return -1;
    }
    
    // Wrap the parameterized function in a no-parameter lambda
    auto wrappedFunc = [func, param]() -> chaiscript::Boxed_Value {
        return func(param);
    };
    
    int taskId = ++m_taskCounter;
    m_tasks.emplace_back(wrappedFunc, taskId);
    
    LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [chai_async] Task " << taskId << " queued with 1 param (async)" << std::endl;
    return taskId;
}

// Async execution - two parameters
int chai_async::executeAsync(const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value, chaiscript::Boxed_Value)>& func, 
                            chaiscript::Boxed_Value param1, chaiscript::Boxed_Value param2) {
    if (!func) {
        return -1;
    }
    
    // Wrap the parameterized function in a no-parameter lambda
    auto wrappedFunc = [func, param1, param2]() -> chaiscript::Boxed_Value {
        return func(param1, param2);
    };
    
    int taskId = ++m_taskCounter;
    m_tasks.emplace_back(wrappedFunc, taskId);
    
    LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [chai_async] Task " << taskId << " queued with 2 params (async)" << std::endl;
    return taskId;
}

// Update method - processes one task per call (call this every frame)
void chai_async::update() {
    // Find the next uncompleted task
    for (auto& task : m_tasks) {
        if (!task.completed) {
            try {
                LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [chai_async] Executing task " << task.id << std::endl;
                task.result = task.func();
                task.completed = true;
                LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [chai_async] Task " << task.id << " completed" << std::endl;
                return; // Process only one task per update
            } catch (const std::exception& e) {
                LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [chai_async] Task " << task.id << " error: " << e.what() << std::endl;
                task.completed = true;
                task.result = chaiscript::Boxed_Value();
                return;
            } catch (...) {
                LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [chai_async] Task " << task.id << " unknown error" << std::endl;
                task.completed = true;
                task.result = chaiscript::Boxed_Value();
                return;
            }
        }
    }
}

// Check if task is completed
bool chai_async::isTaskComplete(int taskId) {
    auto it = std::find_if(m_tasks.begin(), m_tasks.end(), 
                          [taskId](const AsyncTask& task) { return task.id == taskId; });
    
    if (it != m_tasks.end()) {
        return it->completed;
    }
    return false; // Task not found
}

// Get task result
chaiscript::Boxed_Value chai_async::getTaskResult(int taskId) {
    auto it = std::find_if(m_tasks.begin(), m_tasks.end(), 
                          [taskId](const AsyncTask& task) { return task.id == taskId; });
    
    if (it != m_tasks.end() && it->completed) {
        return it->result;
    }
    return chaiscript::Boxed_Value(); // Task not found or not completed
}

// Utility functions
int chai_async::getTaskCount() const {
    int pendingCount = 0;
    for (const auto& task : m_tasks) {
        if (!task.completed) {
            pendingCount++;
        }
    }
    return pendingCount;
}

int chai_async::getTotalTasksCreated() const {
    return m_taskCounter.load();
}

void chai_async::sleep(double milliseconds) {
    if (milliseconds > 0 && milliseconds < 60000) {
        auto start = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::milliseconds(static_cast<long long>(milliseconds));
        
        while (std::chrono::high_resolution_clock::now() - start < duration) {
            // Busy wait (no threading required)
        }
    }
}

// Bind async functions to ChaiScript
void chai_async::bindToChaiScript(chaiscript::ChaiScript& chai) {
    #ifdef __HAVE_CHAISCRIPT__
    
    try {
        // Async functions that queue tasks
        chai.add(chaiscript::fun([this](const std::function<chaiscript::Boxed_Value()>& func) -> int {
            return this->executeAsync(func);
        }), "async");
        
        chai.add(chaiscript::fun([this](const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value)>& func, 
                                       chaiscript::Boxed_Value param) -> int {
            return this->executeAsync(func, param);
        }), "async");
        
        chai.add(chaiscript::fun([this](const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value, chaiscript::Boxed_Value)>& func, 
                                       chaiscript::Boxed_Value param1, chaiscript::Boxed_Value param2) -> int {
            return this->executeAsync(func, param1, param2);
        }), "async");
        
        // Task management functions
        chai.add(chaiscript::fun([this](int taskId) -> bool {
            return this->isTaskComplete(taskId);
        }), "isTaskComplete");
        
        chai.add(chaiscript::fun([this](int taskId) -> chaiscript::Boxed_Value {
            return this->getTaskResult(taskId);
        }), "getTaskResult");
        
        // Update function (call this every frame)
        chai.add(chaiscript::fun([this]() {
            this->update();
        }), "updateAsync");
        
        // Utility functions
        chai.add(chaiscript::fun([this]() -> int {
            return this->getTaskCount();
        }), "asyncTaskCount");
        
        chai.add(chaiscript::fun([this]() -> int {
            return this->getTotalTasksCreated();
        }), "getTotalTasksCreated");
        
        chai.add(chaiscript::fun([this](double milliseconds) {
            this->sleep(milliseconds);
        }), "sleep");
        
        LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [chai_async] Thread-free ChaiScript bindings registered" << std::endl;
        
    } catch (const std::exception& e) {
        LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [chai_async] Failed to bind to ChaiScript: " << e.what() << std::endl;
    }
    
    #endif
}

} // namespace love