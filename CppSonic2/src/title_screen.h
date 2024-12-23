#pragma once

#include "common.h"

struct Title_Screen {
	void init();
	void deinit();

	void update(float delta);
	void draw(float delta);
};

extern Title_Screen title_screen;
