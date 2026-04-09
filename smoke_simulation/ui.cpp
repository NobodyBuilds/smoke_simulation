#include "param.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "host.h"
#include <algorithm>

void debug() {
    ImGui::Begin("Simulation Control");

    // ── Simulation ──────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::DragFloat("Tile Size", &settings.tilesize, 0.5f, 1.0f, 900.0f, "%.1f")) {
            restart();
        }
        ImGui::SameLine(); ImGui::TextDisabled("(restart)");
        ImGui::DragFloat("Radius", &settings.radius, 0.5f, 1.0f, 500.0f, "%.1f");
    }

    // ── Solver ───────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Solver", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Iterations", &settings.itters, 1, 10);
        settings.itters = std::clamp(settings.itters, 1, 500);

        ImGui::SliderFloat("SOR \xcf\x89", &settings.sor, 1.0f, 1.99f, "%.2f");
        ImGui::DragFloat("Damp", &settings.damp, 0.001f, 0.8f, 1.0f, "%.3f");
        ImGui::DragFloat("k", &settings.k, 0.01f, 0.0f, 2.0f, "%.2f");
    }

    // ── Fluid Physics ────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Fluid Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Density", &settings.density, 0.05f, 0.1f, 10.0f, "%.2f");
        ImGui::DragFloat("Viscosity", &settings.visc, 0.0001f, 0.0f, 0.5f, "%.4f");
        ImGui::DragFloat("Vorticity", &settings.vorticity, 0.1f, 0.0f, 50.0f, "%.1f");
        ImGui::DragFloat("Dissipation", &settings.dissipation, 0.001f, 0.0f, 0.5f, "%.3f");
        ImGui::DragFloat("velocity scale", &settings.vscale, 1.0f, 1.0f, 50.0f);
        ImGui::DragFloat("add smoke scale", &settings.dscale, 0.1f, 0.1f, 50.0f);
        ImGui::DragFloat("bouyancy", &settings.bouyancy, 0.5f, 0.0f, 100.0f);
    }

    ImGui::End();
}

extern "C" void ui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ── Stats Overlay ─────────────────────────────────────────────
    ImGuiWindowFlags overlay_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(220, 100));
    ImGui::Begin("Overlay", nullptr, overlay_flags);
    ImGui::Text("FPS   avg:%.1f  max:%.1f  min:%.1f", settings.avgFps, settings.maxFps, settings.minFps);
    ImGui::Text("Frame %.2f ms", settings.fuc_ms);
    ImGui::Text("Div error: %d", settings.diverror);
    ImGui::Text("Grid: %d x %d  cells/r: %.1f", settings.w, settings.h, settings.radiuscells);
    ImGui::End();

    debug();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}