#include "editor.h"

#ifdef EDITOR

#include "sprite.h"
#include "assets.h"
#include "texture.h"

#undef Remove

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "nfd/nfd.h"
#include "IconsFontAwesome5.h"
#include "subprocess.h"

Editor editor;

void Editor::init(int argc, char* argv[]) {
	SDL_MaximizeWindow(get_window_handle());

	actions = malloc_bump_array<Action>(10'000);
	action_index = -1;

	process_name = argv[0];
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

static ivec2 view_get_tile_pos(const View& view, ivec2 tile_size, ivec2 clamp_size, vec2 screen_pos) {
	vec2 pos = screen_pos;
	pos -= view.thing_p0;
	pos /= view.zoom;

	int tile_x = clamp((int)(pos.x / tile_size.x), 0, clamp_size.x - 1);
	int tile_y = clamp((int)(pos.y / tile_size.y), 0, clamp_size.y - 1);

	return {tile_x, tile_y};
}

static ivec2 view_get_in_tile_pos(const View& view, ivec2 tile_size, ivec2 clamp_size, vec2 screen_pos) {
	vec2 pos = screen_pos;
	pos -= view.thing_p0;
	pos /= view.zoom;

	int x = clamp((int)pos.x, 0, clamp_size.x * tile_size.x - 1);
	int y = clamp((int)pos.y, 0, clamp_size.y * tile_size.y - 1);

	x %= tile_size.x;
	y %= tile_size.y;

	return {x, y};
}

enum PanAndZoomFlags {
	DONT_MOVE_VIEW_WITH_ARROW_KEYS = 1 << 0,
};

static void pan_and_zoom(View& view,
						 vec2 view_size,
						 vec2 thing_size,
						 PanAndZoomFlags flags,
						 void (*user_callback)()) {
	if (view_size.x == 0) view_size.x = ImGui::GetContentRegionAvail().x;
	if (view_size.y == 0) view_size.y = ImGui::GetContentRegionAvail().y;

	if (view_size.x < 50) view_size.x = 50;
	if (view_size.y < 50) view_size.y = 50;

	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	// The invisible button is the view.
	ImGui::InvisibleButton("PAN_AND_ZOOM",
						   view_size,
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
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
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

		ImVec2 view_center_pos = (ImGui::GetItemRectMin() + ImGui::GetItemRectMax()) / 2.0f;

		ImVec2 mouse_pos_in_thing = (io.MousePos - view.thing_p0) / view.zoom;
		ImVec2 view_center_pos_in_thing = (view_center_pos - view.thing_p0) / view.zoom;

		// zoom with mouse
		float mouse_wheel = io.MouseWheel;
		if (ImGui::IsItemHovered() && mouse_wheel != 0) {
			if (mouse_wheel > 0) {
				while (mouse_wheel > 0) {
					view.zoom *= 1.25f;
					mouse_wheel--;
				}
				Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

				vec2 new_thing_p0 = (io.MousePos / view.zoom - mouse_pos_in_thing) * view.zoom;
				view.scrolling += new_thing_p0 - view.thing_p0;
			} else {
				mouse_wheel = -mouse_wheel;
				while (mouse_wheel > 0) {
					view.zoom /= 1.25f;
					mouse_wheel--;
				}
				Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

				vec2 new_thing_p0 = (io.MousePos / view.zoom - mouse_pos_in_thing) * view.zoom;
				view.scrolling += new_thing_p0 - view.thing_p0;
			}
		}

		// zoom with keys
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
			if (ImGui::IsKeyPressed(ImGuiKey_Equal)) {
				view.zoom *= 1.25f;
				Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

				vec2 new_thing_p0 = (view_center_pos / view.zoom - view_center_pos_in_thing) * view.zoom;
				view.scrolling += new_thing_p0 - view.thing_p0;
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Minus)) {
				view.zoom /= 1.25f;
				Clamp(&view.zoom, MIN_ZOOM, MAX_ZOOM);

				vec2 new_thing_p0 = (view_center_pos / view.zoom - view_center_pos_in_thing) * view.zoom;
				view.scrolling += new_thing_p0 - view.thing_p0;
			}
		}
	}

	view.thing_p0 = ImGui::GetItemRectMin() + view.scrolling;
	view.thing_p1 = view.thing_p0 + thing_size * view.zoom;

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
		ImVec2 p0 = view.thing_p0;
		ImVec2 p1 = view.thing_p1;

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

static bool SmallIconButtonActive(const char* label, bool active) {
	if (active) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	}

	bool res = ImGui::SmallButton(label);

	if (active) {
		ImGui::PopStyleColor();
	}

	return res;
}

