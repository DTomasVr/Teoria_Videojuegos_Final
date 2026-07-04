#include "Sector1.h"
#include "SceneFlow.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/SpriteRenderer.h"
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
#include "../engine/Camera.h"
#include "../engine/Debugger.h"
#include "../engine/Audio.h"

// ============================================================================
//  Parametros comunes del Sector 1.
// ============================================================================
namespace {
    constexpr float ROOM_W = 880.0f, ROOM_H = 700.0f, WALL = 40.0f;
    constexpr float HALF_W = ROOM_W * 0.5f, HALF_H = ROOM_H * 0.5f;

    constexpr float PLAYER_SPEED   = 260.0f;
    constexpr float HURTBOX_RADIUS = 8.0f;
    constexpr float BODY_SIZE      = 44.0f;
    constexpr float DASH_SPEED    = 900.0f, DASH_DURATION = 0.12f;
    constexpr float DASH_IFRAMES  = 0.12f,  DASH_COOLDOWN = 0.5f;

    constexpr float BULLET_RADIUS = 8.0f, BULLET_LIFE = 6.0f;

    // Torreta MK-II (GDD 4.2): angulo fijo, telegrafia con laser rojo, rafaga de
    // balas lentas. La cadencia cambia por fase.
    constexpr float TURRET_INT_P1 = 4.0f;   // Fase 1: 1 rafaga/4 s
    constexpr float TURRET_INT_P2 = 2.5f;   // Fase 2: 1 rafaga/2.5 s
    constexpr int   TURRET_BURST  = 4;
    constexpr float TURRET_SPREAD = 0.16f;
    constexpr float TURRET_BSPEED = 175.0f; // balas lentas
    constexpr float TURRET_TELE   = 0.8f;   // seg de barrido laser previo

    // Nuke (GDD 4.2): zona marcada, 2 s de aviso, detonacion letal, crater persistente.
    constexpr float NUKE_RADIUS = 95.0f, NUKE_INTERVAL = 8.0f, NUKE_WARN = 2.0f;

    // Lanzallamas + parches de fuego (GDD 4.2 / 6.2).
    constexpr float FIRE_RADIUS = 48.0f, FIRE_LIFE = 3.0f;   // parche persiste 3 s
    constexpr float FLAME_RANGE = 300.0f, FLAME_CONE_HALF = 0.3927f; // cono de 45 grados
    constexpr float JET_TIME = 1.2f;        // duracion del chorro por ciclo

    // Minas: ya NO estaticas. Caen del techo (sombra que crece), explotan al aterrizar
    // (letal en el radio) y desaparecen dejando una quemadura NEGRA en el suelo.
    constexpr float MINE_RADIUS = 70.0f;  // radio del estallido
    constexpr float MINE_DROP   = 1.1f;   // seg de caida (telegrafo con sombra)
    constexpr float MINE_GAP    = 2.0f;   // seg de espera entre caidas

    // Rotulo de la sala (aparece con el fundido, en el centro, y se desvanece).
    constexpr float TITLE_TIME = 3.2f, TITLE_FADE = 1.2f;

    // Camara 01.
    constexpr float SURVIVE_TIME = 40.0f, PHASE2_AT = 20.0f;

    constexpr float PI = 3.14159265f;
    const char* SHIPS_SHEET = "assets/kenney_pixelshmup/Tilemap/ships_packed.png";
    constexpr int SHIP_CELL = 32;

    float frand(float a, float b) { return a + (b - a) * ((float)std::rand() / (float)RAND_MAX); }
}

// Componentes locales de esta escena. Van en un namespace anonimo (enlace interno)
// para NO chocar con clases del mismo nombre en otras escenas (p.ej. RoomRenderer
// tambien existe en BulletHell.cpp).
namespace {

// ----------------------------------------------------------------------------
//  Suelo y contorno de la sala (hormigon oscuro).
// ----------------------------------------------------------------------------
class RoomRenderer : public Component {
public:
    void render() override {
        SDL_Renderer* r = gameObject->scene->getRenderer();
        Camera* cam = gameObject->scene->getActiveCamera();
        float sLx, sTy, sRx, sBy;
        if (cam) { cam->worldToScreen(-HALF_W, -HALF_H, sLx, sTy);
                   cam->worldToScreen( HALF_W,  HALF_H, sRx, sBy); }
        else { sLx = -HALF_W; sTy = -HALF_H; sRx = HALF_W; sBy = HALF_H; }
        SDL_FRect floor{ sLx, sTy, sRx - sLx, sBy - sTy };
        SDL_SetRenderDrawColor(r, 30, 28, 26, 255); SDL_RenderFillRect(r, &floor);
        SDL_SetRenderDrawColor(r, 110, 105, 100, 255); SDL_RenderRect(r, &floor);
    }
};

// ----------------------------------------------------------------------------
//  Jugador: movimiento 8-dir + Dash con i-frames + tinte de invulnerabilidad.
// ----------------------------------------------------------------------------
class PlayerShip : public Component {
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
        if (dashKey && !dashPrev && cd <= 0.0f) {
            dashT = DASH_DURATION; cd = DASH_COOLDOWN;
            ddx = (len > 0.0f) ? ix : lastX; ddy = (len > 0.0f) ? iy : lastY;
            if (hp) hp->setInvulnerable(DASH_IFRAMES);
            Audio::play("dash");
        }
        dashPrev = dashKey;

        if (dashT > 0.0f) { rb->velocityX = ddx * DASH_SPEED; rb->velocityY = ddy * DASH_SPEED; dashT -= dt; }
        else { rb->velocityX = ix * PLAYER_SPEED; rb->velocityY = iy * PLAYER_SPEED; }
        if (cd > 0.0f) cd -= dt;

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
    float lastX = 0.0f, lastY = 1.0f, ddx = 0.0f, ddy = 1.0f, dashT = 0.0f, cd = 0.0f;
    bool dashPrev = false;
};

// ----------------------------------------------------------------------------
//  Torreta MK-II: apunta en una direccion FIJA, avisa con un barrido laser rojo y
//  dispara una rafaga de balas lentas. La cadencia la fija la fase de la camara.
// ----------------------------------------------------------------------------
class Turret : public Component {
public:
    BulletPool* pool = nullptr;
    float aimX = 0.0f, aimY = 1.0f;  // direccion de disparo (normalizada)
    float interval = TURRET_INT_P1;
    float rotSpeed = 0.0f;           // deg/s (0 = torreta fija, MK-II de la Camara 01)
    int   burst    = TURRET_BURST;   // balas por rafaga (MK-IV = 5)
    float streamInterval = 0.0f;     // 0 = sin flujo continuo; >0 = una bala cada X seg
                                     // (el flujo NO se detiene aunque salga una rafaga)
    // Riel horizontal (MK-IV, GDD 6.3): la torreta se desliza entre railMin/railMax.
    float railMin = 0.0f, railMax = 0.0f, railSpeed = 0.0f;
    // Barrido ACOTADO: oscila el angulo dentro de +-sweepRange alrededor de sweepBase.
    // Para torretas de pared que barren su semiplano (180 grados) sin girar 360.
    float sweepBase = 0.0f, sweepRange = 0.0f, sweepSpeed = 0.0f;

