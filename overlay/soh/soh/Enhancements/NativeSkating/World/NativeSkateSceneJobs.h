#pragma once
#include "NativeSkateGrindCompiler.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <functional>
#include <memory>
namespace NativeSkateSceneJobs {
// One running job and one replaceable latest request. No actors/resources cross threads.
class Compiler {
    using Result = NativeSkateGrindCompiler::Result;
    using Overrides = NativeSkateGrindCompiler::Overrides;
    struct Job {
        uint64_t token;
        std::vector<float> triangles;
        Overrides overrides;
    };
    std::thread worker;
    mutable std::mutex mutex;
    std::condition_variable changed;
    bool stopping = false, running = false;
    uint64_t generation = 0, discarded = 0;
    std::optional<Job> pending;
    std::unique_ptr<Result> result;
    std::function<Result(const std::vector<float>&, const Overrides&)> compile;
    void Run() {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return stopping || pending.has_value(); });
                if (stopping)
                    return;
                job = std::move(*pending);
                pending.reset();
                running = true;
            }
            std::unique_ptr<Result> done;
            try {
                done = std::make_unique<Result>(compile(job.triangles, job.overrides));
            } catch (const std::exception& e) {
                done = std::make_unique<Result>();
                done->error = e.what();
            } catch (...) {
                done = std::make_unique<Result>();
                done->error = "Scene compiler exception";
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                running = false;
                if (job.token == generation && !stopping)
                    result = std::move(done);
                else
                    ++discarded;
            }
        }
    }

  public:
    explicit Compiler(
        std::function<Result(const std::vector<float>&, const Overrides&)> fn = NativeSkateGrindCompiler::Compile)
        : compile(std::move(fn)) {
    }
    ~Compiler() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
            pending.reset();
        }
        changed.notify_one();
        if (worker.joinable())
            worker.join();
    }
    uint64_t Submit(const std::vector<float>& triangles, const Overrides& overrides) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            ++generation;
            result.reset();
            pending = Job{ generation, triangles, overrides };
            if (!worker.joinable())
                worker = std::thread([this] { Run(); });
        }
        changed.notify_one();
        return generation;
    }
    void Cancel() {
        std::lock_guard<std::mutex> lock(mutex);
        ++generation;
        pending.reset();
        result.reset();
    }
    bool Poll(Result& out) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!result)
            return false;
        out = std::move(*result);
        result.reset();
        return true;
    }
    const char* State() const {
        std::lock_guard<std::mutex> lock(mutex);
        return pending ? "QUEUED" : running ? "RUNNING" : result ? "PUBLISHED" : "IDLE";
    }
    uint64_t Discarded() const {
        std::lock_guard<std::mutex> lock(mutex);
        return discarded;
    }
};
} // namespace NativeSkateSceneJobs
