#include "editor.h"

#ifdef EDITOR

#include "renderer.h"
#include "sprite.h"
#include "assets.h"

#undef Remove

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "nfd/nfd.h"
#include "IconsFontAwesome5.h"

Editor editor;

void Editor::init(int argc, char* argv[]) {
	SDL_MaximizeWindow(get_window_handle());

	actions = malloc_bump_array<Action>(10'000);
}

void Editor::deinit() {
	close_level();
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

enum PanAndZoomFlags {
	DONT_MOVE_VIEW_WITH_ARROW_KEYS = 1 << 0,
};

static void pan_and_zoom(View& view,
						 vec2 invbutton_size,
						 vec2 item_size,
						 PanAndZoomFlags flags,
						 void (*user_callback)()) {
	if (invbutton_size.x == 0) invbutton_size.x = ImGui::GetContentRegionAvail().x;
	if (invbutton_size.y == 0) invbutton_size.y = ImGui::GetContentRegionAvail().y;

	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	ImGui::InvisibleButton("PAN_AND_ZOOM",
						   invbutton_size,
						   ImGuiButtonFlags_MouseButtonLeft
						   | ImGuiButtonFlags_MouseButtonRight
						   | ImGuiButtonFlags_MouseButtonMiddle);

	// pan with mouse
	if (ImGui::IsItemActive()) {
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, -1)) {
			view.scrolling.x += io.MouseDelta.x;
			view.scrolling.y += io.MouseDelta.y;
		}
	}

	// pan with keys
	if (!(flags & DONT_MOVE_VIEW_WITH_ARROW_KEYS)) {
		if (ImGui::IsWindowFocused()) {
			float delta = io.DeltaTime * 60;
			if (ImGui::IsKeyDown(ImGuiKey_UpArrow))    view.scrolling.y += 10 * delta;
			if (ImGui::IsKeyDown(ImGuiKey_DownArrow))  view.scrolling.y -= 10 * delta;
			if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))  view.scrolling.x += 10 * delta;
			if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) view.scrolling.x -= 10 * delta;
		}
	}

	{
		const float MIN_ZOOM = 0.25f;
		const float MAX_ZOOM = 50.0f;

		ImVec2 invbutton_center_pos = (ImGui::GetItemRectMin() + ImGui::GetItemRectMax()) / 2.0f;

		ImVec2 mouse_pos_relative_to_item = io.MousePos / view.zoom - view.item_screen_p0 / view.zoom;
		ImVec2 invbutton_center_pos_relative_to_item = invbutton_center_pos / view.zoom - view.item_screen_p0 / view.zoom;

		// zoom with mouse
		float mouse_wheel = io.MouseWheel;
		if (ImGui::IsItemHovered() && mouse_wheel != 0) {
			if (mouse_wheel > 0) {
				while (mouse_wheel > 0) {
					view.zoom *= 1.25f;
					mouse_wheel--;
				}
				Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

				vec2 new_item_screen_pos = (io.MousePos / view.zoom - mouse_pos_relative_to_item) * view.zoom;
				view.scrolling += new_item_screen_pos - view.item_screen_p0;
			} else {
				mouse_wheel = -mouse_wheel;
				while (mouse_wheel > 0) {
					view.zoom /= 1.25f;
					mouse_wheel--;
				}
				Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

				vec2 new_item_screen_pos = (io.MousePos / view.zoom - mouse_pos_relative_to_item) * view.zoom;
				view.scrolling += new_item_screen_pos - view.item_screen_p0;
			}
		}

		// zoom with keys
		if (ImGui::IsWindowFocused()) {
			if (ImGui::IsKeyPressed(ImGuiKey_Equal)) {
				view.zoom *= 1.25f;
				Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

				vec2 new_texture_pos = (invbutton_center_pos / view.zoom - invbutton_center_pos_relative_to_item) * view.zoom;
				view.scrolling += new_texture_pos - view.item_screen_p0;
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Minus)) {
				view.zoom /= 1.25f;
				Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

				vec2 new_texture_pos = (invbutton_center_pos / view.zoom - invbutton_center_pos_relative_to_item) * view.zoom;
				view.scrolling += new_texture_pos - view.item_screen_p0;
			}
		}
	}

	view.item_screen_p0 = ImGui::GetItemRectMin() + view.scrolling;
	view.item_screen_p1 = view.item_screen_p0 + item_size * view.zoom;

	struct Userdata {
		void (*user_callback)();
		vec2 item_rect_min;
		vec2 item_rect_max;
		vec2 view_scrolling;
		float view_zoom;
	};

	auto callback_wrapper = [](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
		ImGuiIO& io =  ImGui::GetIO();

		Userdata* userdata = (Userdata*) cmd->UserCallbackData;

		{
			set_proj_mat(get_ortho(0, io.DisplaySize.x, io.DisplaySize.y, 0));
		}

		{
			vec2 offset = userdata->item_rect_min + userdata->view_scrolling;
			set_view_mat(get_translation({offset.x, offset.y, 0}));
		}

		{
			// scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
			int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
			int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);

			// Will project scissor/clipping rectangles into framebuffer space
			ImVec2 clip_off = {};         // (0,0) unless using multi-viewports
			ImVec2 clip_scale = io.DisplayFramebufferScale; // (1,1) unless using retina display which are often (2,2)

			// Project scissor/clipping rectangles into framebuffer space
			ImVec2 clip_min((userdata->item_rect_min.x - clip_off.x) * clip_scale.x, (userdata->item_rect_min.y - clip_off.y) * clip_scale.y);
			ImVec2 clip_max((userdata->item_rect_max.x - clip_off.x) * clip_scale.x, (userdata->item_rect_max.y - clip_off.y) * clip_scale.y);

			// Apply scissor/clipping rectangle (Y is inverted in OpenGL)
			glScissor((int)clip_min.x, (int)((float)fb_height - clip_max.y), (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y));
		}

		{
			mat4 model = glm::scale(mat4{1}, vec3{userdata->view_zoom, userdata->view_zoom, 1});
			set_model_mat(model);
		}

		userdata->user_callback();

		Assert(renderer.vertices.count == 0);

		set_view_mat({1});
		set_proj_mat({1});
		set_model_mat({1});
	};

	// draw background
	{
		ImVec2 p0 = view.item_screen_p0;
		ImVec2 p1 = view.item_screen_p1;

		draw_list->PushClipRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

		draw_list->AddImage(get_texture(tex_editor_bg).id,
							p0,
							p1,
							p0 / 4.0f,
							p1 / 4.0f);

		draw_list->PopClipRect();
	}

	{
		Userdata userdata = {};
		userdata.user_callback = user_callback;
		userdata.item_rect_min = ImGui::GetItemRectMin();
		userdata.item_rect_max = ImGui::GetItemRectMax();
		userdata.view_scrolling = view.scrolling;
		userdata.view_zoom = view.zoom;

		draw_list->AddCallback(callback_wrapper, &userdata, sizeof(userdata));
		draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
	}

	// draw border
	{
		ImVec4 border_color = ImGui::GetStyleColorVec4(ImGuiCol_Border);
		draw_list->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::ColorConvertFloat4ToU32(border_color));
	}
}

