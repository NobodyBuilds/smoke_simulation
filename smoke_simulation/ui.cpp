#include "param.h"
#include<imgui.h>
#include<imgui_impl_glfw.h>
#include<imgui_impl_opengl3.h>	
#include "host.h"
#include <algorithm>

void debug() {
	ImGui::Begin("Debug");
	if (ImGui::DragFloat("Tile Size: %.1f", &settings.tilesize, 0.1f, 1.0f, 900.0f)){
		restart();
	}
	ImGui::InputFloat("SOR: %.2f", &settings.sor, 0.1f, 10.0f);
	ImGui::DragFloat("density: %.1f", &settings.density, 0.1f, 0.1f, 10.0f);
	ImGui::InputInt("Iterations", &settings.itters, 1, 100);
	ImGui::InputFloat("viscosity", &settings.visc, 0.0001f, 0.01f);
	ImGui::InputFloat("vorticity", &settings.vorticity, 1.0f, 100.0f);
	ImGui::End();
}

extern "C" void ui() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Overlay window for FPS and frametime in top left corner
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowSize(ImVec2(200, 60));
	ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
	ImGui::Text("FPS: %.1f", settings.avgFps);
	ImGui::Text("Frame Time: %.2f ms", settings.fuc_ms);
	ImGui::Text("error: %d", settings.diverror);
	ImGui::End();

	debug();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}