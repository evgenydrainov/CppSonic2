#include "bunnymark.h"
#include "texture.h"
#include "input.h"
#include "window_creation.h"
#include "font.h"
#include "assets.h"

Bunnymark bunnymark;

#define MAX_BUNNIES (150'000)

static int GetRandomValue(int min, int max) {
	return min + rand() % (max - min + 1);
}

void Bunnymark::init() {
	texBunny = load_texture_from_file("textures/wabbit_alpha.png");
	bunnies = (Bunny*) malloc(MAX_BUNNIES * sizeof(Bunny));
}

void Bunnymark::deinit() {
	free_texture(&texBunny);
	free(bunnies);
}

void Bunnymark::update(float delta) {
	if (is_mouse_button_held(SDL_BUTTON_LEFT)) {
		for (int i = 0; i < 1000; i++) {
			if (bunniesCount < MAX_BUNNIES) {
				bunnies[bunniesCount].position = input.mouse_world_pos;
				bunnies[bunniesCount].speed.x = (float)GetRandomValue(-250, 250)/60.0f;
				bunnies[bunniesCount].speed.y = (float)GetRandomValue(-250, 250)/60.0f;
				bunnies[bunniesCount].color = get_color(GetRandomValue(50, 240),
													GetRandomValue(80, 240),
													GetRandomValue(100, 240), 255);
				bunniesCount++;
			}
		}
	}

	for (int i = 0; i < bunniesCount; i++) {
        bunnies[i].position.x += bunnies[i].speed.x;
        bunnies[i].position.y += bunnies[i].speed.y;

        if (((bunnies[i].position.x + texBunny.width/2) > window.game_width) ||
            ((bunnies[i].position.x + texBunny.width/2) < 0)) bunnies[i].speed.x *= -1;
        if (((bunnies[i].position.y + texBunny.height/2) > window.game_height) ||
            ((bunnies[i].position.y + texBunny.height/2 - 40) < 0)) bunnies[i].speed.y *= -1;
    }
}

void Bunnymark::draw(float delta) {
	render_clear_color(get_color(245, 245, 245));

	for (int i = 0; i < bunniesCount; i++) {
		draw_texture_simple(texBunny, {}, bunnies[i].position, {}, bunnies[i].color);
		// draw_texture(texBunny, {}, bunnies[i].position, {1,1}, {}, 0, bunnies[i].color);
    }

	draw_rectangle(Rectf{0, 0, (float)window.game_width, 40}, color_black);

	{
		string text = tprintf("%.2f fps", window.fps);
		draw_text(get_font(fnt_menu), text, {10, 10}, HALIGN_LEFT, VALIGN_TOP, color_white);
	}

	{
		string text = tprintf("bunnies: %i", bunniesCount);
		draw_text(get_font(fnt_menu), text, {120, 10}, HALIGN_LEFT, VALIGN_TOP, get_color(0, 228, 48));
	}

	{
		string text = tprintf("batched draw calls: %i", renderer.draw_calls);
		draw_text(get_font(fnt_menu), text, {320, 10}, HALIGN_LEFT, VALIGN_TOP, get_color(190, 33, 55));
	}
}
