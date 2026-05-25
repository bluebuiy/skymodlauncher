
#include "modmgr.h"

#include "imgui.h"
#include "prochelper.h"

#include <fstream>
#include <iostream>
#include <format>
#include <filesystem>
#include <unordered_set>

constexpr ImVec4 DELETE_COLOR = ImVec4(0.7f,0.1f,0.1f,1.0f);

void RenderTestUi(ModMgr& mgr)
{
#if 0
    static char strBuf[128] = {0};
    static char strBufOut[256] = {0};
    static bool failRep = false;
    if (ImGui::Begin("Tests"))
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
    }
    ImGui::End();
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
    if (ImGui::Begin("Add New Mod", &mgr.makingNewMod))
    {
        ImGui::InputText("Mod folder", &mgr.newMod.modFile);
        ImGui::Checkbox("Add Enabled" , &mgr.newMod.enabled);
        ImGui::NewLine();
        ImGui::NewLine();
        if (ImGui::Button("Submit"))
        {
            mgr.newMod.loadIndex = mgr.inst.mods.size();
            mgr.inst.mods.emplace_back(mgr.newMod);
            auto path = WordExpand(mgr.config.modFolder);
            if (path)
            {
                std::filesystem::create_directories(std::filesystem::path(*path) / mgr.newMod.modFile);
            }
            mgr.newMod.modFile.clear();
        }
    }
    ImGui::End();
}

void RenderModMgrSettings(ModMgr& mgr)
{
    if (ImGui::Begin("Settings", &mgr.settingsOpen))
    {
        ImGui::InputText("Game Root", &mgr.config.installRoot);
        ImGui::InputText("My Games/Skyrim", &mgr.config.mgRoot);
        ImGui::InputText("Mod folder", &mgr.config.modFolder);
        ImGui::InputText("AppData/Skyrim", &mgr.config.appData);
        ImGui::Separator();
        ImGui::InputText("Config path", &mgr.config.configPath);
        ImGui::InputText("Instance path", &mgr.config.instPath);
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
    if (ImGui::Begin("Plugins"))
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
        if (ImGui::Begin("Edit Exec"))
        {
            ImGui::Text("Name ");
            ImGui::SameLine();
            ImGui::InputText("##Name", &mgr.newExec.execName);
            ImGui::Text("Path ");
            ImGui::SameLine();
            ImGui::InputText("##Path", &mgr.newExec.execPath);
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

void RenderModMgr(ModMgr& mgr)
{
    RenderTestUi(mgr);

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::Button("Save"))
        {
            SaveModMgr(mgr);
        }
        
    }
    ImGui::EndMainMenuBar();
    
    if (ImGui::Begin("Mod Manager"))
    {
        RenderTools(mgr);

        if (ImGui::Button("Settings"))
        {
            mgr.settingsOpen = true;
        }

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
        ImGui::Checkbox("Enable mod removal (doesnt delete the mod folder)", &mgr.enableRemove);
        ImGui::Text("Mod list");
        ImGui::Separator();
        std::sort(mgr.inst.mods.begin(), mgr.inst.mods.end(), [&](ModInfo const & a, ModInfo const & b){
            if (mgr.sortMode == 0)
            {
                return a.loadIndex < b.loadIndex;
            }
            else if (mgr.sortMode == 1)
            {
                return a.modFile < b.modFile;
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
        for (int i = 0; i < mgr.inst.mods.size(); ++i)
        {
            ImGui::PushID(mgr.inst.mods[i].modFile.c_str());
                ImGui::Text(" %3d ", mgr.inst.mods[i].loadIndex);
                ImGui::SameLine();
                ImGui::Checkbox(" ", &mgr.inst.mods[i].enabled);
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
                ImGui::SameLine();
                
                ImGui::Text("%-32s", mgr.inst.mods[i].modFile.c_str());
                if (mgr.enableRemove)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Remove"))
                    {
                        del = i;
                    }
                }
            ImGui::PopID();
        }
        ImGui::Separator();
        if (del != -1)
        {
            mgr.inst.mods.erase(mgr.inst.mods.begin() + del);
        }
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
        SaveModMgr(mgr);
    }
    ImGui::End();

    RenderPluginsList(mgr);

    if (mgr.makingNewMod)
    {
        RenderNewModDialog(mgr);
    }

    if (mgr.settingsOpen)
    {
        RenderModMgrSettings(mgr);
    }
}




