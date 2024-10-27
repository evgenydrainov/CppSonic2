#include "editor.h"

#undef Remove

#include "IconsFontAwesome5.h"
#include "nfd/nfd.h"
#include <glad/gl.h>

#include "imgui/imgui_internal.h"

Editor editor;

void Editor::init() {
	update_window_caption();
}

void Editor::deinit() {

}

static ImVec2 CalcButtonSize(const char* label) {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec2 size = ImGui::CalcTextSize(label) + style.FramePadding * 2.0f;
	return size;
}

static bool ButtonCentered(const char* label) {
	ImVec2 size = CalcButtonSize(label);
	ImVec2 avail = ImGui::GetContentRegionAvail();
	ImGui::SetCursorPosX((avail.x - size.x) / 2.0f);
	return ImGui::Button(label, size);
}

// cnl: create new level
static char cnl_folder[512];
static char cnl_tileset[512];

static void pan_and_zoom(View& view) {
	ImGuiIO& io = ImGui::GetIO();

	// Using InvisibleButton() as a convenience 1) it will advance the layout cursor and 2) allows us to use IsItemHovered()/IsItemActive()
	ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
	ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
	if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
	if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
	ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

	// This will catch our interactions
	ImGui::InvisibleButton("canvas",
						   canvas_sz,
						   ImGuiButtonFlags_MouseButtonLeft
						   | ImGuiButtonFlags_MouseButtonRight
						   | ImGuiButtonFlags_MouseButtonMiddle);
	const bool is_hovered = ImGui::IsItemHovered(); // Hovered
	const bool is_active = ImGui::IsItemActive();   // Held

	const float mouse_threshold_for_pan = 0.0f;
	if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, mouse_threshold_for_pan)) {
		view.scrolling.x += io.MouseDelta.x;
		view.scrolling.y += io.MouseDelta.y;
	}

	if (ImGui::IsWindowFocused()) {
		if (ImGui::IsKeyDown(ImGuiKey_UpArrow))    view.scrolling.y += 10;
		if (ImGui::IsKeyDown(ImGuiKey_DownArrow))  view.scrolling.y -= 10;
		if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))  view.scrolling.x += 10;
		if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) view.scrolling.x -= 10;
	}

	ImVec2 texture_pos = canvas_p0 + view.scrolling;

	ImVec2 mouse_pos_in_texture = io.MousePos / view.zoom - texture_pos / view.zoom;

	const float MIN_ZOOM = 0.25f;
	const float MAX_ZOOM = 50.0f;

	float mouse_wheel = io.MouseWheel;
	if (is_hovered && mouse_wheel != 0) {
		if (mouse_wheel > 0) {
			while (mouse_wheel > 0) {
				view.zoom *= 1.25f;
				mouse_wheel--;
			}
			Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

			ImVec2 new_texture_pos = (io.MousePos / view.zoom - mouse_pos_in_texture) * view.zoom;
			view.scrolling += new_texture_pos - texture_pos;
		} else {
			mouse_wheel = -mouse_wheel;
			while (mouse_wheel > 0) {
				view.zoom /= 1.25f;
				mouse_wheel--;
			}
			Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

			ImVec2 new_texture_pos = (io.MousePos / view.zoom - mouse_pos_in_texture) * view.zoom;
			view.scrolling += new_texture_pos - texture_pos;
		}
	}

	ImGui::SetCursorScreenPos(canvas_p0); // restore cursor pos
}

static ImVec2 get_mouse_pos_in_view(View& view) {
	ImVec2 pos = ImGui::GetMousePos();

	ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
	ImVec2 texture_pos = canvas_p0 + view.scrolling;

	pos -= texture_pos;
	pos /= view.zoom;

	return pos;
}

static bool pos_in_rect(ImVec2 pos, ImVec2 rect_pos, ImVec2 rect_size) {
	ImVec2 p1 = rect_pos;
	ImVec2 p2 = rect_pos + rect_size;
	return (p1.x <= pos.x && pos.x < p2.x)
		&& (p1.y <= pos.y && pos.y < p2.y);
}

