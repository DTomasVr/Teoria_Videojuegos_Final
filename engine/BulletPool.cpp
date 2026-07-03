#include "BulletPool.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"
#include "CircleCollider.h"
#include "Health.h"
#include "Collision.h"

#include <SDL3/SDL.h>

BulletPool::BulletPool(int capacity) {
    if (capacity < 1) capacity = 1;
    pool.resize((size_t)capacity); // reserva de una vez: todas inactivas
}

void BulletPool::setSprite(const std::string& p) {
    path = p;
    // El componente ya esta agregado (la escena esta lista): cargar de inmediato.
    // El AssetManager cachea por ruta, asi que no se duplica si ya estaba cargada.
    if (gameObject && gameObject->scene)
        texture = gameObject->scene->getAssets().loadTexture(path);
}

void BulletPool::setSourceRect(float x, float y, float w, float h) {
    srcX = x; srcY = y; srcW = w; srcH = h; useSrc = true;
}

void BulletPool::setColor(int r, int g, int b, int a) {
    colR = r; colG = g; colB = b; colA = a;
}

void BulletPool::setBounds(float minX, float minY, float maxX, float maxY) {
    bMinX = minX; bMinY = minY; bMaxX = maxX; bMaxY = maxY; hasBounds = true;
}

BulletPool::Bullet* BulletPool::spawn(float x, float y, float vx, float vy,
                                      float radius, float life, int type) {
    // Busca un slot libre empezando por la pista 'nextFree' y dando la vuelta.
    int n = (int)pool.size();
    for (int k = 0; k < n; ++k) {
        int i = (nextFree + k) % n;
        if (!pool[i].active) {
            Bullet& b = pool[i];
            b.x = x; b.y = y; b.vx = vx; b.vy = vy;
            b.radius = radius; b.life = life; b.type = type;
            b.active = true;
            nextFree = (i + 1) % n; // la proxima busqueda arranca despues de esta
            return &b;
        }
    }
    return nullptr; // pool lleno
}

void BulletPool::clear() {
    for (Bullet& b : pool) b.active = false;
    nextFree = 0;
}

int BulletPool::activeCount() const {
    int c = 0;
    for (const Bullet& b : pool) if (b.active) ++c;
    return c;
}

void BulletPool::update(float dt) {
    // 1) Integrar y descartar (siempre corre, aunque el jugador sea invulnerable).
    for (Bullet& b : pool) {
        if (!b.active) continue;
        b.x += b.vx * dt;
        b.y += b.vy * dt;

        if (b.life > 0.0f) {
            b.life -= dt;
            if (b.life <= 0.0f) { b.active = false; continue; }
        }
        if (hasBounds &&
            (b.x < bMinX || b.x > bMaxX || b.y < bMinY || b.y > bMaxY)) {
            b.active = false;
        }
    }

    // 2) Colision O(n): balas contra UNA sola hurtbox.
    if (!target) return;

    // I-frames del Dash: si el objetivo es invulnerable, ni entramos al bucle de
    // colision (mas barato y coincide con el diseno del DOCX: la variable "inmune"
    // evita entrar a los loops de colision).
    Health* hp = target->gameObject->getComponent<Health>();
    if (hp && hp->isInvulnerable()) return;

    float cx = target->centerX();
    float cy = target->centerY();
    float cr = target->radius;

    for (Bullet& b : pool) {
        if (!b.active) continue;
        if (Collision::circleVsCircle(b.x, b.y, b.radius, cx, cy, cr)) {
            b.active = false;     // la bala se consume en el impacto
            if (hp) hp->kill();   // muerte instantanea (respeta invulnerabilidad)
            if (onHit) onHit();
            break;                // un impacto basta: el jugador ya murio
        }
    }
}

void BulletPool::render() {
    SDL_Renderer* renderer = gameObject->scene->getRenderer();
    Camera* cam = gameObject->scene->getActiveCamera();
    float zoom = cam ? cam->getZoom() : 1.0f;

    if (texture) {
        SDL_FRect src{ srcX, srcY, srcW, srcH };
        const SDL_FRect* srcPtr = useSrc ? &src : nullptr;
        SDL_SetTextureColorMod(texture, (Uint8)colR, (Uint8)colG, (Uint8)colB);
        SDL_SetTextureAlphaMod(texture, (Uint8)colA);

        for (const Bullet& b : pool) {
            if (!b.active) continue;
            float side = b.radius * 2.0f * drawScale * zoom;
            float sx, sy;
            if (cam) cam->worldToScreen(b.x, b.y, sx, sy);
            else { sx = b.x; sy = b.y; }
            SDL_FRect dst{ sx - side * 0.5f, sy - side * 0.5f, side, side };
            SDL_RenderTexture(renderer, texture, srcPtr, &dst);
        }
        // Dejar el modulado en neutro: la textura puede compartirse por cache.
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(texture, 255);
    } else {
        // Fallback sin textura: un cuadrado de color por bala (prototipado rapido).
        SDL_SetRenderDrawColor(renderer, (Uint8)colR, (Uint8)colG, (Uint8)colB, (Uint8)colA);
        for (const Bullet& b : pool) {
            if (!b.active) continue;
            float side = b.radius * 2.0f * drawScale * zoom;
            float sx, sy;
            if (cam) cam->worldToScreen(b.x, b.y, sx, sy);
            else { sx = b.x; sy = b.y; }
            SDL_FRect dst{ sx - side * 0.5f, sy - side * 0.5f, side, side };
            SDL_RenderFillRect(renderer, &dst);
        }
    }
}
