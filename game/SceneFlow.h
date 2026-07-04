#pragma once

// Puente minimo para que una ESCENA pida cambiar a OTRA (progresion entre camaras
// del Sector 1). El bucle principal (main) consume la peticion despues de actualizar
// la escena y reconstruye la que toque. Los indices coinciden con las teclas de
// depuracion: 4 = jefe HERCULES-1, 5 = Camara 01, 6 = Camara 02, 7 = Camara 03.
//
// Variables inline (C++17): una sola instancia compartida entre main.cpp y las escenas.
namespace SceneFlow {
    inline int g_request = 0; // 0 = sin peticion pendiente

    inline void requestScene(int index) { g_request = index; }

    // Devuelve la escena pedida (0 = ninguna) y limpia la peticion.
    inline int takeRequested() { int r = g_request; g_request = 0; return r; }
}
