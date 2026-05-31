
#include "modmgr_collection_ui.h"
#include "modmgr_collection.h"
#include "modmgr.h"

#include "imgui.h"


void RenderCollectionWindow(ModMgr& mgr)
{
    if (mgr.collection.status == CollectionStatus::None)
    {
        return;
    }

    if (ImGui::Begin("Collection"))
    {
        if (ImGui::Button("Cancel"))
        {
            mgr.collection = {};
        }
        if (mgr.collection.status == CollectionStatus::FetchingInfo)
        {
            if (mgr.collection.error)
            {
                ImGui::Text("Error loading collection info for %s:%d", mgr.collection.url.slug.c_str(), mgr.collection.url.rev);
                if (ImGui::Button("Retry"))
                {
                    auto url = mgr.collection.url;
                    mgr.collection = {};
                    StartNXMCollectionInstall(mgr, url);
                }
            }
            else
            {
                ImGui::Text("Loading info...");
            }
        }
        else if (mgr.collection.status == CollectionStatus::WaitingForInstallButton)
        {
            ImGui::Text("Compressed Asset Size: %1.1f GB", ((float)mgr.collection.info.totalSize) / 1'000'000'000);
            ImGui::Button("INSTALL");
            ImGui::Text("%s", mgr.collection.info.name.c_str());
            ImGui::TextWrapped("%s", mgr.collection.info.summary.c_str());
            ImGui::TextWrapped("%s", mgr.collection.info.description.c_str());
        }

    }
    ImGui::End();
}







