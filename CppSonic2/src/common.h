#pragma once


// -----------------------------------------------------------
//                 SECTION: Common types.
// -----------------------------------------------------------

#if !defined(_DEBUG) && !defined(NDEBUG)
#error You have to define _DEBUG or NDEBUG.
#endif

// #define PRETEND_MOBILE

#include <SDL.h>
#include <stb/stb_sprintf.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/gl.h>

#include <math.h>
#include <stdint.h>
#include <stdarg.h>   // for va_list
#include <string.h>   // for memcpy

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef ptrdiff_t ssize_t;

using glm::vec2;
using glm::vec3;
using glm::vec4;

using glm::mat4;

struct Rect {
	int x;
	int y;
	int w;
	int h;
};

struct Rectf {
	float x;
	float y;
	float w;
	float h;
};

#define ArrayLength(a) (sizeof(a) / sizeof(a[0]))

#define Kilobytes(x) ((size_t)((x) * (size_t)1024))
#define Megabytes(x) Kilobytes((x) * (size_t)1024)
#define Gigabytes(x) Megabytes((x) * (size_t)1024)

// 
// Logging and assert
// 

#define log_info(fmt, ...)  log_internal(SDL_LOG_PRIORITY_INFO,  fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)  log_internal(SDL_LOG_PRIORITY_WARN,  fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) log_internal(SDL_LOG_PRIORITY_ERROR, fmt, ##__VA_ARGS__)

inline void SDL_PRINTF_VARARG_FUNC(2) log_internal(SDL_LogPriority priority, SDL_PRINTF_FORMAT_STRING const char* fmt, ...) {
#if 1
	static char buf[512];

	// SDL_snprintf doesn't support %g.
	va_list va;
	va_start(va, fmt);
	stb_vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "%s", buf);
#else
	static char buf[STB_SPRINTF_MIN];

	switch (priority) {
		case SDL_LOG_PRIORITY_VERBOSE: {
			static const char buf[] = "VERBOSE: ";
			fwrite(buf, 1, sizeof buf, stdout);
			break;
		}

		case SDL_LOG_PRIORITY_DEBUG: {
			static const char buf[] = "DEBUG: ";
			fwrite(buf, 1, sizeof buf, stdout);
			break;
		}

		case SDL_LOG_PRIORITY_INFO: {
			static const char buf[] = "INFO: ";
			fwrite(buf, 1, sizeof buf, stdout);
			break;
		}

		case SDL_LOG_PRIORITY_WARN: {
			static const char buf[] = "WARN: ";
			fwrite(buf, 1, sizeof buf, stdout);
			break;
		}

		case SDL_LOG_PRIORITY_ERROR: {
			static const char buf[] = "ERROR: ";
			fwrite(buf, 1, sizeof buf, stdout);
			break;
		}

		case SDL_LOG_PRIORITY_CRITICAL: {
			static const char buf[] = "CRITICAL: ";
			fwrite(buf, 1, sizeof buf, stdout);
			break;
		}
	}

	va_list va;
	va_start(va, fmt);

	stb_vsprintfcb([](const char *buf, void *user, int len) -> char* {
		char* orig_buf = (char*) user;

		fwrite(buf, 1, len, stdout);

		return orig_buf;
	}, buf, buf, fmt, va);

	va_end(va);

	char newl = '\n';
	fwrite(&newl, 1, 1, stdout);
#endif
}

//
// For now, asserts will be enabled in release build.
//
#ifdef NDEBUG
	#define Assert(expr) while (!(expr)) panic_and_abort("Assertion Failed: " #expr)
#else
	#define Assert(expr) while (!(expr)) { try_to_exit_fullscreen_properly(); SDL_TriggerBreakpoint(); }
#endif

#define panic_and_abort(fmt, ...) _panic_and_abort(__FILE__ ":" STRINGIFY(__LINE__) ": " fmt, ##__VA_ARGS__)

extern SDL_Window* get_window_handle(); // defined in window_creation.h

inline void try_to_exit_fullscreen_properly() {
	if (SDL_Window* window = get_window_handle()) {
		SDL_SetWindowFullscreen(window, 0);

		// NOTE: if you leave fullscreen right before triggering the debugger or showing a message box,
		// then you have to respond to the OS events here, otherwise you won't actually leave fullscreen and get a stuck window.
		// At least, that was the case before I updated my GPU driver I think.

		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {}
	}
}