    void setOffset(float t) { timer = t; }              // para escalonar N+W vs E
    void setAimAngle(float deg) {                       // para torretas rotativas
        angle = deg * (PI / 180.0f); aimX = std::cos(angle); aimY = std::sin(angle);
    }
    // Barrido de pared: base = normal hacia dentro; rango = medio arco (90 = 180 total).
    void setSweep(float baseDeg, float rangeDeg, float speed) {
        sweepBase = baseDeg * (PI / 180.0f); sweepRange = rangeDeg * (PI / 180.0f); sweepSpeed = speed;
    }

    void update(float dt) override {
        if (sweepSpeed != 0.0f) { // barrido acotado (semiplano de la pared)
            sweepPhase += sweepSpeed * dt;
            angle = sweepBase + std::sin(sweepPhase) * sweepRange;
            aimX = std::cos(angle); aimY = std::sin(angle);
            gameObject->transform->rotation = angle * 180.0f / PI + 90.0f;
        } else if (rotSpeed != 0.0f) { // giro continuo (MK-II rotativa / MK-IV)
            angle += rotSpeed * (PI / 180.0f) * dt;
            aimX = std::cos(angle); aimY = std::sin(angle);
            gameObject->transform->rotation = angle * 180.0f / PI + 90.0f;
        }
        if (railSpeed != 0.0f) { // se desliza por el riel de techo (ida y vuelta)
            float& x = gameObject->transform->x;
            x += railDir * railSpeed * dt;
            if (x >= railMax) { x = railMax; railDir = -1.0f; }
            else if (x <= railMin) { x = railMin; railDir = 1.0f; }
        }
        if (streamInterval > 0.0f) { // flujo continuo de balas (nunca se detiene)
            streamTimer += dt;
            while (streamTimer >= streamInterval) { streamTimer -= streamInterval; fireOne(); }
        }
        if (interval > 0.0f) {       // rafaga de vez en cuando (interval <= 0 = sin rafaga)
            timer += dt;
            if (timer >= interval) { timer -= interval; fire(); }
        }
    }
    void render() override {
        // Barrido laser rojo durante los ultimos TURRET_TELE seg antes de disparar.
        float toFire = interval - timer;
        if (toFire <= TURRET_TELE) {
            float ox = gameObject->transform->x, oy = gameObject->transform->y;
            float pulse = 0.5f + 0.5f * std::sin(timer * 30.0f);
            int a = (int)(90 + 140 * pulse);
            Shapes::line(*gameObject->scene, ox, oy, ox + aimX * 1400.0f, oy + aimY * 1400.0f, 255, 40, 40, a);
        }
    }
private:
    float timer = 0.0f, angle = 0.0f, railDir = 1.0f, sweepPhase = 0.0f, streamTimer = 0.0f;
    void fireOne() { // una sola bala en la direccion actual (flujo)
        if (!pool) return;
        float ox = gameObject->transform->x, oy = gameObject->transform->y;
        float ang = std::atan2(aimY, aimX);
        pool->spawn(ox, oy, std::cos(ang) * TURRET_BSPEED, std::sin(ang) * TURRET_BSPEED,
                    BULLET_RADIUS, BULLET_LIFE);
    }
    void fire() { // rafaga en abanico
        if (!pool) return;
        float ox = gameObject->transform->x, oy = gameObject->transform->y;
        float base = std::atan2(aimY, aimX);
        for (int i = 0; i < burst; ++i) {
            float ang = base + (i - (burst - 1) * 0.5f) * TURRET_SPREAD;
            pool->spawn(ox, oy, std::cos(ang) * TURRET_BSPEED, std::sin(ang) * TURRET_BSPEED,
                        BULLET_RADIUS, BULLET_LIFE);
        }
    }
};

// ----------------------------------------------------------------------------
//  Nuke (bomba de caida): zona fija marcada en el suelo. Inactiva hasta que se
//  ARMA (Fase 2). Cuando esta armada cicla: aviso parpadeante (NUKE_WARN seg) ->
//  detonacion letal dentro del radio + crater persistente + particulas -> espera.
// ----------------------------------------------------------------------------
class Nuke : public Component {
public:
    CircleCollider* target = nullptr;
    Health*         hp = nullptr;
    DecalLayer*     decals = nullptr;
    ParticleSystem* fx = nullptr;
    bool  armed = false;
    float interval = NUKE_INTERVAL; // cadencia (la Camara 03 la acelera por fase)

    void setOffset(float t) { offset = t; timer = t; }
    void disarm() { armed = false; timer = offset; flash = 0.0f; } // conserva el desfase al reintentar

    void update(float dt) override {
        if (flash > 0.0f) { flash -= dt * 4.0f; if (flash < 0.0f) flash = 0.0f; }
        if (!armed) return;
        timer += dt;
        if (timer >= interval) detonate();
    }
    void render() override {
        Scene& s = *gameObject->scene;
        float x = gameObject->transform->x, y = gameObject->transform->y;
        // Marcador de la zona (siempre visible: "marcada desde el inicio").
        Shapes::outlineCircle(s, x, y, NUKE_RADIUS, 150, 40, 40, armed ? 120 : 70);
        if (armed) {
            float toDet = interval - timer;
            if (toDet <= NUKE_WARN) {
                float pulse = 0.5f + 0.5f * std::sin(timer * 18.0f);
                Shapes::fillCircle(s, x, y, NUKE_RADIUS, 200, 40, 20, (int)(30 + 90 * pulse));
            }
        }
        if (flash > 0.0f) Shapes::fillCircle(s, x, y, NUKE_RADIUS, 255, 230, 180, (int)(200 * flash));
    }
private:
    float timer = 0.0f, flash = 0.0f, offset = 0.0f;
    void detonate() {
        float x = gameObject->transform->x, y = gameObject->transform->y;
        if (target && hp && Collision::pointInCircle(target->centerX(), target->centerY(), x, y, NUKE_RADIUS))
            hp->kill();
        if (fx) fx->emitBurst(x, y, 30);
        if (decals)
            for (int i = 0; i < 16; ++i) {
                float ox = frand(-NUKE_RADIUS * 0.7f, NUKE_RADIUS * 0.7f);
                float oy = frand(-NUKE_RADIUS * 0.7f, NUKE_RADIUS * 0.7f);
                decals->stampRect(x + ox, y + oy, frand(10.0f, 26.0f), frand(10.0f, 26.0f), 16, 12, 14, 220);
            }
        flash = 1.0f; timer = 0.0f;
    }
};

// ----------------------------------------------------------------------------
//  Parches de fuego: pool de manchas de napalm que caen (las suelta el lanzallamas),
//  persisten FIRE_LIFE seg, son letales por contacto y dejan quemadura permanente.
// ----------------------------------------------------------------------------
class FirePatches : public Component {
public:
    CircleCollider* target = nullptr;
    Health*         hp = nullptr;
    DecalLayer*     decals = nullptr;

    void spawnAt(float x, float y) {
        for (P& p : pool) if (!p.active) { p.x = x; p.y = y; p.life = FIRE_LIFE; p.active = true; return; }
    }
    void clear() { for (P& p : pool) p.active = false; }