static void AddGrid(ImDrawList* list, ImVec2 pos,
					ImVec2 tile_size, glm::ivec2 grid_size,
					ImU32 col = IM_COL32(0xff, 0xff, 0xff, 0xff)) {
	for (int x = 0; x <= grid_size.x; x++) {
		ImVec2 p = pos + ImVec2(x * tile_size.x, 0);
		list->AddLine(p, p + ImVec2(0, grid_size.y * tile_size.y), col);
	}

	for (int y = 0; y <= grid_size.y; y++) {
		ImVec2 p = pos + ImVec2(0, y * tile_size.y);
		list->AddLine(p, p + ImVec2(grid_size.x * tile_size.x, 0), col);
	}
}

void Editor::clear_state() {
	if (tileset_texture.ID != 0) {
		glDeleteTextures(1, &tileset_texture.ID);
	}
	tileset_texture = {};

	if (tilemap_tiles.data) {
		free(tilemap_tiles.data);
	}
	tilemap_tiles = {};

	heightmap_view   = {};
	tilemap_view     = {};
	tile_select_view = {};
	tilemap_width    = 0;
	tilemap_height   = 0;
	selected_tile_index = 0;
}

void Editor::update(float delta) {
	ImGuiIO& io = ImGui::GetIO();

	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	ImGuiID cnl_popup_id = ImGui::GetID("Create New Level");

	auto try_open_cnl_popup = [&]() {
		if (!ImGui::IsPopupOpen(cnl_popup_id, 0)) {
			ImGui::OpenPopup(cnl_popup_id);
			cnl_folder[0] = 0;
			cnl_tileset[0] = 0;
		}
	};

	auto try_open_level = [&]() {
		char* path = nullptr;
		nfdresult_t res = NFD_PickFolder(nullptr, &path);
		defer { free(path); };

		if (res != NFD_OKAY) {
			return;
		}

		clear_state();

		current_level_dir = path;
		is_level_open = true;

		update_window_caption();

		tilemap_width = 256;
		tilemap_height = 256;

		tilemap_tiles.count = tilemap_width * tilemap_height;
		tilemap_tiles.data = (u32*) calloc(tilemap_tiles.count, sizeof(tilemap_tiles[0]));

		tileset_texture = load_texture_from_file((current_level_dir / "Tileset.png").string().c_str());

		heightmap_view.scrolling = (io.DisplaySize - ImVec2(tileset_texture.width, tileset_texture.height)) / 2.0f;
	};

	if (ImGui::IsKeyPressed(ImGuiKey_N) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
		try_open_cnl_popup();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_O) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
		try_open_level();
	}

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Level", "Ctrl+N")) {
				try_open_cnl_popup();
			}

			if (ImGui::MenuItem("Open Level...", "Ctrl+O")) {
				try_open_level();
			}

			ImGui::EndMenu();
		}

		switch (mode) {
			case MODE_HEIGHTMAP: {
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

	int tileset_width  = tileset_texture.width  / 16;
	int tileset_height = tileset_texture.height / 16;

	switch (mode) {
		case MODE_HEIGHTMAP: {
			{
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
				defer { ImGui::PopStyleVar(); };

				ImGui::Begin("Heightmap Editor##heightmap_editor");
				defer { ImGui::End(); };

				if (!is_level_open) {
					const char* text = "No level loaded.";
					ImVec2 text_size = ImGui::CalcTextSize(text);
					ImVec2 avail = ImGui::GetContentRegionAvail();
					ImGui::SetCursorPos((avail - text_size) / 2.0f);
					ImGui::TextUnformatted(text);
					break;
				}

				if (tileset_texture.ID == 0) {
					const char* text = "No tileset loaded.";
					ImVec2 text_size = ImGui::CalcTextSize(text);
					ImVec2 avail = ImGui::GetContentRegionAvail();
					ImGui::SetCursorPos((avail - text_size) / 2.0f);
					ImGui::TextUnformatted(text);
					break;
				}

				pan_and_zoom(heightmap_view);

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				ImVec2 texture_size(tileset_texture.width * heightmap_view.zoom, tileset_texture.height * heightmap_view.zoom);
				ImVec2 texture_pos = ImGui::GetCursorScreenPos() + heightmap_view.scrolling;

				draw_list->AddImage(tileset_texture.ID, texture_pos, texture_pos + texture_size);

				AddGrid(draw_list,
						texture_pos,
						ImVec2(16 * heightmap_view.zoom, 16 * heightmap_view.zoom),
						{tileset_texture.width / 16, tileset_texture.height / 16});
			}
			break;
		}

		case MODE_TILEMAP: {
			auto tilemap_editor_window = [&]() {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
				defer { ImGui::PopStyleVar(); };

				ImGui::Begin("Tilemap Editor##tilemap_editor");
				defer { ImGui::End(); };

				if (!is_level_open) {
					const char* text = "No level loaded.";
					ImVec2 text_size = ImGui::CalcTextSize(text);
					ImVec2 avail = ImGui::GetContentRegionAvail();
					ImGui::SetCursorPos((avail - text_size) / 2.0f);
					ImGui::TextUnformatted(text);
					return;
				}

				pan_and_zoom(tilemap_view);

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				ImVec2 tilemap_pos = ImGui::GetCursorScreenPos() + tilemap_view.scrolling;
				ImVec2 tilemap_size(tilemap_width * 16 * tilemap_view.zoom, tilemap_height * 16 * tilemap_view.zoom);

				// item = invisible button
				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
					if (pos_in_rect(ImGui::GetMousePos(), tilemap_pos, tilemap_size)) {
						ImVec2 pos = get_mouse_pos_in_view(tilemap_view);

						int tile_x = clamp((int)(pos.x / 16), 0, tilemap_width  - 1);
						int tile_y = clamp((int)(pos.y / 16), 0, tilemap_height - 1);
						tilemap_tiles[tile_x + tile_y * tilemap_width] = selected_tile_index;
					}
				}

				bool is_erasing = false;
				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
					if (pos_in_rect(ImGui::GetMousePos(), tilemap_pos, tilemap_size)) {
						ImVec2 pos = get_mouse_pos_in_view(tilemap_view);

						int tile_x = clamp((int)(pos.x / 16), 0, tilemap_width  - 1);
						int tile_y = clamp((int)(pos.y / 16), 0, tilemap_height - 1);
						tilemap_tiles[tile_x + tile_y * tilemap_width] = 0;
						is_erasing = true;
					}
				}

				for (int y = 0; y < tilemap_height; y++) {
					for (int x = 0; x < tilemap_width; x++) {
						u32 tile = tilemap_tiles[x + y * tilemap_width];

						if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
							if (pos_in_rect(ImGui::GetMousePos(), tilemap_pos, tilemap_size)) {
								ImVec2 pos = get_mouse_pos_in_view(tilemap_view);

								int tile_x = clamp((int)(pos.x / 16), 0, tilemap_width  - 1);
								int tile_y = clamp((int)(pos.y / 16), 0, tilemap_height - 1);

								if (tile_x == x && tile_y == y && !is_erasing) {
									tile = selected_tile_index;
								}
							}
						}

						if (tile == 0) {
							continue;
						}

						int tile_u = (tile % tileset_width) * 16;
						int tile_v = (tile / tileset_width) * 16;
						ImVec2 tileset_texture_size(tileset_texture.width, tileset_texture.height);

						ImVec2 p = tilemap_pos + ImVec2(x * 16 * tilemap_view.zoom, y * 16 * tilemap_view.zoom);
						draw_list->AddImage(tileset_texture.ID, p, p + ImVec2(16, 16) * tilemap_view.zoom,
											ImVec2(tile_u, tile_v) / tileset_texture_size,
											ImVec2(tile_u + 16, tile_v + 16) / tileset_texture_size);
					}
				}

				AddGrid(draw_list, tilemap_pos, ImVec2(16 * tilemap_view.zoom, 16 * tilemap_view.zoom), {tilemap_width, tilemap_height});
			};

			tilemap_editor_window();

			auto tile_select_window = [&]() {
				ImGui::Begin("Tileset##tilemap_editor");
				defer { ImGui::End(); };

				if (!is_level_open) {
					return;
				}

				pan_and_zoom(tile_select_view);

				ImVec2 texture_size(tileset_texture.width * tile_select_view.zoom, tileset_texture.height * tile_select_view.zoom);
				ImVec2 texture_pos = ImGui::GetCursorScreenPos() + tile_select_view.scrolling;

				// item = invisible button
				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
					if (pos_in_rect(ImGui::GetMousePos(), texture_pos, texture_size)) {
						ImVec2 pos = get_mouse_pos_in_view(tile_select_view);

						int tile_x = clamp((int)(pos.x / 16), 0, tileset_width  - 1);
						int tile_y = clamp((int)(pos.y / 16), 0, tileset_height - 1);
						selected_tile_index = tile_x + tile_y * tileset_width;
					}
				}

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				draw_list->AddImage(tileset_texture.ID, texture_pos, texture_pos + texture_size);

				{
					int tile_x = selected_tile_index % tileset_width;
					int tile_y = selected_tile_index / tileset_width;
					ImVec2 p = texture_pos + ImVec2(tile_x * 16 * tile_select_view.zoom, tile_y * 16 * tile_select_view.zoom);
					draw_list->AddRect(p, p + ImVec2(16 * tile_select_view.zoom, 16 * tile_select_view.zoom), IM_COL32(255, 255, 255, 255), 0, 0, 2);
				}
			};

			tile_select_window();
			break;
		}
	}

	ImGui::SetNextWindowPos(io.DisplaySize / 2.0f, ImGuiCond_Always, {0.5f, 0.5f});
	if (ImGui::BeginPopupModal("Create New Level", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize)) {
		if (ImGui::Button("Open...##folder")) {
			char* path = nullptr;
			nfdresult_t res = NFD_PickFolder(nullptr, &path);
			defer { free(path); };

			if (res == NFD_OKAY) {
				ImStrncpy(cnl_folder, path, sizeof(cnl_folder));
			}
		}

		ImGui::SameLine();
		ImGui::InputText("Folder", cnl_folder, sizeof(cnl_folder));

		if (ImGui::Button("Open...##tileset")) {
			char* path = nullptr;
			nfdresult_t res = NFD_OpenDialog("png", nullptr, &path);
			defer { free(path); };

			if (res == NFD_OKAY) {
				ImStrncpy(cnl_tileset, path, sizeof(cnl_tileset));
			}
		}

		ImGui::SameLine();
		ImGui::InputText("Tileset", cnl_tileset, sizeof(cnl_tileset));

		bool error = false;
		const char* error_msg = nullptr;

		auto check_for_errors = [&]() {
			if (is_level_open) {
				error = true;
				error_msg = "programmer error";
				return;
			}

			if (cnl_folder[0] == 0) {
				error = true;
				return;
			}

			std::error_code ec;

			if (!std::filesystem::exists(cnl_folder, ec)) {
				error = true;
				error_msg = "Folder must exist.";
				return;
			}

			if (!std::filesystem::is_empty(cnl_folder, ec)) {
				error = true;
				error_msg = "Folder must be empty.";
				return;
			}

			if (cnl_tileset[0] == 0) {
				error = true;
				return;
			}

			if (!std::filesystem::is_regular_file(cnl_tileset, ec)) {
				error = true;
				error_msg = "Tileset file doesn't exist.";
				return;
			}
		};

		check_for_errors();

		if (error_msg) {
			ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE " %s", error_msg);
		}

		ImGui::BeginDisabled(error);

		if (ButtonCentered("Create")) {
			auto try_create_new_level = [&]() {
				if (error) return false;

				clear_state();

				current_level_dir = cnl_folder;
				is_level_open = true;

				update_window_caption();

				tilemap_width = 256;
				tilemap_height = 256;

				tilemap_tiles.count = tilemap_width * tilemap_height;
				tilemap_tiles.data = (u32*) calloc(tilemap_tiles.count, sizeof(tilemap_tiles[0]));

				std::filesystem::copy_file(cnl_tileset, current_level_dir / "Tileset.png");

				tileset_texture = load_texture_from_file((current_level_dir / "Tileset.png").string().c_str());

				heightmap_view.scrolling = (io.DisplaySize - ImVec2(tileset_texture.width, tileset_texture.height)) / 2.0f;

				return true;
			};

			if (try_create_new_level()) {
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndDisabled();

		if (ButtonCentered("Cancel")) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void Editor::draw(float delta) {

}

void Editor::update_window_caption() {
	if (current_level_dir.empty()) {
		SDL_SetWindowTitle(get_window_handle(), "Editor");
	} else {
		char buf[512];
		stb_snprintf(buf, sizeof(buf), "Editor - (%s)", current_level_dir.string().c_str());
		SDL_SetWindowTitle(get_window_handle(), buf);
	}
}