SDL_NORETURN inline void _panic_and_abort(const char* fmt, ...) {
	char buf[512];

	va_list va;
	va_start(va, fmt);
	stb_vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	log_error("%s", buf);

	try_to_exit_fullscreen_properly();
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", buf, get_window_handle());

	SDL_Quit();
	exit(1);
}


// 
// For loop
// 

#define For(it, arr)    for (auto it = arr.begin(); it != arr.end(); it++)
#define Remove(it, arr) (it = array_remove(&arr, it), it--)

#define Repeat(n)       for (int CONCAT(_i__, __LINE__) = (int)(n); CONCAT(_i__, __LINE__)--;)

#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)

#define STRINGIFY_INTERNAL(x) #x
#define STRINGIFY(x) STRINGIFY_INTERNAL(x)

// 
// Defer statement
// Stolen from Jonathan Blow's microsoft_craziness.h
// 

template<typename T>
struct ExitScope {
	T lambda;
	ExitScope(T lambda):lambda(lambda){}
	~ExitScope(){lambda();}
	ExitScope(const ExitScope&);
private:
	ExitScope& operator =(const ExitScope&);
};

class ExitScopeHelp {
public:
	template<typename T>
	ExitScope<T> operator+(T t){ return t;}
};

#ifdef _MSC_VER
#define defer __pragma(warning(push)) __pragma(warning(disable:4189)) const auto& CONCAT(_defer__, __LINE__) = ExitScopeHelp() + [&]() __pragma(warning(pop))
#else
#define defer const auto& CONCAT(_defer__, __LINE__) = ExitScopeHelp() + [&]()
#endif

// 
// Automatically convert an enum to a string.
// 

#define GENERATE_ENUM(x) x,
#define GENERATE_STRING(x) #x,

#define DEFINE_NAMED_ENUM(Type, List) \
	enum Type { List(GENERATE_ENUM) }; \
	inline const char* Get##Type##Name(Type val) { \
		static const char* names[] = { List(GENERATE_STRING) }; \
		Assert(val >= 0); \
		Assert(val < ArrayLength(names)); \
		return names[val]; \
	}

#define GENERATE_ENUM_WITH_VALUES(name, value) name = value,
#define GENERATE_ENUM_CASE(name, value) case name: return #name;

#define DEFINE_NAMED_ENUM_WITH_VALUES(Type, List) \
	enum Type { List(GENERATE_ENUM_WITH_VALUES) }; \
	inline const char* Get##Type##Name(Type val) { \
		switch (val) { \
			List(GENERATE_ENUM_CASE) \
		} \
		return "unknown"; \
	}

// 
// Human-readable printing for filesizes.
// 

// NOTE: SDL_snprintf doesn't support %g
#define Size_Fmt "%.7g %s"
#define Size_Arg(size) format_size_float(size), format_size_string(size)

inline float format_size_float(size_t bytes) {
	float result = (float)bytes;
	if (bytes >= 1024) {
		result /= 1024.0f;
		bytes /= 1024;
		if (bytes >= 1024) {
			result /= 1024.0f;
			bytes /= 1024;
			if (bytes >= 1024) {
				result /= 1024.0f;
				bytes /= 1024;
			}
		}
	}
	result = floorf(result * 100.0f) / 100.0f;
	return result;
}

inline const char* format_size_string(size_t bytes) {
	const char* result = "bytes";
	if (bytes >= 1024) {
		result = "KB";
		bytes /= 1024;
		if (bytes >= 1024) {
			result = "MB";
			bytes /= 1024;
			if (bytes >= 1024) {
				result = "GB";
				bytes /= 1024;
			}
		}
	}
	return result;
}


inline constexpr vec4 get_color(u32 rgba) {
	return {((rgba >> 24) & 0xFF) / 255.0f,
			((rgba >> 16) & 0xFF) / 255.0f,
			((rgba >>  8) & 0xFF) / 255.0f,
			((rgba >>  0) & 0xFF) / 255.0f};
}

inline constexpr vec4 get_color(u8 r, u8 g, u8 b, u8 a = 0xFF) {
	return {r / 255.0f,
			g / 255.0f,
			b / 255.0f,
			a / 255.0f};
}

inline constexpr vec4 get_color_4bit(u16 rgba) {
	return {((rgba >> 12) & 0xF) / 15.0f,
			((rgba >>  8) & 0xF) / 15.0f,
			((rgba >>  4) & 0xF) / 15.0f,
			((rgba >>  0) & 0xF) / 15.0f};
}


