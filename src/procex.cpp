
#include "modmgr.h"
#include "prochelper.h"

#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mount.h>

#include <iostream>
#include <format>


/*
    To resolve priority changes to individual files, we can use
    a bind mount on the individual file into a special direcotry
    for this purpose and add it as the last lower layer in the fs.
*/



#define PATH_PROC_SETGROUPS	"/proc/self/setgroups"
#define PATH_PROC_UIDMAP	"/proc/self/uid_map"
#define PATH_PROC_GIDMAP	"/proc/self/gid_map"

template <size_t count>
bool x_open_write_close_str(char const * file, char const (&buff)[count])
{
    int len = strlen(buff);
    if (len > count - 1)
    {
        std::cout << "f 1" << std::endl;
        return false;
    }
    int fd = open(file, O_LARGEFILE | O_WRONLY, 0666);
    if (fd == -1)
    {
        std::cout << "f 2" << std::endl;
        return false;
    }
    ssize_t n = 0;
    
    while (len)
    {
        std::cout << "try write " << (char const *)buff << std::endl;
        ssize_t c = write(fd, buff + n, len);
        if (c < 0 && errno != EINTR)
        {
            int i = errno;
            std::cout << "f 3 " << i << std::endl;
            break;
        }
        n += c;
        len -= c;
    }
    close(fd);
    return len == 0;
}


bool SetupPreMountEnv()
{
    if (-1 == unshare(CLONE_NEWNS | CLONE_NEWUSER))
    {
        int i = errno;
        printf("Failed unshare 0  %d\n", i);
        return false;
    }
    
    char uidmap_buf[sizeof("%u 0 1\n") + sizeof(int) * 3];
    uid_t reuid = geteuid();
    gid_t regid = getegid();

    if (!x_open_write_close_str(PATH_PROC_SETGROUPS, "deny"))
    {
        printf("Failed map root 1\n");
        return false;
    }
    sprintf(uidmap_buf, "%u 0 1", (unsigned)reuid);
    if (!x_open_write_close_str(PATH_PROC_UIDMAP, uidmap_buf))
    {
        printf("Failed map root 2\n");
        return false;
    }
    sprintf(uidmap_buf, "%u 0 1", (unsigned)regid);
    if (!x_open_write_close_str(PATH_PROC_GIDMAP, uidmap_buf))
    {
        printf("Failed map root 3\n");
        return false;
    }
	//	mount_or_die("none", "/", NULL, prop_flags);
    int mflags = MS_REC | MS_PRIVATE;
    if (!mount("none", "/", nullptr, mflags, nullptr))
    {
        printf("Failed map root 4\n");
        return false;
    }
    return true;
}

std::optional<std::vector<std::string>> BuildMountCommand(ModMgr& mgr)
{
    std::string installRoot;
    if (auto o = WordExpand(shellFix(mgr.config.installRoot)))
    {
        installRoot = *o;
    }
    else
    {
        std::cout << "Failed to resolve paths" << std::endl;
        return {};
    }

    std::string lowerLayers;
    std::sort(mgr.inst.mods.begin(), mgr.inst.mods.end(), [&](auto& a, auto& b){ return a.loadIndex < b.loadIndex; });
    auto p = std::filesystem::path(mgr.config.modFolder);
    lowerLayers += installRoot;
    if (!mgr.inst.mods.empty())
    {
        lowerLayers += ":";
    }
    for (int i = 0; i < mgr.inst.mods.size(); ++i)
    {
        int index = mgr.inst.mods.size() - i - 1;
        lowerLayers += (p / mgr.inst.mods[index].modFile);
        if (index != 0)
        {
            lowerLayers += ":";
        }
    }
    std::cout << "writing plugins" << std::endl;

    auto pluginPath = std::filesystem::path(mgr.config.appData) / "Plugins.txt";
    if (!WritePluginsTxt(mgr, pluginPath))
    {
        std::cout << "Failed to write plugins.txt" << std::endl;
        return {};
    }

    std::string overwrite;
    if (auto o = WordExpand(shellFix(mgr.config.projectDir / "overwrite")))
    {
        overwrite = *o;
    }
    else
    {
        std::cout << "Failed to resolve paths" << std::endl;
        return {};
    }
    std::string work;
    if (auto o = WordExpand(shellFix(mgr.config.projectDir / "work")))
    {
        work = *o;
    }
    else
    {
        std::cout << "Failed to resolve paths" << std::endl;
        return {};
    }
    std::string layers = std::format("lowerdir={},upperdir={},workdir={}", lowerLayers, overwrite, work);

    return std::vector<std::string>{
        "/usr/bin/mount",
        "-t", "overlay",
        "none",
        "-o", layers,
        installRoot
    };
}

