#include "game.h"

#include "batch_renderer.h"
#include "window_creation.h"

Game game;

void Game::init() {
	font = load_bmfont_file("fonts/ms_gothic.fnt", "fonts/ms_gothic_0.png");
}

void Game::deinit() {

}

void Game::update(float delta) {

}

void Game::draw(float delta) {
	renderer.proj_mat = glm::ortho<float>(0, window.game_width, window.game_height, 0);

	draw_text(font, "Hello, World!", {10, 10});
}
