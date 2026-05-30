
#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>

std::lock_guard<std::mutex> do_lock(std::mutex& m)
{
    return std::lock_guard(m);
}

template <typename TE>
class AsyncTaskProcessor;

template <typename TaskResult>
using ASYNC_COMPLETE_FUNC = void(TaskResult&);

template <typename TE>
class AsyncTaskBase : public std::enable_shared_from_this<AsyncTaskBase<TE>>
{
public:
    std::shared_ptr<AsyncTaskProcessor<TE>> env;
    std::atomic<bool> started = false;

    virtual ~AsyncTaskBase() = default;

    // main api here

    virtual void Start(TE & env) = 0;
    virtual void Stop(TE & env) {};


    // implemented internally
    // called on success or failure
    virtual void CompleteImpl() = 0;
};


template <typename P, typename TE>
class AsyncTask : public AsyncTaskBase<TE>
{
public:
    std::function<ASYNC_COMPLETE_FUNC<P>> finishedCb;
    using TaskEnv = TE;

    virtual void OnFinish(TE& env, P& result) {};

    void CompleteImpl()
    {
        bool owned = false;
        P result;
        OnFinish(this->env->env, result);
        if (finishedCb)
        {
            finishedCb(result);
        }
    }
};

// Task is a AsyncTaskBase<TE>
template <typename TE, typename Task>
class AsyncTaskRef
{
public:
    std::shared_ptr<Task> task;

    void Start(std::shared_ptr<AsyncTaskProcessor<TE>> const & env)
    {
        if (task->started.compare_exchange_strong(false, true))
        {
            task->env = env;
            env->BeginTask(task);
        }
    }
    void Stop()
    {
        if (task->started.compare_exchange_strong(true, false))
        {
            this->env->CancelTask(task);
        }
    }

};

template <typename TE>
struct ProcessorUpdate
{
    std::vector<std::shared_ptr<AsyncTaskBase<TE>>> completedTaskList;
};

template <typename TE>
class AsyncTaskProcessor
{
public:
    // holds on to tasks so the data doesn't poof if outside references disappear
    using Task = std::shared_ptr<AsyncTaskBase<TE>>;
    std::vector<Task> taskList;
    std::vector<Task> completedTasks;
    TE env;
    std::mutex mt;


private:
    bool AddTask(Task const & task)
    {
        auto it = std::find(taskList.begin(), taskList.end(), task);
        if (it != taskList.end())
        {
            taskList.emplace_back(task);
            return true;
        }
        return false;
    }

    bool RemoveTask(Task const & task)
    {
        auto it = std::find(taskList.begin(), taskList.end(), task);
        if (it != taskList.end())
        {
            taskList.erase(it);
            return true;
        }
        return false;
    }
public:

    void BeginTask(Task const & task)
    {
        bool canAdd = false;
        {
            auto __lg = do_lock(mt);
            canAdd = AddTask(task);
            if (canAdd)
            {
                task->Start(env);
            }
        }
    }

    void CancelTask(Task const & task)
    {
        bool canRm = false;
        {
            auto __lg = do_lock(mt);
            canRm = RemoveTask(task);
            if (canRm)
            {
                // problem with this is if canceling can't happen immediately then it will stall
                // problem for another time
                task->Stop(env);
            }
        }
        if (canRm)
        {
            task->CompleteImpl();
            task->started = false;
        }
    }

    void Update()
    {
        ProcessorUpdate<TE> result;
        auto _ = do_lock(mt);
        {
            update(result);
            for (auto&& task : result.completedTaskList)
            {
                RemoveTask(task);
            }
        }

        for (auto&& task : result.completedTaskList)
        {
            task->CompleteImpl();
            task->started = false;
        }

    }

};

template <typename T, typename Data>
auto CreateTask(Data && taskData) -> AsyncTaskRef<typename T::TaskEnv, T>
{
    AsyncTaskRef<typename T::TaskEnv, T> tr;
    tr.task->finishedCb = std::move(taskData);
    return tr;
}

