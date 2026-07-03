#include "Debugger.h"
#include "Scene.h"
#include "BoxCollider.h"
#include "Camera.h"
#include "FollowCamera.h"
#include "GameObject.h"

#include <SDL3/SDL.h>
#include <cmath>

namespace Debug {

static bool g_enabled = false; // estado global del depurador

void setEnabled(bool on) { g_enabled = on; }
void toggle()            { g_enabled = !g_enabled; }
bool isEnabled()         { return g_enabled; }

// Convierte un punto de mundo a pantalla usando la camara activa (si la hay).
static void toScreen(Scene& scene, float wx, float wy,
                     float& sx, float& sy, float& zoom) {
    Camera* cam = scene.getActiveCamera();
    if (cam) {
        cam->worldToScreen(wx, wy, sx, sy);
        zoom = cam->getZoom();
    } else {
        sx = wx; sy = wy; zoom = 1.0f;
    }
}

void drawColliders(Scene& scene) {
    if (!g_enabled) return;
    SDL_Renderer* r = scene.getRenderer();

    for (BoxCollider* c : scene.getColliders()) {
        float sx, sy, zoom;
        toScreen(scene, c->centerX(), c->centerY(), sx, sy, zoom);

        float w = c->width  * zoom;
        float h = c->height * zoom;
        SDL_FRect box{ sx - w * 0.5f, sy - h * 0.5f, w, h };

        if (c->isTrigger) SDL_SetRenderDrawColor(r, 255, 210,  0, 255); // trigger: amarillo
        else              SDL_SetRenderDrawColor(r,   0, 220, 80, 255); // solido:  verde
        SDL_RenderRect(r, &box);
    }
}

void drawDeadZone(Scene& scene, FollowCamera* cam) {
    if (!g_enabled || !cam) return;
    SDL_Renderer* r = scene.getRenderer();

    Camera* active = scene.getActiveCamera();
    float zoom = active ? active->getZoom() : 1.0f;

    int outW = 0, outH = 0;
    SDL_GetCurrentRenderOutputSize(r, &outW, &outH);

    // La zona muerta esta centrada en la camara, que cae en el centro de pantalla.
    float w = cam->deadZoneWidth  * zoom;
    float h = cam->deadZoneHeight * zoom;
    SDL_FRect box{ outW * 0.5f - w * 0.5f, outH * 0.5f - h * 0.5f, w, h };

    SDL_SetRenderDrawColor(r, 80, 170, 255, 255); // azul
    SDL_RenderRect(r, &box);
}

void drawRect(Scene& scene, float x, float y, float w, float h) {
    if (!g_enabled) return;
    SDL_Renderer* r = scene.getRenderer();
    float sx, sy, zoom;
    toScreen(scene, x, y, sx, sy, zoom);
    float dw = w * zoom, dh = h * zoom;
    SDL_FRect box{ sx - dw * 0.5f, sy - dh * 0.5f, dw, dh };
    SDL_SetRenderDrawColor(r, 255, 90, 90, 255); // rojo
    SDL_RenderRect(r, &box);
}

void drawCircle(Scene& scene, float x, float y, float radius) {
    if (!g_enabled) return;
    SDL_Renderer* r = scene.getRenderer();
    float sx, sy, zoom;
    toScreen(scene, x, y, sx, sy, zoom);
    float rr = radius * zoom;

    // Aproximamos el circulo con un poligono de segmentos (suficiente para debug).
    const int SEG = 24;
    SDL_SetRenderDrawColor(r, 255, 90, 90, 255); // rojo, como el resto de primitivas
    float prevX = sx + rr, prevY = sy;
    for (int i = 1; i <= SEG; ++i) {
        float a = (float)i / SEG * 6.2831853f; // 2*PI
        float cx = sx + rr * std::cos(a);
        float cy = sy + rr * std::sin(a);
        SDL_RenderLine(r, prevX, prevY, cx, cy);
        prevX = cx; prevY = cy;
    }
}

void drawLine(Scene& scene, float x1, float y1, float x2, float y2) {
    if (!g_enabled) return;
    SDL_Renderer* r = scene.getRenderer();
    float ax, ay, bx, by, zoom;
    toScreen(scene, x1, y1, ax, ay, zoom);
    toScreen(scene, x2, y2, bx, by, zoom);
    SDL_SetRenderDrawColor(r, 255, 90, 90, 255);
    SDL_RenderLine(r, ax, ay, bx, by);
}

void drawPoint(Scene& scene, float x, float y) {
    if (!g_enabled) return;
    SDL_Renderer* r = scene.getRenderer();
    float sx, sy, zoom;
    toScreen(scene, x, y, sx, sy, zoom);
    SDL_SetRenderDrawColor(r, 255, 90, 90, 255);
    float s = 5.0f; // una crucecita
    SDL_RenderLine(r, sx - s, sy, sx + s, sy);
    SDL_RenderLine(r, sx, sy - s, sx, sy + s);
}

} // namespace Debug
