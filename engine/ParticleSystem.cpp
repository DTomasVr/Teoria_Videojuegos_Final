#include "ParticleSystem.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <cmath>

static SDL_BlendMode toSDLBlend(BlendMode m) {
    switch (m) {
        case BlendMode::None: return SDL_BLENDMODE_NONE;
        case BlendMode::Add:  return SDL_BLENDMODE_ADD;
        case BlendMode::Mod:  return SDL_BLENDMODE_MOD;
        case BlendMode::Alpha:
        default:              return SDL_BLENDMODE_BLEND;
    }
}

static float frand(float a, float b) {
    return a + (b - a) * ((float)std::rand() / (float)RAND_MAX);
}

ParticleSystem::ParticleSystem(int capacity) {
    if (capacity < 1) capacity = 1;
    pool.resize((size_t)capacity);
}

void ParticleSystem::setSprite(const std::string& p) {
    path = p;
    if (gameObject && gameObject->scene)
        texture = gameObject->scene->getAssets().loadTexture(path);
}

void ParticleSystem::setSourceRect(float x, float y, float w, float h) {
    srcX = x; srcY = y; srcW = w; srcH = h; useSrc = true;
}

void ParticleSystem::emit(float x, float y, float vx, float vy, float life,
                          float size0, float size1, int r, int g, int b, int a) {
    int n = (int)pool.size();
    for (int k = 0; k < n; ++k) {
        int i = (nextFree + k) % n;
        if (!pool[i].active) {
            Particle& p = pool[i];
            p.x = x; p.y = y; p.vx = vx; p.vy = vy;
            p.life = life; p.maxLife = (life > 0.0f ? life : 0.0001f);
            p.size0 = size0; p.size1 = size1;
            p.r = r; p.g = g; p.b = b; p.a = a;
            p.active = true;
            nextFree = (i + 1) % n;
            return;
        }
    }
    // Pool lleno: se ignora la particula (los efectos pueden permitirse perder alguna).
}

void ParticleSystem::emitBurst(float x, float y, int count) {
    for (int i = 0; i < count; ++i) {
        float ang = frand(0.0f, 6.2831853f);      // direccion radial aleatoria
        float spd = frand(speedMin, speedMax);
        float life = frand(lifeMin, lifeMax);
        emit(x, y, std::cos(ang) * spd, std::sin(ang) * spd, life,
             sizeStart, sizeEnd, colR, colG, colB, colA);
    }
}

void ParticleSystem::clear() {
    for (Particle& p : pool) p.active = false;
    nextFree = 0;
}

int ParticleSystem::activeCount() const {
    int c = 0;
    for (const Particle& p : pool) if (p.active) ++c;
    return c;
}

void ParticleSystem::update(float dt) {
    for (Particle& p : pool) {
        if (!p.active) continue;
        p.vy += gravity * dt;
        if (drag > 0.0f) {
            float f = 1.0f - drag * dt;
            if (f < 0.0f) f = 0.0f;
            p.vx *= f; p.vy *= f;
        }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.life -= dt;
        if (p.life <= 0.0f) p.active = false;
    }
}

void ParticleSystem::render() {
    SDL_Renderer* renderer = gameObject->scene->getRenderer();
    Camera* cam = gameObject->scene->getActiveCamera();
    float zoom = cam ? cam->getZoom() : 1.0f;

    SDL_FRect src{ srcX, srcY, srcW, srcH };
    const SDL_FRect* srcPtr = useSrc ? &src : nullptr;
    if (texture) SDL_SetTextureBlendMode(texture, toSDLBlend(blend));
    else SDL_SetRenderDrawBlendMode(renderer, toSDLBlend(blend));

    for (const Particle& p : pool) {
        if (!p.active) continue;
        float frac = p.life / p.maxLife;            // 1 al nacer -> 0 al morir
        if (frac < 0.0f) frac = 0.0f;
        float size = (p.size1 + (p.size0 - p.size1) * frac) * zoom; // encoge
        Uint8 alpha = (Uint8)(p.a * frac);          // se desvanece

        float sx, sy;
        if (cam) cam->worldToScreen(p.x, p.y, sx, sy);
        else { sx = p.x; sy = p.y; }
        SDL_FRect dst{ sx - size * 0.5f, sy - size * 0.5f, size, size };

        if (texture) {
            SDL_SetTextureColorMod(texture, (Uint8)p.r, (Uint8)p.g, (Uint8)p.b);
            SDL_SetTextureAlphaMod(texture, alpha);
            SDL_RenderTexture(renderer, texture, srcPtr, &dst);
        } else {
            SDL_SetRenderDrawColor(renderer, (Uint8)p.r, (Uint8)p.g, (Uint8)p.b, alpha);
            SDL_RenderFillRect(renderer, &dst);
        }
    }

    // Restaurar estado neutro para no afectar a otros dibujos.
    if (texture) {
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(texture, 255);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    } else {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
}