    void update(float dt) override {
        for (P& p : pool) { if (!p.active) continue; p.life -= dt; if (p.life <= 0.0f) { p.active = false; scorch(p.x, p.y); } }
        if (target && hp) {
            float cx = target->centerX(), cy = target->centerY();
            for (P& p : pool)
                if (p.active && Collision::pointInCircle(cx, cy, p.x, p.y, FIRE_RADIUS)) { hp->kill(); break; }
        }
    }
    void render() override {
        Scene& s = *gameObject->scene;
        for (P& p : pool) {
            if (!p.active) continue;
            float frac = p.life / FIRE_LIFE; if (frac < 0.0f) frac = 0.0f;
            int a = (int)(60 + 120 * frac);
            Shapes::fillCircle(s, p.x, p.y, FIRE_RADIUS, 255, 110, 20, (int)(a * 0.45f));
            Shapes::fillCircle(s, p.x, p.y, FIRE_RADIUS * 0.6f, 255, 200, 60, a);
        }
    }
private:
    struct P { float x = 0, y = 0, life = 0; bool active = false; };
    std::vector<P> pool = std::vector<P>(128);
    void scorch(float x, float y) {
        if (!decals) return;
        for (int i = 0; i < 5; ++i) {
            float ox = frand(-FIRE_RADIUS * 0.6f, FIRE_RADIUS * 0.6f);
            float oy = frand(-FIRE_RADIUS * 0.6f, FIRE_RADIUS * 0.6f);
            decals->stampRect(x + ox, y + oy, frand(10.0f, 20.0f), frand(10.0f, 20.0f), 22, 14, 10, 180);
        }
    }
};

// ----------------------------------------------------------------------------
//  Lanzallamas de techo (GDD 4.2 / 6.2): cono de 45 grados. Cada 'interval' seg
//  escupe un chorro durante JET_TIME: letal dentro del cono y siembra parches de
//  fuego en el suelo. La cadencia baja por fase. Inactivo hasta la Fase 2.
// ----------------------------------------------------------------------------
class Flamethrower : public Component {
public:
    CircleCollider* target = nullptr;
    Health*         hp = nullptr;
    FirePatches*    patches = nullptr;
    float aimAngle = PI * 0.5f;         // radianes (hacia abajo, dentro de la sala)
    float interval = 4.0f;
    float range = FLAME_RANGE;          // alcance (mayor en las paredes de la Camara 03)
    bool  active = false;
    // Barrido opcional (GDD 6.3): el cono oscila y arrastra el fuego = "ola viajera".
    float sweepBase = PI * 0.5f, sweepRange = 0.0f, sweepSpeed = 0.0f;

    void reset() { timer = offset; spawnAcc = 0.0f; } // conserva el desfase al reintentar
    void setOffset(float t) { offset = t; timer = t; } // para desfasar los 2 lanzallamas

    void update(float dt) override {
        if (sweepSpeed != 0.0f) { // el cono oscila de lado a lado
            sweepPhase += sweepSpeed * dt;
            aimAngle = sweepBase + std::sin(sweepPhase) * sweepRange;
        }
        if (!active) return;
        timer += dt;
        bool jetting = (timer >= interval - JET_TIME);
        if (timer >= interval) timer -= interval;
        if (!jetting) return;

        float ox = gameObject->transform->x, oy = gameObject->transform->y;
        if (target && hp && !hp->isInvulnerable()) { // letal dentro del cono
            float dx = target->centerX() - ox, dy = target->centerY() - oy;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= range && std::fabs(angDiff(std::atan2(dy, dx), aimAngle)) <= FLAME_CONE_HALF)
                hp->kill();
        }
        spawnAcc += dt; // siembra parches de fuego en el suelo dentro del cono
        if (spawnAcc >= 0.12f && patches) {
            spawnAcc = 0.0f;
            float a = aimAngle + frand(-FLAME_CONE_HALF, FLAME_CONE_HALF);
            float d = frand(range * 0.4f, range);
            patches->spawnAt(ox + std::cos(a) * d, oy + std::sin(a) * d);
        }
    }
    void render() override {
        if (!active) return;
        Scene& s = *gameObject->scene;
        float ox = gameObject->transform->x, oy = gameObject->transform->y;
        bool jetting = (timer >= interval - JET_TIME);
        int a = jetting ? 200 : 70; // tenue = telegrafo de direccion; brillante = chorro
        const int N = 9;
        for (int i = 0; i <= N; ++i) {
            float ang = aimAngle - FLAME_CONE_HALF + (2.0f * FLAME_CONE_HALF) * ((float)i / N);
            Shapes::line(s, ox, oy, ox + std::cos(ang) * range, oy + std::sin(ang) * range,
                         255, jetting ? 150 : 90, 30, a);
        }
    }
private:
    float timer = 0.0f, spawnAcc = 0.0f, sweepPhase = 0.0f, offset = 0.0f;
    static float angDiff(float a, float b) { float d = a - b; while (d > PI) d -= 2 * PI; while (d < -PI) d += 2 * PI; return d; }
};

// ----------------------------------------------------------------------------
//  Mina LANZADA desde el techo: ciclo espera -> caida (sombra que crece mientras
//  la mina desciende) -> estallido al aterrizar (letal en el radio) -> desaparece
//  dejando una quemadura NEGRA permanente. Luego repite. Reutilizable.
// ----------------------------------------------------------------------------
class DropMine : public Component {
public:
    CircleCollider* target = nullptr;
    Health*         hp = nullptr;
    DecalLayer*     decals = nullptr;
    bool active = true;
    bool roam = false;   // true = cada caida elige un punto ALEATORIO del mapa

    void setOffset(float t) { timer = t; }        // desfasar las caidas
    void setActive(bool a)  { active = a; }        // la Camara 03 las activa en Fase 2

    void update(float dt) override {
        if (flash > 0.0f) { flash -= dt * 4.0f; if (flash < 0.0f) flash = 0.0f; }
        if (!active) return;
        timer += dt;
        if (state == 0) {                          // esperando para caer
            if (timer >= MINE_GAP) {
                state = 1; timer = 0.0f;
                if (roam) {                        // aparece en un sitio distinto cada vez
                    gameObject->transform->x = frand(-HALF_W + MINE_RADIUS, HALF_W - MINE_RADIUS);
                    gameObject->transform->y = frand(-HALF_H + MINE_RADIUS, HALF_H - MINE_RADIUS);
                }
            }
        } else {                                   // cayendo (telegrafo)
            if (timer >= MINE_DROP) { explode(); state = 0; timer = 0.0f; }
        }
    }
    void render() override {
        Scene& s = *gameObject->scene;
        float x = gameObject->transform->x, y = gameObject->transform->y;
        if (active && state == 1) {                // sombra que crece + mina descendiendo
            float frac = timer / MINE_DROP; if (frac > 1.0f) frac = 1.0f;
            Shapes::fillCircle(s, x, y, MINE_RADIUS * (0.35f + 0.65f * frac), 0, 0, 0, 130);
            float topY = -HALF_H, cy = topY + (y - topY) * frac;
            Shapes::fillCircle(s, x, cy, 12.0f, 255, 180, 40, 255);
        }
        if (flash > 0.0f) Shapes::fillCircle(s, x, y, MINE_RADIUS, 255, 120, 40, (int)(220 * flash));
    }
private:
    int state = 0; float timer = 0.0f, flash = 0.0f;
    void explode() {
        float x = gameObject->transform->x, y = gameObject->transform->y;
        if (target && hp && Collision::pointInCircle(target->centerX(), target->centerY(), x, y, MINE_RADIUS))
            hp->kill();
        flash = 1.0f;
        if (decals)                                // quemadura NEGRA que queda en el suelo
            for (int i = 0; i < 8; ++i) {
                float ox = frand(-MINE_RADIUS * 0.6f, MINE_RADIUS * 0.6f);
                float oy = frand(-MINE_RADIUS * 0.6f, MINE_RADIUS * 0.6f);
                decals->stampRect(x + ox, y + oy, frand(12.0f, 26.0f), frand(12.0f, 26.0f), 6, 6, 8, 220);
            }
    }
};

