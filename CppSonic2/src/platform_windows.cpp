#include "platform.h"

#ifdef _WIN32

// @Cleanup
// "Remove" macro from common.h conflicts with Winapi
#undef Remove

#define NOMINMAX
#include <Windows.h>

#include <SDL_syswm.h>

string get_open_file_name(const char* filter) {
	SDL_Window* window = get_window_handle();

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	HWND hwnd = NULL;  // owner window
	if (SDL_GetWindowWMInfo(window, &info)) {
		hwnd = info.info.win.window;
	}

	OPENFILENAMEA ofn;                 // common dialog box structure
	static char szFile[260];           // buffer for file name

	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = szFile;
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
	// use the contents of szFile to initialize itself.
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	// Display the Open dialog box. 

	if (GetOpenFileNameA(&ofn) == TRUE) {
		return {ofn.lpstrFile, strlen(ofn.lpstrFile)};
	} else {
		return {};
	}
}

#endif
