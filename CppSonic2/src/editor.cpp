#include "editor.h"

#ifdef EDITOR

#undef Remove

#include "IconsFontAwesome5.h"
#include "nfd/nfd.h"
#include <glad/gl.h>

#include "imgui/imgui_internal.h"

#include "subprocess.h"

Editor editor;

void Editor::init() {
	SDL_MaximizeWindow(get_window_handle());

	update_window_caption();

	load_texture_from_file(&tex_idle, "textures/idle.png");
	load_texture_from_file(&tex_layer_set, "textures/layer_set.png");
	load_texture_from_file(&tex_layer_flip, "textures/layer_flip.png");
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

static bool pos_in_rect(ImVec2 pos, ImVec2 rect_pos, ImVec2 rect_size) {
	ImVec2 p1 = rect_pos;
	ImVec2 p2 = rect_pos + rect_size;
	return ((p1.x <= pos.x && pos.x < p2.x)
			&& (p1.y <= pos.y && pos.y < p2.y));
}

static ImVec2 get_mouse_pos_in_view(View view) {
	ImVec2 pos = ImGui::GetMousePos();

	ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
	ImVec2 texture_pos = canvas_p0 + view.scrolling;

	pos -= texture_pos;
	pos /= view.zoom;

	return pos;
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

	ImVec2 window_center_pos = ImGui::GetWindowPos() + ImGui::GetWindowSize() / 2.0f;

	ImVec2 mouse_pos_in_texture = io.MousePos / view.zoom - texture_pos / view.zoom;
	ImVec2 window_center_pos_in_texture = window_center_pos / view.zoom - texture_pos / view.zoom;

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

	if (ImGui::IsWindowFocused()) {
		if (ImGui::IsKeyPressed(ImGuiKey_Equal)) { // repeat=true is fine
			view.zoom *= 1.25f;
			Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

			ImVec2 new_texture_pos = (window_center_pos / view.zoom - window_center_pos_in_texture) * view.zoom;
			view.scrolling += new_texture_pos - texture_pos;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Minus)) { // repeat=true is fine
			view.zoom /= 1.25f;
			Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

			ImVec2 new_texture_pos = (window_center_pos / view.zoom - window_center_pos_in_texture) * view.zoom;
			view.scrolling += new_texture_pos - texture_pos;
		}
	}

	ImGui::SetCursorScreenPos(canvas_p0); // restore cursor pos
}

static bool rectangle_select(Selection& sel,
							 bool is_item_active,
							 ImVec2 texture_pos, ImVec2 texture_size,
							 View view,
							 int width_in_tiles, int height_in_tiles) {
	bool result = false;

	if (is_item_active && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		if (pos_in_rect(ImGui::GetMousePos(), texture_pos, texture_size)) {
			ImVec2 pos = get_mouse_pos_in_view(view);

			int tile_x = clamp((int)(pos.x / 16), 0, width_in_tiles  - 1);
			int tile_y = clamp((int)(pos.y / 16), 0, height_in_tiles - 1);

			sel.dragging = true;
			sel.drag_start_x = tile_x;
			sel.drag_start_y = tile_y;
		} else {
			sel.x = 0;
			sel.y = 0;
			sel.w = 0;
			sel.h = 0;
		}
	}

	if (sel.dragging) {
		ImVec2 pos = get_mouse_pos_in_view(view);

		int tile_x = clamp((int)(pos.x / 16), 0, width_in_tiles  - 1);
		int tile_y = clamp((int)(pos.y / 16), 0, height_in_tiles - 1);

		sel.x = min(tile_x, sel.drag_start_x);
		sel.y = min(tile_y, sel.drag_start_y);
		sel.w = ImAbs(tile_x - sel.drag_start_x) + 1;
		sel.h = ImAbs(tile_y - sel.drag_start_y) + 1;

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			sel.dragging = false;
			result = true;
		}
	}

	if (is_item_active && ImGui::IsMouseClicked(ImGuiMouseButton_Right, 0)) {
		sel.x = 0;
		sel.y = 0;
		sel.w = 0;
		sel.h = 0;
		sel.dragging = false;
	}

	return result;
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

static void AddTile(ImDrawList* draw_list, Tile tile,
					ImVec2 texture_pos, ImVec2 texture_size,
					View view, int tile_x, int tile_y,
					Texture tileset_texture, ImU32 col = IM_COL32(255, 255, 255, 255)) {
	int tileset_width  = tileset_texture.width  / 16;
	int tileset_height = tileset_texture.height / 16;

	int tile_u = (tile.index % tileset_width) * 16;
	int tile_v = (tile.index / tileset_width) * 16;
	ImVec2 tileset_texture_size(tileset_texture.width, tileset_texture.height);

	ImVec2 uv_min = ImVec2(tile_u,      tile_v)      / tileset_texture_size;
	ImVec2 uv_max = ImVec2(tile_u + 16, tile_v + 16) / tileset_texture_size;

	if (tile.hflip) {
		ImSwap(uv_min.x, uv_max.x);
	}
	if (tile.vflip) {
		ImSwap(uv_min.y, uv_max.y);
	}

	ImVec2 p = texture_pos + ImVec2(tile_x, tile_y) * (16 * view.zoom);
	if (/*tile.index == 0*/false) {
		draw_list->AddRectFilled(p, p + ImVec2(16, 16) * view.zoom, IM_COL32(0, 0, 0, (col >> IM_COL32_A_SHIFT) & 0xff));
	} else {
		draw_list->AddImage(tileset_texture.ID, p, p + ImVec2(16, 16) * view.zoom, uv_min, uv_max, col);
	}
}

static void AddArrow(ImDrawList* draw_list,
					 ImVec2 p1, ImVec2 p2, ImU32 col,
					 float thickness, float zoom) {
	draw_list->AddLine(p1, p2, col, thickness * zoom);

	float dir = point_direction(p1.x, p1.y, p2.x, p2.y);

	ImVec2 p3 = p2;
	p3.x += dcos(dir + 90 + 45) * (2 * zoom);
	p3.y -= dsin(dir + 90 + 45) * (2 * zoom);
	draw_list->AddLine(p2, p3, col, thickness * zoom);

	ImVec2 p4 = p2;
	p4.x += dcos(dir - 90 - 45) * (2 * zoom);
	p4.y -= dsin(dir - 90 - 45) * (2 * zoom);
	draw_list->AddLine(p2, p4, col, thickness * zoom);
}

static bool ButtonActive(const char* label, bool active) {
	if (active) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	}

	ImVec2 size = {};
	bool res = ImGui::Button(label, size);

	if (active) {
		ImGui::PopStyleColor();
	}

	return res;
}

// fix the width for icon buttons
static bool IconButtonActive(const char* label, bool active) {
	if (active) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	}

	ImVec2 size = ImVec2(24, 0);
	bool res = ImGui::Button(label, size);

	if (active) {
		ImGui::PopStyleColor();
	}

	return res;
}