// ----------------------------------------------------------------------------
//  Bloque de escombros (GDD 6.2): obstaculo SOLIDO no letal. Bloquea lineas de
//  escape (dilema tactico). El collider solido se agrega aparte en la construccion.
// ----------------------------------------------------------------------------
class DebrisBlock : public Component {
public:
    float w = 120.0f, h = 180.0f;
    void render() override {
        Scene& s = *gameObject->scene;
        float x = gameObject->transform->x, y = gameObject->transform->y;
        Shapes::fillRect(s, x, y, w, h, 70, 66, 60, 255);
        Shapes::outlineRect(s, x, y, w, h, 130, 124, 116, 255);
    }
};

// ----------------------------------------------------------------------------
//  Controlador de camara: temporizador de supervivencia + fases + HUD + victoria
//  y reintento. Sobrevivir el tiempo = camara superada; morir = reinicio inmediato
//  del intento (el gore/crateres persisten). Reutilizable por todas las camaras.
// ----------------------------------------------------------------------------
class Chamber : public Component {
public:
    PhaseTimer*   timer = nullptr;
    TextRenderer *statusText = nullptr, *timerText = nullptr, *nexusText = nullptr;
    ScreenFade*   fade = nullptr;
    GameObject*   player = nullptr;
    Health*       hp = nullptr;
    BulletPool*   pool = nullptr;
    float startX = 0.0f, startY = 0.0f;
    const char* name = "CAMARA";
    const char* introMsg = "ESPECIMEN 0001 EN POSICION";
    int nextScene = 0; // escena a la que avanzar al superar la camara (0 = repetir)

    // Cada camara define como configurar SUS peligros en cada fase (0 = inicio/reintento).
    std::function<void(int)> applyPhase;

    void setPhase(int p) { phase = p; if (applyPhase) applyPhase(p); }
    void onSurvived() { if (!won) win(); }

    void onPlayerDeath() { // reintento inmediato: reinicia el intento desde cero
        won = false; winTimer = 0.0f;
        if (timer) timer->reset();
        setPhase(0);
        if (pool) pool->clear();
        if (player) {
            player->transform->x = startX; player->transform->y = startY;
            if (auto rb = player->getComponent<RigidBody2D>()) { rb->velocityX = 0.0f; rb->velocityY = 0.0f; }
        }
        if (hp) { hp->reset(); hp->setInvulnerable(0.5f); }
        if (fade) fade->blink(0.15f, 0.35f); // parpadeo al aparecer el nuevo clon
        if (nexusText) { nexusText->setText("REINTENTO"); nexusT = 3.0f; }
    }

    void introLine() {
        if (nexusText) { nexusText->setText(introMsg); nexusT = 4.5f; }
        showTitle(name); // el nombre de la sala aparece en el centro con el fundido
    }
    void showNexus(const char* t) { if (nexusText) { nexusText->setText(t); nexusT = 4.0f; } } // linea NEXUS-9 puntual
    void showTitle(const char* t) { if (statusText) { statusText->setText(t); titleTimer = TITLE_TIME; } }

    void update(float dt) override {
        if (nexusT > 0.0f) { nexusT -= dt; if (nexusT <= 0.0f && nexusText) nexusText->setText(""); }
        if (titleTimer > 0.0f && statusText) { // el rotulo central se desvanece solo
            titleTimer -= dt;
            float a = (titleTimer < TITLE_FADE) ? (titleTimer / TITLE_FADE) : 1.0f;
            statusText->setColor(255, 255, 255, (int)(255 * a));
            if (titleTimer <= 0.0f) statusText->setText("");
        }
        if (won) {
            winTimer -= dt;
            if (winTimer <= 0.0f) {
                if (nextScene != 0) SceneFlow::requestScene(nextScene); // avanzar a la siguiente
                else resetChamber();                                   // sin siguiente: repetir
            }
        }
        // El temporizador solo se muestra en modo debug (F1); en juego el HUD es minimo.
        if (timerText && timer)
            timerText->setText(Debug::isEnabled()
                ? std::string("TIEMPO ") + std::to_string((int)std::ceil(timer->remaining()))
                : std::string());
    }

private:
    int phase = 0; bool won = false; float winTimer = 0.0f, nexusT = 0.0f, titleTimer = 0.0f;

    void win() {
        won = true; winTimer = 1.2f;
        if (hp) hp->setInvulnerable(10.0f);
        showTitle("CAMARA SUPERADA");
        if (nexusText) { nexusText->setText("PARAMETROS SUPERADOS"); nexusT = 4.5f; }
        if (fade) fade->fadeOut(0.9f);
    }
    void resetChamber() { // por ahora vuelve a empezar la misma camara (aun no hay 02)
        onPlayerDeath();
        if (fade) fade->fadeIn(0.6f);
    }
};

} // namespace (componentes locales)

