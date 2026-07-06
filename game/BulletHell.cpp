#include "BulletHell.h"
#include "SceneFlow.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/SpriteAnimator.h"
#include "../engine/Lifetime.h"
#include "../engine/AssetManager.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/CircleCollider.h"
#include "../engine/Collision.h"
#include "../engine/Health.h"
#include "../engine/BulletPool.h"
#include "../engine/ParticleSystem.h"
#include "../engine/DecalLayer.h"
#include "../engine/ScreenFade.h"
#include "../engine/Shapes.h"
#include "../engine/TextRenderer.h"
#include "../engine/PhaseTimer.h"
#include "../engine/ChildOf.h"
#include "../engine/Camera.h"
#include "../engine/Debugger.h"
#include "../engine/Audio.h"

//  "REDACTED" - Jefe HERCULES-1. Escena de prueba (tecla 4).
namespace {
    constexpr float ROOM_W = 900.0f, ROOM_H = 700.0f, WALL = 40.0f;
    constexpr float HALF_W = ROOM_W * 0.5f, HALF_H = ROOM_H * 0.5f;

    // Jugador.
    constexpr float PLAYER_SPEED   = 260.0f;
    constexpr float HURTBOX_RADIUS = 8.0f;
    constexpr float BODY_W = 26.0f, BODY_H = 54.0f; // hitbox fisica: cabeza estrecha, cuerpo alto
    constexpr float PLAYER_SCALE   = 0.25f; // celda 256px -> ~64px en el mundo
    constexpr float DASH_SPEED    = 900.0f, DASH_DURATION = 0.12f;
    constexpr float DASH_IFRAMES  = 0.12f,  DASH_COOLDOWN = 0.5f;

    // Balas.
    constexpr float BOSS_BULLET_SPEED = 200.0f;
    constexpr float BULLET_RADIUS = 8.0f, BULLET_LIFE = 6.0f;

    // Chasis + 4 brazos. El cuerpo es solido (~3 tiles) y ocupa el
    constexpr int   ARM_COUNT   = 4;
    constexpr float ARM_OFFSET  = 118.0f;  // centro del brazo (sprites recortados y centrados)
    constexpr float ARM_TIP     = 90.0f;   // muzzle/salida de bala = punta del cañon del arma (~208 del core)
    constexpr float ARM_HIT     = 30.0f;   // radio letal del brazo
    constexpr float CORE_BODY   = 92.0f;   // lado del collider solido del chasis
    constexpr float ARM_SCALE    = 0.22f;  // brazo recortado (614px) -> ~135px, mitad 68 (tip ~186)
    constexpr float WEAPON_R     = 172.0f; // arma en la punta del brazo
    constexpr float WEAPON_SCALE = 0.22f;  // arma acorde al brazo; su cañon llega a ~208

    // Bloques que caen (Fase 2): mas pequeños para que no atasquen la sala, y con
    constexpr float BLOCK_MIN = 48.0f, BLOCK_MAX = 72.0f;
    constexpr float BLOCK_CORE_CLEAR = 155.0f; // radio libre alrededor del chasis

    // Rotacion segun calor: mas rapida cuanto mas calor. A partir del 30% de calor
    constexpr float SPIN_LOW = 28.0f, SPIN_MID = 46.0f, SPIN_HIGH = 68.0f;
    constexpr float SPIN_P2  = 18.0f;
    constexpr float HEAT_MID = 20.0f, HEAT_FAST = 55.0f; // tramos de velocidad
    constexpr float HEAT_REVERSE = 15.0f;                // desde aqui alterna el giro
    // Desde el 30% de calor el sentido de giro ALTERNA cada REVERSE_PERIOD seg
    constexpr float REVERSE_PERIOD = 4.5f;

    // Armas de Fase 1 (los brazos SIEMPRE disparan, sin pausas).
    constexpr float MINIGUN_INTERVAL = 0.08f;
    constexpr float HMG_INTERVAL = 0.5f; constexpr int HMG_BURST = 4;
    constexpr float HMG_SPREAD = 0.22f;

    // Peligros de Fase 2: aparecen ALEATORIAMENTE por todo el mapa (los patrones del
    constexpr float FLAME_RADIUS = 60.0f, FLAME_LIFE = 4.0f, FLAME_WARN = 0.6f;
    constexpr float FLAME_HIT = 38.0f; // radio LETAL < visual (la llama se dibuja cargada a la izq): mas indulgente
    constexpr float FLAME_INT_MAX = 0.55f, FLAME_INT_MIN = 0.20f; // seg entre parches
    constexpr float MINE_RADIUS = 72.0f;                         // radio del estallido
    constexpr float MINE_INT_MAX = 1.3f,  MINE_INT_MIN = 0.45f;   // seg entre minas (MAS bombas)

    // Sobrecalentamiento Fase 1: 20% natural (pasivo) + 80% placas (4 x 20%).
    // Cada placa sube su progreso mientras el jugador la MANTIENE pisada; la Fase 2 se
    // abre solo cuando las 4 estan cerradas al 100% y el calor natural llego al 20%.
    constexpr float NATURAL_MAX = 20.0f, NATURAL_RATE = 1.0f;  // calor pasivo (20 en ~20 s)
    constexpr float PLATE_MAX = 20.0f, PLATE_FILL_RATE = 6.0f; // por placa: 20 en ~3.3 s pisada

    // Fases.
    constexpr float PHASE1_TIME   = 90.0f;   // backstop de Fase 1 (fuerza transicion si te demoras)
    constexpr float TRANSITION_TIME = 3.0f;  // silencio entre fases
    constexpr float FASE2_DURATION = 55.0f;  // Fase 2: sobrevive mientras el suelo se cubre -> victoria
    constexpr float WIN_TIME = 1.3f;
    constexpr float TITLE_TIME = 3.2f, TITLE_FADE = 1.2f; // rotulo central que se desvanece

    constexpr float PI = 3.14159265f;

    // Region de SUELO dentro de map.jpeg (1024x1024): recorta el marco/paredes pintados
    constexpr float MAP_FX0 = 150.0f, MAP_FY0 = 145.0f, MAP_FX1 = 885.0f, MAP_FY1 = 880.0f;

    float frand(float a, float b) { return a + (b - a) * ((float)std::rand() / (float)RAND_MAX); }
    // Margen pequeño: los peligros de Fase 2 deben poder aparecer en los BORDES y
    float randX() { return frand(-HALF_W + 18.0f, HALF_W - 18.0f); }
    float randY() { return frand(-HALF_H + 18.0f, HALF_H - 18.0f); }
}

