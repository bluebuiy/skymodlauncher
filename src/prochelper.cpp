
#include "prochelper.h"

#include <unistd.h>
#include <wordexp.h>
#include <iostream>
#include <sys/wait.h>

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

bool LaunchProc(std::vector<std::string> & cmd, std::string const & wd)
{
    if (cmd.empty() || cmd[0].empty())
    {
        std::cout << "Tried to execute \"\"" << std::endl;
        return {};
    }

    std::cout << "Executing proc: " << cmd[0] << std::endl;

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
        int cd = chdir(wd.c_str());
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
        if (pid != waitpid(pid, nullptr, 0))
        {
            std::cout << "Error waiting for process completion" << std::endl;
        }
    }
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
    int ppp[2];
    if (pipe(ppp) < 0)
    {
        return {};
    }

    int pid = fork();

    if (pid == -1)
    {
        return false;
    }
    else if (pid == 0)
    {
        // child
        dup2(ppp[1], STDOUT_FILENO);
        invoke->invoke();
        exit(0);
    }
    else
    {
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
                std::cout << output;
                output.clear();
            }
        }
    }
    return true;
}



