#include "engine/jobs/job_system.h"
#include "engine/core/log.h"
#include <thread>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <random>

namespace engine::jobs {

struct JobEntry {
    std::function<void()> task;
    JobHandle counter;
    JobHandle dependency;
    JobPriority priority{JobPriority::Normal};
    const char* debug_name{"Job"};
};

struct WorkerQueue {
    std::deque<JobEntry> jobs;
    std::mutex mutex;
};

struct JobSystem::Impl {
    std::vector<std::thread> workers;
    std::vector<std::unique_ptr<WorkerQueue>> worker_queues;
    WorkerQueue global_queues[static_cast<size_t>(JobPriority::Count)];

    std::mutex cv_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop_flag{false};
    uint32_t thread_count{0};

    static thread_local int32_t s_worker_index;

    bool pop_job_from_queue(WorkerQueue& q, JobEntry& out_job) {
        std::lock_guard<std::mutex> lock(q.mutex);
        if (q.jobs.empty()) return false;
        out_job = std::move(q.jobs.front());
        q.jobs.pop_front();
        return true;
    }

    bool pop_job_for_thread(int32_t thread_id, JobEntry& out_job) {
        // 1. Try local worker queue
        if (thread_id >= 0 && thread_id < static_cast<int32_t>(worker_queues.size())) {
            if (pop_job_from_queue(*worker_queues[thread_id], out_job)) {
                return true;
            }
        }

        // 2. Try global priority queues (High -> Normal -> Low)
        for (int p = static_cast<int>(JobPriority::High); p >= 0; --p) {
            if (pop_job_from_queue(global_queues[p], out_job)) {
                return true;
            }
        }

        // 3. Steal from other worker queues
        if (!worker_queues.empty()) {
            size_t count = worker_queues.size();
            size_t start = (thread_id >= 0) ? (thread_id + 1) % count : 0;
            for (size_t i = 0; i < count; ++i) {
                size_t target = (start + i) % count;
                if (pop_job_from_queue(*worker_queues[target], out_job)) {
                    return true;
                }
            }
        }

        return false;
    }

    void execute_job(JobEntry& job) {
        // If dependency is not complete, re-enqueue
        if (job.dependency && job.dependency->counter.load(std::memory_order_acquire) > 0) {
            std::lock_guard<std::mutex> lock(global_queues[static_cast<size_t>(job.priority)].mutex);
            global_queues[static_cast<size_t>(job.priority)].jobs.push_back(std::move(job));
            return;
        }

        if (job.task) {
            job.task();
        }

        if (job.counter) {
            job.counter->counter.fetch_sub(1, std::memory_order_release);
        }
    }