// ============================================================================
//  Cámara 01 — "El Pozo" (GDD 6.1)
// ============================================================================
void buildCamara01(Scene& scene) {
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

    GameObject* logicObj = scene.createGameObject("Chamber");
    auto chamber = logicObj->addComponent<Chamber>();
    chamber->name = "CAMARA 01 - EL POZO";
    chamber->nextScene = 6; // al superarla -> Camara 02

    // Jugador: aparece en el centro (GDD: moverse es sobrevivir).
    GameObject* player = scene.createGameObject("Player");
    player->tag = "player";
    player->transform->x = 0.0f; player->transform->y = 0.0f;
    player->transform->scaleX = player->transform->scaleY = 2.0f;
    auto psr = player->addComponent<SpriteRenderer>(SHIPS_SHEET);
    psr->setSourceRect(0, 0, SHIP_CELL, SHIP_CELL);
    auto rb = player->addComponent<RigidBody2D>(); rb->gravityScale = 0.0f;
    auto body = player->addComponent<BoxCollider>(); body->width = body->height = BODY_SIZE;
    auto hurt = player->addComponent<CircleCollider>(); hurt->radius = HURTBOX_RADIUS;
    auto hp = player->addComponent<Health>();
    player->addComponent<PlayerShip>();

    auto fx = scene.createGameObject("Gore")->addComponent<ParticleSystem>(512);
    fx->setBlendMode(BlendMode::Add);
    fx->colR = 230; fx->colG = 30; fx->colB = 30;
    fx->speedMin = 120.0f; fx->speedMax = 420.0f;
    fx->lifeMin = 0.35f; fx->lifeMax = 0.9f;
    fx->sizeStart = 10.0f; fx->sizeEnd = 1.0f; fx->gravity = 500.0f;

    // 2 zonas de nuke marcadas desde el inicio (inactivas hasta Fase 2). Desfasadas.
    std::vector<Nuke*> nukes;
    const float nukePos[2][2] = { { -190.0f, -70.0f }, { 190.0f, 120.0f } };
    for (int i = 0; i < 2; ++i) {
        GameObject* n = scene.createGameObject("Nuke");
        n->transform->x = nukePos[i][0]; n->transform->y = nukePos[i][1];
        auto nk = n->addComponent<Nuke>();
        nk->target = hurt; nk->hp = hp; nk->decals = decals; nk->fx = fx;
        nk->setOffset(i * (NUKE_INTERVAL * 0.5f)); // alternan (una cada ~4 s)
        nukes.push_back(nk);
    }

    // 3 torretas MK-II en las paredes N, E y W. BARREN 180 grados su semiplano (no
    // giran 360): cada una cubre el arco frente a su pared.
    struct TurretDef { float x, y, baseDeg, offset; };
    const TurretDef tdefs[3] = {
        { 0.0f, -HALF_H + 30.0f, 90.0f,  0.0f },                  // N: barre hacia abajo
        { -HALF_W + 30.0f, 0.0f,  0.0f,  0.0f },                  // W: barre hacia la derecha
        {  HALF_W - 30.0f, 0.0f, 180.0f, -TURRET_INT_P1 * 0.5f }, // E: barre hacia la izquierda
    };
    std::vector<Turret*> turrets;
    for (const TurretDef& td : tdefs) {
        GameObject* t = scene.createGameObject("Turret");
        t->transform->x = td.x; t->transform->y = td.y;
        t->transform->scaleX = t->transform->scaleY = 2.0f;
        auto sr = t->addComponent<SpriteRenderer>(SHIPS_SHEET);
        sr->setSourceRect(1 * SHIP_CELL, 4 * SHIP_CELL, SHIP_CELL, SHIP_CELL);
        auto tu = t->addComponent<Turret>();
        tu->setSweep(td.baseDeg, 90.0f, 1.1f); // +-90 grados = arco de 180
        tu->streamInterval = 0.16f;            // flujo continuo + rafaga cada 'interval'
        tu->setOffset(td.offset);
        turrets.push_back(tu);
    }

    // Pool de balas (apunta a la hurtbox del jugador).
    auto pool = scene.createGameObject("BulletManager")->addComponent<BulletPool>(800);
    pool->setColor(255, 120, 40);
    pool->setBounds(-HALF_W, -HALF_H, HALF_W, HALF_H);
    pool->setTarget(hurt);
    for (Turret* t : turrets) t->pool = pool;

    // HUD.
    auto mkText = [&scene](const char* nm, float yTop, float size) {
        GameObject* o = scene.createGameObject(nm);
        o->transform->x = 0.0f; o->transform->y = yTop; // offset desde el centro-arriba
        auto t = o->addComponent<TextRenderer>();
        t->screenSpace = true; t->anchorX = 0.5f; t->centered = true; t->pixelSize = size;
        return t;
    };
    // Rotulo de la sala: centrado en pantalla, aparece con el fundido y se desvanece.
    GameObject* titleObj = scene.createGameObject("Title");
    titleObj->transform->y = -30.0f; // un poco por encima del centro
    auto statusText = titleObj->addComponent<TextRenderer>();
    statusText->screenSpace = true; statusText->anchorX = 0.5f; statusText->anchorY = 0.5f;
    statusText->centered = true; statusText->pixelSize = 7.0f;
    auto timerText = mkText("Timer", 62.0f, 6.0f);
    auto nexusText = mkText("Nexus", 104.0f, 2.0f);
    nexusText->setColor(150, 200, 220);

    auto fade = scene.createGameObject("Fade")->addComponent<ScreenFade>();
    fade->fadeIn(0.6f);

    // Cablear la camara.
    chamber->timer = nullptr; // se asigna abajo tras crear el PhaseTimer
    chamber->statusText = statusText; chamber->timerText = timerText; chamber->nexusText = nexusText;
    chamber->fade = fade; chamber->player = player; chamber->hp = hp; chamber->pool = pool;
    chamber->startX = 0.0f; chamber->startY = 0.0f;

    // Fases: al armar (Fase 2) se activan los nukes y las torretas aceleran.
    chamber->applyPhase = [turrets, nukes](int p) {
        for (Turret* t : turrets) t->interval = (p >= 1) ? TURRET_INT_P2 : TURRET_INT_P1;
        for (Nuke* n : nukes) { if (p >= 1) n->armed = true; else n->disarm(); }
    };

    // Temporizador de supervivencia con la Fase 2 a los 20 s.
    auto timer = logicObj->addComponent<PhaseTimer>();
    timer->duration = SURVIVE_TIME;
    timer->addPhase(PHASE2_AT, [chamber]() { chamber->setPhase(1); });
    timer->onComplete = [chamber]() { chamber->onSurvived(); };
    chamber->timer = timer;

    chamber->setPhase(0);
    chamber->introLine();

    // GORE + MUERTE: estalla, mancha permanente y reinicia el intento.
    hp->onDeath = [player, fx, decals, chamber]() {
        Audio::play("death");
        float dx = player->transform->x, dy = player->transform->y;
        fx->emitBurst(dx, dy, 48);
        for (int i = 0; i < 10; ++i) {
            float ox = frand(-30.0f, 30.0f), oy = frand(-30.0f, 30.0f);
            decals->stampRect(dx + ox, dy + oy, frand(8.0f, 24.0f), frand(8.0f, 24.0f), 110, 8, 8, 200);
        }
        chamber->onPlayerDeath();
    };
}

