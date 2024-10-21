#include "editor.h"

#include "IconsFontAwesome5.h"
#include "nfd/nfd.h"
#include <glad/gl.h>

Editor editor;

static string get_open_file_name(const char* filter = nullptr) {
	char* path;
	nfdresult_t res = NFD_OpenDialog(filter, nullptr, &path); // @Leak

	if (res == NFD_OKAY) {
		return {path, strlen(path)};
	} else {
		return {};
	}
}

void Editor::init() {
	
}

void Editor::deinit() {

}

void Editor::update(float delta) {
	ImGuiIO& io = ImGui::GetIO();

	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	if (ImGui::BeginMainMenuBar()) {
		switch (mode) {
			case MODE_HEIGHTMAP: {
				if (ImGui::BeginMenu("File")) {
					if (ImGui::MenuItem("Open Image...")) {
						string filename = get_open_file_name("png");
						if (filename.count > 0) {
							if (hmap.texture.ID) {
								glDeleteTextures(1, &hmap.texture.ID);
								hmap.texture.ID = 0;
							}

							hmap = {};

							char* cstr = to_c_string(filename);
							defer { free(cstr); };

							hmap.texture = load_texture_from_file(cstr);

							hmap.scrolling = (io.DisplaySize - ImVec2(hmap.texture.width, hmap.texture.height)) / 2.0f;
						}
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

	ImGui::ShowDemoWindow();

	switch (mode) {
		case MODE_HEIGHTMAP: {
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
			defer { ImGui::PopStyleVar(); };

			if (ImGui::Begin("Heightmap Editor")) {
				if (hmap.texture.ID != 0) {
					// Using InvisibleButton() as a convenience 1) it will advance the layout cursor and 2) allows us to use IsItemHovered()/IsItemActive()
					ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
					ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
					if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
					if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
					ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

					ImDrawList* draw_list = ImGui::GetWindowDrawList();

					// This will catch our interactions
					ImGui::InvisibleButton("canvas",
										   canvas_sz,
										   ImGuiButtonFlags_MouseButtonLeft
										   | ImGuiButtonFlags_MouseButtonMiddle);
					const bool is_hovered = ImGui::IsItemHovered(); // Hovered
					const bool is_active = ImGui::IsItemActive();   // Held
					const ImVec2 origin(canvas_p0.x + hmap.scrolling.x, canvas_p0.y + hmap.scrolling.y); // Lock scrolled origin

					const float mouse_threshold_for_pan = 0.0f;
					if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, mouse_threshold_for_pan)) {
						hmap.scrolling.x += io.MouseDelta.x;
						hmap.scrolling.y += io.MouseDelta.y;
					}

					ImVec2 texture_size(hmap.texture.width * hmap.zoom, hmap.texture.height * hmap.zoom);
					ImVec2 texture_pos = origin;
					ImVec2 texture_center = texture_pos + texture_size / 2.0f;

					draw_list->AddImage(hmap.texture.ID, texture_pos, texture_pos + texture_size);

					for (int tile_y = 0; tile_y < 16; tile_y++) {
						for (int tile_x = 0; tile_x < 16; tile_x++) {
							ImVec2 p_min = texture_pos + ImVec2(tile_x * 16 * hmap.zoom, tile_y * 16 * hmap.zoom);
							draw_list->AddRect(p_min, p_min + ImVec2(16 * hmap.zoom, 16 * hmap.zoom), IM_COL32(0xff, 0xff, 0xff, 0xff));
						}
					}

					ImVec2 mouse_pos_in_texture = io.MousePos / hmap.zoom - texture_pos / hmap.zoom;

					const float MIN_ZOOM = 0.25f;
					const float MAX_ZOOM = 50.0f;

					float mouse_wheel = io.MouseWheel;
					if (is_hovered && mouse_wheel != 0) {
						if (mouse_wheel > 0) {
							while (mouse_wheel > 0) {
								hmap.zoom *= 1.25f;
								mouse_wheel--;
							}
							Clamp(&hmap.zoom, MIN_ZOOM, MAX_ZOOM);

							ImVec2 new_texture_pos = (io.MousePos / hmap.zoom - mouse_pos_in_texture) * hmap.zoom;
							hmap.scrolling += new_texture_pos - texture_pos;
						} else {
							mouse_wheel = -mouse_wheel;
							while (mouse_wheel > 0) {
								hmap.zoom /= 1.25f;
								mouse_wheel--;
							}
							Clamp(&hmap.zoom, MIN_ZOOM, MAX_ZOOM);

							ImVec2 new_texture_pos = (io.MousePos / hmap.zoom - mouse_pos_in_texture) * hmap.zoom;
							hmap.scrolling += new_texture_pos - texture_pos;
						}
					}
				}
			}
			ImGui::End();
			break;
		}
	}
}

void Editor::draw(float delta) {

}
