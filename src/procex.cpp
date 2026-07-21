
#include "modmgr.h"
#include "prochelper.h"
#include "procex.h"

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

std::optional<std::vector<std::string>> BuildMountDataStr(ModMgr& mgr)
{
    return {};
}

std::optional<std::vector<std::vector<std::string>>> BuildMountCommands(ModMgr& mgr)
{
    return {};
}

struct ToolRunner : ProcInvoke
{
    //std::vector<std::vector<std::string>> mountExec;
    std::vector<MountAction> mountActions;
    std::vector<std::string> toolExec;
    std::string wd;
    void invoke()
    {
        std::cout << "Apply actions" << std::endl;
        /*
        for (int i = 0; i < mountExec.size(); ++i)
        {
            if (!LaunchProc(mountExec[i], wd))
            {
                std::cout << "Failed to set up mounts" << std::endl;
                return;
            }
        }
        */
        if (!ApplyMountActions(mountActions))
        {
            std::cout << "Failed setting up filesystem" << std::endl;
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

ModExec const * FindExec(ModMgr& mgr, std::string const & name)
{
    auto toolIt = std::find_if(mgr.inst.customExec.begin(), mgr.inst.customExec.end(), [&](ModExec const & toolDef){
        if (toolDef.execName == name)
        {
            return true;
        }
        return false;
    });

    if (toolIt == mgr.inst.customExec.end())
    {
        toolIt = std::find_if(mgr.inst.builtinExec.begin(), mgr.inst.builtinExec.end(), [&](ModExec const & toolDef){
            if (toolDef.execName == name)
            {
                return true;
            }
            return false;
        });
        if (toolIt == mgr.inst.builtinExec.end())
        {
            std::cout << "Failed to find tool " << name << std::endl;
            return nullptr;
        }
    }

    return &*toolIt;
}

bool InvokeTool(ModMgr& mgr, std::string const & toolName)
{
    ModExec const * exec = FindExec(mgr, toolName);

    if (exec == nullptr)
    {
        return false;
    }

    std::cout << "Found tool" << std::endl;
    
    auto toolDef = *exec;

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
    auto mountActions = GenerateMountActions(mgr);
    if (!mountActions)
    {
        std::cout << "Failed to build mount actions" << std::endl;
        return false;
    }
    launcher.mountActions = *mountActions;
    std::string wd = toolDef.wd;
    if (wd.empty())
    {
        wd = "${GAME_ROOT_DIR}";
    }
    auto tryWd = ReplaceEnvVariables(mgr, wd, true);
    if (!tryWd)
    {
        std::cout << "Failed substitutions for working directory" << std::endl;
        return false;
    }
    tryWd = WordExpand(shellFix(*tryWd));
    if (!tryWd)
    {
        std::cout << "Failed substitutions for working directory 2" << std::endl;
        return false;
    }
    launcher.wd = *tryWd;

    auto expExecPath = ReplaceEnvVariables(mgr, toolDef.execPath, true);
    if (!expExecPath)
    {
        std::cout << "Failed variable subsitution:\n" << toolDef.execPath << std::endl;
        return false;
    }
    launcher.toolExec.emplace_back(*expExecPath);

    for (auto&& arg : toolDef.args)
    {
        std::cout << arg << std::endl;
        auto narg = ReplaceEnvVariables(mgr, arg, true);
        if (!narg)
        {
            std::cout << "Failed variable subsitution:\n" << arg << std::endl;
            return false;
        }
        std::cout << *narg << std::endl;
        auto exparg = WordExpand(shellFix(*narg));
        if (!exparg)
        {
            std::cout << "Failed to resolve string" << std::endl;
            return false;
        }
        std::cout << *exparg << std::endl;
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
    std::vector<std::vector<std::string>> mountCmds;
    std::vector<std::string> args;
    virtual void invoke() override
    {
        bool failed = false;
        for (int i = 0; i < mountCmds.size(); ++i)
        {
            if (!LaunchProc(mountCmds[i], ""))
            {
                failed = true;
                break;
            }
        }
        if (!failed)
        {
            LaunchProc(args, "");
        }
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

    auto mountCmds = BuildMountCommands(mgr);
    if (!mountCmds)
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
    runner.mountCmds = *mountCmds;
    runner.args = args;
    return ForkInvoke(&runner);
    //return ExecArgs(args);
}

std::optional<std::vector<MountAction>> GenerateMountActions(ModMgr& mgr)
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

    std::cout << installRoot << std::endl;

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
    
    std::filesystem::path modDir;
    if (auto o = WordExpand(shellFix(mgr.config.modFolder)))
    {
        modDir = *o;
    }
    else
    {
        return {};
    }

    std::vector<ModInstallId> installList;
    for (auto&& mod : mgr.inst.modInstalls)
    {
        installList.push_back(mod.first);
    }

    std::sort(installList.begin(), installList.end(), [&](auto& a, auto& b){ return mgr.inst.modInstalls[a].loadIndex < mgr.inst.modInstalls[b].loadIndex; });
    

    std::vector<MountAction> mountActions;

    int modIndex = 0;
    int leafIndex = 0;

    while (modIndex < installList.size())
    {
        auto& ma = mountActions.emplace_back();

        for (int i = 0; i < 128 && modIndex < installList.size(); ++modIndex)
        {
            int index = installList.size() - modIndex - 1;
            if (mgr.inst.modInstalls[installList[index]].enabled)
            {
                ma.lower.emplace_back(modDir / mgr.inst.modInstalls[installList[index]].installDir);
                ++i;
            }
        }
        
        ma.mountPoint = mgr.config.projectDir / ".fs" / std::format("m{}", leafIndex);

        ++leafIndex;
    }

    // one less for the install root
    if (leafIndex > 127)
    {
        // i think the real limit is 256 but 16k mods should be enough for anyone.
        // If i can find proper documentation on this, I will update it.
        std::cout << "Exceeded overlay lowerdir limit of 128 (127 mods)" << std::endl;
        return {};
    }

    auto& ma = mountActions.emplace_back();

    ma.lower.emplace_back(installRoot);

    for (int i = 0; i < leafIndex; ++i)
    {
        ma.lower.emplace_back(mgr.config.projectDir / ".fs" / std::format("m{}", i));
    }
    ma.work = mgr.config.projectDir / ".fs" / "wf";
    ma.mountPoint = installRoot;
    ma.upper = overwrite;

    return mountActions;
}

bool ApplyMountActions(std::vector<MountAction> const & actions)
{
    for (auto&& action : actions)
    {
        std::filesystem::path p(action.mountPoint);
        std::filesystem::create_directories(p);
        if (action.work)
        {
            p = *action.work;
            std::filesystem::create_directories(p);
        }
        if (action.upper)
        {
            p = *action.upper;
            std::filesystem::create_directories(p);
        }
    }


    int c = 0;

    for (auto&& action : actions)
    {
        std::cout << "mount to: " << action.mountPoint << std::endl;

        int mfd = fsopen("overlay", 0);
        if (mfd == -1)
        {
            std::cout << "f 0 " << errno << std::endl;
            close(mfd);
            return false;
        }

        for (int i = 0; i < action.lower.size(); ++i)
        {
            if (-1 == fsconfig(mfd, FSCONFIG_SET_STRING, "lowerdir+", action.lower[i].c_str(), 0))
            {
                std::cout << "f 1 " << i << " " << errno << std::endl;
                close(mfd);
                return false;
            }
        }

        if (action.work)
        {
            if (-1 == fsconfig(mfd, FSCONFIG_SET_STRING, "workdir", action.work->c_str(), 0))
            {
                std::cout << "f 3 " << errno << std::endl;
                close(mfd);
                return false;
            }
        }

        if (action.upper)
        {
            if (-1 == fsconfig(mfd, FSCONFIG_SET_STRING, "upperdir", action.upper->c_str(), 0))
            {
                std::cout << "f 4 " << errno << std::endl;
                close(mfd);
                return false;
            }
        }

        if (-1 == fsconfig(mfd, FSCONFIG_SET_STRING, "xino", "auto", 0))
        {
            std::cout << "f 5 " << errno << std::endl;
            close(mfd);
            return false;
        }

        if (-1 == fsconfig(mfd, FSCONFIG_CMD_CREATE, NULL, NULL, 0))
        {
            std::cout << "f 6 " << errno << std::endl;
            close(mfd);
            return false;
        }

        // possibly MOUNT_ATTR_RDONLY for all the lower ones
        int mt = fsmount(mfd, 0, MOUNT_ATTR_NODEV);
        if (mt == -1)
        {
            std::cout << "f 7 " << errno << std::endl;
            close(mfd);
            return false;
        }
        close(mfd);

        int mr = move_mount(mt, "", 0, action.mountPoint.c_str(), MOVE_MOUNT_F_EMPTY_PATH);
        if (mr == -1)
        {
            std::cout << "f 8 " << errno << std::endl;
            std::cout << "Failed to attatch mount" << std::endl;
            close(mt);
            return false;
        }
    }

    return true;
}