static void draw_grid_exact(vec2 tile_size, ivec2 grid_size, vec4 color) {
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

static bool pos_in_rect(vec2 pos, vec2 rect_min, vec2 rect_max) {
	ImRect rect;
	rect.Min = rect_min;
	rect.Max = rect_max;
	return rect.Contains(pos);
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

	if (ImGui::IsKeyPressed(ImGuiKey_Y, true) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) try_redo();

	if (ImGui::IsKeyPressed(ImGuiKey_H, true) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) show_undo_history_window ^= true;

	if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) try_run_game();

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
			if (action_index == -1) {
				ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
			} else {
				char buf[256];
				stb_snprintf(buf, sizeof(buf), "Undo %s", GetActionTypeName(actions[action_index].type));
				if (ImGui::MenuItem(buf, "Ctrl+Z")) try_undo();
			}

			if (action_index + 1 >= actions.count) {
				ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
			} else {
				char buf[256];
				stb_snprintf(buf, sizeof(buf), "Redo %s", GetActionTypeName(actions[action_index + 1].type));
				if (ImGui::MenuItem(buf, "Ctrl+Y")) try_redo();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Undo History", "Ctrl+H")) show_undo_history_window ^= true;

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Run")) {
			if (ImGui::MenuItem("Run Game", "F5")) try_run_game();

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

	auto undo_history_window = [&]() {
		if (!show_undo_history_window) return;

		ImGui::Begin("Undo History", &show_undo_history_window);
		defer { ImGui::End(); };

		int i = 0;
		For (it, actions) {
			if (i == action_index) {
				ImGui::Text("->");
				ImGui::SameLine();
			}

			ImGui::Text("[%d] %s", i, GetActionTypeName(it->type));

			if (it->type == ACTION_SET_TILE_HEIGHT) {
				ImGui::SameLine();
				int num_changes = it->set_tile_height.sets.count;
				ImGui::Text("(%d %s)", num_changes, (num_changes == 1) ? "change" : "changes");
			} else if (it->type == ACTION_SET_TILE_WIDTH) {
				ImGui::SameLine();
				int num_changes = it->set_tile_width.sets.count;
				ImGui::Text("(%d %s)", num_changes, (num_changes == 1) ? "change" : "changes");
			} else if (it->type == ACTION_SET_TILES) {
				ImGui::SameLine();
				int num_changes = it->set_tiles.sets.count;
				ImGui::Text("(%d %s)", num_changes, (num_changes == 1) ? "change" : "changes");

				ImGui::SameLine();
				int layer_index = it->set_tiles.sets[0].layer_index;
				ImGui::Text("(layer %d)", layer_index);
			}

			i++;
		}
	};

	undo_history_window();

	// render notifications
	{
		int i = 0;

		ImVec2 pos = io.DisplaySize - ImVec2(20, 20);

		For (it, notifications) {
			if (it->timer > 0) {
				it->timer -= io.DeltaTime;

				// fade in
				it->alpha += io.DeltaTime * 4.0f;
				if (it->alpha > 1) it->alpha = 1;
			} else {
				// fade out
				it->alpha -= io.DeltaTime * 6.0f;
				if (it->alpha <= 0) {
					array_remove(&notifications, it);
					it--;
					continue;
				}
			}

			ImGuiWindowFlags window_flags = (ImGuiWindowFlags_NoDecoration
											 | ImGuiWindowFlags_NoInputs
											 | ImGuiWindowFlags_AlwaysAutoResize
											 | ImGuiWindowFlags_NoFocusOnAppearing
											 | ImGuiWindowFlags_NoSavedSettings);

			ImGui::SetNextWindowPos(pos, ImGuiCond_Always, {1, 1});
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, it->alpha);

			char buf[128];
			stb_snprintf(buf, sizeof(buf), "notif%d", i++);

			ImGui::Begin(buf, nullptr, window_flags);

			ImGui::Text(ICON_FA_INFO_CIRCLE " Info");
			ImGui::Text("%s", it->buf);

			pos.y -= ImGui::GetWindowSize().y + 5;

			ImGui::End();

			ImGui::PopStyleVar();
		}
	}
}

