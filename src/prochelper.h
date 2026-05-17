#pragma once

#include <string>
#include <vector>
#include <optional>

std::string mfix(std::string const & s);

std::string shellUnfix(std::string const & s);
std::string shellFix(std::string const & s);

void removecrlf(std::string & str);

std::vector<std::string> ShellSplit(std::string const & str);

std::optional<std::string> WordExpand(std::string const & in);
std::optional<std::string> WordExpand2(std::string const & in);

bool ExecArgs(std::vector<std::string> & args);


std::optional<std::string> LaunchProcForOutput(std::vector<std::string> & cmd, std::string const & wd);

bool LaunchProc(std::vector<std::string> & cmd, std::string const & wd);

struct ProcInvoke
{
    virtual ~ProcInvoke() = default;
    virtual void invoke() = 0;
};

bool ForkInvoke(ProcInvoke * invoke);


bool ExecThisProcessUnshared(std::vector<std::string> const & args);


