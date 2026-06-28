
#include "modmgr.h"

#include "imgui.h"
#include "prochelper.h"
#include "modmgr_collection_ui.h"

#include <fstream>
#include <iostream>
#include <format>
#include <filesystem>
#include <unordered_set>

constexpr auto WINDOW_ALIGN_FLAG = ImGuiCond_Always;
constexpr ImVec4 DELETE_COLOR = ImVec4(0.7f,0.1f,0.1f,1.0f);

void RenderTestUi(ModMgr& mgr)
{
#if 1
    if (mgr.openTestUi)
    {
        mgr.openTestUi = false;
        ImGui::OpenPopup("testui");
    }
    static char strBuf[128] = {0};
    static char strBufOut[256] = {0};
    static bool failRep = false;
    if (ImGui::BeginPopupModal("testui"))
    {
        ImGui::InputText("Var rep test", strBuf, sizeof(strBuf));
        ImGui::Checkbox("Fail rep", &failRep);
        if (ImGui::Button("Replace"))
        {
            std::string str(strBuf);
            auto rep = ReplaceEnvVariables(mgr, str, failRep);
            if (rep)
            {
                strncpy(strBufOut, rep->c_str(), 128);
                strBufOut[127] = 0;
            }
        }
        ImGui::TextWrapped("%s", strBufOut);
        ImGui::EndPopup();
    }
#endif
}

void LaunchShell(ModMgr& mgr)
{
    std::string confPath;
    if (auto o = WordExpand(shellFix(mgr.config.configPath)))
    {
        confPath = *o;
    }
    else
    {
        std::cout << "Failed to resolve config path" << std::endl;
        return;
    }
    std::vector<std::string> launchArgs = {"-c", confPath, "-x", "/usr/bin/ls -al #{GAME_ROOT_DIR}"};
    ExecToolProgram ex;
    ex.args = launchArgs;
    ForkInvoke(&ex);
}

void RenderNewModDialog(ModMgr& mgr)
{
    const auto dispSize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), WINDOW_ALIGN_FLAG, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300, 200), WINDOW_ALIGN_FLAG);
    if (ImGui::Begin("Add New Mod", &mgr.makingNewMod))
    {
        int currentItem = static_cast<int>(mgr.newMod.sourceType);
        if (ImGui::Combo("Type", &currentItem, "Empty\0Nexus\0NxmColBundle\0Direct\0Local\0"))
        {
            mgr.newMod.sourceType = static_cast<FileSource>(currentItem);
        }
        ImGui::InputText("Name", &mgr.newMod.name);
        ImGui::InputText("Logical Name", &mgr.newMod.logicalName);

        int curInstType = static_cast<int>(mgr.newMod.installType);
        if (ImGui::Combo("Install Type", &curInstType, "Data\0Root\0Undetermined\0"))
        {
            mgr.newMod.installType = static_cast<ModInstallType>(curInstType);
        }

        if (mgr.newMod.sourceType == FileSource::Nexus)
        {
            ImGui::InputInt("modId", &mgr.newMod.nxmModId, -1);
            ImGui::InputInt("fileId", &mgr.newMod.nxmFileId, -1);
            ImGui::InputText("version", &mgr.newMod.version);
        }
        else if (mgr.newMod.sourceType == FileSource::CollectionBundle)
        {
            ImGui::InputText("Slug", &mgr.newMod.nxmColSlug);
            ImGui::InputInt("rev", &mgr.newMod.nxmColRev, -1);
        }
        else if (mgr.newMod.sourceType == FileSource::Independent)
        {
            ImGui::TextColored(ImVec4(1,0,0,1), "NOT IMPLEMENTED!");
            ImGui::InputText("download url", &mgr.newMod.url);
        }
        else if (mgr.newMod.sourceType == FileSource::Manual)
        {
            ImGui::TextColored(ImVec4(1,0,0,1), "NOT IMPLEMENTED!");
            ImGui::InputText("Path", &mgr.newMod.path);
        }

        ImGui::NewLine();
        ImGui::NewLine();
        if (ImGui::Button("Submit"))
        {
            auto mmid = FindModManifest(mgr, mgr.newMod);
            if (mmid.id != 0)
            {
                std::cout << "An identical mod manifest already exists!" << std::endl;
            }
            else
            {
                CreateModManifest(mgr, mgr.newMod);
            }
        }
    }
    ImGui::End();
}

