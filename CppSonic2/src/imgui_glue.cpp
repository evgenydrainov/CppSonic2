#include "imgui_glue.h"

#ifndef IMGUI_DISABLE

#include "window_creation.h"

#undef Remove

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_opengl3.h"

#include "IconsFontAwesome5.h"

// don't assert on failure
static ImFont* AddFontFromFileTTF(ImFontAtlas* atlas, const char* filename, float size_pixels, const ImFontConfig* font_cfg_template, const ImWchar* glyph_ranges)
{
	IM_ASSERT(!atlas->Locked && "Cannot modify a locked ImFontAtlas between NewFrame() and EndFrame/Render()!");
	size_t data_size = 0;
	void* data = ImFileLoadToMemory(filename, "rb", &data_size, 0);
	if (!data)
	{
		// IM_ASSERT_USER_ERROR(0, "Could not load font file!");
		return NULL;
	}
	ImFontConfig font_cfg = font_cfg_template ? *font_cfg_template : ImFontConfig();
	if (font_cfg.Name[0] == '\0')
	{
		// Store a short copy of filename into into the font name for convenience
		const char* p;
		for (p = filename + strlen(filename); p > filename && p[-1] != '/' && p[-1] != '\\'; p--) {}
		ImFormatString(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "%s, %.0fpx", p, size_pixels);
	}
	return atlas->AddFontFromMemoryTTF(data, (int)data_size, size_pixels, &font_cfg, glyph_ranges);
}

void init_imgui() {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	//io.ConfigDockingWithShift = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowMinSize = ImVec2(200, 200);

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window.handle, window.gl_context);
	ImGui_ImplOpenGL3_Init();

	// Load Fonts
	{
#ifdef _WIN32
		ImFont* font = AddFontFromFileTTF(io.Fonts, "C:\\Windows\\Fonts\\segoeui.ttf", 18, nullptr, io.Fonts->GetGlyphRangesDefault());
#else
		ImFont* font = AddFontFromFileTTF(io.Fonts, "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf", 18, nullptr, io.Fonts->GetGlyphRangesDefault());
#endif

		if (!font) {
			font = AddFontFromFileTTF(io.Fonts, "segoeui.ttf", 18, nullptr, io.Fonts->GetGlyphRangesDefault());
		}

		if (!font) {
			font = io.Fonts->AddFontDefault();
		}

		float baseFontSize = 13.0f; // 13.0f is the size of the default font. Change to the font size you use.
		float iconFontSize = baseFontSize; // * 2.0f / 3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly

		static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };

		ImFontConfig icons_config;
		icons_config.MergeMode = true;
		icons_config.PixelSnapH = true;
		icons_config.GlyphMinAdvanceX = iconFontSize;
		AddFontFromFileTTF(io.Fonts, "fonts/" FONT_ICON_FILE_NAME_FAS, iconFontSize, &icons_config, icons_ranges); // Merge into first font

		io.Fonts->Build();
	}
}

void deinit_imgui() {
	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

bool imgui_handle_event(const SDL_Event& ev) {
	return ImGui_ImplSDL2_ProcessEvent(&ev);
}

void imgui_begin_frame() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void imgui_render() {
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	ImGuiIO& io = ImGui::GetIO();

	// Rendering
	ImGui::Render();
	glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
	//glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
	//glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

#endif
