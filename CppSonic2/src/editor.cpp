#include "editor.h"

#ifdef EDITOR

#include "sprite.h"
#include "assets.h"
#include "texture.h"
#include "package.h"

#undef Remove

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "nfd/nfd.h"
#include "IconsFontAwesome5.h"
#include "subprocess.h"

Editor editor;

void Editor::init(int argc, char* argv[]) {
	SDL_MaximizeWindow(get_window_handle());

	process_name = argv[0];

	if (argc >= 3) {
		if (strcmp(argv[1], "--editor") == 0) {
			try_open_level(argv[2]);
		}
	}

	{
		char* LAPTOP_MODE = SDL_getenv("LAPTOP_MODE"); // @Leak
		if (LAPTOP_MODE) {
			laptop_mode = atoi(LAPTOP_MODE);
		}
	}
}

void Editor::deinit() {
	close_level();
}

static bool IsKeyPressedNoMod(ImGuiKey key, bool repeat = false) {
	if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) return false;
	if (ImGui::IsKeyDown(ImGuiKey_RightCtrl)) return false;

	if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) return false;
	if (ImGui::IsKeyDown(ImGuiKey_RightShift)) return false;

	if (ImGui::IsKeyDown(ImGuiKey_LeftAlt)) return false;
	if (ImGui::IsKeyDown(ImGuiKey_RightAlt)) return false;

	return ImGui::IsKeyPressed(key, repeat);
}

