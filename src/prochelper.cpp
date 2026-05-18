
#include "prochelper.h"

#include <unistd.h>
#include <wordexp.h>
#include <iostream>
#include <sys/wait.h>

#include <format>
#include <algorithm>

// probably dont need this?
std::string shellUnfix(std::string const & s)
{
    std::string ret;
    bool back = false;
    for (int i = 0; i < s.size(); ++i)
    {
        if (back == false)
        {
            if (s[i] == '\\')
            {
                back = true;
            }
            else
            {
                ret.push_back(s[i]);
            }
        }
        else if (back == true)
        {
            back = false;
            ret.push_back(s[i]);
        }
    }
    return ret;
}

std::string shellFix(std::string const & s)
{
    std::string ret;
    for (int i = 0; i < s.size(); ++i)
    {
        if (s[i] == ' ')
        {
            ret.push_back('\\');
        }
        ret.push_back(s[i]);
    }
    return ret;
}

std::string mfix(std::string const & s)
{
    auto b = WordExpand(shellFix(s));
    if (b)
    {
        return shellFix(*b);
    }
    return shellFix(s);
}

void removecrlf(std::string & str)
{
    auto end = std::remove_if(str.begin(), str.end(), [](char c){
        return c == '\r' || c == '\n';
    });
    str.erase(end, str.end());
}

std::vector<std::string> ShellSplit(std::string const & str)
{
    wordexp_t wexp;
    int err = wordexp(str.c_str(), &wexp, WRDE_NOCMD);
    if (err != 0)
    {
        return {};
    }

    std::vector<std::string> ret;
    for (int i = 0; i < wexp.we_wordc; ++i)
    {
        ret.emplace_back(wexp.we_wordv[i]);
    }

    wordfree(&wexp);
    return ret;
}

std::optional<std::string> WordExpand(std::string const & in)
{
    wordexp_t wexp;
    int err = wordexp(in.c_str(), &wexp, WRDE_NOCMD);
    if (err != 0)
    {
        return {};
    }

    std::string result;
    for (int i = 0; i < wexp.we_wordc; ++i)
    {
        result.append(wexp.we_wordv[i]);
    }
    wordfree(&wexp);

    return result;
}

std::optional<std::string> WordExpand2(std::string const & in)
{
    auto str = mfix(in);
    auto o = WordExpand(in);
    if (!o)
    {
        return {};
    }
    return shellUnfix(*o);
}

bool ExecArgs(std::vector<std::string> & args)
{
    std::cout << "Execv " << args[0] << std::endl;
    std::vector<char *> argsv;
    for (int i = 0; i < args.size(); ++i)
    {
        std::cout << "    " << args[i] << std::endl;
        argsv.push_back(args[i].data());
    }
    argsv.push_back(nullptr);
    if (!execv(args[0].c_str(), argsv.data()))
    {
        return false;
    }
    // cant return true
    return true;
}

bool LaunchProc(std::vector<std::string> & cmd, std::string const & wd)
{
    if (cmd.empty() || cmd[0].empty())
    {
        std::cout << "Tried to execute \"\"" << std::endl;
        return false;
    }

    std::cout << "Executing proc: " << cmd[0] << std::endl;

    std::vector<char *> args;
    for (int i = 0; i < cmd.size(); ++i)
    { 
        std::cout << "    " << cmd[i] << std::endl;
        args.push_back(cmd[i].data());
    }
    args.push_back(nullptr);

    int pid = fork();

    if (pid == -1)
    {
        return false;
    }
    else if (pid == 0)
    {
        // child
        int cd = wd.size() ? chdir(wd.c_str()) : 0;
        if (cd == 0)
        {
            std::cout << "execv " << cmd[0] << std::endl;
            if (!execv(cmd[0].data(), args.data()))
            {
                std::cout << "Failed to launch process" << std::endl;
            }
        }
        else
        {
            std::cout << "Failed to change working directory" << std::endl;
        }
        exit(1);
    }
    else
    {
        int ret = 0;
        if (pid != waitpid(pid, &ret, 0))
        {
            std::cout << "Error waiting for process completion" << std::endl;
            return false;
        }
        else if (ret != 0)
        {
            std::cout << "Process failed with " << ret << std::endl;
            return false;
        }
    }
    return true;
}

