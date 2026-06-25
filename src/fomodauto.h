
#pragma once

#include <vector>
#include <string>
#include <algorithm>

namespace FomodAuto
{
    struct Choice
    {
        std::string name;
        int index = -1;
    };

    struct Group
    {
        std::string name;
        std::vector<Choice> choices;
        Choice const * GetChoice(std::string const & name) const
        {
            auto it = std::find_if(choices.begin(), choices.end(), [&](Choice const & s) { return s.name == name; });
            if (it == choices.end())
            {
                return nullptr;
            }
            return &(*it);
        }
    };

    struct Step
    {
        std::string name;
        std::vector<Group> groups;
        Group const * GetGroup(std::string const & name) const
        {
            auto it = std::find_if(groups.begin(), groups.end(), [&](Group const & s) { return s.name == name; });
            if (it == groups.end())
            {
                return nullptr;
            }
            return &(*it);
        }
    };

    struct Config
    {
        std::vector<Step> steps;
        Step const * GetStep(std::string const & name) const
        {
            auto it = std::find_if(steps.begin(), steps.end(), [&](Step const & s) { return s.name == name; });
            if (it == steps.end())
            {
                return nullptr;
            }
            return &(*it);
        }
    };

}


struct ManualInstallFile
{
    std::string path;
    std::string md5;
};

// it's called Manual because someone had to manually pick out the files they wanted
struct ManualInstallConfig
{
    std::vector<ManualInstallFile> paths;
};

