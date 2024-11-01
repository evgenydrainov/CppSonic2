#include "game.h"

#include "batch_renderer.h"
#include "window_creation.h"

Game game;

void Game::load_level(const char* path) {
	{
		char buf[512];
		stb_snprintf(buf, sizeof(buf), "%s/Tileset.png", path);

		tileset_texture = load_texture_from_file(buf);
	}

	
}

void Game::init() {
	font = load_bmfont_file("fonts/ms_gothic.fnt", "fonts/ms_gothic_0.png");

	load_level("levels/GHZ_Act1");

	// @Leak
	texture_crouch   = load_texture_from_file("textures/crouch.png");
	texture_idle     = load_texture_from_file("textures/idle.png");
	texture_look_up  = load_texture_from_file("textures/look_up.png");
	texture_peelout  = load_texture_from_file("textures/peelout.png");
	texture_roll     = load_texture_from_file("textures/roll.png");
	texture_run      = load_texture_from_file("textures/run.png");
	texture_skid     = load_texture_from_file("textures/skid.png");
	texture_spindash = load_texture_from_file("textures/spindash.png");
	texture_walk     = load_texture_from_file("textures/walk.png");

	player.pos.x = window.game_width  / 2.0f;
	player.pos.y = window.game_height / 2.0f;
}

void Game::deinit() {

}

void Game::update(float delta) {

}

void Game::draw(float delta) {
	renderer.proj_mat = glm::ortho<float>(0, window.game_width, window.game_height, 0);

	draw_texture_centered(texture_idle, player.pos);
}