static vec2 view_get_pos(const View& view, vec2 screen_pos) {
	vec2 pos = screen_pos;
	pos -= view.thing_p0;
	pos /= view.zoom;

	return pos;
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

enum {
	DONT_MOVE_VIEW_WITH_ARROW_KEYS = 1 << 0,
};

static void pan_and_zoom(View& view,
						 vec2 view_size,
						 vec2 thing_size,
						 u32 flags,
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

	if (editor.laptop_mode) {
		// pan with touchpad
		if (ImGui::IsItemHovered()) {
			if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
				view.scrolling.x += io.MouseWheelH * 30;
				view.scrolling.y += io.MouseWheel  * 30;
			}
		}
	} else {
		// pan with mouse
		if (ImGui::IsItemActive()) {
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, -1)) {
				view.scrolling.x += io.MouseDelta.x;
				view.scrolling.y += io.MouseDelta.y;
			}
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

		if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || !editor.laptop_mode) {
			// zoom with mouse
			if (ImGui::IsItemHovered() && io.MouseWheel != 0) {
				view.zoom *= powf(1.25f, io.MouseWheel);
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

	view.item_rect_min = ImGui::GetItemRectMin();
	view.item_rect_max = ImGui::GetItemRectMax();
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

static bool SmallButton2(const char* label, ImVec2 size) {
	ImGuiContext& g = *GImGui;
	float backup_padding_y = g.Style.FramePadding.y;
	g.Style.FramePadding.y = 0.0f;
	bool pressed = ImGui::ButtonEx(label, size, ImGuiButtonFlags_AlignTextBaseLine);
	g.Style.FramePadding.y = backup_padding_y;
	return pressed;
}

static bool SmallIconButtonActive(const char* label, bool active) {
	if (active) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	}

	ImVec2 size = ImVec2(16 + ImGui::GetStyle().FramePadding.x * 2, 0);
	bool res = SmallButton2(label, size);

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

static vec2 sprite_get_uv0(const Sprite& s, int frame_index) {
	const Texture& t = s.texture;

	const SpriteFrame& f = s.frames[frame_index];

	Rect src = {f.u, f.v, f.w, f.h};

	float u1 = src.x / (float)t.width;
	float v1 = src.y / (float)t.height;
	return {u1, v1};
}

static vec2 sprite_get_uv1(const Sprite& s, int frame_index) {
	const Texture& t = s.texture;

	const SpriteFrame& f = s.frames[frame_index];

	Rect src = {f.u, f.v, f.w, f.h};

	float u2 = (src.x + src.w) / (float)t.width;
	float v2 = (src.y + src.h) / (float)t.height;
	return {u2, v2};
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

template <typename T>
static void UndoableComboEnum(const char* label,
							  const char* (*GetEnumName)(T t),
							  int object_index,
							  u32 field_offset,
							  T num_enums) {
	static_assert(sizeof(T) == 4);

	T* field_ptr = (T*) ((u8*)&editor.objects[object_index] + field_offset);

	if (ImGui::BeginCombo(label, GetEnumName(*field_ptr))) {
		for (int i = 0; i < num_enums; i++) {
			if (ImGui::Selectable(GetEnumName((T) i), i == *field_ptr)) {
				if (i != *field_ptr) {
					T data_to = (T) i;

					Action action = {};
					action.type = ACTION_SET_OBJECT_FIELD;
					action.set_object_field.index = object_index;
					action.set_object_field.field_offset = field_offset;
					action.set_object_field.field_size = sizeof(T);

					static_assert(sizeof(T) <= sizeof(action.set_object_field.data_from));

					memcpy(action.set_object_field.data_from, field_ptr, sizeof(T));
					memcpy(action.set_object_field.data_to, &data_to, sizeof(T));

					editor.action_add_and_perform(action);
				}
			}

			if (i == *field_ptr) {
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}
}

static void UndoableCombo(const char* label,
						  int object_index,
						  u32 field_offset,
						  const char** values,
						  int num_values) {
	int* field_ptr = (int*) ((u8*)&editor.objects[object_index] + field_offset);

	if (ImGui::BeginCombo(label, values[*field_ptr])) {
		for (int i = 0; i < num_values; i++) {
			if (ImGui::Selectable(values[i], i == *field_ptr)) {
				if (i != *field_ptr) {
					int data_to = (int) i;

					Action action = {};
					action.type = ACTION_SET_OBJECT_FIELD;
					action.set_object_field.index = object_index;
					action.set_object_field.field_offset = field_offset;
					action.set_object_field.field_size = sizeof(int);

					static_assert(sizeof(int) <= sizeof(action.set_object_field.data_from));

					memcpy(action.set_object_field.data_from, field_ptr, sizeof(int));
					memcpy(action.set_object_field.data_to, &data_to, sizeof(int));

					editor.action_add_and_perform(action);
				}
			}

			if (i == *field_ptr) {
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}
}

// TODO: dragging doesn't work
static void UndoableDragFloat2(const char* label,
							   int object_index,
							   u32 field_offset,
							   float v_speed = 1,
							   float v_min = 0,
							   float v_max = 0,
							   const char* format = "%.3f") {
	vec2* field_ptr = (vec2*) ((u8*)&editor.objects[object_index] + field_offset);
	vec2 copy = *field_ptr;

	ImGui::DragFloat2(label, &copy[0], v_speed, v_min, v_max, format);

	if (!ImGui::IsItemActive()) {
		if (*field_ptr != copy) {
			Action action = {};
			action.type = ACTION_SET_OBJECT_FIELD;
			action.set_object_field.index = object_index;
			action.set_object_field.field_offset = field_offset;
			action.set_object_field.field_size = sizeof(vec2);

			static_assert(sizeof(vec2) <= sizeof(action.set_object_field.data_from));

			memcpy(action.set_object_field.data_from, field_ptr, sizeof(vec2));
			memcpy(action.set_object_field.data_to, &copy, sizeof(vec2));

			editor.action_add_and_perform(action);
		}
	}
}

// TODO: dragging doesn't work
static void UndoableDragFloat(const char* label,
							  int object_index,
							  u32 field_offset,
							  float v_speed = 1,
							  float v_min = 0,
							  float v_max = 0,
							  const char* format = "%.3f") {
	float* field_ptr = (float*) ((u8*)&editor.objects[object_index] + field_offset);
	float copy = *field_ptr;

	ImGui::DragFloat(label, &copy, v_speed, v_min, v_max, format);

	if (!ImGui::IsItemActive()) {
		if (*field_ptr != copy) {
			Action action = {};
			action.type = ACTION_SET_OBJECT_FIELD;
			action.set_object_field.index = object_index;
			action.set_object_field.field_offset = field_offset;
			action.set_object_field.field_size = sizeof(float);

			static_assert(sizeof(float) <= sizeof(action.set_object_field.data_from));

			memcpy(action.set_object_field.data_from, field_ptr, sizeof(float));
			memcpy(action.set_object_field.data_to, &copy, sizeof(float));

			editor.action_add_and_perform(action);
		}
	}
}

static void UndoableInputFloat2(const char* label,
								int object_index,
								u32 field_offset,
								const char* format = "%.3f") {
	vec2* field_ptr = (vec2*) ((u8*)&editor.objects[object_index] + field_offset);
	vec2 copy = *field_ptr;

	ImGui::InputFloat2(label, &copy[0], format);

	if (!ImGui::IsItemActive()) {
		if (*field_ptr != copy) {
			Action action = {};
			action.type = ACTION_SET_OBJECT_FIELD;
			action.set_object_field.index = object_index;
			action.set_object_field.field_offset = field_offset;
			action.set_object_field.field_size = sizeof(vec2);

			static_assert(sizeof(vec2) <= sizeof(action.set_object_field.data_from));

			memcpy(action.set_object_field.data_from, field_ptr, sizeof(vec2));
			memcpy(action.set_object_field.data_to, &copy, sizeof(vec2));

			editor.action_add_and_perform(action);
		}
	}
}

static void UndoableInputFloat(const char* label,
							   int object_index,
							   u32 field_offset) {
	float* field_ptr = (float*) ((u8*)&editor.objects[object_index] + field_offset);
	float copy = *field_ptr;

	ImGui::InputFloat(label, &copy);

	if (!ImGui::IsItemActive()) {
		if (*field_ptr != copy) {
			Action action = {};
			action.type = ACTION_SET_OBJECT_FIELD;
			action.set_object_field.index = object_index;
			action.set_object_field.field_offset = field_offset;
			action.set_object_field.field_size = sizeof(float);

			static_assert(sizeof(float) <= sizeof(action.set_object_field.data_from));

			memcpy(action.set_object_field.data_from, field_ptr, sizeof(float));
			memcpy(action.set_object_field.data_to, &copy, sizeof(float));

			editor.action_add_and_perform(action);
		}
	}
}

// TODO: type safety? what if we pass a field offset to a field of another type (not u32)?
static void UndoableCheckboxFlags(const char* label,
								  int object_index,
								  u32 field_offset,
								  u32 flags_value) {
	u32* field_ptr = (u32*) ((u8*)&editor.objects[object_index] + field_offset);
	u32 copy = *field_ptr;

	ImGui::CheckboxFlags(label, &copy, flags_value);

	if (!ImGui::IsItemActive()) {
		if (*field_ptr != copy) {
			Action action = {};
			action.type = ACTION_SET_OBJECT_FIELD;
			action.set_object_field.index = object_index;
			action.set_object_field.field_offset = field_offset;
			action.set_object_field.field_size = sizeof(u32);

			static_assert(sizeof(u32) <= sizeof(action.set_object_field.data_from));

			memcpy(action.set_object_field.data_from, field_ptr, sizeof(u32));
			memcpy(action.set_object_field.data_to, &copy, sizeof(u32));

			editor.action_add_and_perform(action);
		}
	}
}

static void draw_editor_object_gizmos() {
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

void Editor::try_pick_and_open_level() {
	char* path = nullptr;
	nfdresult_t res = NFD_PickFolder(nullptr, &path);
	defer { free(path); };

	if (res != NFD_OKAY) {
		if (res == NFD_ERROR) log_error("NFD Error: %s", NFD_GetError());
		return;
	}

	try_open_level(path);
}

void Editor::try_open_level(const char* path) {
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

	actions = malloc_bump_array<Action>(10'000);
	action_index = -1;
	saved_action_index = -1;

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
	objects_editor.object_index = -1;

	free_texture(&heightmap);
	free_texture(&widthmap);

	free(actions.data);
	actions = {};
	action_index = -1;
	saved_action_index = -1;

	is_level_open = false;
	update_window_caption();
}

void Editor::try_save_level() {
	if (!is_level_open) return;

	write_tilemap(tm, (current_level_dir / "Tilemap.bin").u8string().c_str());
	write_tileset(ts, (current_level_dir / "Tileset.bin").u8string().c_str());

	write_objects(objects, (current_level_dir / "Objects.bin").u8string().c_str());

	saved_action_index = action_index;
	update_window_caption();

	notify(NOTIF_INFO, "Saved.");
}

void Editor::try_load_layer_from_binary_file() {
	if (!is_level_open) {
		return;
	}

	char* path = nullptr;
	nfdresult_t res = NFD_OpenDialog(nullptr, nullptr, &path);
	defer { free(path); };

	if (res != NFD_OKAY) {
		if (res == NFD_ERROR) log_error("NFD Error: %s", NFD_GetError());
		return;
	}

	size_t filesize;
	u8* filedata = get_file(path, &filesize);

	if (!filedata) {
		return;
	}

	SDL_RWops* f = SDL_RWFromConstMem(filedata, filesize);
	defer { SDL_RWclose(f); };

	int width;
	SDL_RWread(f, &width, sizeof width, 1);
	Assert(width > 0);

	int height;
	SDL_RWread(f, &height, sizeof height, 1);
	Assert(height > 0);

	array<Tile> loaded = calloc_array<Tile>(width * height);
	SDL_RWread(f, loaded.data, sizeof(loaded[0]), loaded.count);

	array<Tile> tiles = get_tiles_array(tm, tilemap_editor.layer_index);

	int copy_w = min(tm.width, width);
	int copy_h = min(tm.height, height);

	for (int y = 0; y < copy_h; y++) {
		for (int x = 0; x < copy_w; x++) {
			tiles[x + y * tm.width] = loaded[x + y * width];
		}
	}
}

void Editor::update(float delta) {
	ImGuiIO& io = ImGui::GetIO();

	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	auto try_clear_layer = [&]() {
		if (!is_level_open) return;

		array<Tile> tiles = get_tiles_array(tm, tilemap_editor.layer_index);

		For (it, tiles) *it = {};
	};

	auto try_delete_all_objects = [&]() {
		if (!is_level_open) return;

		objects.count = 0;
		objects_editor.object_index = -1;
	};

	// main menu bar hotkeys
	if (ImGui::IsKeyPressed(ImGuiKey_O, false) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) try_pick_and_open_level();

	if (ImGui::IsKeyPressed(ImGuiKey_S, false) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) try_save_level();

	if (ImGui::IsKeyPressed(ImGuiKey_Z, true) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) try_undo();

	if (ImGui::IsKeyPressed(ImGuiKey_Y, true) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) try_redo();

	if (ImGui::IsKeyPressed(ImGuiKey_H, true) && ImGui::IsKeyDown(ImGuiKey_LeftAlt)) show_undo_history_window ^= true;

	if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) try_run_game();

	// main menu bar
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Level", "Ctrl+N")) {}

			if (ImGui::MenuItem("Open Level...", "Ctrl+O")) try_pick_and_open_level();

			if (ImGui::MenuItem("Save Level", "Ctrl+S")) try_save_level();

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

			if (ImGui::MenuItem("Undo History", "Alt+H")) show_undo_history_window ^= true;

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Run")) {
			if (ImGui::MenuItem("Run Game", "F5")) try_run_game();

			ImGui::EndMenu();
		}

		if (state == STATE_TILEMAP_EDITOR) {
			if (ImGui::BeginMenu("Tilemap##2")) {
				// if (ImGui::MenuItem("Load Layer from Binary File...")) try_load_layer_from_binary_file();

				// if (ImGui::MenuItem("Clear Layer")) try_clear_layer();

				ImGui::EndMenu();
			}
		} else if (state == STATE_OBJECTS_EDITOR) {
			if (ImGui::BeginMenu("Objects##2")) {
				// if (ImGui::MenuItem("Delete All")) try_delete_all_objects();

				ImGui::EndMenu();
			}
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

			if (i == saved_action_index) {
				ImGui::SameLine();
				ImGui::Text("(Saved)");
			}

			if (i == actions.count - 1) {
				ImGui::SetItemDefaultFocus();
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

			if (it->type == NOTIF_INFO) {
				ImGui::Text(ICON_FA_INFO_CIRCLE " Info");
			} else if (it->type == NOTIF_WARN) {
				ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE " Warning");
			}
			ImGui::Text("%s", it->buf);

			pos.y -= ImGui::GetWindowSize().y + 5;

			ImGui::End();

			ImGui::PopStyleVar();
		}
	}
}

void Editor::try_undo() {
	if (!is_level_open) return;

	if (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		notify(NOTIF_WARN, "Can't undo while dragging the mouse.");
		return;
	}

	if (ImGui::GetActiveID() != 0) {
		notify(NOTIF_WARN, "Can't undo while interacting with a widget.");
		return;
	}

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

	update_window_caption();
}

void Editor::try_redo() {
	if (!is_level_open) return;

	if (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		notify(NOTIF_WARN, "Can't redo while dragging the mouse.");
		return;
	}

	if (ImGui::GetActiveID() != 0) {
		notify(NOTIF_WARN, "Can't redo while interacting with a widget.");
		return;
	}

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

	update_window_caption();
}

void Editor::action_add(const Action& action) {
	Assert(is_level_open);

	// TODO: handle when actions array runs out of capacity

	if (action_index < saved_action_index) {
		saved_action_index = -99;
	}

	int new_count = action_index + 1;
	for (int i = new_count; i < actions.count; i++) {
		free_action(&actions[i]);
	}
	actions.count = new_count;

	array_add(&actions, action);
	action_index++;

	update_window_caption();
}

void Editor::action_add_and_perform(const Action& action) {
	Assert(is_level_open);

	action_add(action);
	action_perform(action);
}

void Editor::action_perform(const Action& action) {
	Assert(is_level_open);

	// perform action
	switch (action.type) {
		case ACTION_NONE: break;

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

		case ACTION_ADD_OBJECT: {
			array_add(&objects, action.add_object.o);
			objects_editor.object_index = -1;
			break;
		}

		case ACTION_REMOVE_OBJECT: {
			array_remove(&editor.objects, action.remove_object.index);
			objects_editor.object_index = -1;
			break;
		}

		case ACTION_SET_OBJECT_FIELD: {
			u8* obj_member = (u8*)&editor.objects[action.set_object_field.index] + action.set_object_field.field_offset;
			memcpy(obj_member, action.set_object_field.data_to, action.set_object_field.field_size);
			break;
		}

		default: {
			Assert(!"action not implemented");
			break;
		}
	}
}

void Editor::action_revert(const Action& action) {
	Assert(is_level_open);

	// revert action
	switch (action.type) {
		case ACTION_NONE: break;

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

		case ACTION_ADD_OBJECT: {
			array_remove(&objects, objects.count - 1);
			objects_editor.object_index = -1;
			break;
		}

		case ACTION_REMOVE_OBJECT: {
			array_insert(&objects, action.remove_object.index, action.remove_object.o);
			objects_editor.object_index = -1;
			break;
		}

		case ACTION_SET_OBJECT_FIELD: {
			u8* obj_member = (u8*)&editor.objects[action.set_object_field.index] + action.set_object_field.field_offset;
			memcpy(obj_member, action.set_object_field.data_from, action.set_object_field.field_size);
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
	static bool dont_update_selected_tile_index;

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
									{1, 1, 1, 0.5f});
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
					 0,
					 callback);

		if (tool == TOOL_SELECT) {
			if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				if (pos_in_rect(ImGui::GetMousePos(), tileset_view.thing_p0, tileset_view.thing_p1)) {
					ivec2 tile_pos = view_get_tile_pos(tileset_view,
													   {16, 16},
													   {editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
													   ImGui::GetMousePos());

					int tile_index = tile_pos.x + tile_pos.y * (editor.tileset_texture.width / 16);

					if (!dont_update_selected_tile_index) {
						selected_tile_index = tile_index;
					}
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
			} else if (tool == TOOL_AUTO) {
				if (ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					ivec2 tile_pos = view_get_tile_pos(tileset_view,
													   {16, 16},
													   {editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
													   ImGui::GetMousePos());

					int tile_index = tile_pos.x + tile_pos.y * (editor.tileset_texture.width / 16);

					Action action = {};
					action.type = ACTION_SET_TILE_HEIGHT;

					for (int x = 0; x < 16; x++) {
						int height = 0;

						int pixel_x = tile_pos.x * 16 + x;
						int pixel_y = tile_pos.y * 16 + 15;
						vec4 pixel = surface_get_pixel(editor.tileset_surface, pixel_x, pixel_y);
						if (pixel.a == 0) {
							// flipped height
							for (int y = 0; y < 16; y++) {
								int pixel_x = tile_pos.x * 16 + x;
								int pixel_y = tile_pos.y * 16 + y;
								vec4 pixel = surface_get_pixel(editor.tileset_surface, pixel_x, pixel_y);
								if (pixel.a != 0) {
									height = 0xFF - y;
								} else {
									break;
								}
							}
						} else {
							// normal height
							for (int y = 15; y >= 0; y--) {
								int pixel_x = tile_pos.x * 16 + x;
								int pixel_y = tile_pos.y * 16 + y;
								vec4 pixel = surface_get_pixel(editor.tileset_surface, pixel_x, pixel_y);
								if (pixel.a != 0) {
									height = 16 - y;
								} else {
									break;
								}
							}
						}

						SetTileHeight set = {};
						set.tile_index = tile_index;
						set.in_tile_pos_x = x;
						set.height_from = get_tile_heights(editor.ts, tile_index)[x];
						set.height_to = height;

						array_add(&action.set_tile_height.sets, set);
					}

					editor.action_add_and_perform(action);
				}
			}
		} else if (mode == MODE_WIDTHS) {
			if (tool == TOOL_AUTO) {
				if (ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					ivec2 tile_pos = view_get_tile_pos(tileset_view,
													   {16, 16},
													   {editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
													   ImGui::GetMousePos());

					int tile_index = tile_pos.x + tile_pos.y * (editor.tileset_texture.width / 16);

					Action action = {};
					action.type = ACTION_SET_TILE_WIDTH;

					for (int y = 0; y < 16; y++) {
						int width = 0;

						int pixel_x = tile_pos.x * 16 + 15;
						int pixel_y = tile_pos.y * 16 + y;
						vec4 pixel = surface_get_pixel(editor.tileset_surface, pixel_x, pixel_y);
						if (pixel.a == 0) {
							// flipped width
							for (int x = 0; x < 16; x++) {
								int pixel_x = tile_pos.x * 16 + x;
								int pixel_y = tile_pos.y * 16 + y;
								vec4 pixel = surface_get_pixel(editor.tileset_surface, pixel_x, pixel_y);
								if (pixel.a != 0) {
									width = 0xFF - x;
								} else {
									break;
								}
							}
						} else {
							// normal width
							for (int x = 15; x >= 0; x--) {
								int pixel_x = tile_pos.x * 16 + x;
								int pixel_y = tile_pos.y * 16 + y;
								vec4 pixel = surface_get_pixel(editor.tileset_surface, pixel_x, pixel_y);
								if (pixel.a != 0) {
									width = 16 - x;
								} else {
									break;
								}
							}
						}

						SetTileWidth set = {};
						set.tile_index = tile_index;
						set.in_tile_pos_y = y;
						set.width_from = get_tile_widths(editor.ts, tile_index)[y];
						set.width_to = width;

						array_add(&action.set_tile_width.sets, set);
					}

					editor.action_add_and_perform(action);
				}
			}
		} else if (mode == MODE_ANGLES) {
			if (tool == TOOL_BRUSH) {
				static bool dragging;
				static vec2 start_pos;

				vec2 pos = view_get_pos(tileset_view, ImGui::GetMousePos());

				if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					if (!dragging) {
						start_pos = pos;
						dragging = true;
					}

					ImDrawList* draw_list = ImGui::GetWindowDrawList();
					AddArrow(draw_list,
							 tileset_view.thing_p0 + start_pos * tileset_view.zoom,
							 tileset_view.thing_p0 + pos * tileset_view.zoom,
							 IM_COL32_WHITE, 0.5, tileset_view.zoom);
				} else {
					if (dragging) {
						if (point_distance(start_pos.x, start_pos.y, pos.x, pos.y) > 1) {
							int tile_x = clamp((int)(start_pos.x / 16), 0, editor.tileset_texture.width  / 16 - 1);
							int tile_y = clamp((int)(start_pos.y / 16), 0, editor.tileset_texture.height / 16 - 1);
							int tile_index = tile_x + tile_y * (editor.tileset_texture.width / 16);
							float angle = point_direction(start_pos.x, start_pos.y, pos.x, pos.y);

							Action action = {};
							action.type = ACTION_SET_TILE_ANGLE;
							action.set_tile_angle.tile_index = tile_index;
							action.set_tile_angle.angle_from = editor.ts.angles[tile_index];
							action.set_tile_angle.angle_to = angle;

							editor.action_add_and_perform(action);
						}

						dragging = false;
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

		dont_update_selected_tile_index = false;

		float angle = editor.ts.angles[selected_tile_index];
		ImGui::InputFloat("Angle", &angle, 0, 0, nullptr, ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_CharsDecimal);
		if (ImGui::IsItemActive()) {
			dont_update_selected_tile_index = true;
		} else {
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

				char buf[32];
				stb_snprintf(buf, sizeof(buf), "[%d]##heights", i);
				ImGui::InputInt(buf, &height, 0, 0, ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_CharsDecimal);
				if (ImGui::IsItemActive()) {
					dont_update_selected_tile_index = true;
				} else {
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

				char buf[32];
				stb_snprintf(buf, sizeof(buf), "[%d]##widths", i);
				ImGui::InputInt(buf, &width, 0, 0, ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_CharsDecimal);
				if (ImGui::IsItemActive()) {
					dont_update_selected_tile_index = true;
				} else {
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

			if (IconButtonActive(ICON_FA_SQUARE_FULL, tool == TOOL_RECTANGLE)) tool = TOOL_RECTANGLE;
			if (IsKeyPressedNoMod(ImGuiKey_R)) tool = TOOL_RECTANGLE;
			ImGui::SetItemTooltip("Rectangle\nShortcut: R");
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

			// brush action preview
			Action brush_action = {};
			if (tilemap_editor.tool == TOOL_BRUSH) {
				brush_action = tilemap_editor.get_brush_action();
			}
			defer { free_action(&brush_action); };
			editor.action_perform(brush_action);
			defer { editor.action_revert(brush_action); };

			// rectangle action preview
			Action rectangle_action = {};
			if (tilemap_editor.tool == TOOL_RECTANGLE && tilemap_editor.tilemap_selection.dragging) {
				rectangle_action = tilemap_editor.get_rectangle_action();
			}
			defer { free_action(&rectangle_action); };
			editor.action_perform(rectangle_action);
			defer { editor.action_revert(rectangle_action); };

			// draw layers
			int visual_layer_index[4] = {1, 2, 3, 0};

			auto draw_layer = [&](int i) {
				if (tilemap_editor.layer_visible[i]) {
					vec4 color = color_white;

					if (tilemap_editor.highlight_current_layer) {
						if (visual_layer_index[i] > visual_layer_index[tilemap_editor.layer_index]) {
							color.a = 0.1f;
						} else if (visual_layer_index[i] < visual_layer_index[tilemap_editor.layer_index]) {
							color = get_color(40, 40, 40);
						}
					}

					ivec2 pos_from = view_get_tile_pos(tilemap_editor.tilemap_view, {16, 16}, {editor.tm.width, editor.tm.height}, tilemap_editor.tilemap_view.item_rect_min);
					ivec2 pos_to   = view_get_tile_pos(tilemap_editor.tilemap_view, {16, 16}, {editor.tm.width, editor.tm.height}, tilemap_editor.tilemap_view.item_rect_max);
					pos_to.x++;
					pos_to.y++;

					// draw tilemap layer
					for (int y = pos_from.y; y < pos_to.y; y++) {
						for (int x = pos_from.x; x < pos_to.x; x++) {
							Tile tile = get_tile(editor.tm, x, y, i);

							if (tile.index == 0) {
								continue;
							}

							Rect src;
							src.x = (tile.index % (editor.tileset_texture.width / 16)) * 16;
							src.y = (tile.index / (editor.tileset_texture.width / 16)) * 16;
							src.w = 16;
							src.h = 16;

							draw_texture_simple(editor.tileset_texture, src, {x * 16.0f, y * 16.0f}, {}, color, {tile.hflip, tile.vflip});
						}
					}

					// draw layer collision
					if (tilemap_editor.edit_collision) {
						if (i == tilemap_editor.layer_index) {
							for (int y = pos_from.y; y < pos_to.y; y++) {
								for (int x = pos_from.x; x < pos_to.x; x++) {
									Tile tile = get_tile(editor.tm, x, y, i);

									vec4 color;
									if (tile.top_solid && tile.lrb_solid) {
										color = {1, 1, 1, 0.75f};
									} else if (tile.top_solid && !tile.lrb_solid) {
										color = {0.5f, 0.5f, 1, 0.75f};
									} else if (!tile.top_solid && tile.lrb_solid) {
										color = {1, 0.5f, 0.5f, 0.75f};
									} else { // !top_solid && !lrb_solid
										continue;
									}

									Rect src;
									src.x = (tile.index % (editor.tileset_texture.width / 16)) * 16;
									src.y = (tile.index / (editor.tileset_texture.width / 16)) * 16;
									src.w = 16;
									src.h = 16;

									if (tile.index == 0) {
										draw_rectangle({x * 16.0f, y * 16.0f, 16, 16}, color);
									} else {
										draw_texture_simple(editor.heightmap, src, {x * 16.0f, y * 16.0f}, {}, color, {tile.hflip, tile.vflip});
									}
								}
							}
						}
					}

					// draw hovered brush
					if (tilemap_editor.tool == TOOL_BRUSH) {
						if (i == tilemap_editor.layer_index) {
							ivec2 mouse_pos = view_get_tile_pos(tilemap_editor.tilemap_view,
																{16, 16},
																{editor.tm.width, editor.tm.height},
																ImGui::GetMousePos());

							for (int y = 0; y < tilemap_editor.brush_h; y++) {
								for (int x = 0; x < tilemap_editor.brush_w; x++) {
									if (mouse_pos.x + x >= editor.tm.width)  continue;
									if (mouse_pos.y + y >= editor.tm.height) continue;

									if (tilemap_editor.brush[x + y * tilemap_editor.brush_w].index == 0) {
										continue;
									}

									bool dont_skip_tile_index_0 = false;

									Tile tile;
									Texture t;

									vec4 color = color_white;

									if (tilemap_editor.edit_collision) {
										tile = get_tile(editor.tm, mouse_pos.x + x, mouse_pos.y + y, tilemap_editor.layer_index);
										t = editor.heightmap;
										dont_skip_tile_index_0 = true;

										if (tilemap_editor.brush[x + y * tilemap_editor.brush_w].top_solid && tilemap_editor.brush[x + y * tilemap_editor.brush_w].lrb_solid) {
											color = {1, 1, 1, 1};
										} else if (tilemap_editor.brush[x + y * tilemap_editor.brush_w].top_solid && !tilemap_editor.brush[x + y * tilemap_editor.brush_w].lrb_solid) {
											color = {0.5f, 0.5f, 1, 1};
										} else if (!tilemap_editor.brush[x + y * tilemap_editor.brush_w].top_solid && tilemap_editor.brush[x + y * tilemap_editor.brush_w].lrb_solid) {
											color = {1, 0.5f, 0.5f, 1};
										} else { // !top_solid && !lrb_solid
											color = {0, 0, 0, 1};
										}
									} else {
										tile = tilemap_editor.brush[x + y * tilemap_editor.brush_w];
										t = editor.tileset_texture;
									}

									if (!dont_skip_tile_index_0) {
										if (tile.index == 0) {
											continue;
										}
									}

									Rect src;
									src.x = (tile.index % (editor.tileset_texture.width / 16)) * 16;
									src.y = (tile.index / (editor.tileset_texture.width / 16)) * 16;
									src.w = 16;
									src.h = 16;

									vec2 pos;
									pos.x = (mouse_pos.x + x) * 16;
									pos.y = (mouse_pos.y + y) * 16;

									if (tile.index == 0) { // can only happen in solidity_mode
										draw_rectangle({pos.x, pos.y, 16, 16}, color);
									} else {
										draw_texture_simple(t, src, pos, {}, color, {tile.hflip, tile.vflip});
									}
								}
							}
						}
					}
				}
			};

			draw_layer(3);
			draw_layer(0);
			draw_layer(1);
			draw_layer(2);

			// draw selection
			if (tilemap_editor.tilemap_selection.w > 0 && tilemap_editor.tilemap_selection.h > 0) {
				draw_rectangle_outline_thick({(float)tilemap_editor.tilemap_selection.x * 16, (float)tilemap_editor.tilemap_selection.y * 16, (float)tilemap_editor.tilemap_selection.w * 16, (float)tilemap_editor.tilemap_selection.h * 16},
											 1,
											 color_white);
			}

			// draw objects
			if (tilemap_editor.show_objects) {
				draw_objects(editor.objects, SDL_GetTicks() / (1000.0f / 60.0f), true, false);

				draw_editor_object_gizmos();
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
					 0,
					 callback);

		if (tool == TOOL_SELECT || tool == TOOL_RECTANGLE) {
			if (ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
				ivec2 tile_pos = view_get_tile_pos(tilemap_view,
												   {16, 16},
												   {editor.tm.width, editor.tm.height},
												   ImGui::GetMousePos());

				if (!tilemap_selection.dragging) {
					tilemap_selection.dragging_x1 = tile_pos.x;
					tilemap_selection.dragging_y1 = tile_pos.y;
					tilemap_selection.dragging = true;
				}

				int dragging_x2 = tile_pos.x;
				int dragging_y2 = tile_pos.y;

				tilemap_selection.x = min(tilemap_selection.dragging_x1, dragging_x2);
				tilemap_selection.y = min(tilemap_selection.dragging_y1, dragging_y2);
				tilemap_selection.w = ImAbs(dragging_x2 - tilemap_selection.dragging_x1) + 1;
				tilemap_selection.h = ImAbs(dragging_y2 - tilemap_selection.dragging_y1) + 1;

				if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					rectangle_erasing = false;
				} else {
					rectangle_erasing = true;
				}
			} else {
				if (tilemap_selection.dragging) {
					if (tool == TOOL_RECTANGLE) {
						if (brush_w > 0 && brush_h > 0) {
							Action action = get_rectangle_action();
							editor.action_add_and_perform(action);
						}
						tilemap_selection = {};
					}

					tilemap_selection.dragging = false;
				}
			}
		}

		if (tool == TOOL_SELECT) {
			// handle ctrl+c and ctrl+x
			if ((ImGui::IsKeyPressed(ImGuiKey_C, false) || ImGui::IsKeyPressed(ImGuiKey_X, false)) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
				if (tilemap_selection.w > 0 && tilemap_selection.h > 0) {
					if (layer_visible[layer_index]) {
						Action cut_action = {};
						cut_action.type = ACTION_SET_TILES;

						// copy tiles to brush
						brush.count = 0;
						for (int tile_y = tilemap_selection.y; tile_y < tilemap_selection.y + tilemap_selection.h; tile_y++) {
							for (int tile_x = tilemap_selection.x; tile_x < tilemap_selection.x + tilemap_selection.w; tile_x++) {
								Tile tile = get_tile(editor.tm, tile_x, tile_y, layer_index);
								array_add(&brush, tile);
							
								SetTile set = {};
								set.tile_index = tile_x + tile_y * editor.tm.width;
								set.layer_index = layer_index;
								set.tile_from = tile;
								set.tile_to = {};
								array_add(&cut_action.set_tiles.sets, set);
							}
						}
						brush_w = tilemap_selection.w;
						brush_h = tilemap_selection.h;

						if (ImGui::IsKeyPressed(ImGuiKey_X, false)) {
							editor.action_add_and_perform(cut_action);

							editor.notify(NOTIF_INFO, "Cut %d tiles.", brush_w * brush_h);
						} else {
							free_action(&cut_action);

							editor.notify(NOTIF_INFO, "Copied %d tiles.", brush_w * brush_h);
						}

						tool = TOOL_BRUSH;
						tilemap_selection = {};
					} else {
						editor.notify(NOTIF_WARN, "Trying to cut/copy from invisible layer!");
					}
				}
			}
		}

		bool set_tiles_dragging = false;

		// handle paint and erase
		if (tool == TOOL_BRUSH) {
			if (ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
				set_tiles_dragging = true;

				if (pos_in_rect(ImGui::GetMousePos(), tilemap_view.thing_p0, tilemap_view.thing_p1)) {
					ivec2 mouse_pos = view_get_tile_pos(tilemap_view,
														{16, 16},
														{editor.tm.width, editor.tm.height},
														ImGui::GetMousePos());

					for (int y = 0; y < brush_h; y++) {
						for (int x = 0; x < brush_w; x++) {
							// don't modify invisible layers
							if (!layer_visible[layer_index]) continue;

							// out of bounds
							if ((mouse_pos.x + x) >= editor.tm.width)  continue;
							if ((mouse_pos.y + y) >= editor.tm.height) continue;

							if (brush[x + y * brush_w].index == 0) continue;

							int tile_index = (mouse_pos.x + x) + (mouse_pos.y + y) * editor.tm.width;

							SetTile* found = nullptr;
							For (it, set_tiles) {
								if (it->tile_index == tile_index) {
									found = it;
									break;
								}
							}

							if (found) {
								if (!edit_collision) {
									if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
										found->tile_to = brush[x + y * brush_w];
									} else {
										found->tile_to = {};
									}
								}
							} else {
								SetTile set = {};
								set.tile_index = tile_index;
								set.layer_index = layer_index;
								set.tile_from = get_tile_by_index(editor.tm, tile_index, layer_index);

								if (edit_collision) {
									if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
										set.tile_to = set.tile_from;
										set.tile_to.top_solid = brush[x + y * brush_w].top_solid;
										set.tile_to.lrb_solid = brush[x + y * brush_w].lrb_solid;
									} else {
										set.tile_to = set.tile_from;
										set.tile_to.top_solid = 0;
										set.tile_to.lrb_solid = 0;
									}
								} else {
									if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
										set.tile_to = brush[x + y * brush_w];
									} else {
										set.tile_to = {};
									}
								}

								array_add(&set_tiles, set);
							}
						}
					}
				}
			}
		}

		// shortcuts that modify the brush
		if (tool == TOOL_BRUSH || tool == TOOL_RECTANGLE) {
			if (IsKeyPressedNoMod(ImGuiKey_H)) {
				for (int x = 0; x < brush_w / 2; x++) {
					for (int y = 0; y < brush_h; y++) {
						Tile temp = brush[x + y * brush_w];
						brush[x + y * brush_w] = brush[(brush_w - x - 1) + y * brush_w];
						brush[(brush_w - x - 1) + y * brush_w] = temp;
					}
				}

				For (it, brush) it->hflip ^= 1;
			}

			if (IsKeyPressedNoMod(ImGuiKey_V)) {
				for (int y = 0; y < brush_h / 2; y++) {
					for (int x = 0; x < brush_w; x++) {
						Tile temp = brush[x + y * brush_w];
						brush[x + y * brush_w] = brush[x + (brush_h - y - 1) * brush_w];
						brush[x + (brush_h - y - 1) * brush_w] = temp;
					}
				}

				For (it, brush) it->vflip ^= 1;
			}

			if (IsKeyPressedNoMod(ImGuiKey_T)) {
				For (it, brush) it->top_solid ^= 1;
			}

			if (IsKeyPressedNoMod(ImGuiKey_L)) {
				For (it, brush) it->lrb_solid ^= 1;
			}
		}

		if (!set_tiles_dragging) {
			if (set_tiles.count > 0) {
				Action action = get_brush_action();
				editor.action_add_and_perform(action);

				set_tiles.count = 0;
			}
		}

		ImVec2 cursor = ImGui::GetCursorScreenPos();

		ImGui::Checkbox("Show Objects", &show_objects);
		if (IsKeyPressedNoMod(ImGuiKey_O, true)) show_objects ^= true;
		ImGui::SetItemTooltip("Shortcut: O");

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		ImGui::Checkbox("Highlight Current Layer", &highlight_current_layer);
		if (ImGui::IsKeyPressed(ImGuiKey_H, false) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) highlight_current_layer ^= true;
		ImGui::SetItemTooltip("Shortcut: Ctrl+H");

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		ImGui::Checkbox("Edit Collision", &edit_collision);
		if (IsKeyPressedNoMod(ImGuiKey_C, true)) edit_collision ^= true;
		ImGui::SetItemTooltip("Shortcut: C");

		bottom_part_height = (ImGui::GetCursorScreenPos() - cursor).y;
	};

	tilemap_editor_window();

	auto layers_window = [&]() {
		ImGui::Begin("Layers##tilemap_editor");
		defer { ImGui::End(); };

		auto layer_button = [&](int i, const char* name) {
			const char* icon = layer_visible[i] ? ICON_FA_EYE : ICON_FA_EYE_SLASH;

			ImGui::PushID(i);

			if (SmallIconButtonActive(icon, layer_visible[i])) {
				layer_visible[i] ^= true;
			}

			ImGui::SameLine();

			if (ImGui::Selectable(name, layer_index == i)) {
				layer_index = i;
			}

			ImGui::PopID();
		};

		layer_button(2, "Layer C");
		layer_button(1, "Layer B");
		layer_button(0, "Layer A");
		layer_button(3, "Layer D");
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
			if (tilemap_editor.tileset_selection_w > 0 && tilemap_editor.tileset_selection_h > 0) {
				draw_rectangle_outline_thick({(float)tilemap_editor.tileset_selection_x * 16, (float)tilemap_editor.tileset_selection_y * 16, (float)tilemap_editor.tileset_selection_w * 16, (float)tilemap_editor.tileset_selection_h * 16},
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
					 0,
					 callback);

		if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			ivec2 tile_pos = view_get_tile_pos(tileset_view,
											   {16, 16},
											   {editor.tileset_texture.width / 16, editor.tileset_texture.height / 16},
											   ImGui::GetMousePos());

			if (!tileset_dragging) {
				tileset_dragging_x1 = tile_pos.x;
				tileset_dragging_y1 = tile_pos.y;
				tileset_dragging = true;
			}

			int dragging_x2 = tile_pos.x;
			int dragging_y2 = tile_pos.y;

			tileset_selection_x = min(tileset_dragging_x1, dragging_x2);
			tileset_selection_y = min(tileset_dragging_y1, dragging_y2);
			tileset_selection_w = ImAbs(dragging_x2 - tileset_dragging_x1) + 1;
			tileset_selection_h = ImAbs(dragging_y2 - tileset_dragging_y1) + 1;
		} else {
			if (tileset_dragging) {
				// copy tiles to brush
				brush.count = 0;
				for (int tile_y = tileset_selection_y; tile_y < tileset_selection_y + tileset_selection_h; tile_y++) {
					for (int tile_x = tileset_selection_x; tile_x < tileset_selection_x + tileset_selection_w; tile_x++) {
						int tile_index = tile_x + tile_y * (editor.tileset_texture.width / 16);

						Tile tile = {};
						tile.index = tile_index;
						tile.top_solid = true;
						tile.lrb_solid = true;
						array_add(&brush, tile);
					}
				}
				brush_w = tileset_selection_w;
				brush_h = tileset_selection_h;

				tileset_dragging = false;
			}
		}
	};

	tileset_window();
}

Action TilemapEditor::get_brush_action() {
	Assert(tool == TOOL_BRUSH);

	Action action = {};
	action.type = ACTION_SET_TILES;

	// copy array
	For (it, set_tiles) {
		array_add(&action.set_tiles.sets, *it);
	}

	return action;
}

Action TilemapEditor::get_rectangle_action() {
	Assert(tool == TOOL_RECTANGLE);

	Action action = {};
	action.type = ACTION_SET_TILES;

	if (brush_w > 0 && brush_h > 0) {
		int brush_y = 0;
		for (int y = tilemap_selection.y; y < tilemap_selection.y + tilemap_selection.h; y++) {
			int brush_x = 0;
			for (int x = tilemap_selection.x; x < tilemap_selection.x + tilemap_selection.w; x++) {
				SetTile set = {};
				set.tile_index = x + y * editor.tm.width;
				set.layer_index = layer_index;
				set.tile_from = get_tile(editor.tm, x, y, layer_index);

				if (edit_collision) {
					if (rectangle_erasing) {
						set.tile_to = set.tile_from;
						set.tile_to.top_solid = 0;
						set.tile_to.lrb_solid = 0;
					} else {
						set.tile_to = set.tile_from;
						set.tile_to.top_solid = brush[brush_x + brush_y * brush_w].top_solid;
						set.tile_to.lrb_solid = brush[brush_x + brush_y * brush_w].lrb_solid;
					}
				} else {
					if (rectangle_erasing) {
						set.tile_to = {};
					} else {
						set.tile_to = brush[brush_x + brush_y * brush_w];
					}
				}

				array_add(&action.set_tiles.sets, set);

				brush_x++;
				brush_x %= brush_w;
			}

			brush_y++;
			brush_y %= brush_h;
		}
	}

	return action;
}

void ObjectsEditor::update(float delta) {
	auto objects_editor_window = [&]() {
		ImGui::Begin("Objects Editor##objects_editor");
		defer { ImGui::End(); };

		if (!editor.is_level_open) {
			ImGui::Text("No level opened.");
			return;
		}

		auto callback = []() {
			ObjectsEditor& objects_editor = editor.objects_editor;

			// reuse the same view
			View& tilemap_view = editor.tilemap_editor.tilemap_view;

			ivec2 pos_from = view_get_tile_pos(tilemap_view, {16, 16}, {editor.tm.width, editor.tm.height}, tilemap_view.item_rect_min);
			ivec2 pos_to   = view_get_tile_pos(tilemap_view, {16, 16}, {editor.tm.width, editor.tm.height}, tilemap_view.item_rect_max);
			pos_to.x++;
			pos_to.y++;

			// draw layers
			draw_tilemap_layer(editor.tm, 3, editor.tileset_texture, pos_from.x, pos_from.y, pos_to.x, pos_to.y, color_white);
			draw_tilemap_layer(editor.tm, 0, editor.tileset_texture, pos_from.x, pos_from.y, pos_to.x, pos_to.y, color_white);
			draw_tilemap_layer(editor.tm, 2, editor.tileset_texture, pos_from.x, pos_from.y, pos_to.x, pos_to.y, color_white);

			draw_objects(editor.objects, SDL_GetTicks() / (1000.0f / 60.0f), true, false);

			draw_editor_object_gizmos();

			// selected object
			if (objects_editor.object_index != -1) {
				const Object& o = editor.objects[objects_editor.object_index];
				vec2 size = get_object_size(o);

				draw_rectangle_outline_thick({o.pos.x - size.x/2, o.pos.y - size.y/2, size.x, size.y},
											 1,
											 color_white);
			}

			// draw grids
			{
				if (tilemap_view.zoom > 2) {
					draw_grid_exact({16, 16},
									{editor.tm.width, editor.tm.height},
									get_color(0xffffff80));
				}

				draw_grid_exact({256, 256},
								{editor.tm.width / 16, editor.tm.height / 16},
								color_white);
			}

			break_batch();
		};

		// reuse the same view
		View& tilemap_view = editor.tilemap_editor.tilemap_view;

		u32 pan_and_zoom_flags = 0;

		// move object with arrow keys
		{
			static vec2 pos = {-9999, -9999};
			static bool modified = false;

			if (object_index != -1) {
				Object& obj = editor.objects[object_index];

				if (ImGui::GetActiveID() == 0 && ImGui::IsWindowFocused()) {
					if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
						modified = true;
						if (pos.x == -9999) pos = obj.pos;

						if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
							obj.pos.y = ceilf_to(obj.pos.y, 16);
							obj.pos.y -= 16;
						} else {
							obj.pos.y -= 1;
						}
					}

					if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
						modified = true;
						if (pos.x == -9999) pos = obj.pos;

						if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
							obj.pos.y = floorf_to(obj.pos.y, 16);
							obj.pos.y += 16;
						} else {
							obj.pos.y += 1;
						}
					}

					if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
						modified = true;
						if (pos.x == -9999) pos = obj.pos;

						if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
							obj.pos.x = ceilf_to(obj.pos.x, 16);
							obj.pos.x -= 16;
						} else {
							obj.pos.x -= 1;
						}
					}

					if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
						modified = true;
						if (pos.x == -9999) pos = obj.pos;

						if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
							obj.pos.x = floorf_to(obj.pos.x, 16);
							obj.pos.x += 16;
						} else {
							obj.pos.x += 1;
						}
					}

					pan_and_zoom_flags |= DONT_MOVE_VIEW_WITH_ARROW_KEYS;
				}

				if (!ImGui::IsKeyDown(ImGuiKey_UpArrow)
					&& !ImGui::IsKeyDown(ImGuiKey_DownArrow)
					&& !ImGui::IsKeyDown(ImGuiKey_LeftArrow)
					&& !ImGui::IsKeyDown(ImGuiKey_RightArrow))
				{
					if (modified) {
						if (pos != obj.pos) {
							Action action = {};
							action.type = ACTION_SET_OBJECT_FIELD;
							action.set_object_field.index = object_index;
							action.set_object_field.field_offset = offsetof(Object, pos);
							action.set_object_field.field_size = sizeof(vec2);

							static_assert(sizeof(vec2) <= sizeof(action.set_object_field.data_from));

							memcpy(action.set_object_field.data_from, &pos, sizeof(vec2));
							memcpy(action.set_object_field.data_to, &obj.pos, sizeof(vec2));

							editor.action_add_and_perform(action);
						}

						modified = false;
						pos = {-9999, -9999};
					}
				}
			}
		}

		pan_and_zoom(tilemap_view,
					 {},
					 {editor.tm.width * 16, editor.tm.height * 16},
					 pan_and_zoom_flags,
					 callback);

		// add objects
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ADD_OBJECT")) {
				ObjType type;
				Assert(payload->DataSize == sizeof(type));
				type = *(ObjType*)payload->Data;

				vec2 pos = floor(view_get_pos(tilemap_view, ImGui::GetMousePos()));

				Object o = {};
				o.pos = pos;
				o.type = type;

				switch (o.type) {
					case OBJ_SPRING:
					case OBJ_SPRING_DIAGONAL: {
						o.spring.direction = DIR_UP;
						break;
					}

					case OBJ_SPIKE: {
						o.spike.direction = DIR_UP;
						break;
					}

					case OBJ_LAYER_SWITCHER_VERTICAL: {
						o.layswitch.radius.y = 128;
						o.layswitch.priority_1 = 1;
						o.layswitch.priority_2 = 1;
						break;
					}

					case OBJ_LAYER_SWITCHER_HORIZONTAL: {
						o.layswitch.radius.x = 128;
						o.layswitch.priority_1 = 1;
						o.layswitch.priority_2 = 1;
						break;
					}

					case OBJ_MOVING_PLATFORM: {
						o.mplatform.radius = {32, 6};
						o.mplatform.offset = {0, 32};
						o.mplatform.time_multiplier = 1.0f / 32.0f;
						break;
					}
				}

				Action action = {};
				action.type = ACTION_ADD_OBJECT;
				action.add_object.o = o;

				editor.action_add_and_perform(action);
			}

			ImGui::EndDragDropTarget();
		}

		// drag object position
		{
			static bool dragging;
			static vec2 drag_offset;
			static vec2 pos;

			if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				if (dragging) {
					Object& obj = editor.objects[object_index];
					vec2 mouse_pos = view_get_pos(tilemap_view, ImGui::GetMousePos());

					obj.pos = floor(mouse_pos + drag_offset);

					if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
						obj.pos.x = floorf_to(obj.pos.x, 16);
						obj.pos.y = floorf_to(obj.pos.y, 16);
					}
				} else {
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
						int new_object_index = -1;
						float closest_dist = INFINITY;
						for (int i = 0; i < editor.objects.count; i++) {
							const Object& o = editor.objects[i];
							vec2 size = get_object_size(o);
							vec2 mouse_pos = view_get_pos(tilemap_view, ImGui::GetMousePos());

							if (point_in_rect(mouse_pos, {o.pos.x - size.x/2, o.pos.y - size.y/2, size.x, size.y})) {
								float dist = point_distance(mouse_pos, o.pos);
								if (dist < closest_dist) {
									closest_dist = dist;
									new_object_index = i;
								}
							}
						}

						if (object_index == new_object_index && object_index != -1) {
							dragging = true;
							drag_offset = editor.objects[object_index].pos - view_get_pos(tilemap_view, ImGui::GetMousePos());
							pos = editor.objects[object_index].pos;
						}

						object_index = new_object_index;
					}
				}
			} else {
				if (dragging) {
					Assert(object_index != -1);

					Object& obj = editor.objects[object_index];

					if (pos != obj.pos) {
						Action action = {};
						action.type = ACTION_SET_OBJECT_FIELD;
						action.set_object_field.index = object_index;
						action.set_object_field.field_offset = offsetof(Object, pos);
						action.set_object_field.field_size = sizeof(vec2);

						static_assert(sizeof(vec2) <= sizeof(action.set_object_field.data_from));

						memcpy(action.set_object_field.data_from, &pos, sizeof(vec2));
						memcpy(action.set_object_field.data_to, &obj.pos, sizeof(vec2));

						editor.action_add_and_perform(action);
					}

					dragging = false;
					drag_offset = {};
					pos = {};
				}
			}
		}
	};

	objects_editor_window();

	auto object_types_window = [&]() {
		ImGui::Begin("Object Types##object_editor");
		defer { ImGui::End(); };

		int id = 0;

		auto button = [&](ObjType type) {
			const Sprite& s = get_object_sprite(type);

			if (s.width + ImGui::GetStyle().FramePadding.x * 2 > ImGui::GetContentRegionAvail().x) {
				ImGui::NewLine();
			}

			ImGui::PushID(id++);
			ImGui::ImageButton("object button", s.texture.id, ImVec2(s.width, s.height), sprite_get_uv0(s, 0), sprite_get_uv1(s, 0));
			ImGui::PopID();

			ImGui::SameLine();

			ImGui::SetItemTooltip("%s", GetObjTypeName(type));

			if (ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("ADD_OBJECT", &type, sizeof(type));

				ImGui::Text("%s", GetObjTypeName(type));

				ImGui::EndDragDropSource();
			}
		};

		button(OBJ_PLAYER_INIT_POS);
		button(OBJ_RING);
		button(OBJ_MONITOR);
		button(OBJ_SPRING);
		button(OBJ_SPRING_DIAGONAL);
		button(OBJ_SPIKE);
		button(OBJ_MOVING_PLATFORM);
		button(OBJ_LAYER_SWITCHER_VERTICAL);
		button(OBJ_LAYER_SWITCHER_HORIZONTAL);
		button(OBJ_MOSQUI);
	};

	object_types_window();

	auto objects_window = [&]() {
		ImGui::Begin("Objects##object_editor");
		defer { ImGui::End(); };

		for (int i = 0; i < editor.objects.count; i++) {
			char buf[64];
			stb_snprintf(buf, sizeof(buf), "%d: %s", i, GetObjTypeName(editor.objects[i].type));

			if (ImGui::Selectable(buf, i == object_index)) {
				object_index = i;
			}

			/*if (object_index == i) {
				ImGui::SetItemDefaultFocus();
			}*/
		}
	};

	objects_window();

	auto object_properties_window = [&]() {
		ImGui::Begin("Object Properties##object_editor");
		defer { ImGui::End(); };

		if (object_index == -1) {
			return;
		}

		Object* o = &editor.objects[object_index];

		ImGui::Text("ID: %d", object_index);

		UndoableDragFloat2("Position", object_index, offsetof(Object, pos), 1, 0, 0, "%.0f");

		switch (o->type) {
			case OBJ_MONITOR: {
				UndoableComboEnum("Monitor Icon",
								  GetMonitorIconName,
								  object_index,
								  offsetof(Object, monitor.icon),
								  NUM_MONITOR_ICONS);
				break;
			}

			case OBJ_SPRING:
			case OBJ_SPRING_DIAGONAL: {
				UndoableComboEnum("Spring Color",
								  GetSpringColorName,
								  object_index,
								  offsetof(Object, spring.color),
								  NUM_SPING_COLORS);

				UndoableComboEnum("Direction",
								  GetDirectionName,
								  object_index,
								  offsetof(Object, spring.direction),
								  NUM_DIRS);

				UndoableCheckboxFlags("Small Hitbox", object_index, offsetof(Object, flags), FLAG_SPRING_SMALL_HITBOX);
				break;
			}

			case OBJ_SPIKE: {
				UndoableComboEnum("Direction",
								  GetDirectionName,
								  object_index,
								  offsetof(Object, spike.direction),
								  NUM_DIRS);
				break;
			}

			case OBJ_LAYER_SWITCHER_VERTICAL:
			case OBJ_LAYER_SWITCHER_HORIZONTAL: {
				if (o->type == OBJ_LAYER_SWITCHER_VERTICAL) {
					UndoableDragFloat("Height Radius", object_index, offsetof(Object, layswitch.radius.y));
				} else {
					UndoableDragFloat("Width Radius", object_index, offsetof(Object, layswitch.radius.x));
				}

				{
					const char* values[] = {"A", "B"};
					UndoableCombo("Layer 1", object_index, offsetof(Object, layswitch.layer_1), values, ArrayLength(values));
					UndoableCombo("Layer 2", object_index, offsetof(Object, layswitch.layer_2), values, ArrayLength(values));
				}

				{
					const char* values[] = {"Low", "High"};
					UndoableCombo("Priority 1", object_index, offsetof(Object, layswitch.priority_1), values, ArrayLength(values));
					UndoableCombo("Priority 2", object_index, offsetof(Object, layswitch.priority_2), values, ArrayLength(values));
				}

				UndoableCheckboxFlags("Ground Only", object_index, offsetof(Object, flags), FLAG_LAYER_SWITCHER_GROUND_ONLY);
				break;
			}

			case OBJ_MOVING_PLATFORM: {
				{
					// TODO: this code used to set default radius when you changed the sprite index
					// o->mplatform.radius = {32, 6};
					// o->mplatform.radius = {64, 14};
					const char* values[] = {"spr_EEZ_platform1", "spr_EEZ_platform2"};
					UndoableCombo("Sprite Index", object_index, offsetof(Object, mplatform.sprite_index), values, ArrayLength(values));
				}

				UndoableDragFloat2("Radius", object_index, offsetof(Object, mplatform.radius));
				UndoableDragFloat2("Offset", object_index, offsetof(Object, mplatform.offset));
				UndoableInputFloat("Time Multiplier", object_index, offsetof(Object, mplatform.time_multiplier));
				break;
			}
		}

		if (ImGui::Button("Remove Object")) {
			Action action = {};
			action.type = ACTION_REMOVE_OBJECT;
			action.remove_object.index = object_index;
			action.remove_object.o = editor.objects[object_index];

			editor.action_add_and_perform(action);
			return;
		}
	};

	object_properties_window();
}

void Editor::draw(float delta) {
}

void Editor::update_window_caption() {
	if (!is_level_open) {
		SDL_SetWindowTitle(get_window_handle(), "Editor");
		return;
	}

	const char* prefix = "";
	if (action_index != saved_action_index) {
		prefix = "*";
	}

	char buf[512];
	stb_snprintf(buf, sizeof(buf), "%sEditor - (%s)", prefix, current_level_dir.u8string().c_str());
	SDL_SetWindowTitle(get_window_handle(), buf);
}

#endif
