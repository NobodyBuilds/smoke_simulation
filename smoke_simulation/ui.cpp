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
	ImGui::Text("FPS: %.1f", settings.fps);
	ImGui::Text("Frame Time: %.2f ms", settings.fuc_ms);
	ImGui::End();

	debug();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}