void RenderModMgrSettings(ModMgr& mgr)
{
    const auto dispSize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(dispSize.x * 0.8f, dispSize.y * 0.6f), ImGuiCond_Always);
    if (ImGui::Begin("Settings", &mgr.settingsOpen, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
    {
        ImGui::InputText("Game Root", &mgr.config.installRoot);
        ImGui::InputText("My Games/Skyrim", &mgr.config.mgRoot);
        ImGui::InputText("Mod folder", &mgr.config.modFolder);
        ImGui::InputText("AppData/Skyrim", &mgr.config.appData);
        ImGui::Separator();
        ImGui::InputText("Config path", &mgr.config.configPath);
        ImGui::InputText("Instance path", &mgr.config.instPath);
        ImGui::Separator();
        ImGui::InputText("Nexus API key", &mgr.config.nexusApiKey, ImGuiInputTextFlags_Password);
        ImGui::Separator();
        
        if (ImGui::CollapsingHeader("Custom Variables"))
        {
            ImGui::Text("Name ");
            ImGui::SameLine();
            ImGui::InputText("##name", &mgr.newVariable.name);
            ImGui::Text("Value");
            ImGui::SameLine();
            ImGui::InputText("##value", &mgr.newVariable.value);
            if (ImGui::Button("Add"))
            {
                auto ex = std::find_if(mgr.inst.customVariables.begin(), mgr.inst.customVariables.end(), [&](CustomVariable const & cv){
                    return cv.name == mgr.newVariable.name;
                });
                if (!mgr.newVariable.name.empty() && ex == mgr.inst.customVariables.end())
                {
                    mgr.inst.customVariables.emplace_back(mgr.newVariable);
                }
                mgr.newVariable.name.clear();
                mgr.newVariable.value.clear();
            }
            ImGui::Separator();
            int del = -1;
            ImGui::PushID("variable");
            for (int i = 0; i < mgr.inst.customVariables.size(); ++i)
            {
                ImGui::PushID(mgr.inst.customVariables[i].name.c_str());
                
                ImGui::PushStyleColor(ImGuiCol_Button, DELETE_COLOR);
                if (ImGui::Button("X"))
                {
                    del = i;
                }
                ImGui::PopStyleColor(1);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200);
                ImGui::InputText("##name", &mgr.inst.customVariables[i].name);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(800);
                ImGui::InputText("##val", &mgr.inst.customVariables[i].value);
                ImGui::PopID();
            }
            ImGui::PopID();

            if (del != -1)
            {
                mgr.inst.customVariables.erase(mgr.inst.customVariables.begin() + del);
            }

        }
    }
    ImGui::End();
}

