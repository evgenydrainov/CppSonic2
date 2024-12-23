#include "title_screen.h"

#include "window_creation.h"
#include "renderer.h"
#include "font.h"
#include "assets.h"
#include "program.h"

Title_Screen title_screen;

void Title_Screen::init() {
}

void Title_Screen::deinit() {
}

void Title_Screen::update(float delta) {
	if (program.transition_t != 0) {
		return;
	}

	if (is_key_pressed(SDL_SCANCODE_Z)) {
		program.set_program_mode(PROGRAM_GAME);
	}
}

void Title_Screen::draw(float delta) {
	break_batch();
	renderer.proj_mat = glm::ortho<float>(0, window.game_width, window.game_height, 0);
	renderer.view_mat = {1};
	renderer.model_mat = {1};
	glViewport(0, 0, window.game_width, window.game_height);

	const Font& font = get_font(fnt_menu);

	vec2 pos = {window.game_width / 2, window.game_height / 2};

	draw_text(font, "this is the title screen.", pos, HALIGN_CENTER, VALIGN_MIDDLE);
	pos.y += 40;

	draw_text(font, "press z to start", pos, HALIGN_CENTER, VALIGN_MIDDLE);
	pos.y += 40;

	break_batch();
}
