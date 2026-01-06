#include "chai_async.h"
#include "../LibretroLog.h"
#include <chrono>
#include <algorithm>
#include <future>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#ifdef __HAVE_CHAISCRIPT__
using namespace chaiscript;
#endif

namespace love {

chai_async::chai_async() : stopPool(false) {
    for (int i = 0; i < chai_async::THREAD_POOL_SIZE; ++i) {
        workers.emplace_back([this]() {
            // Crash-proof: no logging, just a minimal operation
            static std::atomic<int> started{0};
            started.fetch_add(1, std::memory_order_relaxed);
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] { return this->stopPool || !this->taskQueue.empty(); });
                    if (this->stopPool && this->taskQueue.empty()) {
                        return;
                    }
                    task = std::move(this->taskQueue.front());
                    this->taskQueue.pop();
                }
                try {
                    task();
                } catch (...) {
                    // Swallow all exceptions to keep thread alive
                }
            }
        });
    }
}

chai_async::~chai_async() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stopPool = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers) {
        if (worker.joinable())
            worker.join();
    }
    m_tasks.clear();
}

// Singleton instance
static chai_async* g_chai_async_instance = nullptr;

chai_async* chai_async::getInstance() {
    if (!g_chai_async_instance) {
        static std::mutex singleton_mutex;
        std::lock_guard<std::mutex> lock(singleton_mutex);
        if (!g_chai_async_instance) {
            g_chai_async_instance = new chai_async();
        }
    }
    return g_chai_async_instance;
}



int chai_async::executeAsync(const std::function<chaiscript::Boxed_Value()>& func) {
    if (!func) {
        LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [chai_async] Invalid function provided" << std::endl;
        return -1;
    }
    int taskId = ++m_taskCounter;
    auto taskPtr = std::make_shared<std::packaged_task<chaiscript::Boxed_Value()>>(func);
    auto fut = taskPtr->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        taskQueue.emplace([taskPtr]() { (*taskPtr)(); });
        m_tasks.emplace_back(taskId, std::move(fut));
        // Cleanup completed tasks to prevent memory leaks
        m_tasks.erase(std::remove_if(m_tasks.begin(), m_tasks.end(), [](const AsyncTask& task) {
            return task.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }), m_tasks.end());
    }
    condition.notify_one();
    return taskId;
}

int chai_async::executeAsync(const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value)>& func, 
                            chaiscript::Boxed_Value param) {
    if (!func) {
        return -1;
    }
    int taskId = ++m_taskCounter;
    auto taskPtr = std::make_shared<std::packaged_task<chaiscript::Boxed_Value()>>([func, param]() { return func(param); });
    auto fut = taskPtr->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        taskQueue.emplace([taskPtr]() { (*taskPtr)(); });
        m_tasks.emplace_back(taskId, std::move(fut));
        // Cleanup completed tasks to prevent memory leaks
        m_tasks.erase(std::remove_if(m_tasks.begin(), m_tasks.end(), [](const AsyncTask& task) {
            return task.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }), m_tasks.end());
    }
    condition.notify_one();
    return taskId;
}

int chai_async::executeAsync(const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value, chaiscript::Boxed_Value)>& func, 
                            chaiscript::Boxed_Value param1, chaiscript::Boxed_Value param2) {
    if (!func) {
        return -1;
    }
    int taskId = ++m_taskCounter;
    auto taskPtr = std::make_shared<std::packaged_task<chaiscript::Boxed_Value()>>([func, param1, param2]() { return func(param1, param2); });
    auto fut = taskPtr->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        taskQueue.emplace([taskPtr]() { (*taskPtr)(); });
        m_tasks.emplace_back(taskId, std::move(fut));
        // Cleanup completed tasks to prevent memory leaks
        m_tasks.erase(std::remove_if(m_tasks.begin(), m_tasks.end(), [](const AsyncTask& task) {
            return task.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }), m_tasks.end());
    }
    condition.notify_one();
    return taskId;
}

void chai_async::update() {
    // No-op: tasks are now truly async
}

bool chai_async::isTaskComplete(int taskId) {
    std::unique_lock<std::mutex> lock(queueMutex);
    auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
        [taskId](const AsyncTask& task) { return task.id == taskId; });
    if (it != m_tasks.end()) {
        auto status = it->future.wait_for(std::chrono::seconds(0));
        if (status == std::future_status::ready) {
            m_tasks.erase(it);
            return true;
        }
    }
    return false;
}

chaiscript::Boxed_Value chai_async::getTaskResult(int taskId) {
    std::unique_lock<std::mutex> lock(queueMutex);
    auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
        [taskId](const AsyncTask& task) { return task.id == taskId; });
    if (it != m_tasks.end()) {
        auto status = it->future.wait_for(std::chrono::seconds(0));
        if (status == std::future_status::ready) {
            try {
                auto result = it->future.get();
                m_tasks.erase(it);
                return result;
            } catch (...) {
                m_tasks.erase(it);
                return chaiscript::Boxed_Value();
            }
        }
    }
    return chaiscript::Boxed_Value();
}

int chai_async::getTaskCount() const {
    std::unique_lock<std::mutex> lock(queueMutex);
    int pendingCount = 0;
    for (const auto& task : m_tasks) {
        auto status = task.future.wait_for(std::chrono::seconds(0));
        if (status != std::future_status::ready) {
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

void chai_async::bindToChaiScript(chaiscript::ChaiScript& chai) {
#ifdef __HAVE_CHAISCRIPT__
    try {
        // Register all overloads with unique names
        chai.add(chaiscript::fun([this](const std::function<chaiscript::Boxed_Value()>& func) -> int {
            return this->executeAsync(func);
        }), "async0");
        chai.add(chaiscript::fun([this](const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value)>& func, 
                                       chaiscript::Boxed_Value param) -> int {
            return this->executeAsync(func, param);
        }), "async1");
        chai.add(chaiscript::fun([this](const std::function<chaiscript::Boxed_Value(chaiscript::Boxed_Value, chaiscript::Boxed_Value)>& func, 
                                       chaiscript::Boxed_Value param1, chaiscript::Boxed_Value param2) -> int {
            return this->executeAsync(func, param1, param2);
        }), "async2");

        // ChaiScript wrapper for 'async' that dispatches based on argument count
        chai.eval(R"(
            def async(f) {
                async0(f);
            }
            def async(f, a) {
                async1(f, a);
            }
            def async(f, a, b) {
                async2(f, a, b);
            }
        )");
        chai.add(chaiscript::fun([this](int taskId) -> bool {
            return this->isTaskComplete(taskId);
        }), "isTaskComplete");
        chai.add(chaiscript::fun([this](int taskId) -> chaiscript::Boxed_Value {
            return this->getTaskResult(taskId);
        }), "getTaskResult");
        chai.add(chaiscript::fun([this]() {
            this->update();
        }), "updateAsync");
        chai.add(chaiscript::fun([this]() -> int {
            return this->getTaskCount();
        }), "asyncTaskCount");
        chai.add(chaiscript::fun([this]() -> int {
            return this->getTotalTasksCreated();
        }), "getTotalTasksCreated");
        chai.add(chaiscript::fun([this](double milliseconds) {
            this->sleep(milliseconds);
        }), "sleep");
        // LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [chai_async] Thread-free ChaiScript bindings registered" << std::endl;
    } catch (const std::exception& e) {
        LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [chai_async] Failed to bind to ChaiScript: " << e.what() << std::endl;
    }
#endif
}

} // namespace love