void Editor::try_undo() {
	if (action_index == -1) {
		notify(NOTIF_INFO, "Nothing to Undo.");
		return;
	}

	//ImGui::ClearActiveID();

	const Action& action = actions[action_index];
	action_revert(action);

	notify(NOTIF_INFO, "Undid %s.", GetActionTypeName(action.type));

	// decrement after reverting action
	action_index--;
}

void Editor::try_redo() {
	if (action_index + 1 >= actions.count) {
		notify(NOTIF_INFO, "Nothing to Redo.");
		return;
	}

	//ImGui::ClearActiveID();

	// increment before performing action
	action_index++;

	const Action& action = actions[action_index];
	action_perform(action);

	notify(NOTIF_INFO, "Redid %s.", GetActionTypeName(action.type));
}

void Editor::action_add(const Action& action) {
	// TODO: handle when actions array runs out of capacity

	int new_count = action_index + 1;
	for (int i = new_count; i < actions.count; i++) {
		free_action(&actions[i]);
	}
	actions.count = new_count;

	array_add(&actions, action);
	action_index++;
}

void Editor::action_add_and_perform(const Action& action) {
	action_add(action);
	action_perform(action);
}

void Editor::action_perform(const Action& action) {
	// perform action
	switch (action.type) {
		case ACTION_SET_TILE_HEIGHT: {
			For (it, action.set_tile_height.sets) {
				int tile_index = it->tile_index;
				int in_tile_pos_x = it->in_tile_pos_x;

				auto heights = get_tile_heights(ts, tile_index);
				heights[in_tile_pos_x] = it->height_to;
			}
			gen_heightmap_texture(&heightmap, ts, tileset_texture);
			break;
		}

		case ACTION_SET_TILE_WIDTH: {
			For (it, action.set_tile_width.sets) {
				int tile_index = it->tile_index;
				int in_tile_pos_y = it->in_tile_pos_y;

				auto widths = get_tile_widths(ts, tile_index);
				widths[in_tile_pos_y] = it->width_to;
			}
			gen_widthmap_texture(&widthmap, ts, tileset_texture);
			break;
		}

		case ACTION_SET_TILE_ANGLE: {
			int tile_index = action.set_tile_angle.tile_index;
			ts.angles[tile_index] = action.set_tile_angle.angle_to;
			break;
		}

		case ACTION_SET_TILES: {
			For (it, action.set_tiles.sets) {
				set_tile_by_index(&tm, it->tile_index, it->layer_index, it->tile_to);
			}
			break;
		}

		default: {
			Assert(!"action not implemented");
			break;
		}
	}
}