constexpr vec4 color_white  = get_color(0xffffffff);
constexpr vec4 color_black  = get_color(0x000000ff);
constexpr vec4 color_red    = get_color(0xff0000ff);
constexpr vec4 color_green  = get_color(0x00ff00ff);
constexpr vec4 color_blue   = get_color(0x0000ffff);
constexpr vec4 color_yellow = get_color(0xffff00ff);

constexpr vec4 color_cornflower_blue = get_color(0x6495edff);



// --------------------------------------------------------
//                  SECTION: Math
// --------------------------------------------------------



// 
// Cirno's Perfect Math Library
// 

#ifndef PI
#define PI 3.14159265359f
#endif

template <typename T>
inline T min(T a, T b) {
	return (a < b) ? a : b;
}

template <typename T>
inline T max(T a, T b) {
	return (a > b) ? a : b;
}

template <typename T>
inline T clamp(T a, T mn, T mx) {
	// Prioritize lower bound for situations like
	//   clamp(index, 0, size - 1) when size is 0.
	return max(min(a, mx), mn);
}

template <typename T>
inline T lerp(T a, T b, float f) {
	return a + (b - a) * f;
}

template <typename T>
inline T lerp_delta(T a, T b, float f, float delta) {
	f = 1.0f - f;
	return lerp(a, b, 1.0f - powf(f, delta));
}

template <typename T>
inline T lerp3(T a, T b, T c, float f) {
	if (f >= 0.5f) {
		return lerp(b, c, (f - 0.5f) * 2.0f);
	} else {
		return lerp(a, b, f * 2.0f);
	}
}

inline vec2 normalize0(vec2 v) {
	float length = sqrtf(v.x * v.x + v.y * v.y);
	if (length != 0) {
		return {v.x / length, v.y / length};
	} else {
		return {0, 0};
	}
}

template <typename T>
inline T approach(T start, T end, T shift) {
	return start + clamp(end - start, -shift, shift);
}

inline float to_degrees(float rad) {
	return rad * (180.0f / PI);
}

inline float to_radians(float deg) {
	return deg * (PI / 180.0f);
}

inline float dsin(float deg) { return sinf(to_radians(deg)); }
inline float dcos(float deg) { return cosf(to_radians(deg)); }

inline float point_distance(float x1, float y1, float x2, float y2) {
	float dx = x2 - x1;
	float dy = y2 - y1;
	return sqrtf(dx * dx + dy * dy);
}

inline float point_distance(vec2 p1, vec2 p2) {
	return point_distance(p1.x, p1.y, p2.x, p2.y);
}

inline float point_direction(float x1, float y1, float x2, float y2) {
	return to_degrees(atan2f(y1 - y2, x2 - x1));
}

inline float point_direction(vec2 p1, vec2 p2) {
	return point_direction(p1.x, p1.y, p2.x, p2.y);
}

inline bool circle_vs_circle(float x1, float y1, float r1, float x2, float y2, float r2) {
	float dx = x2 - x1;
	float dy = y2 - y1;
	float r = r1 + r2;
	return (dx * dx + dy * dy) < (r * r);
}

inline bool rect_vs_rect(Rectf r1, Rectf r2) {
	return (r1.x + r1.w > r2.x
			&& r1.x <= r2.x + r2.w
			&& r1.y + r1.h > r2.y
			&& r1.y <= r2.y + r2.h);
}

inline bool point_in_rect(vec2 p, Rectf r) {
	return (p.x >= r.x
			&& p.x < r.x + r.w
			&& p.y >= r.y
			&& p.y < r.y + r.h);
}

inline bool point_in_triangle(vec2 pt, vec2 v1, vec2 v2, vec2 v3) {
	auto sign = [](vec2 p1, vec2 p2, vec2 p3) {
		return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
	};

	float d1 = sign(pt, v1, v2);
	float d2 = sign(pt, v2, v3);
	float d3 = sign(pt, v3, v1);

	bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

	return !(has_neg && has_pos);
}

inline bool circle_vs_rotated_rect(float circle_x, float circle_y, float circle_radius,
								   float rect_center_x, float rect_center_y, float rect_w, float rect_h, float rect_dir) {
	float dx = circle_x - rect_center_x;
	float dy = circle_y - rect_center_y;

	float x_rotated = rect_center_x - (dx * dsin(rect_dir)) - (dy * dcos(rect_dir));
	float y_rotated = rect_center_y + (dx * dcos(rect_dir)) - (dy * dsin(rect_dir));

	float x_closest = clamp(x_rotated, rect_center_x - rect_w / 2.0f, rect_center_x + rect_w / 2.0f);
	float y_closest = clamp(y_rotated, rect_center_y - rect_h / 2.0f, rect_center_y + rect_h / 2.0f);

	dx = x_closest - x_rotated;
	dy = y_closest - y_rotated;

	return (dx * dx + dy * dy) < (circle_radius * circle_radius);
}