void RenderPluginsList(ModMgr& mgr)
{
    const auto dispSize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(dispSize.x * 0.4f, 240 + 20), WINDOW_ALIGN_FLAG);
    ImGui::SetNextWindowSize(ImVec2(dispSize.x * 0.2f, dispSize.y - 240 - 20), WINDOW_ALIGN_FLAG);
    if (ImGui::Begin("Plugins", nullptr))
    {
        if (ImGui::Button("Refresh Plugins"))
        {
            DiscoverPlugins(mgr);
        }

        int move = -1;
        int dir = 0;

        for (int i = 0; i < mgr.inst.pluginList.size(); ++i)
        {
            ImGui::PushID(mgr.inst.pluginList[i].pluginName.c_str());
            {
                ImGui::Text("%3d", i);
                ImGui::SameLine();
                ImGui::Checkbox("", &mgr.inst.pluginList[i].enabled);
                ImGui::SameLine();
                ImGui::BeginDisabled(i == 0);
                if (ImGui::Button("^"))
                {
                    move = i;
                    dir = -1;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(i == mgr.inst.pluginList.size() -1);
                if (ImGui::Button("V"))
                {
                    move = i;
                    dir = 1;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::Text("%s", mgr.inst.pluginList[i].pluginName.c_str());
            }
            ImGui::PopID();
        }

        if (move != -1)
        {
            if (dir == 1 && move != mgr.inst.pluginList.size() - 1)
            {
                std::swap(mgr.inst.pluginList[move], mgr.inst.pluginList[move + 1]);
            }
            else if (dir == -1 && move != 0)
            {
                std::swap(mgr.inst.pluginList[move], mgr.inst.pluginList[move - 1]);
            }
            SaveModMgr(mgr);
        }
    }
    ImGui::End();
}

void RenderTools(ModMgr& mgr)
{
    mgr.selectingExec ^= ImGui::Button("Select Tool");
    ImGui::BeginDisabled(mgr.addingExec | mgr.modifyingExec);
    if (ImGui::Button("New Tool"))
    {
        mgr.addingExec = true;
    }
    if (ImGui::Button("Modify Tool"))
    {
        bool found = false;
        for (int i = 0; i < mgr.inst.customExec.size(); ++i)
        {
            if (mgr.inst.customExec[i].execName == mgr.currentExec)
            {
                found = true;
                mgr.newExec = mgr.inst.customExec[i];
                break;
            }
        }
        mgr.modifyingExec = found;
    }
    ImGui::EndDisabled();
    if (mgr.addingExec || mgr.modifyingExec)
    {
        const auto dispSize = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
        if (ImGui::Begin("Edit Exec"))
        {
            ImGui::Text("Name ");
            ImGui::SameLine();
            ImGui::InputText("##Name", &mgr.newExec.execName);
            ImGui::Text("Path ");
            ImGui::SameLine();
            ImGui::InputText("##Path", &mgr.newExec.execPath);
            ImGui::Text("Working Directory ");
            if (ImGui::BeginItemTooltip())
            {
                ImGui::Text("If empty, defaults to ${GAME_ROOT_DIR}");
                ImGui::EndTooltip();
            }
            ImGui::SameLine();
            ImGui::InputText("##Wd", &mgr.newExec.wd);
            ImGui::Text("Arguments:");
            int del = -1;
            for (int i = 0; i < mgr.newExec.args.size(); ++i)
            {
                ImGui::PushID(i);
                ImGui::PushStyleColor(ImGuiCol_Button, DELETE_COLOR);
                if (ImGui::Button("X"))
                {
                    del = i;
                }
                ImGui::PopStyleColor(1);
                ImGui::SameLine();
                ImGui::InputText("", &mgr.newExec.args[i]);
                ImGui::PopID();
            }
            if (del != -1)
            {
                mgr.newExec.args.erase(mgr.newExec.args.begin() + del);
            }
            if (ImGui::Button("Add Arg"))
            {
                mgr.newExec.args.emplace_back("");
            }
            if (ImGui::Button("Submit"))
            {
                bool dup = false;
                int index = -1;
                for (int i = 0; i < mgr.inst.customExec.size(); ++i)
                {
                    if (mgr.inst.customExec[i].execName == mgr.newExec.execName)
                    {
                        dup = true;
                        index = i;
                        break;
                    }
                }

                for (int i = 0; i < mgr.inst.builtinExec.size(); ++i)
                {
                    if (mgr.inst.builtinExec[i].execName == mgr.newExec.execName)
                    {
                        dup = true;
                    }
                }
                if (mgr.modifyingExec)
                {
                    mgr.inst.customExec[index] = mgr.newExec;
                    mgr.newExec = ModExec{};
                    mgr.addingExec = false;
                    mgr.modifyingExec = false;
                }
                else if (!dup && mgr.addingExec)
                {
                    mgr.inst.customExec.push_back(mgr.newExec);
                    mgr.newExec = ModExec{};
                    mgr.addingExec = false;
                    mgr.modifyingExec = false;
                }
            }
            ImGui::SameLine(0, 15);
            if (ImGui::Button("Cancel"))
            {
                mgr.addingExec = false;
                mgr.modifyingExec = false;
                mgr.newExec = ModExec{};
            }
        }
        ImGui::End();
    }
    else if (mgr.selectingExec)
    {
        ImGui::BeginChild("execList", ImVec2(0,100), ImGuiChildFlags_FrameStyle);

        std::vector<std::string> execNames;
        for (int i = 0; i < mgr.inst.builtinExec.size(); ++i)
        {
            if (ImGui::RadioButton(mgr.inst.builtinExec[i].execName.c_str(), mgr.inst.builtinExec[i].execName == mgr.currentExec))
            {
                mgr.currentExec = mgr.inst.builtinExec[i].execName;
                mgr.selectingExec = false;
            }
        }
        for (int i = 0; i < mgr.inst.customExec.size(); ++i)
        {
            if (ImGui::RadioButton(mgr.inst.customExec[i].execName.c_str(), mgr.inst.customExec[i].execName == mgr.currentExec))
            {
                mgr.currentExec = mgr.inst.customExec[i].execName;
                mgr.selectingExec = false;
            }
        }
        ImGui::EndChild();
    }
    else
    {
        ImGui::Text("Run tool: %s", mgr.currentExec.c_str());
        ImGui::SameLine();
        ImGui::BeginDisabled(mgr.currentExec.empty());
        if (ImGui::Button("Launch"))
        {
            LaunchExec(mgr, mgr.currentExec);
        }
        ImGui::EndDisabled();
    }
}

void RenderModDownloads(ModMgr& mgr)
{
    const auto dispSize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(dispSize.x * 0.6f, 20), WINDOW_ALIGN_FLAG);
    ImGui::SetNextWindowSize(ImVec2(dispSize.x * 0.4f, dispSize.y - 20), WINDOW_ALIGN_FLAG);
    if (ImGui::Begin("Downloads"))
    {
        ImGui::InputText("Search", &mgr.dlSearch);
        for (int i = 0; i < mgr.downloadSessions.size(); ++i)
        {
            auto mf = GetModManifest(mgr, mgr.downloadSessions[i].id);
            if (mgr.dlSearch.empty() || mf && (mf->logicalName.find(mgr.dlSearch, 0) != std::string::npos || mf->name.find(mgr.dlSearch, 0) != std::string::npos))
            {
                ImGui::PushID(mgr.downloadSessions[i].id.id);

                ImGui::PushStyleColor(ImGuiCol_Button, DELETE_COLOR);
                if (ImGui::Button("X"))
                {
                    mgr.downloadSessions[i].remove = true;
                }
                ImGui::SameLine();
                if (mgr.downloadSessions[i].state == ModDlState::ModDownload)
                {
                    if (ImGui::Button("||"))
                    {
                        mgr.downloadSessions[i].pause = true;
                    }
                }
                else if (mgr.downloadSessions[i].state == ModDlState::ModPaused)
                {
                    if (ImGui::Button(">"))
                    {
                        mgr.downloadSessions[i].unpause = true;
                    }
                }
                ImGui::PopStyleColor(1);
                ImGui::SameLine();
                char const * state = "Unknown";
                switch(mgr.downloadSessions[i].state)
                {
                    case ModDlState::UrlQuery:
                    {
                        state = "Fetching Info";
                        break;
                    }
                    case ModDlState::Error:
                    {
                        state = "Error";
                        break;
                    }
                    case ModDlState::ModDownload:
                    {
                        state = "Downloading";
                        break;
                    }
                    case ModDlState::ModPaused:
                    {
                        state = "Paused";
                        break;
                    }
                    case ModDlState::Canceled:
                    {
                        state = "Canceled";
                        break;
                    }
                    case ModDlState::Complete:
                    {
                        state = "Complete";
                        break;
                    }
                }
                ImGui::Text("%-14s ", state);
                bool doQuickInstall = false;
                if (mgr.downloadSessions[i].state == ModDlState::Complete)
                {
                    ImGui::SameLine();
                    doQuickInstall = ImGui::Button("Install");
                    if (ImGui::BeginItemTooltip())
                    {
                        ImGui::Text("Extracts mod contents directly to the mod directory");
                        ImGui::EndTooltip();
                    }
                }
                ImGui::SameLine();
                auto mf = GetModManifest(mgr, mgr.downloadSessions[i].id);
                ImGui::Text("%-64s", mf ? mf->logicalName.c_str() : mgr.downloadSessions[i].fileName.c_str());
                ImGui::SameLine();
                ImGui::Text(" ? ");
                if (ImGui::BeginItemTooltip())
                {
                    ImGui::Text("Unzips files into /Data, so that they are effectively at ${GAME_ROOT_DIR}/Data");
                    ImGui::EndTooltip();
                }
                if (doQuickInstall)
                {
                    InstallMod(mgr, mgr.downloadSessions[i].id, {});
                }

                ImGui::PopID();
            }
        }
    }
    ImGui::End();
}

