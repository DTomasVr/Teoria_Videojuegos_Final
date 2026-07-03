#include "Audio.h"

#include <SDL3/SDL.h>
#include <unordered_map>

// El header de SDL3_mixer solo se incluye si esta disponible. Asi, si el motor se
// compila sin la libreria, el audio queda en stubs (no rompe el build). Con la lib
// instalada (rutas ya cableadas en el .vcxproj) se usa la API nueva MIX_*.
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

void shutdown() {
    for (auto& [n, a] : g_sounds) MIX_DestroyAudio(a);
    g_sounds.clear();
    if (g_mixer) { MIX_DestroyMixer(g_mixer); g_mixer = nullptr; }
    MIX_Quit();
}

#else // sin SDL3_mixer: stubs que no hacen nada

bool init()                                            { SDL_Log("Audio: SDL3_mixer no disponible; sin sonido."); return false; }
bool load(const std::string&, const std::string&)      { return false; }
void play(const std::string&)                          {}
void shutdown()                                        {}

#endif

} // namespace Audio
