#include "param.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "host.h"
#include <algorithm>
#include "cuda.h"

// ── FPS ring buffer ───────────────────────────────────────────────────────────
static float s_fpsHistory[128] = {};
static int   s_fpsOffset = 0;
static float s_fpsAccum = 0.f;
static int   s_fpsSamples = 0;

static void pushFps(float fps) {
    s_fpsAccum += fps;
    s_fpsSamples++;
    if (s_fpsSamples >= 6) {
        s_fpsHistory[s_fpsOffset] = s_fpsAccum / s_fpsSamples;
        s_fpsOffset = (s_fpsOffset + 1) % 128;
        s_fpsAccum = 0.f;
        s_fpsSamples = 0;
    }
}

// ── tooltip helper ────────────────────────────────────────────────────────────
static void tip(const char* desc) {
    ImGui::SetItemTooltip(desc);
}

static bool s_panelOpen = true;

static void debug() {
    bool sync = false;

    // ── collapsed pill ───────────────────────────────────────────────────────
    if (!s_panelOpen) {
        ImGuiWindowFlags pill_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize;
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("##pill", nullptr, pill_flags);
        ImGui::Text("%.1f fps", settings.avgFps);
        ImGui::SameLine();
        if (ImGui::SmallButton("▼")) s_panelOpen = true;
        ImGui::End();
        return;
    }

    // ── main panel ───────────────────────────────────────────────────────────
    ImGuiWindowFlags panel_flags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always); // fixed narrow width
    ImGui::SetNextWindowBgAlpha(0.82f);
    ImGui::Begin("Sim Control", nullptr, panel_flags);

    // collapse button
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16);
    if (ImGui::SmallButton("▲")) { s_panelOpen = false; ImGui::End(); return; }

    // ── Stats — always visible, never collapsible ─────────────────────────
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::PlotLines("##fps", s_fpsHistory, 128, s_fpsOffset,
        nullptr, 0.f, 165.f, ImVec2(w, 40));

    if (ImGui::BeginTable("fps_table", 3,
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("avg", ImGuiTableColumnFlags_WidthFixed, 65);
        ImGui::TableSetupColumn("max", ImGuiTableColumnFlags_WidthFixed, 65);
        ImGui::TableSetupColumn("min", ImGuiTableColumnFlags_WidthFixed, 65);
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("%.1f", settings.avgFps);
        ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", settings.maxFps);
        ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f", settings.minFps);
        ImGui::EndTable();
    }
    ImGui::Text("%.2f ms  Div:%d", settings.fuc_ms, settings.diverror);
    ImGui::Text("%dx%d  %d cells", settings.w, settings.h, settings.cells);

    ImGui::Separator();

    // ── Simulation ───────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Simulation")) {
        ImGui::PushItemWidth(-60);
        if (sync |= ImGui::DragFloat("Tile##ts", &settings.tilesize, 0.01f, 0.001f, 900.f, "%.1f"))
            restart();
        
        ImGui::SameLine(); ImGui::TextDisabled("rst");
        tip("World-space size of each grid cell in pixels.\n"
            "Smaller = higher resolution, heavier GPU cost.\n"
            "Changing this restarts the simulation.");
        
        sync |= ImGui::DragFloat("Radius##r", &settings.radius, 0.5f, 1.f, 500.f, "%.1f");
        tip("Mouse brush radius in pixels.\n"
            "Affects both density injection and velocity stirring.");
        ImGui::PopItemWidth();
    }

    // ── Solver ───────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Solver")) {
        ImGui::PushItemWidth(-60);
        sync |= ImGui::InputInt("Iters##it", &settings.itters, 1, 10);
        settings.itters = std::clamp(settings.itters, 1, 500);
        tip("Pressure Poisson iterations per frame.\n"
            "More = lower divergence error, higher GPU cost.\n"
            "Watch 'Div' above. 20-40 usually sufficient.");

        sync |= ImGui::SliderFloat("SOR##sor", &settings.sor, 0.01f, 1.99f, "%.2f");
        tip("Successive Over-Relaxation factor.\n"
            "1.0 = standard GS. 1.5-1.9 = faster convergence.\n"
            "Near 2.0 can cause solver divergence.");
        ImGui::PopItemWidth();

        ImGui::PushStyleColor(ImGuiCol_Text, replay.recording
            ? ImVec4(1, .2f, .2f, 1) : ImGui::GetStyleColorVec4(ImGuiCol_Text));
        if (ImGui::SmallButton(replay.recording ? "■ Stop" : "● Rec")) {
            if (replay.recording) { replay.recording = false; }
            else { replay.count = replay.head = 0; replay.recording = true; replay.playing = false; }
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::BeginDisabled(replay.count == 0);
        if (ImGui::SmallButton(replay.playing ? "■ Stop" : "▶ Play")) {
            replay.playing = !replay.playing;
            if (replay.playing) { replay.head = 0; replay.recording = false; }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Text("%d/%d", replay.count, ReplayBuffer::CAP);
    }

    // ── Fluid Physics ────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Fluid Physics")) {
        ImGui::PushItemWidth(-60);
        sync |= ImGui::DragFloat("Density##d", &settings.density, 0.05f, 0.1f, 10.f, "%.2f");
        tip("Fluid mass per unit volume (rho).\n"
            "Scales pressure gradient in projection.\n"
            "Higher = softer pressure response.");

        sync |= ImGui::DragFloat("Viscosity##v", &settings.visc, 0.0001f, 0.f, 0.5f, "%.4f");
        tip("Kinematic viscosity — smooths velocity differences\n"
            "between neighboring cells (implicit diffusion).\n"
            "0 = inviscid. Higher = syrupy flow.");

        sync |= ImGui::DragFloat("Vorticity##vo", &settings.vorticity, 0.1f, 0.f, 50.f, "%.1f");
        tip("Vorticity Confinement strength.\n"
            "Re-injects curl lost to numerical dissipation.\n"
            "Too high causes instability.");

        sync |= ImGui::DragFloat("Dissipation##di", &settings.dissipation, 0.001f, 0.f, 0.5f, "%.3f");
        tip("Exponential density decay per second.\n"
            "density *= (1 - dissipation * dt) each frame.\n"
            "0 = smoke stays forever.");

        sync |= ImGui::DragFloat("Vel Scale##vs", &settings.vscale, 1.f, 1.f, 50.f, "%.1f");
        tip("Mouse-drag velocity input sensitivity multiplier.");

        sync |= ImGui::DragFloat("Smoke##ds", &settings.dscale, 0.1f, 0.1f, 50.f, "%.1f");
        tip("Density added per second inside the mouse brush.\n"
            "density += dscale * dt per frame.");

        sync |= ImGui::DragFloat("Buoyancy##b", &settings.bouyancy, 0.5f, 0.f, 100.f, "%.1f");
        tip("Upward acceleration on the velocity field.\n"
            "vy += buoyancy * dt each step.\n"
            "Not coupled to temperature.");
        ImGui::PopItemWidth();
    }

    if (sync) copyparams();

    ImGui::End();
}

extern "C" void ui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    pushFps(settings.avgFps);
    debug();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}