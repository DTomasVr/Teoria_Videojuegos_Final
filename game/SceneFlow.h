#pragma once

// Puente minimo para que una ESCENA pida cambiar a OTRA (progresion entre camaras
namespace SceneFlow {
    inline int g_request = -1; // -1 = sin peticion pendiente (0 es una escena valida: el menu)

    inline void requestScene(int index) { g_request = index; }

    // Devuelve la escena pedida (-1 = ninguna) y limpia la peticion.
    inline int takeRequested() { int r = g_request; g_request = -1; return r; }
}
