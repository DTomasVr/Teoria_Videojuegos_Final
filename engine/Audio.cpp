#include "Audio.h"

#include <SDL3/SDL.h>
#include <unordered_map>
#include <string>

// El header de SDL3_mixer solo se incluye si esta disponible. Asi, si el motor se compila sin SDL3_mixer, el audio queda deshabilitado y no hay errores de compilacion.
#if __has_include(<SDL3_mixer/SDL_mixer.h>)
  #include <SDL3_mixer/SDL_mixer.h>
  #define SDLUPC_HAS_MIXER 1
#else
  #define SDLUPC_HAS_MIXER 0
#endif

namespace Audio {

#if SDLUPC_HAS_MIXER

static MIX_Mixer* g_mixer = nullptr;
static std::unordered_map<std::string, MIX_Audio*> g_sounds;
static MIX_Track* g_musicTrack = nullptr;                       // pista unica de musica de fondo
static std::unordered_map<std::string, MIX_Audio*> g_music;    // cache de temas de musica
static std::string g_currentMusic;                             // ruta del tema sonando ahora

bool init() {
    if (g_mixer) return true;
    if (!MIX_Init()) {
        SDL_Log("Audio: MIX_Init fallo: %s", SDL_GetError());
        return false;
    }
    // Dispositivo de salida por defecto, formato por defecto (spec = nullptr).
    g_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!g_mixer) {
        SDL_Log("Audio: no se pudo crear el mixer: %s", SDL_GetError());
        MIX_Quit();
        return false;
    }
    return true;
}

bool load(const std::string& name, const std::string& path) {
    if (!g_mixer) return false;
    if (g_sounds.find(name) != g_sounds.end()) return true; // ya cargado
    MIX_Audio* audio = MIX_LoadAudio(g_mixer, path.c_str(), true); // predecode = true
    if (!audio) {
        SDL_Log("Audio: no se pudo cargar '%s': %s", path.c_str(), SDL_GetError());
        return false;
    }
    g_sounds[name] = audio;
    return true;
}

void play(const std::string& name) {
    if (!g_mixer) return;
    auto it = g_sounds.find(name);
    if (it == g_sounds.end()) return; // no cargado: silencio, sin ruido en el log
    MIX_PlayAudio(g_mixer, it->second);
}

void playMusic(const std::string& path) {
    if (!g_mixer) return;
    if (path == g_currentMusic) return; // ya es el tema actual: no reiniciar (musica fluida)

    MIX_Audio* audio = nullptr;
    auto it = g_music.find(path);
    if (it != g_music.end()) audio = it->second;
    else {
        audio = MIX_LoadAudio(g_mixer, path.c_str(), false); // streaming (los temas son largos)
        if (!audio) {
            SDL_Log("Audio: no se pudo cargar la musica '%s': %s", path.c_str(), SDL_GetError());
            return;
        }
        g_music[path] = audio;
    }
    if (!g_musicTrack) g_musicTrack = MIX_CreateTrack(g_mixer);
    if (!g_musicTrack) return;
    MIX_SetTrackAudio(g_musicTrack, audio);
    SDL_PropertiesID opts = SDL_CreateProperties();
    SDL_SetNumberProperty(opts, MIX_PROP_PLAY_LOOPS_NUMBER, -1); // -1 = bucle infinito
    MIX_PlayTrack(g_musicTrack, opts);
    SDL_DestroyProperties(opts);
    g_currentMusic = path;
}

void stopMusic() {
    if (g_musicTrack) MIX_StopTrack(g_musicTrack, 0);
    g_currentMusic.clear();
}

void shutdown() {
    if (g_musicTrack) { MIX_DestroyTrack(g_musicTrack); g_musicTrack = nullptr; }
    for (auto& [n, a] : g_music) MIX_DestroyAudio(a);
    g_music.clear();
    g_currentMusic.clear();
    for (auto& [n, a] : g_sounds) MIX_DestroyAudio(a);
    g_sounds.clear();
    if (g_mixer) { MIX_DestroyMixer(g_mixer); g_mixer = nullptr; }
    MIX_Quit();
}

#else // sin SDL3_mixer: stubs que no hacen nada

bool init()                                            { SDL_Log("Audio: SDL3_mixer no disponible; sin sonido."); return false; }
bool load(const std::string&, const std::string&)      { return false; }
void play(const std::string&)                          {}
void playMusic(const std::string&)                     {}
void stopMusic()                                       {}
void shutdown()                                        {}

#endif

} // namespace Audio
