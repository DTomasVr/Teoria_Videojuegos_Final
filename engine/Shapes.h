#pragma once

class Scene;

// Primitivas de dibujo en coordenadas de MUNDO para el GAMEPLAY (no para debug).

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
    void ring(Scene& scene, float cx, float cy, float radius, float thickness,
              int r, int g, int b, int a);

    void line(Scene& scene, float x1, float y1, float x2, float y2,
              int r, int g, int b, int a);
}