inline float lengthdir_x(float len, float dir) { return  dcos(dir) * len; }
inline float lengthdir_y(float len, float dir) { return -dsin(dir) * len; }

inline vec2 lengthdir_v2(float len, float dir) {
	return {lengthdir_x(len, dir), lengthdir_y(len, dir)};
}

inline float wrapf(float a, float b) {
	a = fmodf(a, b);
	if (a < 0) {
		a += b;
	}
	return a;
}

template <typename T>
inline T wrap(T a, T b) {
	a = a % b;
	if (a < 0) {
		a += b;
	}
	return a;
}

inline float angle_wrap(float deg) {
	return wrapf(deg, 360.0f);
}

inline float angle_difference(float dest, float src) {
	float res = dest - src;
	res = angle_wrap(res + 180.0f) - 180.0f;
	return res;
}

inline float signf(float x) {
	if (x > 0) return 1;
	if (x == 0) return 0;
	return -1;
}

inline int sign_int(float x) {
	if (x > 0) return 1;
	if (x == 0) return 0;
	return -1;
}

inline float floorf_to(float a, float b) {
	return floorf(a / b) * b;
}

inline float roundf_to(float a, float b) {
	return roundf(a / b) * b;
}

inline float ceilf_to(float a, float b) {
	return ceilf(a / b) * b;
}

inline mat4 get_ortho(float left, float right, float bottom, float top) {
	return glm::ortho<float>(left, right, bottom, top);
}

inline mat4 get_translation(vec3 v) {
	return glm::translate<float>(mat4{1}, v);
}

// 
// One-Liners
// 
template <typename T>
inline void Clamp(T* a, T mn, T mx) { *a = clamp(*a, mn, mx); }

template <typename T>
inline void Lerp(T* a, T b, float f) { *a = lerp(*a, b, f); }

template <typename T>
inline void Lerp_delta(T* a, T b, float f, float delta) { *a = lerp_delta(*a, b, f, delta); }

template <typename T>
inline void Approach(T* start, T end, T shift) { *start = approach(*start, end, shift); }



// ----------------------------------------------------
//               SECTION: Memory Arena
// ----------------------------------------------------



// 
// Stolen from https://bytesbeneath.com/p/the-arena-custom-memory-allocators
// 

#define is_power_of_two(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)

inline uintptr_t align_forward(uintptr_t ptr, size_t align) {
	Assert(is_power_of_two(align));
	return (ptr + (align - 1)) & ~(align - 1);
}

struct Arena {
	static constexpr size_t DEFAULT_ALIGNMENT = sizeof(void*);

	u8*    data;
	size_t count;
	size_t capacity;
};

inline Arena malloc_arena(size_t capacity) {
	Arena a = {};
	a.data     = (u8*) malloc(capacity);
	a.capacity = capacity;

	Assert(a.data);

	return a;
}

inline void free_arena(Arena* a) {
	free(a->data);
	*a = {};
}

inline u8* arena_push(Arena* a, size_t size, size_t alignment = Arena::DEFAULT_ALIGNMENT) {
	uintptr_t curr_ptr = (uintptr_t)a->data + (uintptr_t)a->count;
	uintptr_t offset = align_forward(curr_ptr, alignment);
	offset -= (uintptr_t)a->data;

	Assert(offset + size <= a->capacity && "Arena out of memory");

	u8* ptr = a->data + offset;
	a->count = offset + size;

	return ptr;
}

inline Arena arena_create_from_arena(Arena* arena, size_t capacity) {
	Arena a = {};
	a.data     = arena_push(arena, capacity);
	a.capacity = capacity;

	return a;
}



// ----------------------------------------------------
//                SECTION: Array Type
// ----------------------------------------------------



// Standard linear bump array
template <typename T>
struct bump_array {
	T*     data;
	size_t count;
	size_t capacity;

	T& operator[](size_t i) {
		// Assert(i >= 0);
		Assert(i < count);
		return data[i];
	}

	T operator[](size_t i) const {
		// Assert(i >= 0);
		Assert(i < count);
		return data[i];
	}

	T* begin() const { return data; }
	T* end()   const { return data + count; }
};