// Componentes locales de esta escena. Namespace anonimo (enlace interno) para no
namespace {

// Configura el sprite ANIMADO del clon. Hojas 4x4 (fila = direccion, columnas = frames):
inline void setupPlayerAnim(GameObject* player) {
    player->transform->scaleX = player->transform->scaleY = PLAYER_SCALE;
    player->addComponent<SpriteRenderer>(); // sin textura: la pone el animator
    auto anim = player->addComponent<SpriteAnimator>(252, 252, 4);
    const char* IDLE = "assets/redacted/player/idle.png";
    const char* RUN  = "assets/redacted/player/run.png";
    const char* DIAG = "assets/redacted/player/run_diag.png";
    const float IDLE_FPS = 3.0f, RUN_FPS = 9.0f; // idle mas lento (respiracion)
    anim->addRowAnimation("idle_down",  IDLE, 248, 256, 0, IDLE_FPS);
    anim->addRowAnimation("idle_up",    IDLE, 248, 256, 1, IDLE_FPS);
    anim->addRowAnimation("idle_right", IDLE, 248, 256, 2, IDLE_FPS);
    anim->addRowAnimation("idle_left",  IDLE, 248, 256, 3, IDLE_FPS);
    anim->addRowAnimation("run_up",    RUN, 252, 252, 0, RUN_FPS); // fila 0 = ARRIBA (norte)
    anim->addRowAnimation("run_down",  RUN, 252, 252, 1, RUN_FPS); // fila 1 = ABAJO (sur)
    anim->addRowAnimation("run_right", RUN, 252, 252, 2, RUN_FPS);
    anim->addRowAnimation("run_left",  RUN, 252, 252, 3, RUN_FPS);
    anim->addRowAnimation("run_up_right",   DIAG, 252, 252, 0, RUN_FPS);
    anim->addRowAnimation("run_up_left",    DIAG, 252, 252, 1, RUN_FPS);
    anim->addRowAnimation("run_down_right", DIAG, 252, 252, 2, RUN_FPS);
    anim->addRowAnimation("run_down_left",  DIAG, 252, 252, 3, RUN_FPS);
    anim->play("idle_down");
}

// Explosion de un solo uso: hoja de 6 frames que se reproduce y el objeto se
inline void spawnExplosion(Scene& scene, float x, float y, float worldSize) {
    GameObject* e = scene.createGameObject("Explosion");
    e->transform->x = x; e->transform->y = y;
    e->transform->scaleX = e->transform->scaleY = worldSize / 768.0f;
    e->addComponent<SpriteRenderer>(); // textura la pone el animator
    auto anim = e->addComponent<SpriteAnimator>(768, 768, 6);
    anim->addStripAnimation("boom", "assets/redacted/fx/explosion.png", 768, 768, 18.0f, false);
    anim->play("boom");
    e->addComponent<Lifetime>()->seconds = 6.0f / 18.0f + 0.05f; // dura lo que la animacion
}

// Dibuja una textura suelta en coordenadas de MUNDO (con la camara), centrada.
inline void drawWorldTex(Scene& scene, SDL_Texture* t, float wx, float wy, float scale, float rotDeg) {
    if (!t) return;
    Camera* cam = scene.getActiveCamera();
    float zoom = cam ? cam->getZoom() : 1.0f;
    float sx, sy;
    if (cam) cam->worldToScreen(wx, wy, sx, sy); else { sx = wx; sy = wy; }
    float w = 0, h = 0; SDL_GetTextureSize(t, &w, &h);
    float s = scale * zoom;
    SDL_FRect dst{ sx - w * s * 0.5f, sy - h * s * 0.5f, w * s, h * s };
    SDL_RenderTextureRotated(scene.getRenderer(), t, nullptr, &dst, rotDeg, nullptr, SDL_FLIP_NONE);
}

// Dibuja un FRAME (recorte opcional) de una textura en el mundo, con tamaño de mundo
inline void drawWorldFrame(Scene& scene, SDL_Texture* t, float wx, float wy,
                           float worldW, float worldH, const SDL_FRect* src,
                           float rotDeg, int r, int g, int b, int a) {
    if (!t) return;
    Camera* cam = scene.getActiveCamera();
    float zoom = cam ? cam->getZoom() : 1.0f;
    float sx, sy;
    if (cam) cam->worldToScreen(wx, wy, sx, sy); else { sx = wx; sy = wy; }
    float w = worldW * zoom, h = worldH * zoom;
    SDL_FRect dst{ sx - w * 0.5f, sy - h * 0.5f, w, h };
    SDL_SetTextureColorMod(t, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_SetTextureAlphaMod(t, (Uint8)a);
    SDL_RenderTextureRotated(scene.getRenderer(), t, src, &dst, rotDeg, nullptr, SDL_FLIP_NONE);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

// Destello de disparo: reproduce fx/muzzle.png (3 frames de 670px) UNA vez, orientado
constexpr int   MUZZLE_FRAMES = 3;
constexpr float MUZZLE_FPS = 26.0f, MUZZLE_SIZE = 46.0f;

class MuzzleFlash : public Component {
public:
    SDL_Texture* tex = nullptr; float rot = 0.0f;
    void update(float dt) override { clock += dt; }
    void render() override {
        int f = (int)(clock * MUZZLE_FPS); if (f >= MUZZLE_FRAMES) f = MUZZLE_FRAMES - 1;
        SDL_FRect src{ (float)(f * 670), 0.0f, 670.0f, 670.0f };
        drawWorldFrame(*gameObject->scene, tex, gameObject->transform->x, gameObject->transform->y,
                       MUZZLE_SIZE, MUZZLE_SIZE, &src, rot, 255, 255, 255, 255);
    }
private:
    float clock = 0.0f;
};

inline void spawnMuzzle(Scene& scene, float x, float y, float rotDeg) {
    GameObject* g = scene.createGameObject("Muzzle");
    g->transform->x = x; g->transform->y = y;
    auto m = g->addComponent<MuzzleFlash>();
    m->tex = scene.getAssets().loadTexture("assets/redacted/fx/muzzle.png");
    m->rot = rotDeg;
    g->addComponent<Lifetime>()->seconds = (float)MUZZLE_FRAMES / MUZZLE_FPS + 0.02f;
}

//  Suelo y contorno de la arena.
class RoomRenderer : public Component {
public:
    void awake() override { mapTex = gameObject->scene->getAssets().loadTexture("assets/redacted/map.jpeg"); }
    void render() override {
        SDL_Renderer* r = gameObject->scene->getRenderer();
        Camera* cam = gameObject->scene->getActiveCamera();
        float sLx, sTy, sRx, sBy;
        if (cam) { cam->worldToScreen(-HALF_W, -HALF_H, sLx, sTy);
                   cam->worldToScreen( HALF_W,  HALF_H, sRx, sBy); }
        else { sLx = -HALF_W; sTy = -HALF_H; sRx = HALF_W; sBy = HALF_H; }
        SDL_FRect floor{ sLx, sTy, sRx - sLx, sBy - sTy };
        SDL_FRect mapSrc{ MAP_FX0, MAP_FY0, MAP_FX1 - MAP_FX0, MAP_FY1 - MAP_FY0 };
        if (mapTex) SDL_RenderTexture(r, mapTex, &mapSrc, &floor);       // solo el suelo (sin paredes pintadas)
        else { SDL_SetRenderDrawColor(r, 26, 26, 30, 255); SDL_RenderFillRect(r, &floor); }
        SDL_SetRenderDrawColor(r, 120, 120, 130, 255); SDL_RenderRect(r, &floor);
    }
private:
    SDL_Texture* mapTex = nullptr;
};

//  Rotacion continua del chasis (BossRoom fija degPerSec, con signo para invertir).
class Spin : public Component {
public:
    float degPerSec = 0.0f;
    void update(float dt) override { gameObject->transform->rotation += degPerSec * dt; }
};

//  Jugador: movimiento 8-dir + Dash con i-frames + tinte de invulnerabilidad.
class BulletHellController : public Component {
public:
    void update(float dt) override {
        const bool* k = SDL_GetKeyboardState(nullptr);
        auto rb = gameObject->getComponent<RigidBody2D>();
        auto hp = gameObject->getComponent<Health>();
        if (!rb) return;

        float ix = 0.0f, iy = 0.0f;
        if (k[SDL_SCANCODE_LEFT]  || k[SDL_SCANCODE_A]) ix -= 1.0f;
        if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) ix += 1.0f;
        if (k[SDL_SCANCODE_UP]    || k[SDL_SCANCODE_W]) iy -= 1.0f;
        if (k[SDL_SCANCODE_DOWN]  || k[SDL_SCANCODE_S]) iy += 1.0f;
        float len = std::sqrt(ix * ix + iy * iy);
        if (len > 0.0f) { ix /= len; iy /= len; lastX = ix; lastY = iy; }

        bool dashKey = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_LSHIFT];
        if (dashKey && !dashPrev && cooldownTimer <= 0.0f) {
            dashTimer = DASH_DURATION; cooldownTimer = DASH_COOLDOWN;
            dashDirX = (len > 0.0f) ? ix : lastX;
            dashDirY = (len > 0.0f) ? iy : lastY;
            if (hp) hp->setInvulnerable(DASH_IFRAMES);
            Audio::play("dash");
        }
        dashPrev = dashKey;

        bool moving = (len > 0.0f) || (dashTimer > 0.0f);
        if (dashTimer > 0.0f) {
            rb->velocityX = dashDirX * DASH_SPEED; rb->velocityY = dashDirY * DASH_SPEED;
            dashTimer -= dt;
        } else { rb->velocityX = ix * PLAYER_SPEED; rb->velocityY = iy * PLAYER_SPEED; }
        if (cooldownTimer > 0.0f) cooldownTimer -= dt;

        // Animacion: correr tiene 8 direcciones (ortogonales + diagonales); en reposo
        if (len > 0.0f) {
            const bool L = ix < -0.01f, R = ix > 0.01f, U = iy < -0.01f, D = iy > 0.01f;
            if      (U && R) runDir = "up_right";
            else if (U && L) runDir = "up_left";
            else if (D && R) runDir = "down_right";
            else if (D && L) runDir = "down_left";
            else if (R)      runDir = "right";
            else if (L)      runDir = "left";
            else if (U)      runDir = "up";
            else             runDir = "down";
            if (std::fabs(ix) > std::fabs(iy)) idleDir = (ix < 0.0f) ? "left" : "right";
            else                               idleDir = (iy < 0.0f) ? "up" : "down";
        }
        if (auto anim = gameObject->getComponent<SpriteAnimator>())
            anim->play(moving ? ("run_" + runDir) : ("idle_" + idleDir));

        if (auto sr = gameObject->getComponent<SpriteRenderer>()) {
            if (hp && hp->isInvulnerable()) sr->setColor(120, 200, 255, 180);
            else                            sr->setColor(255, 255, 255, 255);
        }
    }
    void render() override {
        auto cc = gameObject->getComponent<CircleCollider>();
        if (cc) Debug::drawCircle(*gameObject->scene, cc->centerX(), cc->centerY(), cc->radius);
    }
private:
    float lastX = 0.0f, lastY = -1.0f, dashDirX = 0.0f, dashDirY = -1.0f;
    float dashTimer = 0.0f, cooldownTimer = 0.0f; bool dashPrev = false;
    std::string runDir = "down", idleDir = "down";
};

//  Campo de parches de fuego (Fase 2,   7.5). Aparecen en posiciones ALEATORIAS
class FlamePatchField : public Component {
public:
    CircleCollider* target = nullptr;
    Health*         hp = nullptr;
    DecalLayer*     decals = nullptr;
    float intensity = 0.0f; // 0..1, lo fija BossRoom desde el calor

