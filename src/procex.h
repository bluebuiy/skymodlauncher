
#pragma once

#include <vector>
#include <string>
#include <optional>

struct ModMgr;

struct MountAction
{
    std::vector<std::string> lower;
    std::string mountPoint;
    std::optional<std::string> work;
    std::optional<std::string> upper;
};

std::optional<std::vector<MountAction>> GenerateMountActions(ModMgr & mgr);

bool ApplyMountActions(std::vector<MountAction> const & actions);

