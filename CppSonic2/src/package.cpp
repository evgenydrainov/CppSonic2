#include "package.h"

Package package;

void init_package() {
	package.filedata = (u8*) malloc(package.MAX_FILESIZE);
	Assert(package.filedata);
}

void deinit_package() {
	free(package.filedata);
}

u8* get_file(const char* fname, size_t* out_filesize) {
	SDL_RWops* f = SDL_RWFromFile(fname, "rb");

	if (!f) {
		log_error("Couldn't open file \"%s\"", fname);
		return nullptr;
	}

	defer { SDL_RWclose(f); };

	SDL_RWseek(f, 0, RW_SEEK_END);
	size_t filesize = (size_t) SDL_RWtell(f);

	Assert(filesize <= package.MAX_FILESIZE);

	SDL_RWseek(f, 0, RW_SEEK_SET);
	SDL_RWread(f, package.filedata, filesize, 1);

	*out_filesize = filesize;
	return package.filedata;
}

string get_file_str(const char* fname) {
	size_t filesize;
	u8* filedata = get_file(fname, &filesize);

	if (filedata) {
		return {(char*) filedata, filesize};
	} else {
		return {}; // Make sure result.count is zero
	}
}

array<u8> get_file_arr(const char* fname) {
	size_t filesize;
	u8* filedata = get_file(fname, &filesize);

	if (filedata) {
		return {filedata, filesize};
	} else {
		return {}; // Make sure result.count is zero
	}
}
