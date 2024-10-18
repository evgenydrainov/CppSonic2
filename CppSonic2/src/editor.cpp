#include "editor.h"

#include "IconsFontAwesome5.h"

Editor editor;

void Editor::init() {

}

void Editor::deinit() {

}

void Editor::update(float delta) {
	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	if (ImGui::BeginMainMenuBar()) {
		switch (mode) {
			case MODE_HEIGHTMAP: {
				if (ImGui::BeginMenu("File")) {
					if (ImGui::MenuItem("Open Image...")) {
						
					}

					if (ImGui::MenuItem("Open Heightmap...")) {}

					ImGui::EndMenu();
				}
				break;
			}
		}

		// centered
		{
			static float main_menu_bar_width;

			ImGui::SetCursorScreenPos(ImVec2((ImGui::GetWindowWidth() - main_menu_bar_width) / 2.0f, 0.0f));

			ImVec2 cursor = ImGui::GetCursorPos();

			if (ImGui::MenuItem("Heightmap", nullptr, mode == MODE_HEIGHTMAP)) mode = MODE_HEIGHTMAP;
			if (ImGui::MenuItem("Tilemap", nullptr, mode == MODE_TILEMAP)) mode = MODE_TILEMAP;

			main_menu_bar_width = ImGui::GetCursorPos().x - cursor.x;
		}

		ImGui::EndMainMenuBar();
	}

	switch (mode) {
		case MODE_HEIGHTMAP: {
			if (ImGui::Begin("Heightmap Editor")) {
				
			}
			ImGui::End();
			break;
		}
	}
}

void Editor::draw(float delta) {

}