void Editor::action_revert(const Action& action) {
	// revert action
	switch (action.type) {
		case ACTION_SET_TILE_HEIGHT: {
			For (it, action.set_tile_height.sets) {
				int tile_index = it->tile_index;
				int in_tile_pos_x = it->in_tile_pos_x;

				auto heights = get_tile_heights(ts, tile_index);
				heights[in_tile_pos_x] = it->height_from;
			}
			gen_heightmap_texture(&heightmap, ts, tileset_texture);
			break;
		}

		case ACTION_SET_TILE_WIDTH: {
			For (it, action.set_tile_width.sets) {
				int tile_index = it->tile_index;
				int in_tile_pos_y = it->in_tile_pos_y;

				auto widths = get_tile_widths(ts, tile_index);
				widths[in_tile_pos_y] = it->width_from;
			}
			gen_widthmap_texture(&widthmap, ts, tileset_texture);
			break;
		}

		case ACTION_SET_TILE_ANGLE: {
			int tile_index = action.set_tile_angle.tile_index;
			ts.angles[tile_index] = action.set_tile_angle.angle_from;
			break;
		}

		case ACTION_SET_TILES: {
			For (it, action.set_tiles.sets) {
				set_tile_by_index(&tm, it->tile_index, it->layer_index, it->tile_from);
			}
			break;
		}

		default: {
			Assert(!"action not implemented");
			break;
		}
	}
}

bool Editor::try_run_game() {
	if (!is_level_open) {
		return false;
	}

	// SDL converts argv to utf8, so `process_name` is utf8
	auto str = current_level_dir.u8string();
	const char *command_line[] = {process_name, "--game", str.c_str(), NULL};

	// @Utf8
	// it seems like this library doesn't convert from utf8 to windows wide char
	subprocess_s subprocess;
	int result = subprocess_create(command_line, subprocess_option_inherit_environment, &subprocess);
	if (result != 0) {
		return false;
	}

	// do we need to `subprocess_destroy`?

	return true;
}

void Editor::notify(NotificationType type, const char* fmt, ...) {
	Notification notif = {};
	notif.type = type;

	va_list va;
	va_start(va, fmt);
	stb_vsnprintf(notif.buf, sizeof(notif.buf), fmt, va);
	va_end(va);

	array_insert(&notifications, 0, notif);
}

void free_action(Action* action) {
	switch (action->type) {
		case ACTION_SET_TILE_HEIGHT: {
			array_free(&action->set_tile_height.sets);
			break;
		}

		case ACTION_SET_TILE_WIDTH: {
			array_free(&action->set_tile_width.sets);
			break;
		}
	}
	*action = {};
}

