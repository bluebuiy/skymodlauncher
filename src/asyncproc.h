
#pragma once

#include "asyncengine.h"

#include <string>
#include <thread>
#include <mutex>

class AsyncProcessEngine;

struct AsyncProcessResult
{
    int exitCode;
    std::string output;
};

struct AsyncProcessTask : public AsyncTask<AsyncProcessResult, AsyncProcessEngine>
{
    int pid = 0;
    std::vector<std::string> args;
    std::string wd = "/";
    bool captureStdOut = false;

    ~AsyncProcessTask();

    void Start(AsyncProcessEngine&);
    void Stop(AsyncProcessEngine&);
    void OnFinish(AsyncProcessEngine&, AsyncProcessResult& outResult);
};

struct AsyncProcessState
{
    int pid = -1;
    int exitCode = -1;
    std::shared_ptr<AsyncTaskBase<AsyncProcessEngine>> task;
};

class AsyncProcessEngine
{
public:
    std::mutex mt;
    std::vector<AsyncProcessState> procs;
    std::vector<AsyncProcessState> failed;

    void update(ProcessorUpdate<AsyncProcessEngine>& result);
};

using ProcessTask = AsyncTaskRef<AsyncProcessEngine, AsyncProcessTask>;



