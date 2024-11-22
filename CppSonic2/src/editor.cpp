#include "editor.h"

#ifdef EDITOR

#undef Remove

#include "IconsFontAwesome5.h"
#include "nfd/nfd.h"
#include <glad/gl.h>

#include "imgui/imgui_internal.h"

Editor editor;

void Editor::init() {
	SDL_MaximizeWindow(get_window_handle());

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
		if (ImGui::IsKeyPressed(ImGuiKey_Equal)) {
			view.zoom *= 1.25f;
			Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

			ImVec2 new_texture_pos = (window_center_pos / view.zoom - window_center_pos_in_texture) * view.zoom;
			view.scrolling += new_texture_pos - texture_pos;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Minus)) {
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
	if (tile.index == 0) {
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

static bool ButtonActive(const char* label, bool active, bool icon) {
	if (active) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	}

	ImVec2 size = {};
	if (icon) size = ImVec2(24, 0);

	bool res = ImGui::Button(label, size);

	if (active) {
		ImGui::PopStyleColor();
	}

	return res;
}

void Editor::clear_state() {
	free_texture(&tileset_texture);

	free_tilemap(&tm);
	free_tileset(&ts);

	mode = {};
	hmode = {};
	heightmap_view   = {};
	tilemap_view     = {};
	tile_select_view = {};
	selected_tiles = {};
	tool = {};
	htool = {};
	tilemap_select_tool_selection = {};
	tilemap_rect_tool_selection = {};
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

		load_texture_from_file(&tileset_texture, (current_level_dir / "Tileset.png").string().c_str());
		load_surface_from_file(&tileset_surface, (current_level_dir / "Tileset.png").string().c_str());

		read_tilemap(&tm, (current_level_dir / "Tilemap.bin").string().c_str());
		read_tileset(&ts, (current_level_dir / "Tileset.bin").string().c_str());

		gen_heightmap_texture(&heightmap, ts, tileset_texture);
		gen_widthmap_texture(&widthmap, ts, tileset_texture);

		heightmap_view.scrolling = (io.DisplaySize - ImVec2(tileset_texture.width, tileset_texture.height)) / 2.0f;
	};

	auto try_save_level = [&]() {
		if (!is_level_open) return;

		write_tilemap(tm, (current_level_dir / "Tilemap.bin").string().c_str());
		write_tileset(ts, (current_level_dir / "Tileset.bin").string().c_str());
	};

	if (ImGui::IsKeyPressed(ImGuiKey_N) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
		try_open_cnl_popup();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_O) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
		try_open_level();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
		try_save_level();
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

	if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
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

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				{
					ImU32 col = /*heightmap_show_collision*/true ? IM_COL32(255, 255, 255, 128) : IM_COL32_WHITE;
					draw_list->AddImage(tileset_texture.ID, texture_pos, texture_pos + texture_size, {0, 0}, {1, 1}, col);
				}

				if (hmode == HMODE_HEIGHTS || hmode == HMODE_ANGLES) {
					ImU32 col = IM_COL32(255, 255, 255, 128);
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
								IM_COL32(255, 255, 255, 255));
					}
				}

				// drag an arrow
				{
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

						if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
							dragging = false;
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

				if (ButtonActive(ICON_FA_PEN, htool == HTOOL_BRUSH, true)) htool = HTOOL_BRUSH;
				ImGui::SetItemTooltip("Brush\nShortcut: B");

				if (ButtonActive(ICON_FA_ERASER, htool == HTOOL_ERASER, true)) htool = HTOOL_ERASER;
				ImGui::SetItemTooltip("Eraser\nShortcut: E");

				if (ButtonActive("A", htool == HTOOL_AUTO, true)) htool = HTOOL_AUTO;
				ImGui::SetItemTooltip("Auto-Tool\nShortcut: A");

				if (ImGui::IsKeyPressed(ImGuiKey_B)) htool = HTOOL_BRUSH;
				if (ImGui::IsKeyPressed(ImGuiKey_E)) htool = HTOOL_ERASER;
				if (ImGui::IsKeyPressed(ImGuiKey_A)) htool = HTOOL_AUTO;
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

				if (ButtonActive("Heights", hmode == HMODE_HEIGHTS, false)) hmode = HMODE_HEIGHTS;

				ImGui::SameLine();
				if (ButtonActive("Widths", hmode == HMODE_WIDTHS, false)) hmode = HMODE_WIDTHS;

				ImGui::SameLine();
				if (ButtonActive("Angles", hmode == HMODE_ANGLES, false)) hmode = HMODE_ANGLES;
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

				bool is_drawing = false;
				bool is_erasing = false;

				if (tool == TOOL_BRUSH) {
					// item = invisible button from pan_and_zoom
					if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
						if (pos_in_rect(ImGui::GetMousePos(), tilemap_pos, tilemap_size)) {
							ImVec2 pos = get_mouse_pos_in_view(tilemap_view);

							int tile_x = clamp((int)(pos.x / 16), 0, tm.width  - 1);
							int tile_y = clamp((int)(pos.y / 16), 0, tm.height - 1);

							for (int y = 0; y < selected_tiles.h; y++) {
								for (int x = 0; x < selected_tiles.w; x++) {
									Tile tile = {};
									tile.index = (selected_tiles.x + x) + (selected_tiles.y + y) * (tileset_texture.width / 16);
									if ((tile_x + x) >= 0 && (tile_x + x) < tm.width
										&& (tile_y + y) >= 0 && (tile_y + y) < tm.height)
									{
										tm.tiles_a[(tile_x + x) + (tile_y + y) * tm.width] = tile;
									}
								}
							}
							is_drawing = true;
						}
					}

					if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
						if (pos_in_rect(ImGui::GetMousePos(), tilemap_pos, tilemap_size)) {
							ImVec2 pos = get_mouse_pos_in_view(tilemap_view);

							int tile_x = clamp((int)(pos.x / 16), 0, tm.width  - 1);
							int tile_y = clamp((int)(pos.y / 16), 0, tm.height - 1);

							tm.tiles_a[tile_x + tile_y * tm.width] = {};
							is_erasing = true;
						}
					}
				}

				if (tool == TOOL_SELECT) {
					rectangle_select(tilemap_select_tool_selection, is_item_active,
									 tilemap_pos, tilemap_size, tilemap_view, tm.width, tm.height);
				}

				if (tool == TOOL_RECT) {
					bool result = rectangle_select(tilemap_rect_tool_selection, is_item_active,
												   tilemap_pos, tilemap_size, tilemap_view, tm.width, tm.height);
					if (result) {
						auto sel = tilemap_rect_tool_selection;
						for (int y = sel.y; y < sel.y + sel.h; y++) {
							for (int x = sel.x; x < sel.x + sel.w; x++) {
								//tm.tiles_a[x + y * tm.width] = selected_tile;
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

				for (int y = 0; y < tm.height; y++) {
					for (int x = 0; x < tm.width; x++) {
						Tile tile = tm.tiles_a[x + y * tm.width];
						ImU32 col = IM_COL32(255, 255, 255, 255);

						if (tool == TOOL_BRUSH || tool == TOOL_RECT) {
							if (!is_erasing) {
								if (x >= mouse_tile_x && x < mouse_tile_x + selected_tiles.w
									&& y >= mouse_tile_y && y < mouse_tile_y + selected_tiles.h
									&& mouse_tile_x != -1 && mouse_tile_y != -1)
								{
									int xx = x - mouse_tile_x;
									int yy = y - mouse_tile_y;
									xx += selected_tiles.x;
									yy += selected_tiles.y;
									int tile_index = xx + yy * (tileset_texture.width / 16);

									tile = {};
									tile.index = tile_index;

									if (!is_drawing) col = IM_COL32(255, 255, 255, 180);
								}
							}
						}

						if (tool == TOOL_RECT) {
							auto sel = tilemap_rect_tool_selection;
							if (sel.dragging) {
								if (x >= sel.x && x < sel.x + sel.w && y >= sel.y && y < sel.y + sel.h) {
									//tile = selected_tile;
								}
							}
						}

						if (tile.index == 0) {
							continue;
						}

						AddTile(draw_list, tile, tilemap_pos, tilemap_size,
								tilemap_view, x, y, tileset_texture, col);
					}
				}

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
						draw_list->AddRect(p, p + ImVec2(tilemap_select_tool_selection.w, tilemap_select_tool_selection.h) * 16.0f * tilemap_view.zoom, IM_COL32(255, 255, 255, 255), 0, 0, 2);
					}
				}

				// draw grids
				{
					draw_list->AddRect(tilemap_pos, tilemap_pos + tilemap_size, IM_COL32(255, 255, 255, 255));

					if (tilemap_view.zoom > 2) {
						AddGrid(draw_list, tilemap_pos, ImVec2(16, 16) * tilemap_view.zoom, {tm.width, tm.height}, IM_COL32(255, 255, 255, 128));
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

				rectangle_select(selected_tiles, ImGui::IsItemActive(),
								 texture_pos, texture_size,
								 tile_select_view,
								 tileset_width, tileset_height);

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				draw_list->AddImage(tileset_texture.ID, texture_pos, texture_pos + texture_size);

				if (selected_tiles.w > 0) {
					Assert(selected_tiles.h > 0);

					ImVec2 p = texture_pos + ImVec2(selected_tiles.x * 16, selected_tiles.y * 16) * tile_select_view.zoom;
					ImVec2 p2 = p + ImVec2(selected_tiles.w * 16, selected_tiles.h * 16) * tile_select_view.zoom;
					draw_list->AddRect(p, p2, IM_COL32_WHITE, 0, 0, 2);
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

				if (ButtonActive(ICON_FA_PEN, tool == TOOL_BRUSH, true)) tool = TOOL_BRUSH;
				ImGui::SetItemTooltip("Brush");

				if (ButtonActive(ICON_FA_SQUARE_FULL, tool == TOOL_RECT, true)) {
					tool = TOOL_RECT;
					tilemap_rect_tool_selection = {};
				}
				ImGui::SetItemTooltip("Rectangle");

				if (ButtonActive(ICON_FA_OBJECT_GROUP, tool == TOOL_SELECT, true)) {
					tool = TOOL_SELECT;
					tilemap_select_tool_selection = {};
				}
				ImGui::SetItemTooltip("Select");
			};

			tool_select_window();
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

				current_level_dir = cnl_folder;
				is_level_open = true;

				update_window_caption();

				tm.width  = 768;
				tm.height = 128;

				tm.tiles_a = calloc_array<Tile>(tm.width * tm.height);
				tm.tiles_b = calloc_array<Tile>(tm.width * tm.height);

				std::filesystem::copy_file(cnl_tileset, current_level_dir / "Tileset.png");

				load_texture_from_file(&tileset_texture, (current_level_dir / "Tileset.png").string().c_str());
				load_surface_from_file(&tileset_surface, (current_level_dir / "Tileset.png").string().c_str());

				Assert(tileset_texture.width  % 16 == 0);
				Assert(tileset_texture.height % 16 == 0);

				ts.count = (tileset_texture.width / 16) * (tileset_texture.height / 16);
				ts.heights = calloc_array<u8>(ts.count * 16);
				ts.widths  = calloc_array<u8>(ts.count * 16);
				ts.angles  = calloc_array<float>(ts.count);

				heightmap_view.scrolling = (io.DisplaySize - ImVec2(tileset_texture.width, tileset_texture.height)) / 2.0f;

				write_tilemap(tm, (current_level_dir / "Tilemap.bin").string().c_str());
				write_tileset(ts, (current_level_dir / "Tileset.bin").string().c_str());

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

#endif