template <typename T>
inline bump_array<T> bump_array_from_arena(Arena* a, size_t capacity) {
	bump_array<T> arr = {};
	arr.data     = (T*) arena_push(a, capacity * sizeof(T));
	arr.capacity = capacity;
	return arr;
}

template <typename T>
inline bump_array<T> malloc_bump_array(size_t capacity) {
	bump_array<T> arr = {};
	arr.data     = (T*) malloc(capacity * sizeof(T));
	arr.capacity = capacity;
	return arr;
}

template <typename T>
inline T* array_add(bump_array<T>* arr, const T& val) {
	Assert(arr->count < arr->capacity);

	T* result = &arr->data[arr->count];
	*result = val;
	arr->count++;

	return result;
}

template <typename T>
inline T* array_remove(bump_array<T>* arr, T* it) {
	Assert(arr->count > 0);

	Assert(it >= arr->begin());
	Assert(it <  arr->end());

	memmove(it, it + 1, (size_t)arr->end() - (size_t)it - sizeof(T));
	arr->count--;

	return it;
}

template <typename T>
inline T* array_remove(bump_array<T>* arr, size_t index) {
	// Assert(index >= 0);
	Assert(index < arr->count);

	return array_remove(arr, arr->begin() + index);
}

template <typename T>
inline T* array_insert(bump_array<T>* arr, T* it, const T& val) {
	Assert(arr->count < arr->capacity);

	Assert(it >= arr->begin());
	Assert(it <= arr->end());

	memmove(it + 1, it, (size_t)arr->end() - (size_t)it);

	*it = val;
	arr->count++;

	return it;
}

template <typename T>
inline T* array_insert(bump_array<T>* arr, size_t index, const T& val) {
	// Assert(index >= 0); // for when I switch to signed sizes
	Assert(index <= arr->count);

	return array_insert(arr, arr->begin() + index, val);
}



// "Array view"
template <typename T>
struct array {
	T*     data;
	size_t count;

	array() = default;

	array(T* data, size_t count) : data(data), count(count) {}

	template <size_t N>
	array(const T (&arr)[N]) : data((T*) &arr[0]), count(N) {}

	array(bump_array<T> arr) : data(arr.data), count(arr.count) {}

	T& operator[](size_t i) {
		Assert(i >= 0);
		Assert(i < count);
		return data[i];
	}

	T operator[](size_t i) const {
		Assert(i >= 0);
		Assert(i < count);
		return data[i];
	}

	T* begin() const { return data; }
	T* end()   const { return data + count; }

	bool operator==(const array& other) const {
		if (count != other.count) {
			return false;
		}

		for (size_t i = 0; i < count; i++) {
			if (data[i] != other[i]) {
				return false;
			}
		}

		return true;
	}

	bool operator!=(const array& other) const {
		return !(*this == other);
	}
};

template <typename T>
inline array<T> calloc_array(size_t count) {
	array<T> arr = {};
	arr.data  = (T*) calloc(count, sizeof(T));
	arr.count = count;
	return arr;
}

template <typename T>
inline bool starts_with(array<T> arr, array<T> prefix) {
	if (arr.count < prefix.count) {
		return false;
	}

	arr.count = prefix.count;
	return arr == prefix;
}



// -----------------------------------------------
//             SECTION: String Type
// -----------------------------------------------


#define Str_Fmt      "%.*s"
#define Str_Arg(str) (unsigned int)(str).count, (str).data

struct string {
	char*  data;
	size_t count;

	string() = default;

	string(char* data, size_t count) : data(data), count(count) {}

	template <size_t N>
	string(const char (&arr)[N]) : data((char*) &arr[0]), count(N - 1) {}

	string(bump_array<char> arr) : data(arr.data), count(arr.count) {}

	char& operator[](size_t i) {
		Assert(i >= 0);
		Assert(i < count);
		return data[i];
	}

	char operator[](size_t i) const {
		Assert(i >= 0);
		Assert(i < count);
		return data[i];
	}

	char* begin() const { return data; }
	char* end()   const { return data + count; }

	bool operator==(const string& other) const {
		if (count != other.count) {
			return false;
		}

		for (size_t i = 0; i < count; i++) {
			if (data[i] != other[i]) {
				return false;
			}
		}

		return true;
	}

	bool operator!=(const string& other) const {
		return !(*this == other);
	}
};

