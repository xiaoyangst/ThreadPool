#include "AsyncThreadPool.h"

AsyncThreadPool::AsyncThreadPool(int min, int max, int queMaxSize)
    : m_min(min),
      m_max(max),
      m_queSize(queMaxSize),
      m_curThreadNum(min),
      m_idleThreadNum(min),
      m_exitThreadNum(0),
      m_stop(false) {
  m_manager = std::make_unique<std::thread>(&AsyncThreadPool::manager, this);
  for (int i = 0; i < m_curThreadNum; ++i) {
    auto th = std::thread(&AsyncThreadPool::worker, this);
    std::thread::id id = th.get_id();
    m_threadPoolMap[id] = std::move(th);
  }
}

bool AsyncThreadPool::addTask(AsyncThreadPool::taskCallBack cb) {
  std::lock_guard<std::mutex> lg(m_queueMtx);
  if (m_taskQueue.size() < m_queSize) {
    m_taskQueue.push(std::move(cb));
    m_cond.notify_one();
    return true;
  }
  return false;
}

void AsyncThreadPool::worker() {
  while (!m_stop.load()) {
    taskCallBack task;
    std::unique_lock<std::mutex> lock(m_queueMtx);
    while (m_taskQueue.empty() && !m_stop.load()) { // 不用 lambda 那种是方便直接 return 关闭线程
      m_cond.wait(lock);
      if (m_exitThreadNum.load() > 0) {
        m_exitThreadNum.fetch_sub(1);
        m_curThreadNum.fetch_sub(1);
        m_idleThreadNum.fetch_sub(1);
        {
          std::lock_guard<std::mutex> find_lock(m_findVec);
          m_threadFindVec.push_back(std::this_thread::get_id());
        }
        return;
      }
    }
    if (!m_taskQueue.empty() && !m_stop) {
      task = std::move(m_taskQueue.front());
      m_taskQueue.pop();
      lock.unlock();  // 拿到任务函数就可以释放锁了
      if (task) {
        m_idleThreadNum.fetch_sub(1);
        task();
        m_idleThreadNum.fetch_add(1);
      }
    }
  }
}

void AsyncThreadPool::manager() {
  while (!m_stop.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(ASYNC_MANAGER_THREAD_SLEEP_TIME));
    int idleThreadNum = m_idleThreadNum.load();
    int curThreadNum = m_curThreadNum.load();
    // 什么时候增加线程？没有空闲线程，但要让已有线程数量小于最大线程数量，那么就可以增加
    if (idleThreadNum == 0 && curThreadNum < m_max) {
      auto th = std::thread(&AsyncThreadPool::worker, this);
      m_curThreadNum.fetch_add(1);
      m_idleThreadNum.fetch_add(1);
      std::thread::id id = th.get_id();
      m_threadPoolMap[id] = std::move(th);
    }
      // 什么时候减少线程？有空闲线程且为已有的线程数量的一半，但要让任务线程数量大于最小线程数量，那么就可以减少
    else if (idleThreadNum > (curThreadNum / 2) && curThreadNum > m_min) {
      int exit_num = m_idleThreadNum.load();
      int result = std::min(exit_num, ASYNC_EXIT_THREAD_NUM);  // 确保销毁的线程小于等于空闲线程的数量
      m_exitThreadNum.store(result);
      m_cond.notify_all();
      std::this_thread::sleep_for(std::chrono::seconds(ASYNC_MANAGER_THREAD_SLEEP_TIME));
      for (const auto id : m_threadFindVec) {
        if ((m_threadPoolMap.find(id) != m_threadPoolMap.end()) && m_threadPoolMap[id].joinable()) {
          m_threadPoolMap[id].join();
          m_threadPoolMap.erase(id);
        }
      }
      m_threadFindVec.clear();
    }
  }
}
AsyncThreadPool::~AsyncThreadPool() {
  m_stop.store(true);
  m_cond.notify_all();
  for (auto &item : m_threadPoolMap) {
    if (item.second.joinable()) {
      item.second.join();
    }
  }
  if (m_manager->joinable()) {
    m_manager->join();
  }
}
