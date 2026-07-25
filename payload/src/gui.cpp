#include "imgui.h"
#include "shared_state.h"
#include "gui.h"

void gui_render() {
    bool menuOpen = false;
    state_lock();
    menuOpen = g_state.menuOpen;
    state_unlock();

    //=== ALWAYS-ON debug corner overlay ===
    {
        ImGui::SetNextWindowPos(ImVec2(4, 4), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.60f);
        ImGui::Begin("##dbg", nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs);

        float fps = ImGui::GetIO().Framerate;
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[OVERLAY ACTIVE]");
        ImGui::Text("FPS: %.0f  Menu: %s", fps, menuOpen ? "OPEN" : "closed");

        double px = 0, py = 0, pz = 0;
        state_lock();
        px = g_state.posX; py = g_state.posY; pz = g_state.posZ;
        state_unlock();
        ImGui::Text("Pos: %.1f %.1f %.1f", px, py, pz);

        ImGui::End();
    }

    if (!menuOpen) return;

    //=== Main menu ===
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGui::Begin("Minecraft Client", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Minecraft Client v0.1");
    ImGui::Separator();

    double x = 0.0, y = 0.0, z = 0.0;
    state_lock();
    x = g_state.posX; y = g_state.posY; z = g_state.posZ;
    state_unlock();

    ImGui::Text("Position:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.1f, 1.0f, 0.1f, 1.0f), "X=%.2f  Y=%.2f  Z=%.2f", x, y, z);

    static bool enableFlight = false;
    static bool enableSpeed  = false;
    static bool enableNoFall = false;

    ImGui::Separator();
    ImGui::Text("Features");
    ImGui::Checkbox("Flight",     &enableFlight);
    ImGui::Checkbox("Speed",      &enableSpeed);
    ImGui::Checkbox("NoFall",     &enableNoFall);

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Right Shift to toggle menu");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Status: %s", g_state.running ? "Running" : "Stopped");

    ImGui::End();
}