inline bool is_whitespace(char ch) {
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

inline bool is_numeric(char ch) {
	return ch >= '0' && ch <= '9';
}

inline string advance(string* str, size_t i = 1) {
	i = min(i, str->count);
	string result = {str->data, i};
	str->count -= i;
	str->data  += i;
	return result;
}

inline void eat_whitespace(string* str) {
	while (str->count > 0 && is_whitespace(*str->data)) {
		advance(str);
	}
}

inline string eat_non_whitespace(string* str) {
	string result = {str->data, 0};

	while (str->count > 0 && !is_whitespace(*str->data)) {
		advance(str);
		result.count++;
	}

	return result;
}

inline string eat_numeric(string* str) {
	string result = {str->data, 0};

	while (str->count > 0 && is_numeric(*str->data)) {
		advance(str);
		result.count++;
	}

	return result;
}

inline string eat_line(string* str) {
	string result = {str->data, 0};

	while (str->count > 0 && *str->data != '\n') {
		advance(str);
		result.count++;
	}

	// Skip newline character.
	if (str->count > 0 && *str->data == '\n') {
		advance(str);
	}

	return result;
}

inline u32 string_to_u32(string str, bool* done = nullptr) {
	if (str.count == 0) {
		if (done) *done = false;
		return 0;
	}

	u32 result = 0;

	for (size_t i = 0; i < str.count; i++) {
		if (!is_numeric(str[i])) {
			if (done) *done = false;
			return 0;
		}

		result *= 10;
		result += str[i] - '0';
	}

	if (done) *done = true;
	return result;
}

inline int string_to_int(string str, bool* out_done = nullptr) {
	if (str.count == 0) {
		if (out_done) *out_done = false;
		return 0;
	}

	int negative = 1;

	while (str.count > 0 && *str.data == '-') {
		negative = -negative;
		advance(&str);
	}

	bool done;
	u32 result = string_to_u32(str, &done);

	if (!done) {
		if (out_done) *out_done = false;
		return 0;
	}

	if (out_done) *out_done = true;
	return (int)result * negative;
}

inline float string_to_f32(string str, bool* done = nullptr) {
	if (str.count == 0) {
		if (done) *done = false;
		return 0;
	}

	float negative = 1;

	while (str.count > 0 && *str.data == '-') {
		negative = -negative;
		advance(&str);
	}

	float result = 0;

	while (str.count > 0 && *str.data != '.') {
		if (!is_numeric(*str.data)) {
			if (done) *done = false;
			return 0;
		}

		result *= 10;
		result += *str.data - '0';

		advance(&str);
	}

	if (str.count > 0 && *str.data == '.') {
		advance(&str);

		float f = 0.1f;

		while (str.count > 0) {
			if (!is_numeric(*str.data)) {
				if (done) *done = false;
				return 0;
			}

			result += (*str.data - '0') * f;
			f /= 10;

			advance(&str);
		}
	}

	if (done) *done = true;
	return result * negative;
}

inline bool starts_with(string str, string prefix) {
	if (str.count < prefix.count) {
		return false;
	}

	str.count = prefix.count;
	return str == prefix;
}

inline bool string_contains(string str, char ch) {
	For (it, str) {
		if (*it == ch) return true;
	}
	return false;
}

template <size_t N>
inline string SDL_PRINTF_VARARG_FUNC(2) Sprintf(char (&buf)[N], SDL_PRINTF_FORMAT_STRING const char* fmt, ...) {
	va_list va;
	va_start(va, fmt);

	static_assert(N < (size_t)INT_MAX, "");

	int result = stb_vsnprintf(buf, (int)N, fmt, va);
	va_end(va);

	Assert(result > 0);
	size_t count = min((size_t)result, N - 1);

	return {buf, count};
}


// You have to free() the C string
inline char* to_c_string(string str) {
	char* c_str = (char*) malloc(str.count + 1);
	Assert(c_str);
	memcpy(c_str, str.data, str.count);
	c_str[str.count] = 0;
	return c_str;
}


// You have to free() string.data
inline string copy_string(string str) {
	if (str.count == 0) return {};

	string result;
	result.data  = (char*) malloc(str.count);
	result.count = str.count;

	Assert(result.data);
	memcpy(result.data, str.data, str.count);

	return result;
}


// You have to free() string.data
inline string copy_c_string(const char* str) {
	if (!str) return {};

	size_t len = strlen(str);

	if (len == 0) return {};

	string result;
	result.data  = (char*) malloc(len);
	result.count = len;

	Assert(result.data);
	memcpy(result.data, str, len);

	return result;
}