void TilesetEditor::update(float delta) {
	auto tileset_editor_window = [&]() {
		ImGui::Begin("Tileset Editor##tileset_editor");
		defer { ImGui::End(); };

		// tools child window
		auto tools_child_window = [&]() {
			ImGui::BeginChild("Tools Child Window", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX);
			defer { ImGui::EndChild(); };

			if (IconButtonActive(ICON_FA_MOUSE_POINTER, tool == TOOL_SELECT)) tool = TOOL_SELECT;
			if (IsKeyPressedNoMod(ImGuiKey_S)) tool = TOOL_SELECT;
			ImGui::SetItemTooltip("Select\nShortcut: S");

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
			} else if (tileset_editor.mode == MODE_ANGLES) {
				for (int tile_index = 0; tile_index < editor.ts.angles.count; tile_index++) {
					float x = (tile_index % (editor.tileset_texture.width / 16)) * 16 + 8;
					float y = (tile_index / (editor.tileset_texture.width / 16)) * 16 + 8;
					float angle = editor.ts.angles[tile_index];
					if (angle != -1) {
						draw_arrow_thick({x, y}, 8, angle, 2, 1, color_white);
					}
				}
			}

			// draw grid and border
			{
				if (tileset_editor.tileset_view.zoom > 2) {
					draw_grid_exact({16, 16},
									{editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
									color_white);
				}

				Rectf rect;
				rect.x = (tileset_editor.selected_tile_index % (editor.tileset_texture.width / 16)) * 16;
				rect.y = (tileset_editor.selected_tile_index / (editor.tileset_texture.width / 16)) * 16;
				rect.w = 16;
				rect.h = 16;
				draw_rectangle_outline_thick(rect, 1, color_white);

				draw_rectangle_outline_exact({0, 0, (float)editor.tileset_texture.width, (float)editor.tileset_texture.height},
											 color_white);
			}

			break_batch();
		};

		pan_and_zoom(tileset_view,
					 {},
					 {editor.tileset_texture.width, editor.tileset_texture.height},
					 (PanAndZoomFlags) 0,
					 callback);

		if (tool == TOOL_SELECT) {
			if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				if (pos_in_rect(ImGui::GetMousePos(), tileset_view.thing_p0, tileset_view.thing_p1)) {
					ivec2 tile_pos = view_get_tile_pos(tileset_view,
													   {16, 16},
													   {editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
													   ImGui::GetMousePos());

					int tile_index = tile_pos.x + tile_pos.y * (editor.tileset_texture.width / 16);

					selected_tile_index = tile_index;
				}
			}
		}

		bool set_tile_heights_dragging = false;

		if (mode == MODE_HEIGHTS) {
			if (tool == TOOL_BRUSH) {
				if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					set_tile_heights_dragging = true;

					if (pos_in_rect(ImGui::GetMousePos(), tileset_view.thing_p0, tileset_view.thing_p1)) {
						ivec2 tile_pos = view_get_tile_pos(tileset_view,
														   {16, 16},
														   {editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
														   ImGui::GetMousePos());

						ivec2 in_tile_pos = view_get_in_tile_pos(tileset_view,
																 {16, 16},
																 {editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
																 ImGui::GetMousePos());

						int tile_index = tile_pos.x + tile_pos.y * (editor.tileset_texture.width / 16);

						auto heights = get_tile_heights(editor.ts, tile_index);
						int height_from = heights[in_tile_pos.x];
						heights[in_tile_pos.x] = 16 - in_tile_pos.y;

						gen_heightmap_texture(&editor.heightmap, editor.ts, editor.tileset_texture);

						SetTileHeight* found = nullptr;
						For (it, set_tile_heights) {
							if (it->tile_index == tile_index && it->in_tile_pos_x == in_tile_pos.x) {
								found = it;
								break;
							}
						}

						if (found) {
							found->height_to = heights[in_tile_pos.x];
						} else {
							SetTileHeight set = {};
							set.tile_index = tile_index;
							set.in_tile_pos_x = in_tile_pos.x;
							set.height_from = height_from;
							set.height_to = heights[in_tile_pos.x];

							array_add(&set_tile_heights, set);
						}
					}
				}
			}
		}

		if (!set_tile_heights_dragging) {
			if (set_tile_heights.count > 0) {
				Action action = {};
				action.type = ACTION_SET_TILE_HEIGHT;

				// copy array
				For (it, set_tile_heights) {
					array_add(&action.set_tile_height.sets, *it);
				}

				editor.action_add(action);

				set_tile_heights.count = 0;
			}
		}
	};

	tileset_editor_window();

	auto tile_properties_window = [&]() {
		ImGui::Begin("Tile Properties##tileset_editor");
		defer { ImGui::End(); };

		if (!editor.is_level_open) {
			ImGui::Text("No level opened.");
			return;
		}

		ImGui::Text("Tile ID: %d", selected_tile_index);

		float angle = editor.ts.angles[selected_tile_index];
		ImGui::InputFloat("Angle", &angle, 0, 0, nullptr, ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_CharsDecimal);
		if (!ImGui::IsItemActive()) {
			if (editor.ts.angles[selected_tile_index] != angle) {
				Action action = {};
				action.type = ACTION_SET_TILE_ANGLE;
				action.set_tile_angle.tile_index = selected_tile_index;
				action.set_tile_angle.angle_from = editor.ts.angles[selected_tile_index];
				action.set_tile_angle.angle_to = angle;

				editor.action_add_and_perform(action);
			}
		}

		if (ImGui::CollapsingHeader("Heights")) {
			auto heights = get_tile_heights(editor.ts, selected_tile_index);
			for (int i = 0; i < heights.count; i++) {
				int height = heights[i];

				char buf[16];
				stb_snprintf(buf, sizeof(buf), "[%d]", i);
				ImGui::InputInt(buf, &height, 0, 0, ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_CharsDecimal);
				if (!ImGui::IsItemActive()) {
					if (heights[i] != height) {
						Action action = {};
						action.type = ACTION_SET_TILE_HEIGHT;

						SetTileHeight set = {};
						set.tile_index = selected_tile_index;
						set.in_tile_pos_x = i;
						set.height_from = heights[i];
						set.height_to = height;
						array_add(&action.set_tile_height.sets, set);

						editor.action_add_and_perform(action);
					}
				}
			}
		}

		if (ImGui::CollapsingHeader("Widths")) {
			auto widths = get_tile_widths(editor.ts, selected_tile_index);
			for (int i = 0; i < widths.count; i++) {
				int width = widths[i];

				char buf[16];
				stb_snprintf(buf, sizeof(buf), "[%d]", i);
				ImGui::InputInt(buf, &width, 0, 0, ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_CharsDecimal);
				if (!ImGui::IsItemActive()) {
					if (widths[i] != width) {
						Action action = {};
						action.type = ACTION_SET_TILE_WIDTH;

						SetTileWidth set = {};
						set.tile_index = selected_tile_index;
						set.in_tile_pos_y = i;
						set.width_from = widths[i];
						set.width_to = width;
						array_add(&action.set_tile_width.sets, set);

						editor.action_add_and_perform(action);
					}
				}
			}
		}
	};

	tile_properties_window();
}

void TilemapEditor::update(float delta) {
	auto tilemap_editor_window = [&]() {
		ImGui::Begin("Tilemap Editor##tilemap_editor");
		defer { ImGui::End(); };

		// tools child window
		auto tools_child_window = [&]() {
			ImGui::BeginChild("Tools Child Window", {}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX);
			defer { ImGui::EndChild(); };

			if (IconButtonActive(ICON_FA_MOUSE_POINTER, tool == TOOL_SELECT)) tool = TOOL_SELECT;
			if (IsKeyPressedNoMod(ImGuiKey_S)) tool = TOOL_SELECT;
			ImGui::SetItemTooltip("Select\nShortcut: S");

			if (IconButtonActive(ICON_FA_PEN, tool == TOOL_BRUSH)) tool = TOOL_BRUSH;
			if (IsKeyPressedNoMod(ImGuiKey_B)) tool = TOOL_BRUSH;
			ImGui::SetItemTooltip("Brush\nShortcut: B");

			if (IconButtonActive(ICON_FA_ERASER, tool == TOOL_ERASER)) tool = TOOL_ERASER;
			if (IsKeyPressedNoMod(ImGuiKey_E)) tool = TOOL_ERASER;
			ImGui::SetItemTooltip("Eraser\nShortcut: E");
		};

		tools_child_window();

		ImGui::SameLine();

		ImGui::BeginGroup();
		defer { ImGui::EndGroup(); };

		if (!editor.is_level_open) {
			ImGui::Text("No level opened.");
			return;
		}

		auto callback = []() {
			TilemapEditor& tilemap_editor = editor.tilemap_editor;

			Action action = {};
			action.type = ACTION_SET_TILES;
			// copy array
			For (it, tilemap_editor.set_tiles) {
				array_add(&action.set_tiles.sets, *it);
			}
			defer { free_action(&action); };

			editor.action_perform(action);
			defer { editor.action_revert(action); };

			// draw layers
			for (int i = 0; i < 3; i++) {
				if (tilemap_editor.layer_visible[i]) {
					vec4 color = color_white;

					if (tilemap_editor.highlight_current_layer) {
						if (i > tilemap_editor.layer_index) {
							color.a = 0.2f;
						} else if (i < tilemap_editor.layer_index) {
							color = get_color(80, 80, 80);
						}
					}

					draw_tilemap_layer(editor.tm, i, editor.tileset_texture, 0, 0, editor.tm.width, editor.tm.height, color);

					// draw hovered brush
					if (tilemap_editor.tool == TOOL_BRUSH) {
						if (i == tilemap_editor.layer_index) {
							ivec2 mouse_pos = view_get_tile_pos(tilemap_editor.tilemap_view,
																{16, 16},
																{editor.tm.width, editor.tm.height},
																ImGui::GetMousePos());

							for (int y = 0; y < tilemap_editor.brush_h; y++) {
								for (int x = 0; x < tilemap_editor.brush_w; x++) {
									Tile tile = tilemap_editor.brush[x + y * tilemap_editor.brush_w];

									Rect src;
									src.x = (tile.index % (editor.tileset_texture.width / 16)) * 16;
									src.y = (tile.index / (editor.tileset_texture.width / 16)) * 16;
									src.w = 16;
									src.h = 16;

									vec2 pos;
									pos.x = (mouse_pos.x + x) * 16;
									pos.y = (mouse_pos.y + y) * 16;

									draw_texture_simple(editor.tileset_texture, src, pos);
								}
							}
						}
					}
				}
			}

			// draw objects
			if (tilemap_editor.show_objects) {
				draw_objects(editor.objects, SDL_GetTicks() / (1000.0f / 60.0f), true);

				For (it, editor.objects) {
					switch (it->type) {
						case OBJ_MOVING_PLATFORM: {
							{
								vec2 p1 = it->pos;
								vec2 p2 = it->pos + it->mplatform.offset;

								float length = point_distance(p1, p2);
								float direction = point_direction(p1, p2);

								draw_arrow_thick(p1, length, direction, 2, 1, color_white);
							}

							{
								vec2 p1 = it->pos;
								vec2 p2 = it->pos - it->mplatform.offset;

								float length = point_distance(p1, p2);
								float direction = point_direction(p1, p2);

								draw_arrow_thick(p1, length, direction, 2, 1, color_white);
							}
							break;
						}
					}
				}
			}

			// draw grid and border
			{
				if (tilemap_editor.tilemap_view.zoom > 2) {
					draw_grid_exact({16, 16},
									{editor.tm.width, editor.tm.height},
									get_color(0xffffff80));
				}

				draw_grid_exact({256, 256},
								{editor.tm.width / 16, editor.tm.height / 16},
								color_white);

				draw_rectangle_outline_exact({0, 0, (float)editor.tm.width * 16, (float)editor.tm.height * 16},
											 color_white);
			}

			break_batch();
		};

		static float bottom_part_height;

		pan_and_zoom(tilemap_view,
					 ImGui::GetContentRegionAvail() - ImVec2(0, bottom_part_height),
					 {editor.tm.width * 16, editor.tm.height * 16},
					 (PanAndZoomFlags) 0,
					 callback);


		bool set_tiles_dragging = false;

		if (tool == TOOL_BRUSH) {
			if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				set_tiles_dragging = true;

				if (pos_in_rect(ImGui::GetMousePos(), tilemap_view.thing_p0, tilemap_view.thing_p1)) {
					ivec2 mouse_pos = view_get_tile_pos(tilemap_view,
														{16, 16},
														{editor.tm.width, editor.tm.height},
														ImGui::GetMousePos());

					for (int y = 0; y < brush_h; y++) {
						for (int x = 0; x < brush_w; x++) {
							if (!layer_visible[layer_index]) continue;
							if ((mouse_pos.x + x) >= editor.tm.width)  continue;
							if ((mouse_pos.y + y) >= editor.tm.height) continue;

							int tile_index = (mouse_pos.x + x) + (mouse_pos.y + y) * editor.tm.width;

							SetTile* found = nullptr;
							For (it, set_tiles) {
								if (it->tile_index == tile_index) {
									found = it;
									break;
								}
							}

							if (found) {
								found->tile_to = brush[x + y * brush_w];
							} else {
								SetTile set = {};
								set.tile_index = tile_index;
								set.layer_index = layer_index;
								set.tile_from = get_tile_by_index(editor.tm, tile_index, layer_index);
								set.tile_to = brush[x + y * brush_w];

								array_add(&set_tiles, set);
							}
						}
					}
				}
			}
		}

		if (!set_tiles_dragging) {
			if (set_tiles.count > 0) {
				Action action = {};
				action.type = ACTION_SET_TILES;

				// copy array
				For (it, set_tiles) {
					array_add(&action.set_tiles.sets, *it);
				}

				editor.action_add_and_perform(action);

				set_tiles.count = 0;
			}
		}

		ImVec2 cursor = ImGui::GetCursorScreenPos();

		ImGui::Checkbox("Show Objects", &show_objects);

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

		ImGui::SameLine();
		ImGui::Checkbox("Highlight Current Layer", &highlight_current_layer);

		bottom_part_height = (ImGui::GetCursorScreenPos() - cursor).y;
	};

	tilemap_editor_window();

	auto layers_window = [&]() {
		ImGui::Begin("Layers##tilemap_editor");
		defer { ImGui::End(); };

		if (SmallIconButtonActive(ICON_FA_EYE "##2", layer_visible[2])) layer_visible[2] ^= true;
		ImGui::SameLine();
		if (ImGui::Selectable("Layer C", layer_index == 2)) layer_index = 2;

		if (SmallIconButtonActive(ICON_FA_EYE "##1", layer_visible[1])) layer_visible[1] ^= true;
		ImGui::SameLine();
		if (ImGui::Selectable("Layer B", layer_index == 1)) layer_index = 1;

		if (SmallIconButtonActive(ICON_FA_EYE "##0", layer_visible[0])) layer_visible[0] ^= true;
		ImGui::SameLine();
		if (ImGui::Selectable("Layer A", layer_index == 0)) layer_index = 0;
	};

	layers_window();

	auto tileset_window = [&]() {
		ImGui::Begin("Tileset##tilemap_editor");
		defer { ImGui::End(); };

		if (!editor.is_level_open) {
			ImGui::Text("No level opened.");
			return;
		}

		auto callback = []() {
			TilemapEditor& tilemap_editor = editor.tilemap_editor;

			draw_texture(editor.tileset_texture);

			// draw selection
			if (tilemap_editor.selection_w > 0 && tilemap_editor.selection_h > 0) {
				draw_rectangle_outline_thick({(float)tilemap_editor.selection_x * 16, (float)tilemap_editor.selection_y * 16, (float)tilemap_editor.selection_w * 16, (float)tilemap_editor.selection_h * 16},
											 1,
											 color_white);
			}

			// draw grid and border
			{
				if (tilemap_editor.tileset_view.zoom > 2) {
					draw_grid_exact({16, 16},
									{editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
									{1, 1, 1, 0.5f});
				}

				draw_rectangle_outline_exact({0, 0, (float)editor.tileset_texture.width, (float)editor.tileset_texture.height},
											 color_white);
			}

			break_batch();
		};

		pan_and_zoom(tileset_view,
					 {},
					 {editor.tileset_texture.width, editor.tileset_texture.height},
					 (PanAndZoomFlags) 0,
					 callback);

		if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			ivec2 tile_pos = view_get_tile_pos(tileset_view,
											   {16, 16},
											   {editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
											   ImGui::GetMousePos());

			if (!dragging) {
				dragging_x1 = tile_pos.x;
				dragging_y1 = tile_pos.y;
				dragging = true;
			}

			int dragging_x2 = tile_pos.x;
			int dragging_y2 = tile_pos.y;

			selection_x = min(dragging_x1, dragging_x2);
			selection_y = min(dragging_y1, dragging_y2);
			selection_w = ImAbs(dragging_x2 - dragging_x1) + 1;
			selection_h = ImAbs(dragging_y2 - dragging_y1) + 1;
		} else {
			if (dragging) {
				// copy tiles to brush
				brush.count = 0;
				for (int tile_y = selection_y; tile_y < selection_y + selection_h; tile_y++) {
					for (int tile_x = selection_x; tile_x < selection_x + selection_w; tile_x++) {
						int tile_index = tile_x + tile_y * (editor.tileset_texture.width / 16);

						Tile tile = {};
						tile.index = tile_index;
						tile.top_solid = true;
						tile.lrb_solid = true;
						array_add(&brush, tile);
					}
				}
				brush_w = selection_w;
				brush_h = selection_h;

				dragging = false;
			}
		}
	};

	tileset_window();
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
