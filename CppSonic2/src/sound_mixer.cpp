#include "sound_mixer.h"

#include "package.h"

Mix_Music* g_Music;

void init_mixer() {
	SDL_Init(SDL_INIT_AUDIO);

	{
		log_info("Available audio backends:");

		int num_drivers = SDL_GetNumAudioDrivers();
		for (int i = 0; i < num_drivers; i++) {
			log_info("%s", SDL_GetAudioDriver(i));
		}

		log_info("Current audio backend: %s", SDL_GetCurrentAudioDriver());
	}

	Mix_Init(MIX_INIT_MP3);

	{
		SDL_version cver;
		MIX_VERSION(&cver);
		log_info("Compiled against SDL_mixer %u.%u.%u", cver.major, cver.minor, cver.patch);

		const SDL_version* lver = Mix_Linked_Version();
		log_info("Linked against SDL_mixer %u.%u.%u", lver->major, lver->minor, lver->patch);
	}

	Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 2048);

	{
		int freq;
		u16 format;
		int nchannels;
		Mix_QuerySpec(&freq, &format, &nchannels);

		log_info("Audio Device Frequency: %d", freq);
		log_info("Audio Device Format: %u",    format);
		log_info("Audio Device Channels: %d",  nchannels);

		log_info("Available chunk decoders:");
		for (int i = 0, n = Mix_GetNumChunkDecoders(); i < n; i++) {
			log_info("%s", Mix_GetChunkDecoder(i));
		}

		log_info("Available music decoders:");
		for (int i = 0, n = Mix_GetNumMusicDecoders(); i < n; i++) {
			log_info("%s", Mix_GetMusicDecoder(i));
		}
	}

	{
		float master_volume = 0.5f;
		float sound_volume  = 0.5f;
		float music_volume  = 0.5f;

		char* MASTER_VOLUME = SDL_getenv("MASTER_VOLUME"); // @Leak
		if (MASTER_VOLUME) {
			master_volume = (float) SDL_atof(MASTER_VOLUME);
		}

		char* SOUND_VOLUME = SDL_getenv("SOUND_VOLUME"); // @Leak
		if (SOUND_VOLUME) {
			sound_volume = (float) SDL_atof(SOUND_VOLUME);
		}

		char* MUSIC_VOLUME = SDL_getenv("MUSIC_VOLUME"); // @Leak
		if (MUSIC_VOLUME) {
			music_volume = (float) SDL_atof(MUSIC_VOLUME);
		}

		master_volume = clamp(master_volume, 0.0f, 1.0f);
		sound_volume  = clamp(sound_volume,  0.0f, 1.0f);
		music_volume  = clamp(music_volume,  0.0f, 1.0f);

		Mix_Volume(-1,  (int)(MIX_MAX_VOLUME * (master_volume * sound_volume)));
		Mix_VolumeMusic((int)(MIX_MAX_VOLUME * (master_volume * music_volume)));
	}
}

void deinit_mixer() {
	stop_music();

	Mix_CloseAudio();
	Mix_Quit();

	// quit SDL audio subsystem here?
}

Mix_Chunk* load_sound(const char* fname) {
	auto filedata = get_file_arr(fname);

	SDL_RWops* rw = SDL_RWFromConstMem(filedata.data, filedata.count);
	Mix_Chunk* result = Mix_LoadWAV_RW(rw, 1);

	if (result) {
		log_info("Loaded sound %s", fname);
	}

	return result;
}

void play_sound(Mix_Chunk* chunk, bool stop_all_instances) {
	if (stop_all_instances) {
		stop_sound(chunk);
	}

	Mix_PlayChannel(-1, chunk, 0);
}

void stop_sound(Mix_Chunk* chunk) {
	int nchannels = Mix_AllocateChannels(-1);

	for (int i = 0; i < nchannels; i++) {
		if (Mix_Playing(i) && Mix_GetChunk(i) == chunk) {
			Mix_HaltChannel(i);
		}
	}
}

void play_music(const char* fname) {
	stop_music();

	g_Music = Mix_LoadMUS(fname);
	Mix_PlayMusic(g_Music, -1);
}

void stop_music() {
	if (g_Music) {
		Mix_HaltMusic();
		Mix_FreeMusic(g_Music);
		g_Music = nullptr;
	}
}
