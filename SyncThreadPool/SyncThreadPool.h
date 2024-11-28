/**
  ******************************************************************************
  * @file           : SyncThreadPool.h
  * @author         : xy
  * @brief          : 异步线程池（可获取任务执行结果）+线程数量动态变化+队列大小限制
  * @attention      : None
  * @date           : 2024/11/28
  ******************************************************************************
  */

#ifndef THREADPOOLS_SYNCTHREADPOOL_SYNCTHREADPOOL_H_
#define THREADPOOLS_SYNCTHREADPOOL_SYNCTHREADPOOL_H_
constexpr int ASYNC_MANAGER_THREAD_SLEEP_TIME = 3;
constexpr int ASYNC_EXIT_THREAD_NUM = 2;
constexpr int ASYNC_QUEUE_MAX_SIZE = 1024;
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <unordered_set>
#include <future>
class SyncThreadPool {
  using taskCallBack = std::function<void(void)>;
 public:
  explicit SyncThreadPool(int min = 3, int max = std::thread::hardware_concurrency(),
                          int queMaxSize = ASYNC_QUEUE_MAX_SIZE);
  ~SyncThreadPool();

  SyncThreadPool(const SyncThreadPool &) = delete;
  SyncThreadPool &operator=(const SyncThreadPool &) = delete;
  SyncThreadPool(SyncThreadPool &&) = delete;
  SyncThreadPool &operator=(SyncThreadPool &&) = delete;
 private:
  int m_min;  // 线程数量下线
  int m_max;  // 线程数量上限
  int m_queSize; // 队列上限
  std::atomic<int> m_curThreadNum;   // 我们创建的线程数量，不代表实际工作的线程数量，没有工作的就纳入空闲
  std::atomic<int> m_idleThreadNum;   // 空闲线程数量
  std::atomic<int> m_exitThreadNum;   // 决定移除的线程数量，自行指定
  std::atomic<bool> m_stop;           // 终止线程池，方便未工作完成的线程能够正常退出

  std::mutex m_queueMtx;
  std::queue<taskCallBack> m_taskQueue;  // 任务队列
  std::condition_variable m_cond;
  std::unordered_map<std::thread::id, std::thread> m_threadPoolMap; // 线程池
  std::unique_ptr<std::thread> m_manager;  // 管理线程
  std::mutex m_findVec;
  std::vector<std::thread::id> m_threadFindVec;  // 存储待删除的线程，以 id 标识
 private:
  void worker();  // 执行任务的线程（多个子线程，从任务队列取任务，再无其它）
  void manager(); // 管理线程（一个子线程，按时检测线程池状态并进行调整，调整工作线程数量）
 public:
  template<typename F, typename... Args>
  auto addTask(F &&f, Args &&... args) -> std::optional<std::future<typename std::result_of<F(Args...)>::type>> {
    using returnType = typename std::result_of<F(Args...)>::type;

    // 创建任务并将返回的 future 保存
    auto task = std::make_shared<std::packaged_task<returnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<returnType> res = task->get_future();
    {
      std::lock_guard<std::mutex> lg(m_queueMtx);
      if (m_taskQueue.size() >= m_queSize) {
        return std::nullopt;  // 说明队列已满
      }
      m_taskQueue.emplace([task]() { (*task)(); });
    }

    m_cond.notify_one();
    return std::make_optional(std::move(res));  // 返回有效的任务执行完成的返回值 future
  }

};

#endif //THREADPOOLS_SYNCTHREADPOOL_SYNCTHREADPOOL_H_

/*===========================================Use=============================================
AsyncThreadPool pool;

auto taskResult = pool.addTask([](int a, int b) { return a + b; }, 5, 7);
if (taskResult) {
    std::cout << "Task added successfully! Result: " << taskResult->get() << std::endl;
} else {
    std::cout << "Task queue is full. Task not added." << std::endl;
}
*/