    void awake() override {
        fireTex = gameObject->scene->getAssets().loadTexture("assets/redacted/fx/fire.png");
        siteTex = gameObject->scene->getAssets().loadTexture("assets/redacted/hazard/nuke_site.png");
    }

    void enable() { active = true; spawnTimer = 0.3f; }
    void reset()  { active = false; for (Patch& p : patches) p.active = false; }

    void update(float dt) override {
        animClock += dt;
        if (!active) return;
        spawnTimer -= dt;
        if (spawnTimer <= 0.0f) { spawnRandom(); spawnTimer = interval(); }

        for (Patch& p : patches) {
            if (!p.active) continue;
            if (p.warn > 0.0f) { p.warn -= dt; continue; }   // telegrafo: aun no arde
            p.life -= dt;
            if (p.life <= 0.0f) { p.active = false; scorch(p.x, p.y); }
        }
        if (target && hp) {
            float cx = target->centerX(), cy = target->centerY();
            for (Patch& p : patches)
                if (p.active && p.warn <= 0.0f &&
                    Collision::pointInCircle(cx, cy, p.x, p.y, FLAME_HIT)) { hp->kill(); break; }
        }
    }
    void render() override {
        Scene& s = *gameObject->scene;
        for (Patch& p : patches) {
            if (!p.active) continue;
            if (p.warn > 0.0f) {   // telegrafo: la MISMA marca que la Camara 01 (nuke_site), redimensionada
                float pulse = 0.5f + 0.5f * std::sin(p.warn * 26.0f);
                float d = FLAME_RADIUS * 2.0f;
                drawWorldFrame(s, siteTex, p.x, p.y, d, d, nullptr, 0.0f, 255, 255, 255, (int)(110 + 120 * pulse));
            } else {               // ardiendo: tile de fuego animado
                float frac = p.life / FLAME_LIFE; if (frac < 0.0f) frac = 0.0f;
                int a = (int)(80 + 150 * frac); if (a > 255) a = 255;
                const float fw = FLAME_RADIUS * 2.6f, fh = fw * (544.0f / 817.0f);
                int f = (int)(animClock * 10.0f + p.x * 0.02f); f = ((f % 4) + 4) % 4;
                SDL_FRect src{ (float)f * 817.0f, 0.0f, 817.0f, 544.0f };
                drawWorldFrame(s, fireTex, p.x, p.y, fw, fh, &src, 0.0f, 255, 255, 255, a);
                Debug::drawCircle(s, p.x, p.y, FLAME_HIT); // hitbox real (solo en debug)
            }
        }
    }
private:
    struct Patch { float x = 0, y = 0, life = 0, warn = 0; bool active = false; };
    std::vector<Patch> patches = std::vector<Patch>(80);
    SDL_Texture* fireTex = nullptr; SDL_Texture* siteTex = nullptr; float animClock = 0.0f;
    bool active = false;
    float spawnTimer = 0.0f;

    float interval() const { return FLAME_INT_MAX - (FLAME_INT_MAX - FLAME_INT_MIN) * intensity; }
    void spawnRandom() {
        for (Patch& p : patches)
            if (!p.active) { p.x = randX(); p.y = randY(); p.life = FLAME_LIFE; p.warn = FLAME_WARN; p.active = true; return; }
    }
    void scorch(float x, float y) {
        if (!decals) return;
        for (int i = 0; i < 6; ++i) {
            float ox = frand(-FLAME_RADIUS * 0.6f, FLAME_RADIUS * 0.6f);
            float oy = frand(-FLAME_RADIUS * 0.6f, FLAME_RADIUS * 0.6f);
            decals->stampRect(x + ox, y + oy, frand(10.0f, 22.0f), frand(10.0f, 22.0f), 22, 14, 10, 180);
        }
    }
};

//  Campo de minas (Fase 2,   7.5). Cada mina CAE del techo en una posicion
class MineField : public Component {
public:
    CircleCollider* target = nullptr;
    Health*         hp = nullptr;
    DecalLayer*     decals = nullptr;
    float intensity = 0.0f;

    void awake() override { mineTex = gameObject->scene->getAssets().loadTexture("assets/redacted/hazard/mine_active.png"); }
    void enable() { active = true; spawnTimer = 0.8f; }
    void reset()  { active = false; for (M& m : mines) m.active = false; }

