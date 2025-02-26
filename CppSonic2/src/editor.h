#pragma once

#ifdef EDITOR

#include "common.h"
#include "texture.h"
#include "game.h"

#include <filesystem>

struct View {
	vec2 scrolling;
	float zoom = 1;

	// The thing that is being panned and zoomed inside this view.
	vec2 thing_p0;
	vec2 thing_p1;
};

#define ACTION_TYPE_ENUM(X) \
	X(ACTION_SET_TILE_HEIGHT)

DEFINE_NAMED_ENUM(ActionType, ACTION_TYPE_ENUM)

struct SetTileHeight {
	int tile_index;
	int in_tile_pos_x;
	int height_from;
	int height_to;
};

struct Action {
	ActionType type;
	union {
		struct {
			dynamic_array<SetTileHeight> sets;
		} set_tile_height;
	};
};

void free_action(Action* action);

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

	dynamic_array<SetTileHeight> set_tile_heights;

	void update(float delta);
};

struct TilemapEditor {
	enum Tool {
		TOOL_NONE,
		TOOL_BRUSH,
		TOOL_ERASER,
	};

	Tool tool;
	View tilemap_view;

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
	int action_index = -1;

	bool show_demo_window;
	bool show_undo_history_window;

	const char* process_name;

	void init(int argc, char* argv[]);
	void deinit();
	void update(float delta);
	void draw(float delta);

	void update_window_caption();

	void pick_and_open_level();
	void close_level();
	void try_undo();
	void try_redo();
	void action_add(const Action& action);
	bool try_run_game();
};

extern Editor editor;

#endif
