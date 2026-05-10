#include "ArrayList.hpp"

bool ArrayList::g_showArrayList = true;

void ArrayList::Initialize() {
    // Initialized as enabled
}

void ArrayList::RenderMenu() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::Text("Manage the visibility of your enabled modules list.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    
    ImGui::Checkbox("Show ArrayList", &g_showArrayList);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggles the list of enabled modules on the HUD.");
    }
}