static glm::ivec2 view_get_tile_pos(const View& view, glm::ivec2 tile_size, glm::ivec2 clamp_size, vec2 screen_pos) {
	vec2 pos = screen_pos;
	pos -= view.item_screen_p0;
	pos /= view.zoom;

	int tile_x = clamp((int)(pos.x / tile_size.x), 0, clamp_size.x - 1);
	int tile_y = clamp((int)(pos.y / tile_size.y), 0, clamp_size.y - 1);

	return {tile_x, tile_y};
}

static glm::ivec2 view_get_in_tile_pos(const View& view, glm::ivec2 tile_size, glm::ivec2 clamp_size, vec2 screen_pos) {
	vec2 pos = screen_pos;
	pos -= view.item_screen_p0;
	pos /= view.zoom;

	int x = clamp((int)pos.x, 0, clamp_size.x * tile_size.x - 1);
	int y = clamp((int)pos.y, 0, clamp_size.y * tile_size.y - 1);

	x %= tile_size.x;
	y %= tile_size.y;

	return {x, y};
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

static bool IconButtonActive(const char* label, bool active) {
	if (active) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	}

	ImVec2 size = ImVec2(16 + ImGui::GetStyle().FramePadding.x * 2, 0);
	bool res = ImGui::Button(label, size);

	if (active) {
		ImGui::PopStyleColor();
	}

	return res;
}