void RenderModMgr(ModMgr& mgr)
{
    RenderTestUi(mgr);

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::Button("Save"))
        {
            SaveModMgr(mgr);
        }

        if (ImGui::Button("Settings"))
        {
            mgr.settingsOpen = true;
        }

        if (ImGui::Button("Collection"))
        {
            mgr.openCollectionInput = true;
        }
/*
        if (ImGui::Button("Reset layout"))
        {
            // doesnt work
            ImGui::ClearIniSettings();
        }
        if (ImGui::BeginItemTooltip())
        {
            ImGui::Text("Requires restart");
            ImGui::EndTooltip();
        }
*/

        if (ImGui::Button("Tests"))
        {
            mgr.openTestUi = true;
        }

    }
    ImGui::EndMainMenuBar();
    
    const auto dispSize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(dispSize.x * 0.4f, 20), WINDOW_ALIGN_FLAG);
    ImGui::SetNextWindowSize(ImVec2(dispSize.x * 0.2f, 240), WINDOW_ALIGN_FLAG);
    if (ImGui::Begin("Mod Manager"))
    {
        RenderTools(mgr);

        if (ImGui::Button("Add Mod"))
        {
            mgr.makingNewMod = true;
        }

        ImGui::Text("Sort order");
        if (ImGui::RadioButton("Load order", mgr.sortMode == 0))
        {
            mgr.sortMode = 0;
        }
        if (ImGui::RadioButton("Name", mgr.sortMode == 1))
        {
            mgr.sortMode = 1;
        }
        if (ImGui::RadioButton("Errors", mgr.sortMode == 2))
        {
            mgr.sortMode = 2;
        }
        ImGui::Checkbox("Enable mod removal", &mgr.enableRemove);
        ImGui::Checkbox("Enable error mode", &mgr.enableSetOk);
    }

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(0,20), WINDOW_ALIGN_FLAG);
    ImGui::SetNextWindowSize(ImVec2(dispSize.x * 0.4f, dispSize.y - 20), WINDOW_ALIGN_FLAG);
    if (ImGui::Begin("Mod List"))
    {
        ImGui::InputText("Search", &mgr.modSearch);
        std::vector<ModId> modList;

        for (auto&& install : mgr.inst.modInstalls)
        {
            modList.push_back(install.second.modInstance);
        }

        std::sort(modList.begin(), modList.end(), [&](ModId const & a, ModId const & b){
            auto ia = mgr.inst.modFileManifests.find(a);
            auto ib = mgr.inst.modFileManifests.find(b);
            if (ia == mgr.inst.modFileManifests.end() && ib == mgr.inst.modFileManifests.end())
            {
                return true;
            }
            else if (ia == mgr.inst.modFileManifests.end())
            {
                return false;
            }
            else if (ib == mgr.inst.modFileManifests.end())
            {
                return true;
            }
            if (mgr.sortMode == 0)
            {
                auto iai = mgr.inst.modInstalls.find(ia->second.installInstances.empty() ? ModInstallId{0} : ia->second.installInstances[0]);
                auto ibi = mgr.inst.modInstalls.find(ib->second.installInstances.empty() ? ModInstallId{0} : ib->second.installInstances[0]);
                if (iai == mgr.inst.modInstalls.end() && ibi == mgr.inst.modInstalls.end())
                {
                    return true;
                }
                else if (iai == mgr.inst.modInstalls.end())
                {
                    return false;
                }
                else if (ibi == mgr.inst.modInstalls.end())
                {
                    return true;
                }
                return iai->second.loadIndex < ibi->second.loadIndex;
            }
            else if (mgr.sortMode == 1)
            {
                return ia->second.name < ib->second.name;
            }
            else if (mgr.sortMode == 2)
            {
                auto iai = mgr.inst.modInstalls.find(ia->second.installInstances.empty() ? ModInstallId{0} : ia->second.installInstances[0]);
                auto ibi = mgr.inst.modInstalls.find(ib->second.installInstances.empty() ? ModInstallId{0} : ib->second.installInstances[0]);
                bool af = iai == mgr.inst.modInstalls.end() || iai->second.ok == true;
                bool bf = ibi == mgr.inst.modInstalls.end() || ibi->second.ok == true;
                return (int)af < (int)bf;
                #if 0
                if (iai == mgr.inst.modInstalls.end() && ibi == mgr.inst.modInstalls.end())
                {
                    return true;
                }
                else if (ibi == mgr.inst.modInstalls.end())
                {
                    return false;
                }
                else if (iai->second.ok && ibi->second.ok)
                {
                    return true;
                }
                else if (iai->second.ok == false && ibi->second.ok == true)
                {
                    return false;
                }
                else if (ibi->second.ok == false && iai->second.ok == true)
                {
                    return true;
                }
                else
                {
                    return true;
                }
                #endif
            }
            else
            {
                // ??
                return true;
            }
        });
        int del = -1;
        int mvUp = -1;
        int mvDown = -1;
        for (int i = 0; i < modList.size(); ++i)
        {
            auto mf = GetModManifest(mgr, modList[i]);
            if (
                mf && (
                    mgr.modSearch.empty() || (
                        mf->name.find(mgr.modSearch, 0) != std::string::npos || 
                        mf->logicalName.find(mgr.modSearch, 0) != std::string::npos
                    )
                )
            )
            {
                auto inst = GetModInstall(mgr, mf->installInstances.empty() ? ModInstallId{0} : mf->installInstances[0]);
                ImGui::PushID(mf->logicalName.c_str());

                    if (inst)
                        ImGui::Text("%4d", inst->loadIndex);
                    else
                        ImGui::Text("    ");

                    ImGui::SameLine();
                    bool enb = false;
                    ImGui::Checkbox("##enb", inst ? &inst->enabled : &enb);

                    // need to make actually good load index ui
                    /*
                    ImGui::SameLine();
                    ImGui::BeginDisabled(mgr.inst.mods[i].loadIndex == 0);
                    if (ImGui::Button("^"))
                    {
                        mvUp = i;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::BeginDisabled(mgr.inst.mods[i].loadIndex == mgr.inst.mods.size() - 1);
                    if (ImGui::Button("V"))
                    {
                        mvDown = i;
                    }
                    ImGui::EndDisabled();
                    */
                    
                    ImGui::SameLine();
                    ImGui::BeginDisabled(!inst);
                    if (ImGui::Button("@"))
                    {
                        std::vector<std::string> args = {
                            "/usr/bin/xdg-open",
                            std::string(std::filesystem::path(*WordExpand(mgr.config.modFolder)) / inst->installDir)
                        };
                        LaunchProc(args, "/", false);
                    }
                    ImGui::EndDisabled();

                    if (mgr.enableRemove)
                    {
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Button, DELETE_COLOR);
                        if (ImGui::Button("Delete"))
                        {
                            del = i;
                        }
                        ImGui::PopStyleColor(1);
                    }

                    if (mgr.enableSetOk)
                    {
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Button, DELETE_COLOR);
                        if (ImGui::Button("Clear errors"))
                        {
                            ModInstallId iid = mf->installInstances.empty() ? ModInstallId{0} : mf->installInstances[0];
                            ClearInstallErrors(mgr, iid);
                        }
                        if (ImGui::BeginItemTooltip())
                        {
                            ImGui::Text("Sets ok to true and clear the install messages.");
                            ImGui::EndTooltip();
                        }
                        ImGui::PopStyleColor();
                    }

                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1,0.2,0.2,1), inst->ok ? (inst->installMessages.empty() ? " " : "+") : "!");
                    if (!inst->installMessages.empty() && ImGui::BeginItemTooltip())
                    {
                        for (auto&& msg : inst->installMessages)
                        {
                            ImGui::Text("%s",msg.c_str());
                        }
                        ImGui::EndTooltip();
                    }
                    
                    ImGui::SameLine();
                    ImGui::Text("%-32s", mf->logicalName.c_str());
                ImGui::PopID();
            }
        }
        ImGui::Separator();
        if (del != -1)
        {
            DeleteMod(mgr, modList[del]);
            CorrectLoadIndexes(mgr);
            SaveModMgr(mgr);
        }
        /*
        else if (mvUp != -1)
        {
            int l = mgr.inst.mods[mvUp].loadIndex;
            for (auto&& m : mgr.inst.mods)
            {
                if (m.loadIndex == l - 1)
                {
                    m.loadIndex++;
                    break;
                }
            }
            mgr.inst.mods[mvUp].loadIndex--;
        }
        else if (mvDown != -1)
        {
            int l = mgr.inst.mods[mvDown].loadIndex;
            for (auto&& m : mgr.inst.mods)
            {
                if (m.loadIndex == l + 1)
                {
                    m.loadIndex--;
                    break;
                }
            }
            mgr.inst.mods[mvDown].loadIndex++;
        }
        if (mvDown != -1 || mvUp != -1)
        {
            CorrectLoadIndexes(mgr);
            SaveModMgr(mgr);
        }
        */
    }
    ImGui::End();

    RenderPluginsList(mgr);
    RenderModDownloads(mgr);
    RenderFomod(mgr);
    RenderCollectionWindow(mgr);

    if (mgr.makingNewMod)
    {
        RenderNewModDialog(mgr);
    }

    if (mgr.settingsOpen)
    {
        RenderModMgrSettings(mgr);
    }

    if (mgr.openCollectionInput)
    {
        mgr.openCollectionInput = false;
        if (mgr.inst.collection)
        {
            mgr.inputCollection = mgr.inst.collection->url;
        }
        ImGui::OpenPopup("Input Collection");
    }
    bool open = true;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Input Collection", &open))
    {
        if (!open)
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::InputText("Slug", &mgr.inputCollection.slug);
        ImGui::InputInt("Revision", &mgr.inputCollection.rev, 0, 0);
        ImGui::InputText("Game", &mgr.inputCollection.game);

        if (ImGui::Button("Load Collection"))
        {
            ImGui::CloseCurrentPopup();
            StartNXMCollectionInstall(mgr, mgr.inputCollection);
        }

        ImGui::EndPopup();
    }
}




