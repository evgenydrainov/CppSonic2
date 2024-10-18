#include "editor.h"

#include "imgui/imgui.h"

Editor editor;

void Editor::init() {

}

void Editor::deinit() {

}

void Editor::update(float delta) {
	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Undo", "CTRL+Z")) {}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

void Editor::draw(float delta) {

}
