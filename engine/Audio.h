#pragma once
#include <string>

// Sistema de audio minimo sobre SDL3_mixer (API nueva MIX_*). Patron global tipo
// Debug: un unico mixer para toda la app, que sobrevive a los cambios de escena.
//
// Uso:
//   main:   Audio::init();  ... ; Audio::shutdown();  (requiere SDL_INIT_AUDIO)
//   juego:  Audio::load("disparo", "assets/audio/shoot.wav");
//           Audio::play("disparo");
//
// Los SFX cortos se cargan PREDESCODIFICADOS en RAM (mejor para efectos que suenan
// mucho, DOCX). Todo es tolerante a fallos: si el audio no inicializa o el archivo
// no existe, play() simplemente no hace nada (el juego corre igual, sin sonido).

namespace Audio {
    bool init();      // crea el dispositivo de mezcla; false si no se pudo
    void shutdown();  // libera sonidos y cierra el mixer

    bool load(const std::string& name, const std::string& path); // SFX predescodificado
    void play(const std::string& name);                          // dispara un SFX
}
