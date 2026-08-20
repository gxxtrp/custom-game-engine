#pragma once

#include "engine/core/config.h"
#include "engine/core/containers.h"
#include <functional>
#include <atomic>
#include <memory>
#include <cstdint>
#include <string_view>

namespace engine::jobs {

struct JobCounter {
    std::atomic<uint32_t> counter{1};
};

using JobHandle = std::shared_ptr<JobCounter>;

enum class JobPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Count
};

struct JobDesc {
    std::function<void()> task;
    JobPriority priority{JobPriority::Normal};
    JobHandle dependency{};
    const char* debug_name{"UnnamedJob"};
};

class JobSystem {
public:
    static JobSystem& instance();

    bool init(uint32_t thread_count = 0);
    void shutdown();

    [[nodiscard]] JobHandle dispatch(const JobDesc& desc);
    [[nodiscard]] JobHandle dispatch(std::function<void()> task, JobPriority priority = JobPriority::Normal, const char* name = "Job");

    // Parallel For: slices [0, count) into batches and runs them in parallel
    [[nodiscard]] JobHandle parallel_for(uint32_t count, uint32_t batch_size, 
                                        std::function<void(uint32_t start, uint32_t end)> task, 
                                        JobPriority priority = JobPriority::Normal, 
                                        const char* name = "ParallelFor");

    void wait(const JobHandle& handle);
    bool is_complete(const JobHandle& handle) const;

    uint32_t get_worker_count() const;
    bool is_running() const { return m_running; }

    // Execute one pending job if available (useful for waiting threads)
    bool execute_one_job();

private:
    JobSystem();
    ~JobSystem();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<bool> m_running{false};
};

} // namespace engine::jobs
