#pragma once

#include <string>
#include <vector>
#include <optional>

std::optional<std::string> WordExpand(std::string const & in);


std::optional<std::string> LaunchProcForOutput(std::vector<std::string> & cmd, std::string const & wd);


struct ProcInvoke
{
    virtual ~ProcInvoke() = default;
    virtual void invoke() = 0;
};

bool ForkInvoke(ProcInvoke * invoke);