struct ToolRunner : ProcInvoke
{
    std::vector<std::string> mountExec;
    std::vector<std::string> toolExec;
    std::string wd;
    void invoke()
    {
        if (!LaunchProc(mountExec, wd))
        {
            std::cout << "Failed to set up mounts" << std::endl;
            return;
        }
        if (0 != chdir(wd.c_str()))
        {
            int i = errno;
            std::cout << "Failed to set working directory (" << i << ") to " << wd << std::endl;
            return;
        }
        if (!ExecArgs(toolExec))
        {
            std::cout << "Failed to invoke tool" << std::endl;
        }
    }
};

bool InvokeTool(ModMgr& mgr, std::string const & toolName)
{
    auto toolIt = std::find_if(mgr.inst.customExec.begin(), mgr.inst.customExec.end(), [&](ModExec const & toolDef){
        if (toolDef.execName == toolName)
        {
            return true;
        }
        return false;
    });

    if (toolIt == mgr.inst.customExec.end())
    {
        toolIt = std::find_if(mgr.inst.builtinExec.begin(), mgr.inst.builtinExec.end(), [&](ModExec const & toolDef){
            if (toolDef.execName == toolName)
            {
                return true;
            }
            return false;
        });
        if (toolIt == mgr.inst.builtinExec.end())
        {
            std::cout << "Failed to find tool " << toolName << std::endl;
            return false;
        }
    }

    std::cout << "Found tool" << std::endl;
    
    auto toolDef = *toolIt;

    std::string installRoot;
    if (auto o = WordExpand(shellFix(mgr.config.installRoot)))
    {
        installRoot = *o;
    }
    else
    {
        std::cout << "Failed to resolve paths" << std::endl;
        return false;
    }

    std::cout << "writing plugins" << std::endl;

    if (toolDef.updatePluginList)
    {
        auto pluginPath = std::filesystem::path(mgr.config.appData) / "Plugins.txt";
        if (!WritePluginsTxt(mgr, pluginPath))
        {
            std::cout << "Failed to write plugins.txt" << std::endl;
            return false;
        }
    }

    std::cout << "Setting up launcher" << std::endl;
    ToolRunner launcher;
    auto mountCmd = BuildMountCommand(mgr);
    if (!mountCmd)
    {
        std::cout << "Failed to build mount command" << std::endl;
        return false;
    }
    launcher.mountExec = *mountCmd;
    launcher.wd = installRoot;

    auto expExecPath = ReplaceEnvVariables(mgr, toolDef.execPath, true);
    if (!expExecPath)
    {
        std::cout << "Failed variable subsitution:\n" << toolDef.execPath << std::endl;
        return false;
    }
    launcher.toolExec.emplace_back(*expExecPath);

    for (auto&& arg : toolDef.args)
    {
        auto narg = ReplaceEnvVariables(mgr, arg, true);
        if (!narg)
        {
            std::cout << "Failed variable subsitution:\n" << arg << std::endl;
            return false;
        }
        auto exparg = WordExpand(shellFix(*narg));
        if (!exparg)
        {
            std::cout << "Failed to resolve string" << std::endl;
            return false;
        }
        launcher.toolExec.emplace_back(std::move(*exparg));
    }

    //launcher.toolExec.insert(launcher.toolExec.end(), toolDef.args.begin(), toolDef.args.end());
    
    // should already be in a new process, no extra fork or exec needed.
    return ForkInvoke(&launcher);
    //launcher.invoke();
}

struct ProcRunner : public ProcInvoke
{
    std::string wd;
    std::vector<std::string> mountCmd;
    std::vector<std::string> args;
    virtual void invoke() override
    {
        std::cout << "Fdjklfs" << std::endl;
        LaunchProc(mountCmd, "");
        LaunchProc(args, "");
    }
};


bool InvokeProcess(ModMgr& mgr, std::vector<std::string> & args)
{
    /*
    // not sure how to configure this
    auto pluginPath = std::filesystem::path(mgr.config.appData) / "Plugins.txt";
    if (!WritePluginsTxt(mgr, pluginPath))
    {
        std::cout << "Failed to write plugins.txt" << std::endl;
        return false;
    }
    */

    auto mountCmd = BuildMountCommand(mgr);
    if (!mountCmd)
    {
        std::cout << "Failed to build mount command" << std::endl;
        return false;
    }

    //if (!LaunchProc(*mountCmd, ""))
    //{
    //    std::cout << "Failed to set up mounts" << std::endl;
    //    return false;
    //}

    ProcRunner runner;
    runner.mountCmd = *mountCmd;
    runner.args = args;
    return ForkInvoke(&runner);
    //return ExecArgs(args);
}