static void draw_grid(vec2 tile_size, glm::ivec2 grid_size, vec4 color) {
	for (int x = 0; x <= grid_size.x; x++) {
		vec2 p0 = {x * tile_size.x, 0};
		vec2 p1 = {x * tile_size.x, grid_size.y * tile_size.y};
		draw_line_exact(p0, p1, color);
	}

	for (int y = 0; y <= grid_size.y; y++) {
		vec2 p0 = {0, y * tile_size.y};
		vec2 p1 = {grid_size.x * tile_size.x, y * tile_size.y};
		draw_line_exact(p0, p1, color);
	}
}

void Editor::pick_and_open_level() {
	ImGuiIO& io = ImGui::GetIO();

	char* path = nullptr;
	nfdresult_t res = NFD_PickFolder(nullptr, &path);
	defer { free(path); };

	if (res != NFD_OKAY) {
		if (res == NFD_ERROR) log_error("NFD Error: %s", NFD_GetError());
		return;
	}

	close_level();
	log_info("Opening level %s...", path);

	current_level_dir = std::filesystem::u8path(path);

	tileset_texture = load_texture_from_file((current_level_dir / "Tileset.png").u8string().c_str());
	if (tileset_texture.id == 0) {
		log_error("Couldn't load level: couldn't load tileset texture.");
		close_level();
		return;
	}

	tileset_surface = load_surface_from_file((current_level_dir / "Tileset.png").u8string().c_str());
	if (!tileset_surface) {
		log_error("Couldn't load level: couldn't load tileset surface.");
		close_level();
		return;
	}

	read_tilemap(&tm, (current_level_dir / "Tilemap.bin").u8string().c_str());
	if (tm.tiles_a.count == 0) {
		log_error("Couldn't load level: couldn't read tilemap.");
		close_level();
		return;
	}

	log_info("Read tilemap %d x %d.", tm.width, tm.height);

	read_tileset(&ts, (current_level_dir / "Tileset.bin").u8string().c_str());
	if (ts.heights.count == 0) {
		log_error("Couldn't load level: couldn't read tileset.");
		close_level();
		return;
	}

	objects = malloc_bump_array<Object>(MAX_OBJECTS);

	if (!read_objects(&objects, (current_level_dir / "Objects.bin").u8string().c_str())) {
		log_error("Couldn't load level: couldn't read objects.");
		close_level();
		return;
	}

	gen_heightmap_texture(&heightmap, ts, tileset_texture);
	gen_widthmap_texture(&widthmap, ts, tileset_texture);

	is_level_open = true;
	update_window_caption();
}

void Editor::close_level() {
	current_level_dir.clear();

	free_texture(&tileset_texture);
	free_surface(&tileset_surface);

	free_tilemap(&tm);
	free_tileset(&ts);

	free(objects.data);
	objects = {};

	free_texture(&heightmap);
	free_texture(&widthmap);

	is_level_open = false;
	update_window_caption();
}