static bool ImageButtonActive(const char* str_id, ImTextureID user_texture_id,
							  const ImVec2& image_size,
							  const ImVec2& uv0, const ImVec2& uv1,
							  bool active) {
	ImVec4 bg_col;

	if (active) {
		bg_col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
	} else {
		bg_col = ImGui::GetStyleColorVec4(ImGuiCol_Button);
	}

	ImVec4 tint_col = {1, 1, 1, 1};

	bool res = ImGui::ImageButton(str_id, user_texture_id,
								  image_size,
								  uv0, uv1,
								  bg_col, tint_col);
	return res;
}

#if 0
template <typename F>
static void walk_tilemap(const Tilemap& tm,
						 int layer_index,
						 const F& f) {
	for (int y = 0; y < tm.height; y++) {
		for (int x = 0; x < tm.width; x++) {
			Tile tile = get_tile(tm, x, y, layer_index);
			ImU32 col = IM_COL32(255, 255, 255, 255);

			f(x, y, tile);
		}
	}
}
#endif

static Texture get_object_texture(ObjType type) {
	switch (type) {
		case OBJ_PLAYER_INIT_POS: return editor.tex_idle;
		case OBJ_LAYER_SET:       return editor.tex_layer_set;
		case OBJ_LAYER_FLIP:      return editor.tex_layer_flip;
	}

	Assert(!"invalid object type");
	return {};
}

static ImVec2 get_object_size(const Object& o) {
	switch (o.type) {
		case OBJ_PLAYER_INIT_POS: return {30, 44};
		case OBJ_LAYER_SET:       return {o.layset.radius.x * 2, o.layset.radius.y * 2};
		case OBJ_LAYER_FLIP:      return {o.layflip.radius.x * 2, o.layflip.radius.y * 2};
	}

	auto t = get_object_texture(o.type);
	return ImVec2(t.width, t.height);
}

static bool IsKeyPressedNoMod(ImGuiKey key) {
	if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) return false;
	if (ImGui::IsKeyDown(ImGuiKey_RightCtrl)) return false;

	if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) return false;
	if (ImGui::IsKeyDown(ImGuiKey_RightShift)) return false;

	if (ImGui::IsKeyDown(ImGuiKey_LeftAlt)) return false;
	if (ImGui::IsKeyDown(ImGuiKey_RightAlt)) return false;

	return ImGui::IsKeyPressed(key, false);
}

static void draw_objects(ImDrawList* draw_list,
						 array<Object> objects,
						 ImVec2 tilemap_pos,
						 const View& view) {
	For (it, objects) {
		Texture t = get_object_texture(it->type);

		int w = t.width;
		int h = t.height;

		ImU32 col = IM_COL32_WHITE;

		switch (it->type) {
			case OBJ_LAYER_SET: {
				w = it->layset.radius.x * 2;
				h = it->layset.radius.y * 2;
				if (it->layset.layer == 0) {
					col = IM_COL32(128, 128, 255, 255);
				} else {
					col = IM_COL32(255, 128, 128, 255);
				}
				break;
			}
			case OBJ_LAYER_FLIP: {
				w = it->layflip.radius.x * 2;
				h = it->layflip.radius.y * 2;
				break;
			}
		}

		ImVec2 p = tilemap_pos + ImVec2(it->pos.x - w / 2, it->pos.y - h / 2) * view.zoom;
		ImVec2 p2 = p + ImVec2(w, h) * view.zoom;

		draw_list->AddImage(t.ID, p, p2, {0, 0}, {1, 1}, col);
	}
}