    void update(float dt) override {
        if (!active) return;
        spawnTimer -= dt;
        if (spawnTimer <= 0.0f) { spawnRandom(); spawnTimer = interval(); }
        for (M& m : mines) {
            if (!m.active) continue;
            m.timer -= dt;                       // cuenta atras de la caida
            if (m.timer <= 0.0f) { explode(m); m.active = false; } // estalla y desaparece
        }
    }
    void render() override {
        Scene& s = *gameObject->scene;
        for (M& m : mines) {                     // cayendo: sombra que crece + mina descendiendo
            if (!m.active) continue;
            float frac = 1.0f - m.timer / DROP; if (frac < 0.0f) frac = 0.0f;
            Shapes::fillCircle(s, m.x, m.y, MINE_RADIUS * (0.35f + 0.65f * frac), 0, 0, 0, 130);
            float topY = -HALF_H, cy = topY + (m.y - topY) * frac;
            drawWorldTex(s, mineTex, m.x, cy, 0.14f, 0.0f); // sprite de la mina cayendo
            Debug::drawCircle(s, m.x, m.y, MINE_RADIUS); // zona del estallido (solo en debug)
        }
    }
private:
    SDL_Texture* mineTex = nullptr;
    static constexpr float DROP = 1.0f;          // seg de caida (telegrafo)
    struct M { float x = 0, y = 0, timer = 0; bool active = false; };
    std::vector<M> mines = std::vector<M>(80);
    bool active = false;
    float spawnTimer = 0.0f;

    float interval() const { return MINE_INT_MAX - (MINE_INT_MAX - MINE_INT_MIN) * intensity; }
    void spawnRandom() {
        for (M& m : mines)
            if (!m.active) { m.x = randX(); m.y = randY(); m.timer = DROP; m.active = true; return; }
    }
    void explode(M& m) {
        if (target && hp && Collision::pointInCircle(target->centerX(), target->centerY(), m.x, m.y, MINE_RADIUS))
            hp->kill();
        spawnExplosion(*gameObject->scene, m.x, m.y, MINE_RADIUS * 1.8f); // sprite de explosion
        if (decals)                              // quemadura NEGRA que queda en el suelo
            for (int i = 0; i < 8; ++i) {
                float ox = frand(-MINE_RADIUS * 0.6f, MINE_RADIUS * 0.6f);
                float oy = frand(-MINE_RADIUS * 0.6f, MINE_RADIUS * 0.6f);
                decals->stampRect(m.x + ox, m.y + oy, frand(12.0f, 26.0f), frand(12.0f, 26.0f), 6, 6, 8, 220);
            }
    }
};

//  Brazo del jefe. Fase 1: minigun (par A) o HMG (par B), disparo RADIAL hacia
enum class Weapon { Minigun, HMG };

class BossArm : public Component {
public:
    BulletPool*     pool = nullptr;
    Transform*      coreT = nullptr;
    CircleCollider* target = nullptr;   // hurtbox del jugador (letalidad del brazo)
    Health*         hp = nullptr;
    bool pairA = true;

    Weapon weapon = Weapon::Minigun; // lo fija BossRoom
    bool   firing = false;           // solo dispara balas en Fase 1

    void render() override { // debug (F1): radio letal de contacto del brazo
        Debug::drawCircle(*gameObject->scene, gameObject->transform->x, gameObject->transform->y, ARM_HIT);
    }

    void update(float dt) override {
        // Letalidad de contacto: el brazo es un boom solido, letal SIEMPRE (tambien
        if (target && hp && !hp->isInvulnerable() &&
            Collision::pointInCircle(target->centerX(), target->centerY(),
                                     gameObject->transform->x, gameObject->transform->y,
                                     ARM_HIT + target->radius))
            hp->kill();

        if (!firing) return; // brazos siempre disparan en Fase 1 (sin pausas)

        float ox = gameObject->transform->x, oy = gameObject->transform->y;
        float dx = ox - (coreT ? coreT->x : 0.0f);
        float dy = oy - (coreT ? coreT->y : 0.0f);
        float l = std::sqrt(dx * dx + dy * dy);
        if (l > 0.0f) { dx /= l; dy /= l; } else { dx = 0.0f; dy = 1.0f; }
        float tipX = ox + dx * ARM_TIP, tipY = oy + dy * ARM_TIP;

        t1 += dt;
        if (muzT > 0.0f) muzT -= dt;
        bool fired = false;
        if (weapon == Weapon::Minigun) {
            while (t1 >= MINIGUN_INTERVAL) { t1 -= MINIGUN_INTERVAL; fired = true;
                if (pool) pool->spawn(tipX, tipY, dx * BOSS_BULLET_SPEED, dy * BOSS_BULLET_SPEED, BULLET_RADIUS, BULLET_LIFE); }
        } else {
            while (t1 >= HMG_INTERVAL) { t1 -= HMG_INTERVAL; fired = true; fireBurst(tipX, tipY, dx, dy); }
        }
        // Destello en la punta del brazo (throttle: el minigun dispara muy seguido).
        if (fired && muzT <= 0.0f) {
            spawnMuzzle(*gameObject->scene, tipX, tipY, std::atan2(dy, dx) * 180.0f / PI);
            muzT = 0.05f;
        }
    }
private:
    float t1 = 0.0f, muzT = 0.0f;
    void fireBurst(float x, float y, float dx, float dy) {
        if (!pool) return;
        float base = std::atan2(dy, dx);
        for (int i = 0; i < HMG_BURST; ++i) {
            float a = base + (i - (HMG_BURST - 1) * 0.5f) * HMG_SPREAD;
            pool->spawn(x, y, std::cos(a) * BOSS_BULLET_SPEED, std::sin(a) * BOSS_BULLET_SPEED, BULLET_RADIUS, BULLET_LIFE);
        }
    }
};

//  Bloque de techo caido: obstaculo SOLIDO permanente. Lo crea FallingBlocks al
class SolidBlock : public Component {
public:
    float size = 80.0f;
    float srcX = 0.0f, srcY = 0.0f; // (sin uso con debris.png; lo mantiene FallingBlocks por compat)
    void awake() override { blockTex = gameObject->scene->getAssets().loadTexture("assets/redacted/hazard/debris.png"); }
    void render() override {
        Scene& s = *gameObject->scene;
        float x = gameObject->transform->x, y = gameObject->transform->y;
        drawWorldFrame(s, blockTex, x, y, size, size, nullptr, 0.0f, 255, 255, 255, 255); // sprite de escombros
    }
private:
    SDL_Texture* blockTex = nullptr;
};

//  Bloques de techo que CAEN (Fase 2): por la disfuncion del area, cada cierto
class FallingBlocks : public Component {
public:
    CircleCollider* target = nullptr;
    Health*         hp = nullptr;

    void awake() override { blockTex = gameObject->scene->getAssets().loadTexture("assets/redacted/hazard/debris.png"); }
    void enable() { active = true; spawnTimer = 2.0f; }
    void reset() {
        active = false; falling.clear(); placed.clear();
        for (GameObject* b : landed) if (b) gameObject->scene->destroy(b);
        landed.clear();
    }

    void update(float dt) override {
        if (!active) return;
        spawnTimer -= dt;
        if (spawnTimer <= 0.0f) { spawnTimer = 3.5f; startFall(); }
        for (size_t i = 0; i < falling.size(); ) {
            falling[i].timer -= dt;
            if (falling[i].timer <= 0.0f) { land(falling[i]); falling.erase(falling.begin() + (long)i); }
            else ++i;
        }
    }
    void render() override {
        Scene& s = *gameObject->scene;
        for (const Fall& f : falling) {
            float frac = 1.0f - f.timer / FALL_TIME; if (frac < 0.0f) frac = 0.0f;
            Shapes::fillCircle(s, f.x, f.y, f.size * (0.3f + 0.5f * frac), 0, 0, 0, 150); // sombra que crece
            float topY = -HALF_H, cy = topY + (f.y - topY) * frac;                        // bloque descendiendo
            drawWorldFrame(s, blockTex, f.x, cy, f.size, f.size, nullptr, 0.0f, 255, 255, 255, 255); // sprite de escombros
        }
    }
private:
    static constexpr float FALL_TIME = 1.4f;
    struct Fall   { float x = 0, y = 0, size = 0, timer = 0, srcX = 0, srcY = 0; };
    struct Placed { float x = 0, y = 0, size = 0; }; // bloque ya aterrizado (para no solaparse)
    std::vector<Fall> falling;
    std::vector<GameObject*> landed;
    std::vector<Placed> placed;
    SDL_Texture* blockTex = nullptr;
    bool active = false; float spawnTimer = 0.0f;