// ============================================================================
//  Cámara 02 — "La Trinchera" (GDD 6.2)
// ============================================================================
void buildCamara02(Scene& scene) {
    Audio::load("death", "assets/audio/death.wav");
    Audio::load("dash",  "assets/audio/dash.wav");

    auto cam = scene.createGameObject("MainCamera")->addComponent<Camera>();
    cam->fitToWorld(ROOM_W, ROOM_H); // la sala llena la pantalla; lo de fuera queda negro
    scene.createGameObject("Room")->addComponent<RoomRenderer>();

    auto decals = scene.createGameObject("Decals")->addComponent<DecalLayer>();
    decals->setup(-HALF_W, -HALF_H, (int)ROOM_W, (int)ROOM_H);

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

    GameObject* logicObj = scene.createGameObject("Chamber");
    auto chamber = logicObj->addComponent<Chamber>();
    chamber->name = "CAMARA 02 - LA TRINCHERA";
    chamber->introMsg = "SUPRESION CINETICA. ETAPA 2";
    chamber->nextScene = 7; // al superarla -> Camara 03

    // Bloque de escombros SOLIDO en el cuadrante SO (dilema tactico).
    GameObject* debris = scene.createGameObject("Debris");
    debris->transform->x = -230.0f; debris->transform->y = 150.0f;
    auto db = debris->addComponent<DebrisBlock>(); db->w = 130.0f; db->h = 190.0f;
    auto dc = debris->addComponent<BoxCollider>(); dc->width = 130.0f; dc->height = 190.0f;

    // Jugador (arranca lejos de las minas y del bloque).
    GameObject* player = scene.createGameObject("Player");
    player->tag = "player";
    player->transform->x = 0.0f; player->transform->y = 250.0f;
    player->transform->scaleX = player->transform->scaleY = 2.0f;
    auto psr = player->addComponent<SpriteRenderer>(SHIPS_SHEET);
    psr->setSourceRect(0, 0, SHIP_CELL, SHIP_CELL);
    auto rb = player->addComponent<RigidBody2D>(); rb->gravityScale = 0.0f;
    auto body = player->addComponent<BoxCollider>(); body->width = body->height = BODY_SIZE;
    auto hurt = player->addComponent<CircleCollider>(); hurt->radius = HURTBOX_RADIUS;
    auto hp = player->addComponent<Health>();
    player->addComponent<PlayerShip>();

    auto fx = scene.createGameObject("Gore")->addComponent<ParticleSystem>(512);
    fx->setBlendMode(BlendMode::Add);
    fx->colR = 230; fx->colG = 30; fx->colB = 30;
    fx->speedMin = 120.0f; fx->speedMax = 420.0f;
    fx->lifeMin = 0.35f; fx->lifeMax = 0.9f;
    fx->sizeStart = 10.0f; fx->sizeEnd = 1.0f; fx->gravity = 500.0f;

    // Parches de fuego que siembra el lanzallamas.
    auto firePatches = scene.createGameObject("Fire")->addComponent<FirePatches>();
    firePatches->target = hurt; firePatches->hp = hp; firePatches->decals = decals;

    // 3 minas (caen del techo, estallan y dejan quemadura): eje central + cuadrante SE.
    const float minePos[3][2] = { { 0.0f, -40.0f }, { 0.0f, 150.0f }, { 210.0f, 150.0f } };
    for (int i = 0; i < 3; ++i) {
        GameObject* m = scene.createGameObject("Mine");
        m->transform->x = minePos[i][0]; m->transform->y = minePos[i][1];
        auto mn = m->addComponent<DropMine>();
        mn->target = hurt; mn->hp = hp; mn->decals = decals; mn->setOffset(i * 0.9f);
        mn->roam = true; // caen en sitios aleatorios (no siempre el mismo punto)
    }

    // Lanzallamas de techo, central-norte, cono hacia abajo (inactivo hasta Fase 2).
    GameObject* fl = scene.createGameObject("Flame");
    fl->transform->x = 0.0f; fl->transform->y = -HALF_H + 30.0f;
    auto flame = fl->addComponent<Flamethrower>();
    flame->target = hurt; flame->hp = hp; flame->patches = firePatches;
    flame->aimAngle = PI * 0.5f; flame->active = false;

    // 4 torretas MK-II ROTATIVAS en las paredes (techo N/NE + laterales). Flujo
    // continuo + rafaga.
    struct TDef { float x, y, startDeg; };
    // baseDeg = normal hacia dentro de su pared; barren +-90 grados (no giran 360).
    const TDef tdefs[4] = {
        { -150.0f, -HALF_H + 30.0f,  90.0f },  // techo izquierda: barre hacia abajo
        {  200.0f,  HALF_H - 30.0f, -90.0f },  // suelo centro-derecha: barre hacia arriba
        { -HALF_W + 30.0f,  120.0f,   0.0f },  // pared izquierda: barre hacia la derecha
        {  HALF_W - 30.0f, -120.0f, 180.0f },  // pared derecha: barre hacia la izquierda
    };
    std::vector<Turret*> turrets;
    for (const TDef& td : tdefs) {
        GameObject* t = scene.createGameObject("Turret");
        t->transform->x = td.x; t->transform->y = td.y;
        t->transform->scaleX = t->transform->scaleY = 2.0f;
        auto sr = t->addComponent<SpriteRenderer>(SHIPS_SHEET);
        sr->setSourceRect(1 * SHIP_CELL, 4 * SHIP_CELL, SHIP_CELL, SHIP_CELL);
        auto tu = t->addComponent<Turret>();
        tu->setSweep(td.startDeg, 90.0f, 0.6f); // barre su semiplano y rebota (no dispara a la pared)
        tu->interval = 0.0f;         // SOLO flujo (sin rafaga)
        tu->streamInterval = 0.2f;
        turrets.push_back(tu);
    }

    // Un par de torretas mas PEQUENAS hacia el centro de la sala (crossfire). Giran
    // RAPIDO y no dependen de la fase.
    std::vector<Turret*> midTurrets;
    const float midPos[2][2] = { { -110.0f, -30.0f }, { 110.0f, -30.0f } };
    for (auto& mp : midPos) {
        GameObject* t = scene.createGameObject("TurretMid");
        t->transform->x = mp[0]; t->transform->y = mp[1];
        t->transform->scaleX = t->transform->scaleY = 1.3f; // no tan grandes
        auto sr = t->addComponent<SpriteRenderer>(SHIPS_SHEET);
        sr->setSourceRect(1 * SHIP_CELL, 4 * SHIP_CELL, SHIP_CELL, SHIP_CELL);
        auto tu = t->addComponent<Turret>();
        tu->setAimAngle(90.0f); tu->rotSpeed = 120.0f; tu->interval = 0.0f; // solo flujo
        tu->streamInterval = 0.22f;
        midTurrets.push_back(tu);
    }

    auto pool = scene.createGameObject("BulletManager")->addComponent<BulletPool>(900);
    pool->setColor(255, 120, 40);
    pool->setBounds(-HALF_W, -HALF_H, HALF_W, HALF_H);
    pool->setTarget(hurt);
    for (Turret* t : turrets) t->pool = pool;
    for (Turret* t : midTurrets) t->pool = pool;

    auto mkText = [&scene](const char* nm, float yTop, float size) {
        GameObject* o = scene.createGameObject(nm);
        o->transform->x = 0.0f; o->transform->y = yTop; // offset desde el centro-arriba
        auto t = o->addComponent<TextRenderer>();
        t->screenSpace = true; t->anchorX = 0.5f; t->centered = true; t->pixelSize = size;
        return t;
    };
    // Rotulo de la sala: centrado en pantalla, aparece con el fundido y se desvanece.
    GameObject* titleObj = scene.createGameObject("Title");
    titleObj->transform->y = -30.0f; // un poco por encima del centro
    auto statusText = titleObj->addComponent<TextRenderer>();
    statusText->screenSpace = true; statusText->anchorX = 0.5f; statusText->anchorY = 0.5f;
    statusText->centered = true; statusText->pixelSize = 7.0f;
    auto timerText = mkText("Timer", 62.0f, 6.0f);
    auto nexusText = mkText("Nexus", 104.0f, 2.0f);
    nexusText->setColor(150, 200, 220);

    auto fade = scene.createGameObject("Fade")->addComponent<ScreenFade>();
    fade->fadeIn(0.6f);

    chamber->statusText = statusText; chamber->timerText = timerText; chamber->nexusText = nexusText;
    chamber->fade = fade; chamber->player = player; chamber->hp = hp; chamber->pool = pool;
    chamber->startX = 0.0f; chamber->startY = 250.0f;

    // Fases (GDD 6.2): F2 (18 s) activa el lanzallamas; F3 (38 s) dobla la rotacion
    // de torretas y acorta el ciclo del lanzallamas -> el Dash se vuelve obligatorio.
    chamber->applyPhase = [turrets, flame, firePatches](int p) {
        for (Turret* t : turrets) t->sweepSpeed = (p >= 2) ? 1.1f : 0.6f; // barren mas rapido en F3
        flame->active   = (p >= 1);
        flame->interval = (p >= 2) ? 2.5f : 4.0f;
        if (p == 0) { flame->reset(); firePatches->clear(); } // reintento: apagar y limpiar
    };

    auto timer = logicObj->addComponent<PhaseTimer>();
    timer->duration = 55.0f;
    timer->addPhase(18.0f, [chamber]() { chamber->setPhase(1); });
    timer->addPhase(38.0f, [chamber]() { chamber->setPhase(2); });
    timer->onComplete = [chamber]() { chamber->onSurvived(); };
    chamber->timer = timer;

    chamber->setPhase(0);
    chamber->introLine();

    hp->onDeath = [player, fx, decals, chamber]() {
        Audio::play("death");
        float dx = player->transform->x, dy = player->transform->y;
        fx->emitBurst(dx, dy, 48);
        for (int i = 0; i < 10; ++i) {
            float ox = frand(-30.0f, 30.0f), oy = frand(-30.0f, 30.0f);
            decals->stampRect(dx + ox, dy + oy, frand(8.0f, 24.0f), frand(8.0f, 24.0f), 110, 8, 8, 200);
        }
        chamber->onPlayerDeath();
    };
}

