#include "ScreenFade.h"
#include "GameObject.h"
#include "Scene.h"

#include <SDL3/SDL.h>

void ScreenFade::fadeIn(float seconds) {
    alpha = 1.0f; target = 0.0f;
    speed = (seconds > 0.0f) ? (1.0f / seconds) : 1e9f;
    active = true;
}

void ScreenFade::fadeOut(float seconds) {
    target = 1.0f;
    speed = (seconds > 0.0f) ? (1.0f / seconds) : 1e9f;
    active = true;
}

void ScreenFade::setAlpha(float a01) {
    alpha = a01; target = a01; active = false;
}

void ScreenFade::blink(float outSeconds, float inSeconds) {
    blinking = true; blinkStage = 0; active = false; // el parpadeo manda sobre el fundido normal
    blinkOut = (outSeconds > 0.0f) ? (1.0f / outSeconds) : 1e9f;
    blinkIn  = (inSeconds  > 0.0f) ? (1.0f / inSeconds)  : 1e9f;
}

void ScreenFade::update(float dt) {
    if (blinking) { // respawn: a negro y de vuelta, de una sola pasada
        if (blinkStage == 0) { alpha += blinkOut * dt; if (alpha >= 1.0f) { alpha = 1.0f; blinkStage = 1; } }
        else                 { alpha -= blinkIn  * dt; if (alpha <= 0.0f) { alpha = 0.0f; blinking = false; } }
        return;
    }
    if (!active) return;
    if (alpha < target) { alpha += speed * dt; if (alpha >= target) alpha = target; }
    else                { alpha -= speed * dt; if (alpha <= target) alpha = target; }

    if (alpha == target) {
        active = false;
        if (onComplete) onComplete(); // p.ej. cargar la siguiente sala en negro
    }
}

void ScreenFade::render() {
    if (alpha <= 0.0f) return;
    SDL_Renderer* ren = gameObject->scene->getRenderer();

    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(ren, &w, &h);
    SDL_FRect full{ 0.0f, 0.0f, (float)w, (float)h };

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    Uint8 a = (Uint8)(alpha * 255.0f);
    SDL_SetRenderDrawColor(ren, (Uint8)r, (Uint8)g, (Uint8)b, a);
    SDL_RenderFillRect(ren, &full);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}
