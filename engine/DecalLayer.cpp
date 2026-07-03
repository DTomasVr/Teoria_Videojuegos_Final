#include "DecalLayer.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"

#include <SDL3/SDL.h>

bool DecalLayer::setup(float worldOriginX, float worldOriginY, int pixelW, int pixelH) {
    originX = worldOriginX; originY = worldOriginY;
    texW = pixelW; texH = pixelH;

    SDL_Renderer* r = gameObject->scene->getRenderer();
    target = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888,
                               SDL_TEXTUREACCESS_TARGET, texW, texH);
    if (!target) {
        SDL_Log("DecalLayer: no se pudo crear el render target: %s", SDL_GetError());
        return false;
    }
    // Mezcla normal para acumular marcas con transparencia.
    SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
    clear(); // arrancar transparente
    return true;
}

DecalLayer::~DecalLayer() {
    if (target) SDL_DestroyTexture(target); // somos duenos de este target
}

void DecalLayer::beginStamp() {
    SDL_Renderer* r = gameObject->scene->getRenderer();
    SDL_SetRenderTarget(r, target);
}

void DecalLayer::endStamp() {
    SDL_Renderer* r = gameObject->scene->getRenderer();
    SDL_SetRenderTarget(r, nullptr); // volver a la pantalla
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE); // dejar el estado como estaba
}

void DecalLayer::clear() {
    if (!target) return;
    SDL_Renderer* r = gameObject->scene->getRenderer();
    SDL_SetRenderTarget(r, target);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0); // transparente
    SDL_RenderClear(r);
    endStamp();
}

void DecalLayer::stampRect(float wx, float wy, float w, float h,
                           int r, int g, int b, int a) {
    if (!target) return;
    // Mundo -> texel (1:1). Centrado en el punto.
    float tx = wx - originX;
    float ty = wy - originY;
    SDL_FRect rect{ tx - w * 0.5f, ty - h * 0.5f, w, h };

    SDL_Renderer* ren = gameObject->scene->getRenderer();
    beginStamp();
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND); // acumula con alpha
    SDL_SetRenderDrawColor(ren, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_RenderFillRect(ren, &rect);
    endStamp();
}

void DecalLayer::stampSprite(const std::string& path, float wx, float wy,
                             float w, float h, float rotation,
                             int r, int g, int b, int a) {
    if (!target) return;
    SDL_Texture* tex = gameObject->scene->getAssets().loadTexture(path);
    if (!tex) return;

    float tx = wx - originX;
    float ty = wy - originY;
    SDL_FRect dst{ tx - w * 0.5f, ty - h * 0.5f, w, h };

    SDL_Renderer* ren = gameObject->scene->getRenderer();
    beginStamp();
    SDL_SetTextureColorMod(tex, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_SetTextureAlphaMod(tex, (Uint8)a);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_RenderTextureRotated(ren, tex, nullptr, &dst, rotation, nullptr, SDL_FLIP_NONE);
    SDL_SetTextureColorMod(tex, 255, 255, 255); // dejar la textura en neutro (cache)
    SDL_SetTextureAlphaMod(tex, 255);
    endStamp();
}

void DecalLayer::render() {
    if (!target) return;
    SDL_Renderer* ren = gameObject->scene->getRenderer();
    Camera* cam = gameObject->scene->getActiveCamera();

    // Rectangulo de mundo que cubre la textura, llevado a pantalla con la camara.
    float sLx, sTy, sRx, sBy;
    if (cam) {
        cam->worldToScreen(originX, originY, sLx, sTy);
        cam->worldToScreen(originX + texW, originY + texH, sRx, sBy);
    } else {
        sLx = originX; sTy = originY;
        sRx = originX + texW; sBy = originY + texH;
    }
    SDL_FRect dst{ sLx, sTy, sRx - sLx, sBy - sTy };
    SDL_RenderTexture(ren, target, nullptr, &dst);
}
