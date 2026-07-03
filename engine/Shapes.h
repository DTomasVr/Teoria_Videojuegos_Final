#pragma once

class Scene;

// Primitivas de dibujo en coordenadas de MUNDO para el GAMEPLAY (no para debug).
// A diferencia de Debug::*, esto SIEMPRE dibuja (no depende de una tecla) y admite
// color+alpha, pensado para los "telegrafos" de los peligros de "REDACTED": el
// barrido laser de una torreta, la zona circular parpadeante de un nuke, el anillo
// de una mina, el temporizador proyectado en el suelo, etc.
//
// Todas reciben la Scene (de ahi sacan renderer y camara activa) y dibujan respetando
// la camara (worldToScreen + zoom). El color es RGBA 0..255. Se llaman dentro del
// render() de un componente (la fase de dibujo), igual que un SpriteRenderer.

namespace Shapes {
    void fillRect(Scene& scene, float cx, float cy, float w, float h,
                  int r, int g, int b, int a);
    void outlineRect(Scene& scene, float cx, float cy, float w, float h,
                     int r, int g, int b, int a);

    void fillCircle(Scene& scene, float cx, float cy, float radius,
                    int r, int g, int b, int a);
    void outlineCircle(Scene& scene, float cx, float cy, float radius,
                       int r, int g, int b, int a);

    // Anillo (corona) de grosor 'thickness' en pixeles de mundo: util para el pulso
    // de deteccion de una mina.
    void ring(Scene& scene, float cx, float cy, float radius, float thickness,
              int r, int g, int b, int a);

    void line(Scene& scene, float x1, float y1, float x2, float y2,
              int r, int g, int b, int a);
}
