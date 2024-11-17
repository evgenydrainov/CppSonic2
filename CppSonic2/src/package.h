#pragma once

#include "common.h"

struct Package {
	static constexpr size_t MAX_FILESIZE = Megabytes(10);

	u8* filedata;
};

extern Package package;

void init_package();
void deinit_package();

u8* get_file(const char* fname, size_t* out_filesize);
string get_file_str(const char* fname);
array<u8> get_file_arr(const char* fname);
