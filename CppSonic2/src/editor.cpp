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

static void pan_and_zoom(ImVec2& scrolling, float& zoom) {
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
						   | ImGuiButtonFlags_MouseButtonMiddle);
	const bool is_hovered = ImGui::IsItemHovered(); // Hovered
	const bool is_active = ImGui::IsItemActive();   // Held

	const float mouse_threshold_for_pan = 0.0f;
	if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, mouse_threshold_for_pan)) {
		scrolling.x += io.MouseDelta.x;
		scrolling.y += io.MouseDelta.y;
	}

	if (ImGui::IsWindowFocused()) {
		if (ImGui::IsKeyDown(ImGuiKey_UpArrow))    scrolling.y += 10;
		if (ImGui::IsKeyDown(ImGuiKey_DownArrow))  scrolling.y -= 10;
		if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))  scrolling.x += 10;
		if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) scrolling.x -= 10;
	}

	ImVec2 texture_pos = canvas_p0 + scrolling;

	ImVec2 mouse_pos_in_texture = io.MousePos / zoom - texture_pos / zoom;

	const float MIN_ZOOM = 0.25f;
	const float MAX_ZOOM = 50.0f;

	float mouse_wheel = io.MouseWheel;
	if (is_hovered && mouse_wheel != 0) {
		if (mouse_wheel > 0) {
			while (mouse_wheel > 0) {
				zoom *= 1.25f;
				mouse_wheel--;
			}
			Clamp(&zoom, MIN_ZOOM, MAX_ZOOM);

			ImVec2 new_texture_pos = (io.MousePos / zoom - mouse_pos_in_texture) * zoom;
			scrolling += new_texture_pos - texture_pos;
		} else {
			mouse_wheel = -mouse_wheel;
			while (mouse_wheel > 0) {
				zoom /= 1.25f;
				mouse_wheel--;
			}
			Clamp(&zoom, MIN_ZOOM, MAX_ZOOM);

			ImVec2 new_texture_pos = (io.MousePos / zoom - mouse_pos_in_texture) * zoom;
			scrolling += new_texture_pos - texture_pos;
		}
	}

	ImGui::SetCursorScreenPos(canvas_p0);
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

		if (hmap.texture.ID != 0) {
			glDeleteTextures(1, &hmap.texture.ID);
			hmap.texture.ID = 0;
		}

		if (tmap.tiles.data) {
			free(tmap.tiles.data);
			tmap.tiles.data = nullptr;
		}

		hmap = {};
		tmap = {};

		current_level_dir = path;
		is_level_open = true;

		update_window_caption();

		tmap.width = 256;
		tmap.height = 256;

		tmap.tiles.count = tmap.width * tmap.height;
		tmap.tiles.data = (u32*) malloc(tmap.tiles.count * sizeof(tmap.tiles[0]));

		hmap.texture = load_texture_from_file((current_level_dir / "Tileset.png").string().c_str());

		hmap.scrolling = (io.DisplaySize - ImVec2(hmap.texture.width, hmap.texture.height)) / 2.0f;
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

	switch (mode) {
		case MODE_HEIGHTMAP: {
			int tmap = 0;

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

				if (hmap.texture.ID == 0) {
					const char* text = "No tileset loaded.";
					ImVec2 text_size = ImGui::CalcTextSize(text);
					ImVec2 avail = ImGui::GetContentRegionAvail();
					ImGui::SetCursorPos((avail - text_size) / 2.0f);
					ImGui::TextUnformatted(text);
					break;
				}

				pan_and_zoom(hmap.scrolling, hmap.zoom);

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				ImVec2 texture_size(hmap.texture.width * hmap.zoom, hmap.texture.height * hmap.zoom);
				ImVec2 texture_pos = ImGui::GetCursorScreenPos() + hmap.scrolling;

				draw_list->AddImage(hmap.texture.ID, texture_pos, texture_pos + texture_size);

				for (int tile_y = 0; tile_y < hmap.texture.height / 16; tile_y++) {
					for (int tile_x = 0; tile_x < hmap.texture.width / 16; tile_x++) {
						ImVec2 p_min = texture_pos + ImVec2(tile_x * 16 * hmap.zoom, tile_y * 16 * hmap.zoom);
						draw_list->AddRect(p_min, p_min + ImVec2(16 * hmap.zoom, 16 * hmap.zoom), IM_COL32(0xff, 0xff, 0xff, 0xff));
					}
				}
			}
			break;
		}

		case MODE_TILEMAP: {
			int hmap = 0;

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

				// TODO: pan_and_zoom

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
				const ImVec2 origin(canvas_p0.x + tmap.scrolling.x, canvas_p0.y + tmap.scrolling.y); // Lock scrolled origin

				const float mouse_threshold_for_pan = 0.0f;
				if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, mouse_threshold_for_pan)) {
					tmap.scrolling.x += io.MouseDelta.x;
					tmap.scrolling.y += io.MouseDelta.y;
				}

				if (ImGui::IsWindowFocused()) {
					if (ImGui::IsKeyDown(ImGuiKey_UpArrow))    tmap.scrolling.y += 10;
					if (ImGui::IsKeyDown(ImGuiKey_DownArrow))  tmap.scrolling.y -= 10;
					if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))  tmap.scrolling.x += 10;
					if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) tmap.scrolling.x -= 10;
				}

				ImVec2 tilemap_pos = origin;

				for (int tile_y = 0; tile_y < tmap.height; tile_y++) {
					for (int tile_x = 0; tile_x < tmap.width; tile_x++) {
						ImVec2 p_min = tilemap_pos + ImVec2(tile_x * 16 * tmap.zoom, tile_y * 16 * tmap.zoom);
						draw_list->AddRect(p_min, p_min + ImVec2(16 * tmap.zoom, 16 * tmap.zoom), IM_COL32(0xff, 0xff, 0xff, 0xff));
					}
				}

				ImVec2 mouse_pos_in_texture = io.MousePos / tmap.zoom - tilemap_pos / tmap.zoom;

				const float MIN_ZOOM = 0.25f;
				const float MAX_ZOOM = 50.0f;

				float mouse_wheel = io.MouseWheel;
				if (is_hovered && mouse_wheel != 0) {
					if (mouse_wheel > 0) {
						while (mouse_wheel > 0) {
							tmap.zoom *= 1.25f;
							mouse_wheel--;
						}
						Clamp(&tmap.zoom, MIN_ZOOM, MAX_ZOOM);

						ImVec2 new_texture_pos = (io.MousePos / tmap.zoom - mouse_pos_in_texture) * tmap.zoom;
						tmap.scrolling += new_texture_pos - tilemap_pos;
					} else {
						mouse_wheel = -mouse_wheel;
						while (mouse_wheel > 0) {
							tmap.zoom /= 1.25f;
							mouse_wheel--;
						}
						Clamp(&tmap.zoom, MIN_ZOOM, MAX_ZOOM);

						ImVec2 new_texture_pos = (io.MousePos / tmap.zoom - mouse_pos_in_texture) * tmap.zoom;
						tmap.scrolling += new_texture_pos - tilemap_pos;
					}
				}
			};

			tilemap_editor_window();

			{
				ImGui::Begin("Tileset##tilemap_editor");
				defer { ImGui::End(); };


			}
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

		if (cnl_folder[0] != 0) {
			std::error_code ec;
			if (!std::filesystem::exists(cnl_folder, ec)) {
				ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE " Folder must exist.");
			} else if (!std::filesystem::is_empty(cnl_folder, ec)) {
				ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE " Folder must be empty.");
			}
		}

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

		if (cnl_tileset[0] != 0) {
			std::error_code ec;
			if (!std::filesystem::is_regular_file(cnl_tileset, ec)) {
				ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE " File must exist.");
			}
		}

		if (ButtonCentered("Create")) {
			auto try_create_new_level = [&]() {
				std::error_code ec;

				if (cnl_folder[0] == 0) return;
				if (!std::filesystem::exists(cnl_folder, ec)) return;
				if (!std::filesystem::is_empty(cnl_folder, ec)) return;

				if (cnl_tileset[0] == 0) return;
				if (!std::filesystem::is_regular_file(cnl_tileset, ec)) return;

				if (hmap.texture.ID != 0) {
					glDeleteTextures(1, &hmap.texture.ID);
					hmap.texture.ID = 0;
				}

				if (tmap.tiles.data) {
					free(tmap.tiles.data);
					tmap.tiles.data = nullptr;
				}

				hmap = {};
				tmap = {};

				current_level_dir = cnl_folder;
				is_level_open = true;

				update_window_caption();

				tmap.width = 256;
				tmap.height = 256;

				tmap.tiles.count = tmap.width * tmap.height;
				tmap.tiles.data = (u32*) malloc(tmap.tiles.count * sizeof(tmap.tiles[0]));

				std::filesystem::copy_file(cnl_tileset, current_level_dir / "Tileset.png");

				hmap.texture = load_texture_from_file((current_level_dir / "Tileset.png").string().c_str());

				hmap.scrolling = (io.DisplaySize - ImVec2(hmap.texture.width, hmap.texture.height)) / 2.0f;

				ImGui::CloseCurrentPopup();
			};

			try_create_new_level();
		}

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
