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
}

void Game::deinit() {

}

void Game::update(float delta) {

}

void Game::draw(float delta) {
	renderer.proj_mat = glm::ortho<float>(0, window.game_width, window.game_height, 0);

	draw_texture(tileset_texture);

	draw_text(font, "Hello, World!", {10, 10});
}
