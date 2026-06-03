
#include "asyncproc.h"
#include "prochelper.h"

#include <signal.h>
#include <sys/wait.h>
#include <iostream>

AsyncProcessTask::~AsyncProcessTask()
{
    // actually it's "fine" if we just let the process finish
    if (pid != -1)
    {
        kill(pid, SIGINT);
    }
}

void AsyncProcessTask::Start(AsyncProcessEngine& env)
{
    int pid = fork();
    if (pid == 0)
    {
        if (!ExecArgs(this->args))
        {
            exit(1);
        }
    }
    else if (pid != -1)
    {
        this->pid = pid;
        auto _lg = do_lock(env.mt);
        auto& p = env.procs.emplace_back();
        p.pid = pid;
        p.task = this->shared_from_this();
    }
    else
    {
        this->pid = -1;
        auto _lg = do_lock(env.mt);
        auto& p = env.failed.emplace_back();
        p.task = this->shared_from_this();
    }
}

void AsyncProcessTask::Stop(AsyncProcessEngine& env)
{
    if (pid != -1)
    {
        int killed = kill(pid, SIGINT);
        {
            auto _lg = do_lock(env.mt);
            auto it = std::find_if(env.procs.begin(), env.procs.end(), [pid = this->pid](AsyncProcessState const & ps) {
                return ps.pid == pid;
            });
            if (it != env.procs.end())
            {
                env.procs.erase(it);
            }
        }
    }
}

void AsyncProcessTask::OnFinish(AsyncProcessEngine& env, AsyncProcessResult& result)
{
    result.exitCode = -1;
    {
        auto _lg = do_lock(env.mt);
        auto it = std::find_if(env.procs.begin(), env.procs.end(), [pid = this->pid](AsyncProcessState const & ps) {
            return ps.pid == pid;
        });
        if (it != env.procs.end())
        {
            result.exitCode = it->exitCode;
            env.procs.erase(it);
        }
    }
}

void AsyncProcessEngine::update(ProcessorUpdate<AsyncProcessEngine>& result)
{
    {
        auto _ = do_lock(mt);
        for(auto&& f : failed)
        {
            result.completedTaskList.emplace_back(std::move(f.task));
        }
        failed.clear();

        for (int i = 0; i < procs.size(); ++i)
        {
            int status = 0;
            if (waitpid(procs[i].pid, &status, WNOHANG | WUNTRACED) == procs[i].pid)
            {
                bool done = false;
                if (WIFEXITED(status))
                {
                    procs[i].exitCode = WEXITSTATUS(status);
                    done = true;
                }
                else if (WIFSIGNALED(status))
                {
                    done = true;
                }
                else if (WIFSTOPPED(status))
                {
                    done = true;
                }
                if (done)
                {
                    result.completedTaskList.emplace_back(std::move(procs[i].task));
                }
            }
            else
            {
                int e = errno;
                if (e != EAGAIN)
                {
                    std::cout << errno << std::endl;
                }
                // err, idk what to do rn
            }
        }
    }
}