    // Sitio libre = no cae sobre la maquina central ni solapa un bloque ya aterrizado
    bool freeSpot(float x, float y, float size) const {
        if (std::sqrt(x * x + y * y) < BLOCK_CORE_CLEAR + size * 0.5f) return false;
        const float gap = 8.0f;
        for (const Placed& p : placed)
            if (std::fabs(x - p.x) < (size + p.size) * 0.5f + gap &&
                std::fabs(y - p.y) < (size + p.size) * 0.5f + gap) return false;
        for (const Fall& f : falling)
            if (std::fabs(x - f.x) < (size + f.size) * 0.5f + gap &&
                std::fabs(y - f.y) < (size + f.size) * 0.5f + gap) return false;
        return true;
    }

    void startFall() {
        float size = frand(BLOCK_MIN, BLOCK_MAX);
        for (int tries = 0; tries < 24; ++tries) { // busca un hueco valido
            float x = frand(-HALF_W + size, HALF_W - size);
            float y = frand(-HALF_H + size, HALF_H - size);
            if (!freeSpot(x, y, size)) continue;
            Fall f; f.size = size; f.x = x; f.y = y; f.timer = FALL_TIME;
            // Recorte ALEATORIO del suelo, solo de la parte MEDIA del mapa (evita paredes).
            f.srcX = frand(MAP_FX0 + 40.0f, MAP_FX1 - 40.0f - size);
            f.srcY = frand(MAP_FY0 + 40.0f, MAP_FY1 - 40.0f - size);
            falling.push_back(f);
            return;
        }
        // sin hueco libre este ciclo: no cae bloque (evita amontonar)
    }
    void land(const Fall& f) {
        if (target && hp && Collision::circleVsAABB(target->centerX(), target->centerY(), target->radius,
                                                    f.x, f.y, f.size * 0.5f, f.size * 0.5f))
            hp->kill();
        GameObject* b = gameObject->scene->createGameObject("Block");
        b->transform->x = f.x; b->transform->y = f.y;
        auto bc = b->addComponent<BoxCollider>(); bc->width = f.size; bc->height = f.size;
        auto sb = b->addComponent<SolidBlock>();
        sb->size = f.size; sb->srcX = f.srcX; sb->srcY = f.srcY;
        landed.push_back(b);
        placed.push_back({ f.x, f.y, f.size });
    }
};

//  Controlador del jefe HERCULES-1 de DOS FASES + HUD.
enum class BossPhase { Supresion, Transicion, Erradicacion, Ganado };

class BossRoom : public Component {
public:
    PhaseTimer*   timer = nullptr;
    TextRenderer *statusText = nullptr, *timerText = nullptr, *heatText = nullptr, *nexusText = nullptr;
    ScreenFade*   fade = nullptr;
    GameObject*   player = nullptr;
    Health*       hp = nullptr;
    BulletPool*   pool = nullptr;
    DecalLayer*   decals = nullptr;
    Spin*         coreSpin = nullptr;
    Transform*    coreT = nullptr;
    FlamePatchField* flames = nullptr;
    MineField*    mines = nullptr;
    FallingBlocks* blocks = nullptr;
    ParticleSystem* steam = nullptr;
    ParticleSystem* gore  = nullptr; // gore vivo (se limpia al reintentar)
    std::vector<BossArm*> arms;
    float startX = 0.0f, startY = 0.0f;

    float heat = 0.0f;                 // = naturalHeat + suma de placas (0..100)
    float naturalHeat = 0.0f;          // calor pasivo de Fase 1 (0..NATURAL_MAX)
    float plateProgress[4] = { 0, 0, 0, 0 }; // progreso de cada placa (0..PLATE_MAX)
    BossPhase phase = BossPhase::Supresion;

    bool  platesLocked() const { return phase != BossPhase::Supresion; }
    bool  winning()      const { return phase == BossPhase::Ganado; }
    float shockRadius()  const { return shockR; }

    // Placas: el jugador las MANTIENE pisadas para subir su progreso (Fase 1).
    void addPlateProgress(int i, float dt) {
        if (phase != BossPhase::Supresion || i < 0 || i > 3) return;
        plateProgress[i] += PLATE_FILL_RATE * dt;
        if (plateProgress[i] > PLATE_MAX) plateProgress[i] = PLATE_MAX;
    }
    bool  isPlateClosed(int i) const { return i >= 0 && i < 4 && plateProgress[i] >= PLATE_MAX; }
    float plateFrac(int i) const { return (i >= 0 && i < 4) ? plateProgress[i] / PLATE_MAX : 0.0f; }
    bool  allPlatesClosed() const { return isPlateClosed(0) && isPlateClosed(1) && isPlateClosed(2) && isPlateClosed(3); }

    void onPhase1TimerEnd() { enterTransition(); } // backstop del temporizador
    void onPlayerDeath()    { if (fade) fade->blink(0.15f, 0.35f); resetProgress(false); } // parpadeo al reaparecer

    // Primera linea de NEXUS-9 + rotulo del jefe (tras cablear el HUD, no en awake()).
    void introLine() { setNexus("ESPECIMEN 0001 EN POSICION"); showTitle("HERCULES-1"); }
    void showTitle(const char* t) { if (statusText) { statusText->setText(t); titleTimer = TITLE_TIME; } }

    void update(float dt) override {
        updateRotation(dt);
        pushArmState();
        if (nexusTimer > 0.0f) { nexusTimer -= dt; if (nexusTimer <= 0.0f && nexusText) nexusText->setText(""); }
        if (titleTimer > 0.0f && statusText) { // el rotulo central se desvanece solo
            titleTimer -= dt;
            float a = (titleTimer < TITLE_FADE) ? (titleTimer / TITLE_FADE) : 1.0f;
            statusText->setColor(255, 255, 255, (int)(255 * a));
            if (titleTimer <= 0.0f) statusText->setText("");
        }

        switch (phase) {
            case BossPhase::Supresion: { // 20% natural + 80% placas; abre al cerrar las 4 y llenar el natural
                naturalHeat += NATURAL_RATE * dt; if (naturalHeat > NATURAL_MAX) naturalHeat = NATURAL_MAX;
                heat = naturalHeat + plateProgress[0] + plateProgress[1] + plateProgress[2] + plateProgress[3];
                emitSteam(dt);
                if (allPlatesClosed() && naturalHeat >= NATURAL_MAX) enterTransition();
                break;
            }
            case BossPhase::Transicion:
                transTimer -= dt; if (transTimer <= 0.0f) enterPhase2(); break;
            case BossPhase::Erradicacion: {
                heat = 100.0f; // sobrecalentado: sobrevivir mientras el suelo se cubre
                fase2Timer += dt; emitSteam(dt);
                float inten = fase2Timer / FASE2_DURATION;
                if (inten < 0.0f) inten = 0.0f; if (inten > 1.0f) inten = 1.0f;
                if (flames) flames->intensity = inten; // el suelo se cubre mas con el tiempo
                if (mines)  mines->intensity = inten;
                if (fase2Timer >= FASE2_DURATION) win();
                break;
            }
            case BossPhase::Ganado:
                winTimer -= dt; shockR += 950.0f * dt;
                if (winTimer <= 0.0f) SceneFlow::requestScene(16); break; // -> cinematica de victoria
        }
        updateHUD();
    }

private:
    float transTimer = 0.0f, winTimer = 0.0f, shockR = 0.0f, fase2Timer = 0.0f;
    float reverseClock = 0.0f, steamAcc = 0.0f, nexusTimer = 0.0f, titleTimer = 0.0f;
    int   spinDir = 1;

