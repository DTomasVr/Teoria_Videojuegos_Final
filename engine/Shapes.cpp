#include "Shapes.h"
#include "Scene.h"
#include "Camera.h"
#include "GameObject.h"
#include "Transform.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <vector>

namespace Shapes {

// Mundo -> pantalla con la camara activa (o identidad si no hay camara).
static void toScreen(Scene& scene, float wx, float wy,
                     float& sx, float& sy, float& zoom) {
    Camera* cam = scene.getActiveCamera();
    if (cam) { cam->worldToScreen(wx, wy, sx, sy); zoom = cam->getZoom(); }
    else     { sx = wx; sy = wy; zoom = 1.0f; }
}

void fillRect(Scene& scene, float cx, float cy, float w, float h,
              int r, int g, int b, int a) {
    SDL_Renderer* ren = scene.getRenderer();
    float sx, sy, zoom; toScreen(scene, cx, cy, sx, sy, zoom);
    float dw = w * zoom, dh = h * zoom;
    SDL_FRect rect{ sx - dw * 0.5f, sy - dh * 0.5f, dw, dh };
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_RenderFillRect(ren, &rect);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

void outlineRect(Scene& scene, float cx, float cy, float w, float h,
                 int r, int g, int b, int a) {
    SDL_Renderer* ren = scene.getRenderer();
    float sx, sy, zoom; toScreen(scene, cx, cy, sx, sy, zoom);
    float dw = w * zoom, dh = h * zoom;
    SDL_FRect rect{ sx - dw * 0.5f, sy - dh * 0.5f, dw, dh };
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_RenderRect(ren, &rect);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

void fillCircle(Scene& scene, float cx, float cy, float radius,
                int r, int g, int b, int a) {
    SDL_Renderer* ren = scene.getRenderer();
    float sx, sy, zoom; toScreen(scene, cx, cy, sx, sy, zoom);
    float rr = radius * zoom;

    // Abanico de triangulos (centro + perimetro) via SDL_RenderGeometry.
    const int SEG = 36;
    SDL_FColor col{ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
    std::vector<SDL_Vertex> verts;
    verts.reserve(SEG + 2);
    verts.push_back(SDL_Vertex{ { sx, sy }, col, { 0, 0 } }); // centro
    for (int i = 0; i <= SEG; ++i) {
        float ang = (float)i / SEG * 6.2831853f;
        verts.push_back(SDL_Vertex{
            { sx + rr * std::cos(ang), sy + rr * std::sin(ang) }, col, { 0, 0 } });
    }
    std::vector<int> idx;
    idx.reserve(SEG * 3);
    for (int i = 1; i <= SEG; ++i) { idx.push_back(0); idx.push_back(i); idx.push_back(i + 1); }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(ren, nullptr, verts.data(), (int)verts.size(),
                       idx.data(), (int)idx.size());
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

void outlineCircle(Scene& scene, float cx, float cy, float radius,
                   int r, int g, int b, int a) {
    SDL_Renderer* ren = scene.getRenderer();
    float sx, sy, zoom; toScreen(scene, cx, cy, sx, sy, zoom);
    float rr = radius * zoom;

    const int SEG = 40;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    float px = sx + rr, py = sy;
    for (int i = 1; i <= SEG; ++i) {
        float ang = (float)i / SEG * 6.2831853f;
        float nx = sx + rr * std::cos(ang);
        float ny = sy + rr * std::sin(ang);
        SDL_RenderLine(ren, px, py, nx, ny);
        px = nx; py = ny;
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

void ring(Scene& scene, float cx, float cy, float radius, float thickness,
          int r, int g, int b, int a) {
    // Corona: varios circulos concentricos (barato y suficiente para un telegrafo).
    Camera* cam = scene.getActiveCamera();
    float zoom = cam ? cam->getZoom() : 1.0f;
    int layers = (int)(thickness * zoom);
    if (layers < 1) layers = 1;
    for (int i = 0; i < layers; ++i)
        outlineCircle(scene, cx, cy, radius - (float)i / zoom, r, g, b, a);
}

void line(Scene& scene, float x1, float y1, float x2, float y2,
          int r, int g, int b, int a) {
    SDL_Renderer* ren = scene.getRenderer();
    float ax, ay, bx, by, zoom;
    toScreen(scene, x1, y1, ax, ay, zoom);
    toScreen(scene, x2, y2, bx, by, zoom);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_RenderLine(ren, ax, ay, bx, by);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

} // namespace Shapes