// ============================================================================
//  Cámara 03 — "El Suelo de Matanza" (GDD 6.3). Los 4 sistemas a la vez, 4 fases.
// ============================================================================
void buildCamara03(Scene& scene) {
    Audio::load("death", "assets/audio/death.wav");
    Audio::load("dash",  "assets/audio/dash.wav");

    auto cam = scene.createGameObject("MainCamera")->addComponent<Camera>();
    cam->fitToWorld(ROOM_W, ROOM_H); // la sala llena la pantalla; lo de fuera queda negro
    scene.createGameObject("Room")->addComponent<RoomRenderer>();

    auto decals = scene.createGameObject("Decals")->addComponent<DecalLayer>();
    decals->setup(-HALF_W, -HALF_H, (int)ROOM_W, (int)ROOM_H);

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

    GameObject* logicObj = scene.createGameObject("Chamber");
    auto chamber = logicObj->addComponent<Chamber>();
    chamber->name = "CAMARA 03 - EL SUELO DE MATANZA";
    chamber->introMsg = "SUB-NIVEL 0. TODOS LOS SISTEMAS";
    chamber->nextScene = 4; // al superarla -> Jefe HERCULES-1

    // Jugador (sala abierta, sin cobertura). Arranca al centro, lejos de las torretas
    // de techo y suelo que se deslizan.
    GameObject* player = scene.createGameObject("Player");
    player->tag = "player";
    player->transform->x = 0.0f; player->transform->y = 40.0f;
    player->transform->scaleX = player->transform->scaleY = 2.0f;
    auto psr = player->addComponent<SpriteRenderer>(SHIPS_SHEET);
    psr->setSourceRect(0, 0, SHIP_CELL, SHIP_CELL);
    auto rb = player->addComponent<RigidBody2D>(); rb->gravityScale = 0.0f;
    auto body = player->addComponent<BoxCollider>(); body->width = body->height = BODY_SIZE;
    auto hurt = player->addComponent<CircleCollider>(); hurt->radius = HURTBOX_RADIUS;
    auto hp = player->addComponent<Health>();
    player->addComponent<PlayerShip>();

    auto fx = scene.createGameObject("Gore")->addComponent<ParticleSystem>(512);
    fx->setBlendMode(BlendMode::Add);
    fx->colR = 230; fx->colG = 30; fx->colB = 30;
    fx->speedMin = 120.0f; fx->speedMax = 420.0f;
    fx->lifeMin = 0.35f; fx->lifeMax = 0.9f;
    fx->sizeStart = 10.0f; fx->sizeEnd = 1.0f; fx->gravity = 500.0f;

    auto firePatches = scene.createGameObject("Fire")->addComponent<FirePatches>();
    firePatches->target = hurt; firePatches->hp = hp; firePatches->decals = decals;

    // 4 minas (caen del techo, estallan y dejan quemadura), una por cuadrante,
    // desfasadas; se activan en Fase 2.
    const float minePos[4][2] = { { -260, -190 }, { 260, -190 }, { -260, 190 }, { 260, 190 } };
    std::vector<DropMine*> mines;
    for (int i = 0; i < 4; ++i) {
        GameObject* m = scene.createGameObject("Mine");
        m->transform->x = minePos[i][0]; m->transform->y = minePos[i][1];
        auto mn = m->addComponent<DropMine>();
        mn->target = hurt; mn->hp = hp; mn->decals = decals;
        mn->setOffset(i * 0.8f); mn->setActive(false);
        mn->roam = true; // caen en sitios aleatorios
        mines.push_back(mn);
    }

    // 4 nukes (una por cuadrante), rotando; se arman en Fase 2.
    const float nukePos[4][2] = { { -200, -140 }, { 200, -140 }, { -200, 140 }, { 200, 140 } };
    std::vector<Nuke*> nukes;
    for (int i = 0; i < 4; ++i) {
        GameObject* n = scene.createGameObject("Nuke");
        n->transform->x = nukePos[i][0]; n->transform->y = nukePos[i][1];
        auto nk = n->addComponent<Nuke>();
        nk->target = hurt; nk->hp = hp; nk->decals = decals; nk->fx = fx;
        nk->setOffset(i * 2.5f); // interval 10 -> impacto cada ~2.5 s rotando cuadrantes
        nukes.push_back(nk);
    }

    // 4 lanzallamas, uno por PARED, apuntando hacia dentro y barriendo -> olas de
    // fuego cruzadas por toda la sala (en vez de solo desde el techo).
    std::vector<Flamethrower*> flames;
    struct FlDef { float x, y, baseRad; };
    const FlDef flDefs[4] = {
        { 0.0f, -HALF_H + 30.0f,  PI * 0.5f },   // N: hacia abajo
        { 0.0f,  HALF_H - 30.0f, -PI * 0.5f },   // S: hacia arriba
        { -HALF_W + 30.0f, 0.0f,  0.0f },        // W: hacia la derecha
        {  HALF_W - 30.0f, 0.0f,  PI },          // E: hacia la izquierda
    };
    for (int i = 0; i < 4; ++i) {
        GameObject* fl = scene.createGameObject("Flame");
        fl->transform->x = flDefs[i].x; fl->transform->y = flDefs[i].y;
        auto flame = fl->addComponent<Flamethrower>();
        flame->target = hurt; flame->hp = hp; flame->patches = firePatches;
        flame->aimAngle = flDefs[i].baseRad;
        flame->sweepBase = flDefs[i].baseRad; flame->sweepRange = 0.5f;
        flame->range = 240.0f;                 // muy corto: deja un buen pasillo central
        flame->setOffset(i * 0.4f);            // desfases -> olas viajeras
        flames.push_back(flame);
    }

    // 2 torretas MK-IV en rieles de techo (se deslizan + rotan + rafagas de 5).
    std::vector<Turret*> turrets;
    const float railMin = -320.0f, railMax = 320.0f;
    for (int i = 0; i < 2; ++i) {
        bool bottom = (i == 1);            // una en el techo, otra en el suelo
        GameObject* t = scene.createGameObject("Turret");
        t->transform->x = bottom ? railMax : railMin; // arrancan en extremos opuestos
        t->transform->y = bottom ? (HALF_H - 30.0f) : (-HALF_H + 30.0f);
        t->transform->scaleX = t->transform->scaleY = 2.0f;
        auto sr = t->addComponent<SpriteRenderer>(SHIPS_SHEET);
        sr->setSourceRect(1 * SHIP_CELL, 4 * SHIP_CELL, SHIP_CELL, SHIP_CELL);
        auto tu = t->addComponent<Turret>();
        // Barre +-90 grados frente a su pared (techo mira abajo, suelo arriba) y rebota:
        // nunca gira 360 ni dispara hacia su propia pared.
        tu->setSweep(bottom ? -90.0f : 90.0f, 90.0f, 0.8f);
        tu->interval = 0.0f;               // SOLO flujo (sin rafaga)
        tu->streamInterval = 0.18f;
        tu->railMin = railMin; tu->railMax = railMax;
        turrets.push_back(tu);
    }

    // 2 torretas fijas en las paredes laterales que SOLO disparan rafagas de vez en
    // cuando (sin flujo). Barren su semiplano (no giran 360).
    std::vector<Turret*> sideTurrets;
    struct SideDef { float x, y, baseDeg; };
    const SideDef sideDefs[2] = {
        { -HALF_W + 30.0f, -140.0f,   0.0f },  // pared izquierda: barre hacia la derecha
        {  HALF_W - 30.0f,  140.0f, 180.0f },  // pared derecha: barre hacia la izquierda
    };
    for (auto& sd : sideDefs) {
        GameObject* t = scene.createGameObject("TurretSide");
        t->transform->x = sd.x; t->transform->y = sd.y;
        t->transform->scaleX = t->transform->scaleY = 2.0f;
        auto sr = t->addComponent<SpriteRenderer>(SHIPS_SHEET);
        sr->setSourceRect(1 * SHIP_CELL, 4 * SHIP_CELL, SHIP_CELL, SHIP_CELL);
        auto tu = t->addComponent<Turret>();
        tu->setSweep(sd.baseDeg, 90.0f, 0.5f);
        tu->burst = 4; tu->interval = 2.0f;  // SOLO rafaga (sin flujo)
        sideTurrets.push_back(tu);
    }

    auto pool = scene.createGameObject("BulletManager")->addComponent<BulletPool>(1000);
    pool->setColor(255, 120, 40);
    pool->setBounds(-HALF_W, -HALF_H, HALF_W, HALF_H);
    pool->setTarget(hurt);
    for (Turret* t : turrets) t->pool = pool;
    for (Turret* t : sideTurrets) t->pool = pool;

    auto mkText = [&scene](const char* nm, float yTop, float size) {
        GameObject* o = scene.createGameObject(nm);
        o->transform->x = 0.0f; o->transform->y = yTop; // offset desde el centro-arriba
        auto t = o->addComponent<TextRenderer>();
        t->screenSpace = true; t->anchorX = 0.5f; t->centered = true; t->pixelSize = size;
        return t;
    };
    // Rotulo de la sala: centrado en pantalla, aparece con el fundido y se desvanece.
    GameObject* titleObj = scene.createGameObject("Title");
    titleObj->transform->y = -30.0f; // un poco por encima del centro
    auto statusText = titleObj->addComponent<TextRenderer>();
    statusText->screenSpace = true; statusText->anchorX = 0.5f; statusText->anchorY = 0.5f;
    statusText->centered = true; statusText->pixelSize = 7.0f;
    auto timerText = mkText("Timer", 62.0f, 6.0f);
    auto nexusText = mkText("Nexus", 104.0f, 2.0f);
    nexusText->setColor(150, 200, 220);

    auto fade = scene.createGameObject("Fade")->addComponent<ScreenFade>();
    fade->fadeIn(0.6f);

    chamber->statusText = statusText; chamber->timerText = timerText; chamber->nexusText = nexusText;
    chamber->fade = fade; chamber->player = player; chamber->hp = hp; chamber->pool = pool;
    chamber->startX = 0.0f; chamber->startY = 40.0f;

    // Fases (GDD 6.3): F1(0-10) rieles+ola; F2(10-30) nukes+minas; F3(30-50) todo
    // acelera; F4(50-60) SOBRECARGA SIGMA (todo al maximo).
    chamber->applyPhase = [turrets, flames, nukes, mines, firePatches, chamber](int p) {
        float tMult = (p >= 3) ? 3.0f : (p >= 2) ? 2.0f : 1.0f;
        for (Turret* t : turrets) { t->railSpeed = 130.0f * tMult; t->sweepSpeed = 0.8f * tMult; }

        float fInt  = (p >= 3) ? 1.6f : (p >= 2) ? 2.5f : 4.0f;
        float sMult = (p >= 3) ? 1.5f : 1.0f;
        for (Flamethrower* f : flames) { f->active = true; f->interval = fInt; f->sweepSpeed = 1.6f * sMult; }

        float nInt = (p >= 3) ? 5.0f : (p >= 2) ? 7.0f : 10.0f;
        for (Nuke* n : nukes) { n->interval = nInt; if (p >= 1) n->armed = true; else n->disarm(); }
        for (DropMine* m : mines) m->setActive(p >= 1);

        if (p == 0) { firePatches->clear(); for (Flamethrower* f : flames) f->reset(); }
        if (p >= 3) chamber->showNexus("SOBRECARGA SIGMA");
    };

    auto timer = logicObj->addComponent<PhaseTimer>();
    timer->duration = 60.0f;
    timer->addPhase(10.0f, [chamber]() { chamber->setPhase(1); }); // F2: nukes + minas
    timer->addPhase(30.0f, [chamber]() { chamber->setPhase(2); }); // F3: todo acelera
    timer->addPhase(50.0f, [chamber]() { chamber->setPhase(3); }); // F4: SIGMA
    timer->onComplete = [chamber]() { chamber->onSurvived(); };
    chamber->timer = timer;

    chamber->setPhase(0);
    chamber->introLine();

    hp->onDeath = [player, fx, decals, chamber]() {
        Audio::play("death");
        float dx = player->transform->x, dy = player->transform->y;
        fx->emitBurst(dx, dy, 48);
        for (int i = 0; i < 10; ++i) {
            float ox = frand(-30.0f, 30.0f), oy = frand(-30.0f, 30.0f);
            decals->stampRect(dx + ox, dy + oy, frand(8.0f, 24.0f), frand(8.0f, 24.0f), 110, 8, 8, 200);
        }
        chamber->onPlayerDeath();
    };
}