void Editor::clear_state() {
	free_texture(&tileset_texture);

	free_tilemap(&tm);
	free_tileset(&ts);

	free(brush.data);
	brush = {};
	brush_size = {};

	free(objects.data);
	objects = {};

	mode = {};
	hmode = {};
	heightmap_view   = {};
	tilemap_view     = {};
	tile_select_view = {};
	tilemap_tile_selection = {};
	tool = {};
	htool = {};
	tilemap_select_tool_selection = {};
	tilemap_rect_tool_selection = {};
	show_tile_indices = false;
	show_collision = false;
	layer_index = 0;
	selected_object = -1;
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

		current_level_dir = std::filesystem::u8path(path);
		is_level_open = true;

		update_window_caption();

		load_texture_from_file(&tileset_texture, (current_level_dir / "Tileset.png").u8string().c_str());
		load_surface_from_file(&tileset_surface, (current_level_dir / "Tileset.png").u8string().c_str());

		read_tilemap(&tm, (current_level_dir / "Tilemap.bin").u8string().c_str());
		read_tileset(&ts, (current_level_dir / "Tileset.bin").u8string().c_str());

		objects = malloc_bump_array<Object>(MAX_OBJECTS);
		read_objects(&objects, (current_level_dir / "Objects.bin").u8string().c_str());

		gen_heightmap_texture(&heightmap, ts, tileset_texture);
		gen_widthmap_texture(&widthmap, ts, tileset_texture);

		heightmap_view.scrolling = (io.DisplaySize - ImVec2(tileset_texture.width, tileset_texture.height)) / 2.0f;
	};

	auto try_save_level = [&]() {
		if (!is_level_open) return;

		write_tilemap(tm, (current_level_dir / "Tilemap.bin").u8string().c_str());
		write_tileset(ts, (current_level_dir / "Tileset.bin").u8string().c_str());

		write_objects(objects, (current_level_dir / "Objects.bin").u8string().c_str());
	};

	auto try_run_game = [&]() {
		if (!is_level_open) {
			return false;
		}

#if 1
		// SDL overrides main and converts command line args to utf8, so process_name is utf8
		auto str = current_level_dir.u8string();
		const char *command_line[] = {process_name, "--game", str.c_str(), NULL};

		// @Utf8
		// it seems like this library doesn't convert from utf8 to windows wide char
		subprocess_s subprocess;
		int result = subprocess_create(command_line, subprocess_option_inherit_environment, &subprocess);

		if (result != 0) {
			return false;
		}
#else
		// @Utf8
		static char buf[512];
		stb_snprintf(buf, sizeof buf, "%s --game %s", process_name, current_level_dir.string().c_str());
		system(buf);
#endif

		return true;
	};

	if (ImGui::IsKeyPressed(ImGuiKey_N, false) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
		try_open_cnl_popup();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_O, false) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
		try_open_level();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_S, false) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
		try_save_level();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
		try_run_game();
	}

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Level", "Ctrl+N")) {
				try_open_cnl_popup();
			}

			if (ImGui::MenuItem("Open Level...", "Ctrl+O")) {
				try_open_level();
			}

			if (ImGui::MenuItem("Save Level", "Ctrl+S", nullptr, is_level_open)) {
				try_save_level();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Run")) {
			if (ImGui::MenuItem("Run", "F5")) {
				try_run_game();
			}

			ImGui::EndMenu();
		}

		switch (mode) {
			case MODE_HEIGHTMAP: {
				if (hmode == HMODE_ANGLES) {
					if (ImGui::BeginMenu("Angle Editing")) {
						if (ImGui::MenuItem("Fill whole tileset with angle -1")) {
							for (int i = 0; i < ts.count; i++) {
								ts.angles[i] = -1;
							}
						}

						ImGui::EndMenu();
					}
				}
				break;
			}

			case MODE_TILEMAP: {
				if (ImGui::BeginMenu("View")) {
					if (ImGui::MenuItem("Show Tile Indices", nullptr, show_tile_indices)) {
						show_tile_indices ^= true;
					}
					if (ImGui::MenuItem("Show Collision", nullptr, show_collision)) {
						show_collision ^= true;
					}

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
			if (ImGui::MenuItem("Objects", nullptr, mode == MODE_OBJECTS)) mode = MODE_OBJECTS;

			main_menu_bar_width = ImGui::GetCursorPos().x - cursor.x;
		}

		ImGui::EndMainMenuBar();
	}

	if (IsKeyPressedNoMod(ImGuiKey_F1)) {
		show_demo_window ^= true;
	}
	if (show_demo_window) {
		ImGui::ShowDemoWindow(&show_demo_window);
	}

	switch (mode) {
		case MODE_HEIGHTMAP: {
			ImVec2 main_window_pos = {};

			auto heightmap_editor_window = [&]() {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
				defer { ImGui::PopStyleVar(); };

				ImGui::Begin("Heightmap Editor##heightmap_editor");
				defer { ImGui::End(); };

				main_window_pos = ImGui::GetWindowPos();

				if (!is_level_open) {
					{
						const char* text = "No level loaded.\n";
						ImVec2 text_size = ImGui::CalcTextSize(text);
						ImVec2 avail = ImGui::GetContentRegionAvail();
						ImGui::SetCursorPos(ImGui::GetCursorPos() + (avail - text_size) / 2.0f);
						ImGui::TextUnformatted(text);
					}

					{
						ImGui::Spacing();

						const char* text = "Ctrl + N - Create New Level\n"
							"Ctrl + O - Open Level";
						ImVec2 text_size = ImGui::CalcTextSize(text);
						ImVec2 avail = ImGui::GetContentRegionAvail();
						ImGui::SetCursorPosX((ImGui::GetCursorPos() + (avail - text_size) / 2.0f).x);
						ImGui::TextUnformatted(text);
					}
					return;
				}

				if (tileset_texture.ID == 0) {
					const char* text = "No tileset loaded.";
					ImVec2 text_size = ImGui::CalcTextSize(text);
					ImVec2 avail = ImGui::GetContentRegionAvail();
					ImGui::SetCursorPos((avail - text_size) / 2.0f);
					ImGui::TextUnformatted(text);
					return;
				}

				pan_and_zoom(heightmap_view);

				ImVec2 texture_size(tileset_texture.width * heightmap_view.zoom, tileset_texture.height * heightmap_view.zoom);
				ImVec2 texture_pos = ImGui::GetCursorScreenPos() + heightmap_view.scrolling;

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				// draw background
				{
					ImU32 col = ImGui::ColorConvertFloat4ToU32(*(ImVec4*)&color_cornflower_blue);
					draw_list->AddRectFilled(texture_pos, texture_pos + texture_size, col);
				}

				if (/*heightmap_show_collision*/true) {
					// item = invisible button from pan_and_zoom
					if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
						if (pos_in_rect(ImGui::GetMousePos(), texture_pos, texture_size)) {
							ImVec2 pos = get_mouse_pos_in_view(heightmap_view);

							int tile_x = clamp((int)(pos.x / 16), 0, (tileset_texture.width  / 16) - 1);
							int tile_y = clamp((int)(pos.y / 16), 0, (tileset_texture.height / 16) - 1);

							if (htool == HTOOL_AUTO) {
								if (hmode == HMODE_WIDTHS) {
									auto widths = get_tile_widths(ts, tile_x + tile_y * (tileset_texture.width / 16));
									for (int y = 0; y < 16; y++) {
										int width = 0;

										int pixel_x = tile_x * 16 + 15;
										int pixel_y = tile_y * 16 + y;
										vec4 pixel = surface_get_pixel(tileset_surface, pixel_x, pixel_y);
										if (pixel.a == 0) {
											// flipped height
											for (int x = 0; x < 16; x++) {
												int pixel_x = tile_x * 16 + x;
												int pixel_y = tile_y * 16 + y;
												vec4 pixel = surface_get_pixel(tileset_surface, pixel_x, pixel_y);
												if (pixel.a != 0) {
													width = 0xFF - x;
												} else {
													break;
												}
											}
										} else {
											// normal height
											for (int x = 15; x >= 0; x--) {
												int pixel_x = tile_x * 16 + x;
												int pixel_y = tile_y * 16 + y;
												vec4 pixel = surface_get_pixel(tileset_surface, pixel_x, pixel_y);
												if (pixel.a != 0) {
													width = 16 - x;
												} else {
													break;
												}
											}
										}

										widths[y] = width;
									}
									gen_widthmap_texture(&widthmap, ts, tileset_texture);
								} else if (hmode == HMODE_HEIGHTS) {
									auto heights = get_tile_heights(ts, tile_x + tile_y * (tileset_texture.width / 16));
									for (int x = 0; x < 16; x++) {
										int height = 0;

										int pixel_x = tile_x * 16 + x;
										int pixel_y = tile_y * 16 + 15;
										vec4 pixel = surface_get_pixel(tileset_surface, pixel_x, pixel_y);
										if (pixel.a == 0) {
											// flipped height
											for (int y = 0; y < 16; y++) {
												int pixel_x = tile_x * 16 + x;
												int pixel_y = tile_y * 16 + y;
												vec4 pixel = surface_get_pixel(tileset_surface, pixel_x, pixel_y);
												if (pixel.a != 0) {
													height = 0xFF - y;
												} else {
													break;
												}
											}
										} else {
											// normal height
											for (int y = 15; y >= 0; y--) {
												int pixel_x = tile_x * 16 + x;
												int pixel_y = tile_y * 16 + y;
												vec4 pixel = surface_get_pixel(tileset_surface, pixel_x, pixel_y);
												if (pixel.a != 0) {
													height = 16 - y;
												} else {
													break;
												}
											}
										}

										heights[x] = height;
									}
									gen_heightmap_texture(&heightmap, ts, tileset_texture);
								}
							} else {
								int x = clamp((int)pos.x, 0, tileset_texture.width  - 1);
								int y = clamp((int)pos.y, 0, tileset_texture.height - 1);

								if (hmode == HMODE_WIDTHS) {
									auto widths = get_tile_widths(ts, tile_x + tile_y * (tileset_texture.width / 16));
									if (htool == HTOOL_BRUSH) {
										widths[y % 16] = 16 - (x % 16);
									} else if (htool == HTOOL_ERASER) {
										widths[y % 16] = 0;
									}

									gen_widthmap_texture(&widthmap, ts, tileset_texture);
								} else if (hmode == HMODE_HEIGHTS) {
									auto heights = get_tile_heights(ts, tile_x + tile_y * (tileset_texture.width / 16));
									if (htool == HTOOL_BRUSH) {
										heights[x % 16] = 16 - (y % 16);
									} else if (htool == HTOOL_ERASER) {
										heights[x % 16] = 0;
									}

									gen_heightmap_texture(&heightmap, ts, tileset_texture);
								}
							}
						}
					}

					if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
						if (pos_in_rect(ImGui::GetMousePos(), texture_pos, texture_size)) {
							ImVec2 pos = get_mouse_pos_in_view(heightmap_view);

							int tile_x = clamp((int)(pos.x / 16), 0, (tileset_texture.width  / 16) - 1);
							int tile_y = clamp((int)(pos.y / 16), 0, (tileset_texture.height / 16) - 1);

							int x = clamp((int)pos.x, 0, tileset_texture.width  - 1);
							int y = clamp((int)pos.y, 0, tileset_texture.height - 1);

							if (hmode == HMODE_WIDTHS) {
								auto widths = get_tile_widths(ts, tile_x + tile_y * (tileset_texture.width / 16));
								if (htool == HTOOL_BRUSH) {
									widths[y % 16] = 0xF0 + (15 - (x % 16));
								}

								gen_widthmap_texture(&widthmap, ts, tileset_texture);
							} else if (hmode == HMODE_HEIGHTS) {
								auto heights = get_tile_heights(ts, tile_x + tile_y * (tileset_texture.width / 16));
								if (htool == HTOOL_BRUSH) {
									heights[x % 16] = 0xF0 + (15 - (y % 16));
								}

								gen_heightmap_texture(&heightmap, ts, tileset_texture);
							}
						}
					}
				}

				{
					ImU32 col = IM_COL32(150, 150, 150, 255);
					draw_list->AddImage(tileset_texture.ID, texture_pos, texture_pos + texture_size, {0, 0}, {1, 1}, col);
				}

				if (hmode == HMODE_HEIGHTS || hmode == HMODE_ANGLES) {
					ImU32 col = IM_COL32(255, 255, 255, 150);
					draw_list->AddImage(heightmap.ID, texture_pos, texture_pos + texture_size, {0, 0}, {1, 1}, col);
				}
				if (hmode == HMODE_WIDTHS) {
					draw_list->AddImage(widthmap.ID, texture_pos, texture_pos + texture_size, {0, 0}, {1, 1}, IM_COL32(255, 255, 255, 128));
				}
				if (hmode == HMODE_ANGLES) {
					for (int tile_index = 0; tile_index < ts.count; tile_index++) {
						float angle = ts.angles[tile_index];
						if (angle == -1) {
							continue;
						}

						int tile_x = tile_index % (tileset_texture.width / 16);
						int tile_y = tile_index / (tileset_texture.width / 16);
						ImVec2 p1 = texture_pos + ImVec2(tile_x * 16 + 8, tile_y * 16 + 8) * heightmap_view.zoom;
						ImVec2 p2 = p1;
						p2.x += dcos(angle) * 8 * heightmap_view.zoom;
						p2.y -= dsin(angle) * 8 * heightmap_view.zoom;
						AddArrow(draw_list, p1, p2, IM_COL32_WHITE, 0.5, heightmap_view.zoom);
					}
				}

				// draw grids
				{
					draw_list->AddRect(texture_pos, texture_pos + texture_size, IM_COL32(255, 255, 255, 255));

					if (heightmap_view.zoom > 2) {
						AddGrid(draw_list,
								texture_pos,
								ImVec2(16 * heightmap_view.zoom, 16 * heightmap_view.zoom),
								{tileset_texture.width / 16, tileset_texture.height / 16},
								IM_COL32(255, 255, 255, 110));
					}
				}

				// drag an arrow
				if (hmode == HMODE_ANGLES) {
					static bool dragging;
					static ImVec2 start_pos;

					if (ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
						if (pos_in_rect(ImGui::GetMousePos(), texture_pos, texture_size)) {
							ImVec2 pos = get_mouse_pos_in_view(heightmap_view);

							dragging = true;
							start_pos = pos;
						}
					}

					if (dragging) {
						ImVec2 pos = get_mouse_pos_in_view(heightmap_view);

						// highlight selected tile
						{
							int tile_x = clamp((int)(start_pos.x / 16), 0, tileset_texture.width  / 16 - 1);
							int tile_y = clamp((int)(start_pos.y / 16), 0, tileset_texture.height / 16 - 1);
							ImVec2 p = texture_pos + ImVec2(tile_x * 16, tile_y * 16) * heightmap_view.zoom;
							ImVec2 p2 = p + ImVec2(16, 16) * heightmap_view.zoom;
							draw_list->AddRect(p, p2, IM_COL32(160, 160, 255, 220), 0, 0, 0.75 * heightmap_view.zoom);
						}

						AddArrow(draw_list,
								 texture_pos + start_pos * heightmap_view.zoom,
								 texture_pos + pos * heightmap_view.zoom,
								 IM_COL32_WHITE, 0.5, heightmap_view.zoom);

						if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
							dragging = false;
							if (point_distance(start_pos.x, start_pos.y, pos.x, pos.y) > 1) {
								int tile_x = clamp((int)(start_pos.x / 16), 0, tileset_texture.width  / 16 - 1);
								int tile_y = clamp((int)(start_pos.y / 16), 0, tileset_texture.height / 16 - 1);
								int tile_index = tile_x + tile_y * (tileset_texture.width / 16);
								float angle = point_direction(start_pos.x, start_pos.y, pos.x, pos.y);
								ts.angles[tile_index] = angle;
							}
						}
					}

					if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
						dragging = false;
						if (pos_in_rect(ImGui::GetMousePos(), texture_pos, texture_size)) {
							ImVec2 pos = get_mouse_pos_in_view(heightmap_view);
							int tile_x = clamp((int)(pos.x / 16), 0, tileset_texture.width  / 16 - 1);
							int tile_y = clamp((int)(pos.y / 16), 0, tileset_texture.height / 16 - 1);
							int tile_index = tile_x + tile_y * (tileset_texture.width / 16);
							ts.angles[tile_index] = -1;

							// highlight
							{
								ImVec2 p = texture_pos + ImVec2(tile_x * 16, tile_y * 16) * heightmap_view.zoom;
								ImVec2 p2 = p + ImVec2(16, 16) * heightmap_view.zoom;
								draw_list->AddRect(p, p2, IM_COL32(255, 160, 160, 220), 0, 0, 0.75 * heightmap_view.zoom);
							}
						}
					}
				}
			};

			heightmap_editor_window();

			auto tool_select_window = [&]() {
				ImGui::SetNextWindowPos(ImVec2(main_window_pos.x, io.DisplaySize.y / 2.0f), ImGuiCond_Always, ImVec2(0, 0.5f));

				ImGui::Begin("Tool select##heightmap_editor", nullptr,
							 ImGuiWindowFlags_AlwaysAutoResize
							 | ImGuiWindowFlags_NoDecoration
							 | ImGuiWindowFlags_NoMove
							 | ImGuiWindowFlags_NoSavedSettings);
				defer { ImGui::End(); };

				if (IconButtonActive(ICON_FA_PEN, htool == HTOOL_BRUSH)) htool = HTOOL_BRUSH;
				if (IsKeyPressedNoMod(ImGuiKey_B)) htool = HTOOL_BRUSH;
				ImGui::SetItemTooltip("Brush\nShortcut: B");

				if (IconButtonActive(ICON_FA_ERASER, htool == HTOOL_ERASER)) htool = HTOOL_ERASER;
				if (IsKeyPressedNoMod(ImGuiKey_E)) htool = HTOOL_ERASER;
				ImGui::SetItemTooltip("Eraser\nShortcut: E");

				if (IconButtonActive("A", htool == HTOOL_AUTO)) htool = HTOOL_AUTO;
				if (IsKeyPressedNoMod(ImGuiKey_A)) htool = HTOOL_AUTO;
				ImGui::SetItemTooltip("Auto-Tool\nShortcut: A");
			};

			tool_select_window();

			auto hmode_select_window = [&]() {
				ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2.0f, main_window_pos.y), ImGuiCond_Always, ImVec2(0.5f, 0));

				ImGui::Begin("##heightmap_editor - hmode_select_window", nullptr,
							 ImGuiWindowFlags_AlwaysAutoResize
							 | ImGuiWindowFlags_NoDecoration
							 | ImGuiWindowFlags_NoMove
							 | ImGuiWindowFlags_NoSavedSettings);
				defer { ImGui::End(); };

				if (ButtonActive("Heights", hmode == HMODE_HEIGHTS)) hmode = HMODE_HEIGHTS;

				ImGui::SameLine();
				if (ButtonActive("Widths", hmode == HMODE_WIDTHS)) hmode = HMODE_WIDTHS;

				ImGui::SameLine();
				if (ButtonActive("Angles", hmode == HMODE_ANGLES)) hmode = HMODE_ANGLES;
			};

			hmode_select_window();
			break;
		}

		case MODE_TILEMAP: {
			ImVec2 main_window_pos = {};

			auto tilemap_editor_window = [&]() {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
				defer { ImGui::PopStyleVar(); };

				ImGui::Begin("Tilemap Editor##tilemap_editor");
				defer { ImGui::End(); };

				main_window_pos = ImGui::GetWindowPos();

				if (!is_level_open) {
					const char* text = "No level loaded.";
					ImVec2 text_size = ImGui::CalcTextSize(text);
					ImVec2 avail = ImGui::GetContentRegionAvail();
					ImGui::SetCursorPos((avail - text_size) / 2.0f);
					ImGui::TextUnformatted(text);
					return;
				}

				pan_and_zoom(tilemap_view);
				bool is_item_active = ImGui::IsItemActive();

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				ImVec2 tilemap_pos = ImGui::GetCursorScreenPos() + tilemap_view.scrolling;
				ImVec2 tilemap_size(tm.width * 16 * tilemap_view.zoom, tm.height * 16 * tilemap_view.zoom);

				// draw_list->PushClipRect(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImGui::GetContentRegionAvail(), true);
				// defer { draw_list->PopClipRect(); };

				// tilemap background
				{
					ImU32 col = ImGui::ColorConvertFloat4ToU32(*(ImVec4*)&color_cornflower_blue);
					draw_list->AddRectFilled(tilemap_pos, tilemap_pos + tilemap_size, col);
				}

				bool is_drawing = false;
				bool is_erasing = false;

				if (tool == TOOL_BRUSH) {
					// item = invisible button from pan_and_zoom
					if (ImGui::IsItemActive()) {
						if (pos_in_rect(ImGui::GetMousePos(), tilemap_pos, tilemap_size)) {
							ImVec2 pos = get_mouse_pos_in_view(tilemap_view);

							int tile_x = clamp((int)(pos.x / 16), 0, tm.width  - 1);
							int tile_y = clamp((int)(pos.y / 16), 0, tm.height - 1);

							if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
								for (int y = 0; y < brush_size.y; y++) {
									for (int x = 0; x < brush_size.x; x++) {
										if ((tile_x + x) >= 0 && (tile_x + x) < tm.width
											&& (tile_y + y) >= 0 && (tile_y + y) < tm.height)
										{
											Tile oldtile = get_tile(tm, tile_x + x, tile_y + y, layer_index);

											Tile tile = brush[x + y * brush_size.x];
											// keep solidity
											//tile.top_solid = oldtile.top_solid;
											//tile.lrb_solid = oldtile.lrb_solid;

											set_tile(&tm, tile_x + x, tile_y + y, layer_index, tile);
										}
									}
								}
								is_drawing = true;
							} else if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
								set_tile(&tm, tile_x, tile_y, layer_index, {});
								is_erasing = true;

								//ImVec2 p = tilemap_pos + ImVec2(tile_x * 16, tile_y * 16) * tilemap_view.zoom;
								//ImVec2 p2 = p + ImVec2(16, 16) * tilemap_view.zoom;
								//draw_list->AddRect(p, p2, IM_COL32(255, 0, 0, 255), 0, 0, tilemap_view.zoom);
							}
						}
					}
				}

				if (tool == TOOL_TOP_SOLID_BRUSH) {
					if (ImGui::IsItemActive()) {
						if (pos_in_rect(ImGui::GetMousePos(), tilemap_pos, tilemap_size)) {
							ImVec2 pos = get_mouse_pos_in_view(tilemap_view);

							int tile_x = clamp((int)(pos.x / 16), 0, tm.width  - 1);
							int tile_y = clamp((int)(pos.y / 16), 0, tm.height - 1);

							if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
								Tile tile = get_tile(tm, tile_x, tile_y, layer_index);
								tile.top_solid = 1;
								set_tile(&tm, tile_x, tile_y, layer_index, tile);
							} else if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
								Tile tile = get_tile(tm, tile_x, tile_y, layer_index);
								tile.top_solid = 0;
								set_tile(&tm, tile_x, tile_y, layer_index, tile);
							}
						}
					}
				}

				if (tool == TOOL_LRB_SOLID_BRUSH) {
					if (ImGui::IsItemActive()) {
						if (pos_in_rect(ImGui::GetMousePos(), tilemap_pos, tilemap_size)) {
							ImVec2 pos = get_mouse_pos_in_view(tilemap_view);

							int tile_x = clamp((int)(pos.x / 16), 0, tm.width  - 1);
							int tile_y = clamp((int)(pos.y / 16), 0, tm.height - 1);

							if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
								Tile tile = get_tile(tm, tile_x, tile_y, layer_index);
								tile.lrb_solid = 1;
								set_tile(&tm, tile_x, tile_y, layer_index, tile);
							} else if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
								Tile tile = get_tile(tm, tile_x, tile_y, layer_index);
								tile.lrb_solid = 0;
								set_tile(&tm, tile_x, tile_y, layer_index, tile);
							}
						}
					}
				}

				if (tool == TOOL_SELECT) {
					rectangle_select(tilemap_select_tool_selection, is_item_active,
									 tilemap_pos, tilemap_size, tilemap_view, tm.width, tm.height);

					if (ImGui::IsKeyPressed(ImGuiKey_C, false) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
						if (tilemap_select_tool_selection.w > 0 && tilemap_select_tool_selection.h > 0) {
							free(brush.data);
							brush = {};
							brush_size = {tilemap_select_tool_selection.w, tilemap_select_tool_selection.h};

							brush = calloc_array<Tile>(brush_size.x * brush_size.y);

							int i = 0;
							for (int y = 0; y < tilemap_select_tool_selection.h; y++) {
								for (int x = 0; x < tilemap_select_tool_selection.w; x++) {
									int xx = tilemap_select_tool_selection.x + x;
									int yy = tilemap_select_tool_selection.y + y;
									brush[i] = get_tile(tm, xx, yy, layer_index);
									i++;
								}
							}

							tilemap_tile_selection = {};
							tool = TOOL_BRUSH;
						}
					}
				}

				if (tool == TOOL_RECT) {
					if (brush_size.x == 1 && brush_size.y == 1) {
						bool result = rectangle_select(tilemap_rect_tool_selection, is_item_active,
													   tilemap_pos, tilemap_size, tilemap_view, tm.width, tm.height);
						if (result) {
							auto sel = tilemap_rect_tool_selection;
							for (int y = sel.y; y < sel.y + sel.h; y++) {
								for (int x = sel.x; x < sel.x + sel.w; x++) {
									Tile tile = brush[0];
									set_tile(&tm, x, y, layer_index, tile);
								}
							}
						}
					}
				}

				int mouse_tile_x = -1;
				int mouse_tile_y = -1;
				if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
					if (pos_in_rect(ImGui::GetMousePos(), tilemap_pos, tilemap_size)) {
						ImVec2 pos = get_mouse_pos_in_view(tilemap_view);

						mouse_tile_x = clamp((int)(pos.x / 16), 0, tm.width  - 1);
						mouse_tile_y = clamp((int)(pos.y / 16), 0, tm.height - 1);
					}
				}

				// draw tilemap
				for (int y = 0; y < tm.height; y++) {
					for (int x = 0; x < tm.width; x++) {
						Tile tile = get_tile(tm, x, y, layer_index);
						ImU32 col = IM_COL32(255, 255, 255, 255);

						// mouse hover
						if (tool == TOOL_BRUSH || tool == TOOL_RECT) {
							if (mouse_tile_x != -1 && mouse_tile_y != -1) {
								if (!is_erasing) {
									if (x >= mouse_tile_x && x < mouse_tile_x + brush_size.x
										&& y >= mouse_tile_y && y < mouse_tile_y + brush_size.y)
									{
										int xx = x - mouse_tile_x;
										int yy = y - mouse_tile_y;

										tile = brush[xx + yy * brush_size.x];

										if (!is_drawing) col = IM_COL32(255, 255, 255, 180);
									}
								}
							}
						}

						if (tool == TOOL_RECT) {
							auto sel = tilemap_rect_tool_selection;
							if (sel.dragging) {
								if (brush_size.x == 1 && brush_size.y == 1) {
									if (x >= sel.x && x < sel.x + sel.w && y >= sel.y && y < sel.y + sel.h) {
										tile = brush[0];
									}
								}
							}
						}

						if (tool == TOOL_TOP_SOLID_BRUSH || tool == TOOL_LRB_SOLID_BRUSH) {
							col = IM_COL32(150, 150, 150, 255);
						}

						if (tile.index != 0) {
							AddTile(draw_list, tile, tilemap_pos, tilemap_size,
									tilemap_view, x, y, tileset_texture, col);
						}

						// draw collision
						{
							bool draw_collision = false;

							if (tool == TOOL_TOP_SOLID_BRUSH) {
								if (tile.top_solid) {
									draw_collision = true;
								}
							} else if (tool == TOOL_LRB_SOLID_BRUSH) {
								if (tile.lrb_solid) {
									draw_collision = true;
								}
							} else if (show_collision) {
								if (tile.top_solid || tile.lrb_solid) {
									draw_collision = true;
								}
							}

							if (draw_collision) {
								AddTile(draw_list, tile, tilemap_pos, tilemap_size,
										tilemap_view, x, y, heightmap, IM_COL32_WHITE);

								ImVec2 p = tilemap_pos + ImVec2(x * 16, y * 16) * tilemap_view.zoom;
								ImVec2 p2 = p + ImVec2(16, 16) * tilemap_view.zoom;
								draw_list->AddRect(p, p2, IM_COL32(255, 0, 0, 255), 0, 0, 0.5 * tilemap_view.zoom);
							}
						}

						// draw tile index
						if (show_tile_indices) {
							if (tile.index != 0) {
								char buf[10];
								stb_snprintf(buf, sizeof buf, "%u", tile.index);
								ImVec2 p = tilemap_pos + ImVec2(x * 16, y * 16) * tilemap_view.zoom;

								//ImGui::SetWindowFontScale(tilemap_view.zoom);
								draw_list->AddText(p, IM_COL32_WHITE, buf);
								//ImGui::SetWindowFontScale(1);
							}
						}
					}
				}

				draw_objects(draw_list, objects, tilemap_pos, tilemap_view);

				// draw selection
				if (tool == TOOL_SELECT) {
					if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
						if (pos_in_rect(ImGui::GetMousePos(), tilemap_pos, tilemap_size)) {
							ImVec2 pos = get_mouse_pos_in_view(tilemap_view);

							int tile_x = clamp((int)(pos.x / 16), 0, tm.width  - 1);
							int tile_y = clamp((int)(pos.y / 16), 0, tm.height - 1);

							ImVec2 p = tilemap_pos + ImVec2(tile_x, tile_y) * 16.0f * tilemap_view.zoom;
							draw_list->AddRectFilled(p, p + ImVec2(16, 16) * tilemap_view.zoom, IM_COL32(100, 100, 255, 100));
						}
					}

					if (tilemap_select_tool_selection.w > 0) {
						Assert(tilemap_select_tool_selection.h > 0);

						ImVec2 p = tilemap_pos + ImVec2(tilemap_select_tool_selection.x, tilemap_select_tool_selection.y) * 16.0f * tilemap_view.zoom;
						draw_list->AddRectFilled(p, p + ImVec2(tilemap_select_tool_selection.w, tilemap_select_tool_selection.h) * 16.0f * tilemap_view.zoom, IM_COL32(100, 100, 255, 100));
						draw_list->AddRect(p, p + ImVec2(tilemap_select_tool_selection.w, tilemap_select_tool_selection.h) * 16.0f * tilemap_view.zoom, IM_COL32(255, 255, 255, 255), 0, 0, tilemap_view.zoom);
					}
				}

				// draw grids
				{
					draw_list->AddRect(tilemap_pos, tilemap_pos + tilemap_size, IM_COL32(255, 255, 255, 255));

					if (tilemap_view.zoom > 2) {
						AddGrid(draw_list, tilemap_pos, ImVec2(16, 16) * tilemap_view.zoom, {tm.width, tm.height}, IM_COL32(255, 255, 255, 110));
					}

					AddGrid(draw_list, tilemap_pos, ImVec2(256, 256) * tilemap_view.zoom, {tm.width / 16, tm.height / 16});
				}
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

				int tileset_width  = tileset_texture.width  / 16;
				int tileset_height = tileset_texture.height / 16;

				rectangle_select(tilemap_tile_selection, ImGui::IsItemActive(),
								 texture_pos, texture_size,
								 tile_select_view,
								 tileset_width, tileset_height);
				if (ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
					free(brush.data);
					brush = {};
					brush_size = {tilemap_tile_selection.w, tilemap_tile_selection.h};

					if (brush_size.x > 0 && brush_size.y > 0) {
						// copy the tiles
						brush = calloc_array<Tile>(brush_size.x * brush_size.y);
						int i = 0;
						for (int y = 0; y < tilemap_tile_selection.h; y++) {
							for (int x = 0; x < tilemap_tile_selection.w; x++) {
								int xx = tilemap_tile_selection.x + x;
								int yy = tilemap_tile_selection.y + y;
								Tile tile = {};
								tile.index = xx + yy * (tileset_texture.width / 16);
								brush[i] = tile;
								i++;
							}
						}
					}
				}

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				// background
				{
					ImU32 col = ImGui::ColorConvertFloat4ToU32(*(ImVec4*)&color_cornflower_blue);
					draw_list->AddRectFilled(texture_pos, texture_pos + texture_size, col);
				}

				draw_list->AddImage(tileset_texture.ID, texture_pos, texture_pos + texture_size);

				if (tilemap_tile_selection.w > 0 && tilemap_tile_selection.h > 0) {
					ImVec2 p = texture_pos + ImVec2(tilemap_tile_selection.x * 16, tilemap_tile_selection.y * 16) * tile_select_view.zoom;
					ImVec2 p2 = p + ImVec2(tilemap_tile_selection.w * 16, tilemap_tile_selection.h * 16) * tile_select_view.zoom;
					draw_list->AddRect(p, p2, IM_COL32_WHITE, 0, 0, 1 * tile_select_view.zoom);
				}
			};

			tile_select_window();

			auto tool_select_window = [&]() {
				ImGui::SetNextWindowPos(ImVec2(main_window_pos.x, io.DisplaySize.y / 2.0f), ImGuiCond_Always, ImVec2(0, 0.5f));

				ImGui::Begin("Tool select##tilemap_editor", nullptr,
							 ImGuiWindowFlags_AlwaysAutoResize
							 | ImGuiWindowFlags_NoDecoration
							 | ImGuiWindowFlags_NoMove
							 | ImGuiWindowFlags_NoSavedSettings);
				defer { ImGui::End(); };

				if (IconButtonActive(ICON_FA_PEN, tool == TOOL_BRUSH)) tool = TOOL_BRUSH;
				if (IsKeyPressedNoMod(ImGuiKey_B)) tool = TOOL_BRUSH;
				ImGui::SetItemTooltip("Brush\nShortcut: B");

				if (IconButtonActive(ICON_FA_SQUARE_FULL, tool == TOOL_RECT)) {
					tool = TOOL_RECT;
					tilemap_rect_tool_selection = {};
				}
				if (IsKeyPressedNoMod(ImGuiKey_R)) {
					tool = TOOL_RECT;
					tilemap_rect_tool_selection = {};
				}
				ImGui::SetItemTooltip("Rectangle\nShortcut: R");

				if (IconButtonActive(ICON_FA_OBJECT_GROUP, tool == TOOL_SELECT)) {
					tool = TOOL_SELECT;
					tilemap_select_tool_selection = {};
				}
				if (IsKeyPressedNoMod(ImGuiKey_S)) {
					tool = TOOL_SELECT;
					tilemap_select_tool_selection = {};
				}
				ImGui::SetItemTooltip("Select\nShortcut: S");

				if (IconButtonActive(ICON_FA_PEN "##2", tool == TOOL_TOP_SOLID_BRUSH)) tool = TOOL_TOP_SOLID_BRUSH;
				ImGui::SetItemTooltip("Top Solid Brush");

				if (IconButtonActive(ICON_FA_PEN "##3", tool == TOOL_LRB_SOLID_BRUSH)) tool = TOOL_LRB_SOLID_BRUSH;
				ImGui::SetItemTooltip("Left-Right-Bottom Solid Brush");
			};

			tool_select_window();

			auto top_tool_bar_window = [&]() {
				// ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
				// defer { ImGui::PopStyleVar(); };

				ImGui::Begin("Top Toolbar##top_tool_bar_window");
				defer { ImGui::End(); };

				if (brush_size.x == 1 && brush_size.y == 1) {
					if (IconButtonActive("H", brush[0].hflip)) brush[0].hflip ^= 1;
					if (IsKeyPressedNoMod(ImGuiKey_H)) brush[0].hflip ^= 1;
					ImGui::SetItemTooltip("Horizontal flip\nShortcut: H");

					ImGui::SameLine();
					if (IconButtonActive("V", brush[0].vflip)) brush[0].vflip ^= 1;
					if (IsKeyPressedNoMod(ImGuiKey_V)) brush[0].vflip ^= 1;
					ImGui::SetItemTooltip("Vertical flip\nShortcut: V");

					ImGui::SameLine();
					if (IconButtonActive("T", brush[0].top_solid)) brush[0].top_solid ^= 1;
					ImGui::SetItemTooltip("Top Solid");

					ImGui::SameLine();
					if (ButtonActive("LRB", brush[0].lrb_solid)) brush[0].lrb_solid ^= 1;
					ImGui::SetItemTooltip("Left-Right-Bottom Solid");
				}
			};

			top_tool_bar_window();

			auto layer_select_window = [&]() {
				ImGui::Begin("Layers##tilemap_editor");
				defer { ImGui::End(); };

				const char* labels[] = {"Layer A", "Layer B"};

				for (int i = ArrayLength(labels) - 1; i >= 0; i--) {
					if (ImGui::Selectable(labels[i], layer_index == i)) layer_index = i;
				}
			};

			layer_select_window();
			break;
		}

		case MODE_OBJECTS: {
			auto main_window = [&]() {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
				defer { ImGui::PopStyleVar(); };

				ImGui::Begin("Object Editor##object_editor");
				defer { ImGui::End(); };

				if (!is_level_open) {
					return;
				}

				pan_and_zoom(tilemap_view);
				bool is_item_active = ImGui::IsItemActive();

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				ImVec2 tilemap_pos = ImGui::GetCursorScreenPos() + tilemap_view.scrolling;
				ImVec2 tilemap_size(tm.width * 16 * tilemap_view.zoom, tm.height * 16 * tilemap_view.zoom);

				// add objects
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ADD_OBJECT")) {
						ObjType type;
						Assert(payload->DataSize == sizeof type);
						type = *(ObjType*)payload->Data;

						ImVec2 pos = get_mouse_pos_in_view(tilemap_view);
						pos = ImFloor(pos);

						Object o = {};
						o.pos = {pos.x, pos.y};
						o.type = type;

						o.pos = glm::floor(o.pos);

						auto t = get_object_texture(type);

						switch (o.type) {
							case OBJ_LAYER_SET: {
								o.layset.radius = {8, 24};
								break;
							}
							case OBJ_LAYER_FLIP: {
								o.layflip.radius = {8, 24};
								break;
							}
						}

						array_add(&objects, o);
					}

					ImGui::EndDragDropTarget();
				}

				// select object
				if (is_item_active && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					selected_object = -1;
					for (int i = 0; i < objects.count; i++) {
						const Object& it = objects[i];
						auto size = get_object_size(it);
						auto mouse = get_mouse_pos_in_view(tilemap_view);
						if (point_in_rect({mouse.x, mouse.y}, {it.pos.x - size.x / 2, it.pos.y - size.y / 2, size.x, size.y})) {
							selected_object = i;
							break;
						}
					}
				}

				// tilemap background
				{
					ImU32 col = ImGui::ColorConvertFloat4ToU32(*(ImVec4*)&color_cornflower_blue);
					draw_list->AddRectFilled(tilemap_pos, tilemap_pos + tilemap_size, col);
				}

				// draw tilemap
				for (int y = 0; y < tm.height; y++) {
					for (int x = 0; x < tm.width; x++) {
						Tile tile = get_tile(tm, x, y, layer_index);
						ImU32 col = IM_COL32(255, 255, 255, 255);

						if (tile.index != 0) {
							AddTile(draw_list, tile, tilemap_pos, tilemap_size,
									tilemap_view, x, y, tileset_texture, col);
						}
					}
				}

				// draw objects
				draw_objects(draw_list, objects, tilemap_pos, tilemap_view);

				// highlight selected object
				if (selected_object != -1) {
					const Object& o = objects[selected_object];

					auto size = get_object_size(o);

					ImVec2 p = tilemap_pos + ImVec2(o.pos.x - size.x / 2, o.pos.y - size.y / 2) * tilemap_view.zoom;
					ImVec2 p2 = p + size * tilemap_view.zoom;

					draw_list->AddRect(p, p2, IM_COL32(255, 0, 0, 255), 0, 0, 0.5 * tilemap_view.zoom);
				}

				// draw grids
				{
					draw_list->AddRect(tilemap_pos, tilemap_pos + tilemap_size, IM_COL32(255, 255, 255, 255));

					if (tilemap_view.zoom > 2) {
						AddGrid(draw_list, tilemap_pos, ImVec2(16, 16) * tilemap_view.zoom, {tm.width, tm.height}, IM_COL32(255, 255, 255, 110));
					}

					AddGrid(draw_list, tilemap_pos, ImVec2(256, 256) * tilemap_view.zoom, {tm.width / 16, tm.height / 16});
				}
			};

			main_window();

			auto objects_window = [&]() {
				ImGui::Begin("Objects##object_editor");
				defer { ImGui::End(); };

				for (int i = 0; i < objects.count; i++) {
					char buf[32];
					stb_snprintf(buf, sizeof buf, "%d: %s", i, GetObjTypeName(objects[i].type));

					if (ImGui::Selectable(buf, i == selected_object)) {
						selected_object = i;
					}
				}
			};

			objects_window();

			auto object_list_window = [&]() {
				ImGui::Begin("Object List##object_editor");
				defer { ImGui::End(); };

				int id = 0;

				auto button = [&](ObjType type) {
					auto t = get_object_texture(type);

					ImGui::PushID(id++);
					ImageButtonActive("object button", t.ID, ImVec2(t.width, t.height), {}, {1, 1}, false);
					ImGui::PopID();

					ImGui::SetItemTooltip("%s", GetObjTypeName(type));

					if (ImGui::BeginDragDropSource()) {
						ImGui::SetDragDropPayload("ADD_OBJECT", &type, sizeof type);

						ImGui::Text("%s", GetObjTypeName(type));

						ImGui::EndDragDropSource();
					}
				};

				button(OBJ_PLAYER_INIT_POS);
				button(OBJ_LAYER_SET);
				button(OBJ_LAYER_FLIP);
			};

			object_list_window();

			auto object_properties_window = [&]() {
				ImGui::Begin("Object Properties##object_editor");
				defer { ImGui::End(); };

				if (selected_object == -1) {
					return;
				}

				Object* o = &objects[selected_object];

				ImGui::DragFloat2("Position", glm::value_ptr(o->pos), 1, 0, 0, "%.0f");

				switch (o->type) {
					case OBJ_LAYER_SET: {
						ImGui::DragFloat2("Radius", glm::value_ptr(o->layset.radius), 1, 0, 0, "%.0f");
						
						const char* values[] = {"A", "B"};
						if (ImGui::BeginCombo("Layer", values[o->layset.layer])) {
							if (ImGui::Selectable("A", o->layset.layer == 0)) o->layset.layer = 0;
							if (ImGui::Selectable("B", o->layset.layer == 1)) o->layset.layer = 1;

							ImGui::EndCombo();
						}
						break;
					}

					case OBJ_LAYER_FLIP: {
						ImGui::DragFloat2("Radius", glm::value_ptr(o->layflip.radius), 1, 0, 0, "%.0f");
						break;
					}
				}

				if (ImGui::Button("Delete Object")) {
					array_remove(&objects, selected_object);
					selected_object = -1;
					return;
				}
			};

			object_properties_window();
			break;
		}
	}

	ImGui::SetNextWindowPos(io.DisplaySize / 2.0f, ImGuiCond_Always, {0.5f, 0.5f});
	if (ImGui::BeginPopupModal("Create New Level", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize
							   | ImGuiWindowFlags_NoSavedSettings)) {
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
				error_msg = "Programmer Error!";
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

				current_level_dir = std::filesystem::u8path(cnl_folder);
				is_level_open = true;

				update_window_caption();

				tm.width  = 768;
				tm.height = 128;

				tm.tiles_a = calloc_array<Tile>(tm.width * tm.height);
				tm.tiles_b = calloc_array<Tile>(tm.width * tm.height);

				objects = malloc_bump_array<Object>(MAX_OBJECTS);

				std::filesystem::copy_file(std::filesystem::u8path(cnl_tileset),
										   current_level_dir / "Tileset.png");

				load_texture_from_file(&tileset_texture, (current_level_dir / "Tileset.png").u8string().c_str());
				load_surface_from_file(&tileset_surface, (current_level_dir / "Tileset.png").u8string().c_str());

				Assert(tileset_texture.width  % 16 == 0);
				Assert(tileset_texture.height % 16 == 0);

				ts.count = (tileset_texture.width / 16) * (tileset_texture.height / 16);
				ts.heights = calloc_array<u8>(ts.count * 16);
				ts.widths  = calloc_array<u8>(ts.count * 16);
				ts.angles  = calloc_array<float>(ts.count);

				heightmap_view.scrolling = (io.DisplaySize - ImVec2(tileset_texture.width, tileset_texture.height)) / 2.0f;

				write_tilemap(tm, (current_level_dir / "Tilemap.bin").u8string().c_str());
				write_tileset(ts, (current_level_dir / "Tileset.bin").u8string().c_str());

				write_objects(objects, (current_level_dir / "Objects.bin").u8string().c_str());

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
		stb_snprintf(buf, sizeof(buf), "Editor - (%s)", current_level_dir.u8string().c_str());
		SDL_SetWindowTitle(get_window_handle(), buf);
	}
}

#endif