std::optional<std::string> LaunchProcForOutput(std::vector<std::string> & cmd, std::string const & wd)
{
    if (cmd.empty() || cmd[0].empty())
    {
        std::cout << "Tried to execute \"\"" << std::endl;
        return {};
    }

    std::cout << "Executing proc: " << cmd[0] << std::endl;
    int ppp[2];
    if (pipe(ppp) < 0)
    {
        std::cout << "Failed to create pipe for child output" << std::endl;
        return {};
    }

    std::vector<char *> args;
    for (int i = 0; i < cmd.size(); ++i)
    {
        args.push_back(cmd[i].data());
    }
    args.push_back(nullptr);

    int pid = fork();

    if (pid == -1)
    {
        return {};
    }
    else if (pid == 0)
    {
        // child
        close(ppp[0]);
        dup2(ppp[1], STDOUT_FILENO);
        int cd = chdir(wd.c_str());
        if (cd == 0)
        {
            if (!execv(cmd[0].data(), args.data()))
            {
                exit(1);
            }
        }
        else
        {
            exit(1);
        }
        exit(1);
    }
    else
    {
        close(ppp[1]);
        // parent
        std::string output;

        char buf[1024];
        while (true)
        {
            ssize_t s = read(ppp[0], buf, sizeof(buf));
            if (s == 0)
            {
                break;
            }
            else if (s == -1)
            {
                std::cout << "Error reading pipe" << std::endl;
            }
            else
            {
                output.insert(output.end(), buf, buf + s);
            }
        }
        std::cout << output << std::endl;
        return output;
    }
    return {};
}


bool ForkInvoke(ProcInvoke * invoke)
{
    std::cout << "Forking" << std::endl;
    int pid = fork();

    if (pid == -1)
    {
        std::cout << "Failed to fork" << std::endl;
        return false;
    }
    else if (pid == 0)
    {
        // child
        std::cout << "Running invoker" << std::endl;
        invoke->invoke();
        exit(0);
    }
    else
    {
        std::cout << "Waiting for child.." << std::endl;
        int ret = 0;
        if (pid != waitpid(pid, &ret, 0))
        {
            std::cout << "Error waiting for process completion" << std::endl;
            return false;
        }
        else if (ret != 0)
        {
            std::cout << "Process failed with " << ret << std::endl;
        }
    }
    return true;
}

bool ExecThisProcessUnshared(std::vector<std::string> const & args)
{
    int pid = getpid();
    std::vector<std::string> getThisProcCmd = {"/usr/bin/readlink", "-f", std::format("/proc/{}/exe", pid)};
    auto thisProcPath = LaunchProcForOutput(getThisProcCmd, "/");
    if (!thisProcPath)
    {
        std::cout << "Failed to get proc path" << std::endl;
        return false;
    }
    removecrlf(*thisProcPath);
    std::cout << "Unsharing" << std::endl;
    std::vector<std::string> launchThisProcess = {"/usr/bin/unshare", "--user", "--map-root-user", "--mount", *thisProcPath, "-r"};//, "-c", confPath, "-e", "skse"};
    launchThisProcess.insert(launchThisProcess.end(), args.begin(), args.end());
    
    return ExecArgs(launchThisProcess);
}

// 
bool RunPipedShellScript(std::string const & script)
{
    int pppIn[2];
    if (pipe(pppIn) < 0)
    {
        return false;
    }

    int pid = fork();

    if (pid == -1)
    {
        close(pppIn[0]);
        close(pppIn[1]);
        return false;
    }
    else if (pid == 0)
    {
        // child
        dup2(pppIn[0], STDIN_FILENO);
        close(pppIn[1]);
        std::vector<std::string> arg = {"/usr/bin/sh"};
        ExecArgs(arg);
        exit(1);
    }
    else
    {
        //parent
        close(pppIn[0]);

        ssize_t n = 0;
        ssize_t l = script.size();
        while (l)
        {
            ssize_t s = write(pppIn[1], script.c_str() + n, l);
            int er = errno;
            if (s < 0 && er != EINTR)
            {
                break;
            }
            n += s;
            l -= s;
        }
        close(pppIn[1]);
        return n == 0;
    }

    return false;
}