    void setNexus(const char* t) { if (nexusText) nexusText->setText(t); nexusTimer = 4.5f; }

    void updateRotation(float dt) {
        float speed;
        if (phase == BossPhase::Transicion || phase == BossPhase::Ganado) speed = 0.0f;
        else if (phase == BossPhase::Erradicacion) { speed = SPIN_P2; }
        else {
            speed = (heat < HEAT_MID) ? SPIN_LOW : (heat < HEAT_FAST) ? SPIN_MID : SPIN_HIGH;
            // Desde el 30% de calor, el sentido de giro se invierte cada REVERSE_PERIOD
            if (heat >= HEAT_REVERSE) {
                reverseClock += dt;
                if (reverseClock >= REVERSE_PERIOD) { reverseClock -= REVERSE_PERIOD; spinDir = -spinDir; }
            } else { reverseClock = 0.0f; spinDir = 1; }
        }
        if (coreSpin) coreSpin->degPerSec = speed * spinDir;
    }

    void pushArmState() {
        for (BossArm* a : arms) {
            a->weapon = a->pairA ? Weapon::Minigun : Weapon::HMG;
            a->firing = (phase == BossPhase::Supresion); // Fase 2: brazos giran/letales, sin balas
        }
    }

    void emitSteam(float dt) {
        if (!steam || !coreT) return;
        steamAcc += (heat / 100.0f) * 22.0f * dt;
        while (steamAcc >= 1.0f) {
            steamAcc -= 1.0f;
            float ang = frand(0.0f, 2.0f * PI), r = frand(0.0f, 46.0f);
            steam->emit(coreT->x + std::cos(ang) * r, coreT->y + std::sin(ang) * r,
                        frand(-20.0f, 20.0f), frand(-70.0f, -25.0f),
                        frand(0.6f, 1.2f), 5.0f, 12.0f, 200, 205, 215, 130);
        }
    }

    void enterTransition() {
        if (phase != BossPhase::Supresion) return;
        phase = BossPhase::Transicion; transTimer = TRANSITION_TIME;
        // Las placas pasan a ser LETALES ya mismo: aparta al jugador de la esquina hacia
        // el centro y dale inmunidad durante la transicion, para que no muera al instante
        // sobre la placa que acaba de cerrar (la cinematica tapa este reposicionamiento).
        if (player) {
            player->transform->x *= 0.55f; player->transform->y *= 0.55f;
            if (auto rb = player->getComponent<RigidBody2D>()) { rb->velocityX = 0.0f; rb->velocityY = 0.0f; }
        }
        if (hp) hp->setInvulnerable(TRANSITION_TIME + 0.6f);
        showTitle("ERRADICACION");
        setNexus("MATRIZ SIGMA. ERRADICACION");
        if (pool) pool->clear();
        if (timer) timer->running = false;
    }
    void enterPhase2() {
        phase = BossPhase::Erradicacion;
        if (flames) flames->enable();
        if (mines)  mines->enable();
        if (blocks) blocks->enable();
    }
    void win() {
        phase = BossPhase::Ganado; winTimer = WIN_TIME; shockR = 0.0f;
        if (hp) hp->setInvulnerable(20.0f);
        showTitle("SOBRECALENTADO");
        setNexus("SUPERVIVENCIA MAXIMA. LOTE 0002");
        if (fade) fade->fadeOut(0.9f);
    }

    void resetProgress(bool withFade) {
        heat = 0.0f; naturalHeat = 0.0f; for (float& p : plateProgress) p = 0.0f;
        phase = BossPhase::Supresion;
        transTimer = 0.0f; winTimer = 0.0f; shockR = 0.0f; fase2Timer = 0.0f;
        reverseClock = 0.0f; spinDir = 1;
        if (timer) timer->reset();
        if (steam) steam->clear();       // limpia vapor y gore vivos del intento anterior
        if (gore)  gore->clear();
        if (flames) { flames->reset(); flames->intensity = 0.0f; }
        if (mines)  { mines->reset();  mines->intensity = 0.0f; }
        if (blocks) blocks->reset();
        if (pool) pool->clear();
        if (player) {
            player->transform->x = startX; player->transform->y = startY;
            if (auto rb = player->getComponent<RigidBody2D>()) { rb->velocityX = 0.0f; rb->velocityY = 0.0f; }
        }
        if (hp) { hp->reset(); hp->setInvulnerable(0.5f); }
        setNexus("ESPECIMEN EN POSICION");
        if (withFade) { showTitle("HERCULES-1"); if (fade) fade->fadeIn(0.6f); } // solo al reiniciar tras ganar
    }

    void updateHUD() {
        // El temporizador solo en modo debug (F1); en juego el HUD es minimo.
        if (timerText && timer)
            timerText->setText(Debug::isEnabled()
                ? std::string("TIEMPO ") + std::to_string((int)std::ceil(timer->remaining()))
                : std::string());
        if (heatText)
            heatText->setText(std::string("CALOR ") + std::to_string((int)heat) + "%");
    }
};

//  Onda de choque de victoria: anillo no letal que se expande.
class ShockwaveView : public Component {
public:
    BossRoom* boss = nullptr;
    void render() override {
        if (!boss || !boss->winning() || !boss->coreT) return;
        float r = boss->shockRadius();
        int a = (int)(220.0f * (1.0f - r / 1100.0f)); if (a < 0) a = 0; if (a > 220) a = 220;
        Shapes::outlineCircle(*gameObject->scene, boss->coreT->x, boss->coreT->y, r, 255, 240, 200, a);
    }
};

//  Barra de CALOR (el jefe no tiene HP, tiene un medidor de calor). Reutiliza
class HeatBar : public Component {
public:
    BossRoom* boss = nullptr;
    void awake() override {
        frameTex = gameObject->scene->getAssets().loadTexture("assets/redacted/ui/heatbar_frame.png");
        fillTex  = gameObject->scene->getAssets().loadTexture("assets/redacted/ui/heatbar_fill.png");
    }
    void render() override {
        if (!boss) return;
        SDL_Renderer* r = gameObject->scene->getRenderer();
        int ow = 0, oh = 0; SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
        float fw = ow * 0.34f, fh = fw * (179.0f / 958.0f); // marco (958x179)
        float cx = ow * 0.5f, cy = oh * 0.03f + fh * 0.5f;  // arriba, centrado
        float frac = boss->heat / 100.0f; if (frac < 0.0f) frac = 0.0f; if (frac > 1.0f) frac = 1.0f;
        // El MARCO va primero: su cavidad (pantalla oscura) es OPACA, asi que si fuese
        if (frameTex) {
            SDL_FRect dst{ cx - fw * 0.5f, cy - fh * 0.5f, fw, fh };
            SDL_RenderTexture(r, frameTex, nullptr, &dst);
        }
        // Relleno naranja->rojo ENCIMA, recortado a la fraccion de calor (izq->der): la
        if (fillTex && frac > 0.0f) {
            float fillW = fw * (876.0f / 958.0f), fillH = fh * (106.0f / 179.0f);
            float leftX = cx - fillW * 0.5f;
            SDL_FRect src{ 0.0f, 0.0f, 876.0f * frac, 106.0f };
            SDL_FRect dst{ leftX, cy - fillH * 0.5f, fillW * frac, fillH };
            Uint8 g = (Uint8)(180.0f - 120.0f * frac); // mas rojo con mas calor
            SDL_SetTextureColorMod(fillTex, 255, g, 40);
            SDL_RenderTexture(r, fillTex, &src, &dst);
            SDL_SetTextureColorMod(fillTex, 255, 255, 255);
        }
    }
private:
    SDL_Texture* frameTex = nullptr; SDL_Texture* fillTex = nullptr;
};

//  Placa de presion: en Fase 1 suma calor a pulsos al pisarla. En Fase 2 las placas
class PressurePlate : public Component {
public:
    BossRoom* state = nullptr;
    int   index = 0;              // cual de las 4 placas (0..3)
    float w = 80.0f, h = 80.0f;