    void worker_loop(int32_t thread_id) {
        s_worker_index = thread_id;

        while (!stop_flag.load(std::memory_order_relaxed)) {
            JobEntry job;
            if (pop_job_for_thread(thread_id, job)) {
                execute_job(job);
            } else {
                std::unique_lock<std::mutex> lock(cv_mutex);
                cv.wait_for(lock, std::chrono::milliseconds(1), [this]() {
                    return stop_flag.load(std::memory_order_relaxed);
                });
            }
        }
    }
};

thread_local int32_t JobSystem::Impl::s_worker_index = -1;

JobSystem& JobSystem::instance() {
    static JobSystem s_instance;
    return s_instance;
}

JobSystem::JobSystem() = default;

JobSystem::~JobSystem() {
    shutdown();
}

bool JobSystem::init(uint32_t thread_count) {
    if (m_running) return true;

    if (thread_count == 0) {
        uint32_t hw = std::thread::hardware_concurrency();
        thread_count = (hw > 1) ? (hw - 1) : 1;
    }

    m_impl = std::make_unique<Impl>();
    m_impl->thread_count = thread_count;
    m_impl->stop_flag = false;

    for (uint32_t i = 0; i < thread_count; ++i) {
        m_impl->worker_queues.push_back(std::make_unique<WorkerQueue>());
    }

    for (uint32_t i = 0; i < thread_count; ++i) {
        m_impl->workers.emplace_back(&Impl::worker_loop, m_impl.get(), static_cast<int32_t>(i));
    }

    m_running = true;
    LOG_INFO("JobSystem", "JobSystem initialized with {} worker threads", thread_count);
    return true;
}

void JobSystem::shutdown() {
    if (!m_running) return;

    m_impl->stop_flag.store(true, std::memory_order_release);
    m_impl->cv.notify_all();

    for (auto& worker : m_impl->workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    m_impl->workers.clear();
    m_impl->worker_queues.clear();
    m_impl.reset();
    m_running = false;

    LOG_INFO("JobSystem", "JobSystem shutdown cleanly");
}

JobHandle JobSystem::dispatch(const JobDesc& desc) {
    if (!m_running || !desc.task) return nullptr;

    JobHandle handle = std::make_shared<JobCounter>();
    handle->counter.store(1, std::memory_order_relaxed);

    JobEntry entry{
        .task = desc.task,
        .counter = handle,
        .dependency = desc.dependency,
        .priority = desc.priority,
        .debug_name = desc.debug_name
    };

    int32_t tid = Impl::s_worker_index;
    if (tid >= 0 && tid < static_cast<int32_t>(m_impl->worker_queues.size())) {
        std::lock_guard<std::mutex> lock(m_impl->worker_queues[tid]->mutex);
        m_impl->worker_queues[tid]->jobs.push_back(std::move(entry));
    } else {
        std::lock_guard<std::mutex> lock(m_impl->global_queues[static_cast<size_t>(desc.priority)].mutex);
        m_impl->global_queues[static_cast<size_t>(desc.priority)].jobs.push_back(std::move(entry));
    }

    m_impl->cv.notify_one();
    return handle;
}

JobHandle JobSystem::dispatch(std::function<void()> task, JobPriority priority, const char* name) {
    return dispatch(JobDesc{
        .task = std::move(task),
        .priority = priority,
        .dependency = {},
        .debug_name = name
    });
}

JobHandle JobSystem::parallel_for(uint32_t count, uint32_t batch_size, 
                                  std::function<void(uint32_t start, uint32_t end)> task, 
                                  JobPriority priority, 
                                  const char* name) {
    if (!m_running || count == 0 || !task) return nullptr;

    if (batch_size == 0) batch_size = 1;
    uint32_t num_batches = (count + batch_size - 1) / batch_size;

    JobHandle handle = std::make_shared<JobCounter>();
    handle->counter.store(num_batches, std::memory_order_relaxed);

    for (uint32_t b = 0; b < num_batches; ++b) {
        uint32_t start = b * batch_size;
        uint32_t end = std::min(start + batch_size, count);

        JobEntry entry{
            .task = [task, start, end]() { task(start, end); },
            .counter = handle,
            .dependency = {},
            .priority = priority,
            .debug_name = name
        };

        size_t queue_idx = b % m_impl->worker_queues.size();
        {
            std::lock_guard<std::mutex> lock(m_impl->worker_queues[queue_idx]->mutex);
            m_impl->worker_queues[queue_idx]->jobs.push_back(std::move(entry));
        }
    }

    m_impl->cv.notify_all();
    return handle;
}

void JobSystem::wait(const JobHandle& handle) {
    if (!handle) return;

    while (handle->counter.load(std::memory_order_acquire) > 0) {
        if (!execute_one_job()) {
            std::this_thread::yield();
        }
    }
}

bool JobSystem::is_complete(const JobHandle& handle) const {
    if (!handle) return true;
    return handle->counter.load(std::memory_order_acquire) == 0;
}

uint32_t JobSystem::get_worker_count() const {
    return m_impl ? m_impl->thread_count : 0;
}

bool JobSystem::execute_one_job() {
    if (!m_impl) return false;
    JobEntry job;
    if (m_impl->pop_job_for_thread(Impl::s_worker_index, job)) {
        m_impl->execute_job(job);
        return true;
    }
    return false;
}

} // namespace engine::jobs
