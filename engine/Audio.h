#pragma once
#include <string>

// Sistema de audio minimo sobre SDL3_mixer (API nueva MIX_*).

namespace Audio {
    bool init();      // crea el dispositivo de mezcla; false si no se pudo
    void shutdown();  // libera sonidos y cierra el mixer

    bool load(const std::string& name, const std::string& path); // SFX predescodificado
    void play(const std::string& name);                          // dispara un SFX

    // Musica de fondo EN BUCLE. Carga (y cachea) el archivo y lo reproduce en bucle
    void playMusic(const std::string& path);
    void stopMusic(); // detiene la musica de fondo
}