void Editor::update(float delta) {
	ImGuiIO& io = ImGui::GetIO();

	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	// main menu bar hotkeys
	if (ImGui::IsKeyPressed(ImGuiKey_O, false) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) pick_and_open_level();

	if (ImGui::IsKeyPressed(ImGuiKey_Z, true) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) try_undo();

	// main menu bar
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Level", "Ctrl+N")) {}

			if (ImGui::MenuItem("Open Level...", "Ctrl+O")) pick_and_open_level();

			if (ImGui::MenuItem("Save Level", "Ctrl+S")) {}

			ImGui::Separator();

			if (ImGui::MenuItem("Close Level")) close_level();

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit")) {
			if (actions.count == 0) {
				ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
			} else {
				char buf[256];
				stb_snprintf(buf, sizeof(buf), "Undo %s", GetActionTypeName(actions[actions.count - 1].type));
				if (ImGui::MenuItem(buf, "Ctrl+Z")) try_undo();
			}

			if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}

			ImGui::EndMenu();
		}

		// centered
		{
			static float centered_buttons_total_width;

			ImGui::SetCursorScreenPos({(io.DisplaySize.x - centered_buttons_total_width) / 2, 0});

			ImVec2 cursor = ImGui::GetCursorScreenPos();

			if (ImGui::MenuItem("Tileset", nullptr, state == STATE_TILESET_EDITOR)) state = STATE_TILESET_EDITOR;
			if (ImGui::MenuItem("Tilemap", nullptr, state == STATE_TILEMAP_EDITOR)) state = STATE_TILEMAP_EDITOR;
			if (ImGui::MenuItem("Objects", nullptr, state == STATE_OBJECTS_EDITOR)) state = STATE_OBJECTS_EDITOR;

			centered_buttons_total_width = ImGui::GetCursorScreenPos().x - cursor.x;
		}

		ImGui::EndMainMenuBar();
	}

	if (IsKeyPressedNoMod(ImGuiKey_F1)) {
		show_demo_window ^= true;
	}
	if (show_demo_window) {
		ImGui::ShowDemoWindow(&show_demo_window);
	}

	// update state
	switch (state) {
		case STATE_TILESET_EDITOR: {
			tileset_editor.update(delta);
			break;
		}

		case STATE_TILEMAP_EDITOR: {
			tilemap_editor.update(delta);
			break;
		}

		case STATE_OBJECTS_EDITOR: {
			objects_editor.update(delta);
			break;
		}
	}
}

void Editor::try_undo() {
	if (actions.count == 0) return;

	Action action = actions[actions.count - 1];

	// revert action
	switch (action.type) {
		case ACTION_SET_TILE_HEIGHT: {
			int tile_index = action.set_tile_height.tile_index;
			int in_tile_pos_x = action.set_tile_height.in_tile_pos_x;

			auto heights = get_tile_heights(ts, tile_index);
			heights[in_tile_pos_x] = action.set_tile_height.height_from;

			gen_heightmap_texture(&heightmap, ts, tileset_texture);
			break;
		}
	}

	actions.count--;
}

