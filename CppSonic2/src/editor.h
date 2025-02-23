#pragma once

#ifdef EDITOR

#include "common.h"
#include "texture.h"
#include "game.h"

#include <filesystem>

struct View {
	vec2 scrolling;
	float zoom = 1;

	vec2 item_screen_p0;
	vec2 item_screen_p1;
};

#define ACTION_TYPE_ENUM(X) \
	X(ACTION_SET_TILE_HEIGHT)

DEFINE_NAMED_ENUM(ActionType, ACTION_TYPE_ENUM)

struct Action {
	ActionType type;
	union {
		struct {
			int tile_index;
			int in_tile_pos_x;
			int height_from;
			int height_to;
		} set_tile_height;
	};
};

struct TilesetEditor {
	enum Mode {
		MODE_NONE,
		MODE_HEIGHTS,
		MODE_WIDTHS,
		MODE_ANGLES,
	};

	enum Tool {
		TOOL_NONE,
		TOOL_BRUSH,
		TOOL_ERASER,
		TOOL_AUTO,
	};

	Mode mode;
	Tool tool;
	View tileset_view;

	void update(float delta);
};

struct TilemapEditor {
	void update(float delta);
};

struct ObjectsEditor {
	void update(float delta);
};

struct Editor {
	enum State {
		STATE_TILESET_EDITOR,
		STATE_TILEMAP_EDITOR,
		STATE_OBJECTS_EDITOR,
	};

	State state;

	TilesetEditor tileset_editor;
	TilemapEditor tilemap_editor;
	ObjectsEditor objects_editor;

	std::filesystem::path current_level_dir;
	bool is_level_open;

	Texture tileset_texture;
	SDL_Surface* tileset_surface;

	Tilemap tm;
	Tileset ts;
	bump_array<Object> objects;

	Texture heightmap;
	Texture widthmap;

	bump_array<Action> actions;

	bool show_demo_window;

	void init(int argc, char* argv[]);
	void deinit();
	void update(float delta);
	void draw(float delta);

	void update_window_caption();

	void pick_and_open_level();
	void close_level();
	void try_undo();
};

extern Editor editor;

#endif