    void awake() override {
        activeTex = gameObject->scene->getAssets().loadTexture("assets/redacted/hazard/plate_active.png");
        closedTex = gameObject->scene->getAssets().loadTexture("assets/redacted/hazard/plate_closed.png");
    }

    void onCollision(GameObject* other) override {
        if (!other->hasTag("player")) return;
        if (state && state->platesLocked()) {                 // Fase 2: placa al rojo = letal
            if (auto h2 = other->getComponent<Health>()) h2->kill();
        } else {
            contact = true;                                   // Fase 1: contacto para el calor
        }
    }
    void update(float dt) override {
        animT += dt;
        bool lethal = state ? state->platesLocked() : false;
        bool closed = state ? state->isPlateClosed(index) : false;
        pressed = contact && !lethal && !closed;
        if (pressed && state) state->addPlateProgress(index, dt); // sube mientras se mantenga pisada
        contact = false;
    }
    void render() override {
        Scene& s = *gameObject->scene;
        float x = gameObject->transform->x, y = gameObject->transform->y;
        bool lethal = state ? state->platesLocked() : false;
        bool closed = state ? state->isPlateClosed(index) : false;
        if (lethal) { // Fase 2: placa activa teñida de rojo incandescente = peligro
            float pulse = 0.5f + 0.5f * std::sin(animT * 6.0f);
            drawWorldFrame(s, activeTex, x, y, w, h, nullptr, 0.0f, 255, (int)(55 + 45 * pulse), 35, 235);
        } else if (closed) { // cerrada al 100%: tapa encima
            drawWorldFrame(s, closedTex, x, y, w, h, nullptr, 0.0f, 255, 255, 255, 245);
        } else { // interactiva: se ilumina segun el progreso (verde/ambar), brillante al pisarla
            float frac = state ? state->plateFrac(index) : 0.0f;
            int g = 110 + (int)(110 * frac); if (pressed) g = 255; if (g > 255) g = 255;
            drawWorldFrame(s, activeTex, x, y, w, h, nullptr, 0.0f, 255, g, 90, 255);
        }
    }
private:
    SDL_Texture* activeTex = nullptr; SDL_Texture* closedTex = nullptr;
    bool contact = false, pressed = false; float animT = 0.0f;
};

//  Cartel de transicion a Fase 2: durante los ~3 s de TRANSICION del jefe
class BossFase2Card : public Component {
public:
    BossRoom* boss = nullptr;
    void awake() override { tex = gameObject->scene->getAssets().loadTexture("assets/redacted/scenes/boss_fase2.jpg"); }
    void render() override {
        if (!boss || boss->phase != BossPhase::Transicion || !tex) return;
        SDL_Renderer* r = gameObject->scene->getRenderer();
        int ow = 0, oh = 0; SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
        SDL_FRect dst{ 0.0f, 0.0f, (float)ow, (float)oh };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
private:
    SDL_Texture* tex = nullptr;
};

} // namespace (componentes locales)

//  Construccion de la escena
void buildBulletHell(Scene& scene) {
    Audio::load("death", "assets/audio/death.wav");
    Audio::load("dash",  "assets/audio/dash.wav");

    auto cam = scene.createGameObject("MainCamera")->addComponent<Camera>();
    cam->fitToWorld(ROOM_W, ROOM_H); // la sala llena la pantalla; lo de fuera queda negro
    scene.createGameObject("Room")->addComponent<RoomRenderer>();

    auto decals = scene.createGameObject("Decals")->addComponent<DecalLayer>();
    decals->setup(-HALF_W, -HALF_H, (int)ROOM_W, (int)ROOM_H);

    // Muros solidos.
    struct WallDef { float x, y, w, h; };
    const WallDef walls[4] = {
        { 0.0f, -HALF_H - WALL * 0.5f, ROOM_W + 2 * WALL, WALL },
        { 0.0f,  HALF_H + WALL * 0.5f, ROOM_W + 2 * WALL, WALL },
        { -HALF_W - WALL * 0.5f, 0.0f, WALL, ROOM_H },
        {  HALF_W + WALL * 0.5f, 0.0f, WALL, ROOM_H },
    };
    for (const WallDef& w : walls) {
        GameObject* wall = scene.createGameObject("Wall");
        wall->transform->x = w.x; wall->transform->y = w.y;
        auto bc = wall->addComponent<BoxCollider>(); bc->width = w.w; bc->height = w.h;
    }

    GameObject* logicObj = scene.createGameObject("RoomLogic");
    auto state = logicObj->addComponent<BossRoom>();
    Audio::playMusic("assets/audio/music_boss.ogg"); // tema del jefe HERCULES-1 (en bucle)

    // 4 placas en las 4 esquinas, bajo el jugador.
    const float pX = HALF_W - 100.0f, pY = HALF_H - 100.0f;
    const float platePos[4][2] = { { -pX, -pY }, { pX, -pY }, { -pX, pY }, { pX, pY } };
    int plateIdx = 0;
    for (auto& pp : platePos) {
        GameObject* plate = scene.createGameObject("Plate");
        plate->tag = "plate";
        plate->transform->x = pp[0]; plate->transform->y = pp[1];
        auto bc = plate->addComponent<BoxCollider>();
        bc->width = 80.0f; bc->height = 80.0f; bc->isTrigger = true;
        auto pc = plate->addComponent<PressurePlate>();
        pc->state = state; pc->index = plateIdx++;
    }

    // Jugador.
    GameObject* player = scene.createGameObject("Player");
    player->tag = "player";
    player->transform->x = 0.0f; player->transform->y = HALF_H - 170.0f;
    setupPlayerAnim(player); // clon animado (hojas idle/run direccionales)
    auto rb = player->addComponent<RigidBody2D>(); rb->gravityScale = 0.0f;
    auto body = player->addComponent<BoxCollider>(); body->width = BODY_W; body->height = BODY_H;
    auto hurt = player->addComponent<CircleCollider>(); hurt->radius = HURTBOX_RADIUS;
    auto hp = player->addComponent<Health>();
    player->addComponent<BulletHellController>();

    // Particulas: gore (rojo) y vapor del jefe (gris).
    auto fx = scene.createGameObject("Gore")->addComponent<ParticleSystem>(512);
    fx->setBlendMode(BlendMode::Add);
    fx->colR = 230; fx->colG = 30; fx->colB = 30;
    fx->speedMin = 120.0f; fx->speedMax = 420.0f;
    fx->lifeMin = 0.35f; fx->lifeMax = 0.9f;
    fx->sizeStart = 10.0f; fx->sizeEnd = 1.0f; fx->gravity = 500.0f;

    auto steam = scene.createGameObject("Steam")->addComponent<ParticleSystem>(256);
    steam->setBlendMode(BlendMode::Alpha);

    // Peligros de Fase 2 (inactivos hasta la Erradicacion; aparecen al azar).
    auto flames = scene.createGameObject("Flames")->addComponent<FlamePatchField>();
    flames->target = hurt; flames->hp = hp; flames->decals = decals;
    auto mines = scene.createGameObject("Mines")->addComponent<MineField>();
    mines->target = hurt; mines->hp = hp; mines->decals = decals;
    auto blocks = scene.createGameObject("Blocks")->addComponent<FallingBlocks>();
    blocks->target = hurt; blocks->hp = hp;

    // Chasis central: rota, SOLIDO (cuerpo ~3 tiles) y es padre de los brazos.
    GameObject* core = scene.createGameObject("Core");
    core->tag = "boss";
    auto coreSpin = core->addComponent<Spin>();
    core->transform->scaleX = core->transform->scaleY = 0.42f; // hercules_base 521px -> ~220px
    core->addComponent<SpriteRenderer>("assets/redacted/boss/hercules_base.png");
    auto cbody = core->addComponent<BoxCollider>();
    cbody->width = cbody->height = CORE_BODY; // el jugador no puede pararse en el centro

    std::vector<BossArm*> arms;
    for (int i = 0; i < ARM_COUNT; ++i) {
        GameObject* arm = scene.createGameObject("Arm");
        arm->transform->scaleX = arm->transform->scaleY = ARM_SCALE;
        arm->addComponent<SpriteRenderer>((i % 2 == 0)
            ? "assets/redacted/boss/hercules_arm1.png"   // par A: brazo con sierra
            : "assets/redacted/boss/hercules_arm2.png"); // par B: brazo blindado
        auto ch = arm->addComponent<ChildOf>();
        ch->setParent(core);
        float ang = (float)i / ARM_COUNT * 2.0f * PI;
        ch->localX = std::cos(ang) * ARM_OFFSET;
        ch->localY = std::sin(ang) * ARM_OFFSET;
        ch->localRotation = ang * 180.0f / PI + 90.0f; // el brazo (vertical) apunta hacia AFUERA
        auto a = arm->addComponent<BossArm>();
        a->coreT = core->transform; a->target = hurt; a->hp = hp;
        a->pairA = (i % 2 == 0);
        arms.push_back(a);

        // Arma montada en la PUNTA del brazo: viaja y gira con el conjunto (ChildOf al
        GameObject* wep = scene.createGameObject("ArmWeapon");
        wep->transform->scaleX = wep->transform->scaleY = WEAPON_SCALE;
        wep->addComponent<SpriteRenderer>((i % 2 == 0)
            ? "assets/redacted/boss/hercules_minigun.png"
            : "assets/redacted/boss/hercules_lasergun.png");
        auto wch = wep->addComponent<ChildOf>();
        wch->setParent(core);
        wch->localX = std::cos(ang) * WEAPON_R;
        wch->localY = std::sin(ang) * WEAPON_R;
        wch->localRotation = ang * 180.0f / PI; // arma horizontal: barril hacia afuera
    }

    // Pool de balas.
    auto pool = scene.createGameObject("BulletManager")->addComponent<BulletPool>(1200);
    pool->setColor(255, 255, 255);                       // sin tinte: muestra el arte real
    pool->setSprite("assets/redacted/bullet/bullets.png");
    pool->setStrip(234, 469, 4);                          // 4 variantes; type elige columna
    pool->setRotateToVelocity(true);                     // la bala apunta a donde viaja
    pool->setBounds(-HALF_W, -HALF_H, HALF_W, HALF_H);
    pool->setTarget(hurt);
    for (BossArm* a : arms) a->pool = pool;

    scene.createGameObject("Shock")->addComponent<ShockwaveView>()->boss = state;

    auto timer = logicObj->addComponent<PhaseTimer>();
    timer->duration = PHASE1_TIME;
    timer->onComplete = [state]() { state->onPhase1TimerEnd(); };

    // HUD de texto (fuente 5x7 embebida).
    auto mkText = [&scene](const char* name, float yTop, float size) {
        GameObject* o = scene.createGameObject(name);
        o->layer = 100;                                  // HUD por encima del gameplay
        o->transform->x = 0.0f; o->transform->y = yTop; // offset desde el centro-arriba
        auto t = o->addComponent<TextRenderer>();
        t->screenSpace = true; t->anchorX = 0.5f; t->centered = true; t->pixelSize = size;
        return t;
    };
    // Rotulo central del jefe: aparece con el fundido y se desvanece.
    GameObject* titleObj = scene.createGameObject("Title");
    titleObj->layer = 100;
    titleObj->transform->y = -30.0f;
    auto statusText = titleObj->addComponent<TextRenderer>();
    statusText->screenSpace = true; statusText->anchorX = 0.5f; statusText->anchorY = 0.5f;
    statusText->centered = true; statusText->pixelSize = 7.0f;
    // El texto va DEBAJO de la barra de calor grafica (que ocupa la franja superior).
    auto heatText  = mkText("Heat", 168.0f, 3.0f);
    heatText->setColor(255, 140, 40);
    auto nexusText = mkText("Nexus", 206.0f, 2.0f); // subtitulo NEXUS-9
    nexusText->setColor(150, 200, 220);
    auto timerText = mkText("Timer", 250.0f, 6.0f); // solo visible en debug (F1)

    // Barra de calor (reemplaza la barra de vida: el jefe se mide por calor,).
    auto heatBarObj = scene.createGameObject("HeatBar");
    heatBarObj->layer = 100;
    heatBarObj->addComponent<HeatBar>()->boss = state;

    // Cartel de transicion a Fase 2 (se muestra solo durante la fase TRANSICION).
    auto fase2Card = scene.createGameObject("Fase2Card");
    fase2Card->layer = 150; // sobre el HUD, bajo el velo de fundido
    fase2Card->addComponent<BossFase2Card>()->boss = state;

    auto fadeObj = scene.createGameObject("Fade");
    fadeObj->layer = 200; // el velo de transicion cubre tambien el HUD
    auto fade = fadeObj->addComponent<ScreenFade>();
    fade->fadeIn(0.6f);

    // Cablear el jefe.
    state->timer = timer; state->statusText = statusText; state->timerText = timerText;
    state->heatText = heatText; state->nexusText = nexusText; state->fade = fade;
    state->player = player; state->hp = hp; state->pool = pool; state->decals = decals;
    state->coreSpin = coreSpin; state->coreT = core->transform;
    state->flames = flames; state->mines = mines; state->blocks = blocks; state->steam = steam; state->gore = fx; state->arms = arms;
    state->startX = player->transform->x; state->startY = player->transform->y;
    state->introLine(); // muestra la primera linea de NEXUS-9

    // GORE + MUERTE: estalla, mancha permanente y pierde TODO el progreso.
    Scene* scenePtr = &scene;
    hp->onDeath = [player, fx, decals, state, scenePtr]() {
        Audio::play("death"); // PRIMERO: el sonido suena justo al detectar el impacto
        float dx = player->transform->x, dy = player->transform->y;
        spawnExplosion(*scenePtr, dx, dy, 90.0f); // el clon revienta
        fx->emitBurst(dx, dy, 48);
        for (int i = 0; i < 10; ++i) {
            float ox = frand(-30.0f, 30.0f), oy = frand(-30.0f, 30.0f);
            float s = frand(8.0f, 24.0f);
            decals->stampRect(dx + ox, dy + oy, s, s, 110, 8, 8, 200);
        }
        state->onPlayerDeath();
    };
}