void TilesetEditor::update(float delta) {
	auto tileset_editor_window = [&]() {
		ImGui::Begin("Tileset Editor##tileset_editor");
		defer { ImGui::End(); };

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
			if (IsKeyPressedNoMod(ImGuiKey_Escape)) {
				mode = MODE_NONE;
				tool = TOOL_NONE;
			}
		}

		// tools child window
		auto tools_child_window = [&]() {
			ImGui::BeginChild("Tools Child Window", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX);
			defer { ImGui::EndChild(); };

			if (IconButtonActive(ICON_FA_PEN, tool == TOOL_BRUSH)) tool = TOOL_BRUSH;
			if (IsKeyPressedNoMod(ImGuiKey_B)) tool = TOOL_BRUSH;
			ImGui::SetItemTooltip("Brush\nShortcut: B");

			if (IconButtonActive(ICON_FA_ERASER, tool == TOOL_ERASER)) tool = TOOL_ERASER;
			if (IsKeyPressedNoMod(ImGuiKey_E)) tool = TOOL_ERASER;
			ImGui::SetItemTooltip("Eraser\nShortcut: E");

			if (IconButtonActive("A", tool == TOOL_AUTO)) tool = TOOL_AUTO;
			if (IsKeyPressedNoMod(ImGuiKey_A)) tool = TOOL_AUTO;
			ImGui::SetItemTooltip("Auto-Tool\nShortcut: A");
		};

		tools_child_window();

		ImGui::SameLine();

		ImGui::BeginGroup();
		defer { ImGui::EndGroup(); };

		// mode child window
		auto mode_child_window = [&]() {
			ImGui::BeginChild("Mode Child Window", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
			defer { ImGui::EndChild(); };

			if (ButtonActive("Heights", mode == MODE_HEIGHTS)) mode = MODE_HEIGHTS;
			ImGui::SameLine();

			if (ButtonActive("Widths", mode == MODE_WIDTHS)) mode = MODE_WIDTHS;
			ImGui::SameLine();

			if (ButtonActive("Angles", mode == MODE_ANGLES)) mode = MODE_ANGLES;
		};

		mode_child_window();

		if (!editor.is_level_open) {
			ImGui::Text("No level opened.");
			return;
		}

		auto callback = []() {
			TilesetEditor& tileset_editor = editor.tileset_editor;

			draw_texture(editor.tileset_texture);

			if (tileset_editor.mode == MODE_HEIGHTS) {
				draw_texture(editor.heightmap, {}, {}, {1,1}, {}, 0, get_color(255, 255, 255, 128));
			} else if (tileset_editor.mode == MODE_WIDTHS) {
				draw_texture(editor.widthmap, {}, {}, {1,1}, {}, 0, get_color(255, 255, 255, 128));
			}

			// draw grid and border
			{
				vec4 color = get_color(255, 255, 255, 255);

				if (tileset_editor.tileset_view.zoom > 2) {
					draw_grid({16, 16},
							  {editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
							  color);
				}

				draw_rectangle_outline_exact({0, 0, (float)editor.tileset_texture.width, (float)editor.tileset_texture.height},
											 color);
			}

			break_batch();
		};

		pan_and_zoom(tileset_view,
					 {},
					 {editor.tileset_texture.width, editor.tileset_texture.height},
					 (PanAndZoomFlags) 0,
					 callback);

		if (ImGui::IsItemActive()) {
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				if (mode == MODE_HEIGHTS) {
					if (tool == TOOL_BRUSH) {
						glm::ivec2 tile_pos = view_get_tile_pos(tileset_view,
																{16, 16},
																{editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
																ImGui::GetMousePos());

						glm::ivec2 in_tile_pos = view_get_in_tile_pos(tileset_view,
																	  {16, 16},
																	  {editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
																	  ImGui::GetMousePos());

						int tile_index = tile_pos.x + tile_pos.y * (editor.tileset_texture.width / 16);

						auto heights = get_tile_heights(editor.ts, tile_index);
						int height_from = heights[in_tile_pos.x];
						heights[in_tile_pos.x] = 16 - in_tile_pos.y;

						gen_heightmap_texture(&editor.heightmap, editor.ts, editor.tileset_texture);

						Action action = {};
						action.type = ACTION_SET_TILE_HEIGHT;
						action.set_tile_height.tile_index = tile_index;
						action.set_tile_height.in_tile_pos_x = in_tile_pos.x;
						action.set_tile_height.height_from = height_from;
						action.set_tile_height.height_to = heights[in_tile_pos.x];

						array_add(&editor.actions, action);
					}
				}
			}
		}
	};

	tileset_editor_window();

	auto tile_properties_window = [&]() {
		ImGui::Begin("Tile Properties##tileset_editor");
		defer { ImGui::End(); };
	};

	tile_properties_window();
}

void TilemapEditor::update(float delta) {
}

void ObjectsEditor::update(float delta) {
}

void Editor::draw(float delta) {
}

void Editor::update_window_caption() {
	if (is_level_open) {
		char buf[512];
		stb_snprintf(buf, sizeof(buf), "Editor - (%s)", current_level_dir.u8string().c_str());
		SDL_SetWindowTitle(get_window_handle(), buf);
	} else {
		SDL_SetWindowTitle(get_window_handle(), "Editor");
	}
}

#endif
