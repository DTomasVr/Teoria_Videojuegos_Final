#include "BulletHell.h"

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

// ============================================================================
//  "REDACTED" - Jefe HERCULES-1 (GDD 7). Escena de prueba (tecla 4).
//  PARAMETROS ajustables:
// ============================================================================
namespace {
    constexpr float ROOM_W = 900.0f, ROOM_H = 700.0f, WALL = 40.0f;
    constexpr float HALF_W = ROOM_W * 0.5f, HALF_H = ROOM_H * 0.5f;

    // Jugador.
    constexpr float PLAYER_SPEED   = 260.0f;
    constexpr float HURTBOX_RADIUS = 8.0f;
    constexpr float BODY_SIZE      = 44.0f;
    constexpr float DASH_SPEED    = 900.0f, DASH_DURATION = 0.12f;
    constexpr float DASH_IFRAMES  = 0.12f,  DASH_COOLDOWN = 0.5f;

    // Balas.
    constexpr float BOSS_BULLET_SPEED = 200.0f;
    constexpr float BULLET_RADIUS = 8.0f, BULLET_LIFE = 6.0f;

    // Brazos: 4, anclados al chasis, orbitan a ARM_OFFSET (GDD 7.1).
    constexpr int   ARM_COUNT  = 4;
    constexpr float ARM_OFFSET = 110.0f;
    constexpr float ARM_TIP    = 50.0f; // punta del brazo (desde donde emite)

    // Rotacion segun calor (GDD 7.4): 18 -> 28 -> 40 deg/s; inversion a alto calor.
    constexpr float SPIN_LOW = 18.0f, SPIN_MID = 28.0f, SPIN_HIGH = 40.0f;
    constexpr float SPIN_P2  = 12.0f; // Fase 2: mas lento, mas cobertura
    constexpr float HEAT_MID = 25.0f, HEAT_HIGH = 60.0f;
    constexpr float REVERSE_PERIOD = 15.0f, REVERSE_TIME = 3.0f;

    // Armas Fase 1.
    constexpr float MINIGUN_INTERVAL = 0.09f;                 // brazos 1+3: flujo continuo
    constexpr float HMG_INTERVAL = 0.85f; constexpr int HMG_BURST = 4; // brazos 2+4: rafagas
    constexpr float HMG_SPREAD = 0.22f;

    // Armas Fase 2.
    constexpr float FLAME_DROP_INTERVAL = 0.22f; // brazos 1+3: parches de fuego
    constexpr float MINE_DROP_INTERVAL  = 6.0f;  // brazos 2+4: una mina/6s (GDD 7.5)

    // Parches de fuego / minas.
    constexpr float FLAME_RADIUS = 55.0f, FLAME_LIFE = 4.0f;
    constexpr float MINE_RADIUS = 70.0f, MINE_PULSE = 3.0f, MINE_WINDOW = 0.35f;

    // Placas de presion (GDD 7.4).
    constexpr float PLATE_HEAT = 8.0f;       // +8% por activacion
    constexpr float PLATE_COOLDOWN = 1.5f;   // ritmo de reactivacion mientras la pisas
    constexpr float ARM_PAUSE = 2.5f;        // pausa del brazo mas cercano

    // Fases.
    constexpr float PHASE1_TIME   = 75.0f;   // limite de Fase 1 (tambien HUD)
    constexpr float PHASE1_TARGET = 72.0f;   // calor que cierra la Fase 1
    constexpr float TRANSITION_TIME = 3.0f;  // silencio entre fases (GDD 7.5)
    constexpr float PASSIVE_HEAT  = 2.0f;    // calor/seg pasivo en Fase 2 -> 100 = victoria
    constexpr float WIN_TIME = 1.3f;         // duracion de la onda de choque de victoria

    constexpr float PI = 3.14159265f;
    const char* SHIPS_SHEET = "assets/kenney_pixelshmup/Tilemap/ships_packed.png";
    constexpr int SHIP_CELL = 32;

    float frand(float a, float b) { return a + (b - a) * ((float)std::rand() / (float)RAND_MAX); }
}

// ----------------------------------------------------------------------------
//  Suelo y contorno de la arena.
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
        SDL_SetRenderDrawColor(r, 26, 26, 30, 255); SDL_RenderFillRect(r, &floor);
        SDL_SetRenderDrawColor(r, 120, 120, 130, 255); SDL_RenderRect(r, &floor);
    }
};

// ----------------------------------------------------------------------------
//  Rotacion continua del chasis (BossRoom fija degPerSec, con signo para invertir).
// ----------------------------------------------------------------------------
class Spin : public Component {
public:
    float degPerSec = 0.0f;
    void update(float dt) override { gameObject->transform->rotation += degPerSec * dt; }
};

// ----------------------------------------------------------------------------
//  Jugador: movimiento 8-dir + Dash con i-frames + tinte de invulnerabilidad.
// ----------------------------------------------------------------------------
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

        if (dashTimer > 0.0f) {
            rb->velocityX = dashDirX * DASH_SPEED; rb->velocityY = dashDirY * DASH_SPEED;
            dashTimer -= dt;
        } else { rb->velocityX = ix * PLAYER_SPEED; rb->velocityY = iy * PLAYER_SPEED; }
        if (cooldownTimer > 0.0f) cooldownTimer -= dt;

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
};

// ----------------------------------------------------------------------------
//  Campo de parches de fuego (Fase 2, lanzallamas de brazos 1+3, GDD 7.5). Los
//  parches se ANADEN via spawnAt (los brazos los sueltan segun rotan), persisten,
//  son letales por contacto y dejan quemaduras acumuladas en el suelo (decals).
// ----------------------------------------------------------------------------
class FlamePatchField : public Component {
public:
    CircleCollider* target = nullptr;
    Health*         hp = nullptr;
    DecalLayer*     decals = nullptr;

    void enable() { active = true; }
    void reset()  { active = false; for (Patch& p : patches) p.active = false; }
    void spawnAt(float x, float y) {
        if (!active) return;
        for (Patch& p : patches)
            if (!p.active) { p.x = x; p.y = y; p.life = FLAME_LIFE; p.active = true; return; }
    }

    void update(float dt) override {
        if (!active) return;
        for (Patch& p : patches) {
            if (!p.active) continue;
            p.life -= dt;
            if (p.life <= 0.0f) { p.active = false; scorch(p.x, p.y); }
        }
        if (target && hp) {
            float cx = target->centerX(), cy = target->centerY();
            for (Patch& p : patches)
                if (p.active && Collision::pointInCircle(cx, cy, p.x, p.y, FLAME_RADIUS)) { hp->kill(); break; }
        }
    }
    void render() override {
        Scene& s = *gameObject->scene;
        for (Patch& p : patches) {
            if (!p.active) continue;
            float frac = p.life / FLAME_LIFE; if (frac < 0.0f) frac = 0.0f;
            int a = (int)(60 + 120 * frac);
            Shapes::fillCircle(s, p.x, p.y, FLAME_RADIUS, 255, 110, 20, (int)(a * 0.45f));
            Shapes::fillCircle(s, p.x, p.y, FLAME_RADIUS * 0.6f, 255, 200, 60, a);
        }
    }
private:
    struct Patch { float x = 0, y = 0, life = 0; bool active = false; };
    std::vector<Patch> patches = std::vector<Patch>(48);
    bool active = false;
    void scorch(float x, float y) {
        if (!decals) return;
        for (int i = 0; i < 6; ++i) {
            float ox = frand(-FLAME_RADIUS * 0.6f, FLAME_RADIUS * 0.6f);
            float oy = frand(-FLAME_RADIUS * 0.6f, FLAME_RADIUS * 0.6f);
            decals->stampRect(x + ox, y + oy, frand(10.0f, 22.0f), frand(10.0f, 22.0f), 22, 14, 10, 180);
        }
    }
};

// ----------------------------------------------------------------------------
//  Campo de minas (Fase 2, lanzaminas de brazos 2+4, GDD 7.5). Se ANADEN via
//  spawnAt, se anclan al suelo y pulsan un anillo letal cada MINE_PULSE seg
//  (memorizacion). Persisten hasta el fin del combate.
// ----------------------------------------------------------------------------
class MineField : public Component {
public:
    CircleCollider* target = nullptr;
    Health*         hp = nullptr;

    void enable() { active = true; }
    void reset()  { active = false; for (M& m : mines) m.active = false; }
    void spawnAt(float x, float y) {
        if (!active) return;
        for (M& m : mines)
            if (!m.active) { m.x = x; m.y = y; m.timer = 0.0f; m.pulseT = 0.0f; m.active = true; return; }
    }

    void update(float dt) override {
        if (!active) return;
        float cx = target ? target->centerX() : 0.0f, cy = target ? target->centerY() : 0.0f;
        for (M& m : mines) {
            if (!m.active) continue;
            m.timer += dt;
            if (m.timer >= MINE_PULSE) {
                m.timer -= MINE_PULSE; m.pulseT = MINE_WINDOW;
                if (target && hp && Collision::pointInCircle(cx, cy, m.x, m.y, MINE_RADIUS)) { hp->kill(); break; }
            }
            if (m.pulseT > 0.0f) m.pulseT -= dt;
        }
    }
    void render() override {
        Scene& s = *gameObject->scene;
        for (M& m : mines) {
            if (!m.active) continue;
            Shapes::fillCircle(s, m.x, m.y, 8.0f, 255, 180, 40, 255);
            if (m.pulseT > 0.0f) { int a = (int)(255 * (m.pulseT / MINE_WINDOW));
                Shapes::ring(s, m.x, m.y, MINE_RADIUS, 3.0f, 255, 60, 40, a); }
            else Shapes::outlineCircle(s, m.x, m.y, MINE_RADIUS, 120, 60, 40, 110);
        }
    }
private:
    struct M { float x = 0, y = 0, timer = 0, pulseT = 0; bool active = false; };
    std::vector<M> mines = std::vector<M>(32);
    bool active = false;
};

// ----------------------------------------------------------------------------
//  Brazo del jefe. Cuatro armas segun la fase y el par (GDD 7.4/7.5):
//    Par A (brazos 1+3): Minigun (Fase 1) / Lanzallamas (Fase 2)
//    Par B (brazos 2+4): Ametralladora pesada (Fase 1) / Lanzaminas (Fase 2)
//  Dispara RADIALMENTE hacia afuera del centro: como el brazo rota, el flujo
//  dibuja una espiral. Una placa puede PAUSARLO (dispersion termica).
// ----------------------------------------------------------------------------
enum class Weapon { Minigun, HMG, Flamethrower, MineLauncher };

class BossArm : public Component {
public:
    BulletPool*      pool = nullptr;
    Transform*       coreT = nullptr;
    FlamePatchField* flames = nullptr;
    MineField*       mines = nullptr;
    bool pairA = true;

    Weapon weapon = Weapon::Minigun; // lo fija BossRoom cada frame
    bool   active = false;

    void pause(float t) { if (t > pauseTimer) pauseTimer = t; }

    void update(float dt) override {
        if (weapon != lastWeapon) { t1 = t2 = 0.0f; lastWeapon = weapon; } // sin rafaga al cambiar
        if (pauseTimer > 0.0f) { pauseTimer -= dt; return; }               // pausa por placa
        if (!active) return;

        float ox = gameObject->transform->x, oy = gameObject->transform->y;
        float dx = ox - (coreT ? coreT->x : 0.0f);
        float dy = oy - (coreT ? coreT->y : 0.0f);
        float l = std::sqrt(dx * dx + dy * dy);
        if (l > 0.0f) { dx /= l; dy /= l; } else { dx = 0.0f; dy = 1.0f; }
        float tipX = ox + dx * ARM_TIP, tipY = oy + dy * ARM_TIP;

        t1 += dt; t2 += dt;
        switch (weapon) {
            case Weapon::Minigun:
                while (t1 >= MINIGUN_INTERVAL) { t1 -= MINIGUN_INTERVAL;
                    if (pool) pool->spawn(tipX, tipY, dx * BOSS_BULLET_SPEED, dy * BOSS_BULLET_SPEED, BULLET_RADIUS, BULLET_LIFE); }
                break;
            case Weapon::HMG:
                while (t1 >= HMG_INTERVAL) { t1 -= HMG_INTERVAL; fireBurst(tipX, tipY, dx, dy); }
                break;
            case Weapon::Flamethrower:
                while (t1 >= FLAME_DROP_INTERVAL) { t1 -= FLAME_DROP_INTERVAL;
                    if (flames) flames->spawnAt(tipX + dx * 30.0f, tipY + dy * 30.0f); }
                break;
            case Weapon::MineLauncher:
                while (t2 >= MINE_DROP_INTERVAL) { t2 -= MINE_DROP_INTERVAL;
                    if (mines) mines->spawnAt(tipX, tipY); }
                break;
        }
    }
private:
    float t1 = 0.0f, t2 = 0.0f, pauseTimer = 0.0f;
    Weapon lastWeapon = Weapon::Minigun;
    void fireBurst(float x, float y, float dx, float dy) {
        if (!pool) return;
        float base = std::atan2(dy, dx);
        for (int i = 0; i < HMG_BURST; ++i) {
            float a = base + (i - (HMG_BURST - 1) * 0.5f) * HMG_SPREAD;
            pool->spawn(x, y, std::cos(a) * BOSS_BULLET_SPEED, std::sin(a) * BOSS_BULLET_SPEED, BULLET_RADIUS, BULLET_LIFE);
        }
    }
};

// ----------------------------------------------------------------------------
//  Controlador del jefe HERCULES-1 de DOS FASES + HUD (GDD 7).
//   Fase 1 (SUPRESION): brazos minigun/HMG; rotacion segun calor (18->28->40, con
//     inversiones a alto calor). Pisa las placas para subir el CALOR; cada placa
//     +8% y PAUSA el brazo mas cercano. A PHASE1_TARGET (o agotar el tiempo) -> transicion.
//   TRANSICION: ~3 s de silencio; se retraen los brazos y se bloquean las placas.
//   Fase 2 (ERRADICACION): brazos lanzallamas + lanzaminas llenan el suelo; el calor
//     sube solo hasta 100% = victoria (onda de choque). Solo sobrevivir.
//   MUERTE: se pierde TODO el progreso (calor Y temporizador a cero); respawn instantaneo.
// ----------------------------------------------------------------------------
enum class BossPhase { Supresion, Transicion, Erradicacion, Ganado };

class BossRoom : public Component {
public:
    PhaseTimer*   timer = nullptr;
    TextRenderer *statusText = nullptr, *timerText = nullptr, *heatText = nullptr;
    ScreenFade*   fade = nullptr;
    GameObject*   player = nullptr;
    Health*       hp = nullptr;
    BulletPool*   pool = nullptr;
    DecalLayer*   decals = nullptr;
    Spin*         coreSpin = nullptr;
    Transform*    coreT = nullptr;
    FlamePatchField* flames = nullptr;
    MineField*    mines = nullptr;
    ParticleSystem* steam = nullptr;
    std::vector<BossArm*> arms;
    float startX = 0.0f, startY = 0.0f;

    float heat = 0.0f;
    BossPhase phase = BossPhase::Supresion;

    // Consultas para otros componentes (placas, onda de choque).
    bool  platesLocked() const { return phase != BossPhase::Supresion; }
    bool  winning()      const { return phase == BossPhase::Ganado; }
    float shockRadius()  const { return shockR; }

    // Activacion discreta de una placa: +8% y pausa el brazo mas cercano (GDD 7.4).
    void onPlateActivated(float px, float py) {
        if (phase != BossPhase::Supresion) return;
        heat += PLATE_HEAT;
        pauseNearestArm(px, py);
        if (heat >= PHASE1_TARGET) { heat = PHASE1_TARGET; enterTransition(); }
    }
    void onPhase1TimerEnd() { enterTransition(); }
    void onPlayerDeath()    { resetProgress(false); }

    void update(float dt) override {
        updateRotation(dt);
        pushArmState();
        switch (phase) {
            case BossPhase::Supresion: emitSteam(dt); break;
            case BossPhase::Transicion:
                transTimer -= dt; if (transTimer <= 0.0f) enterPhase2(); break;
            case BossPhase::Erradicacion:
                heat += PASSIVE_HEAT * dt; emitSteam(dt);
                if (heat >= 100.0f) { heat = 100.0f; win(); } break;
            case BossPhase::Ganado:
                winTimer -= dt; shockR += 950.0f * dt;
                if (winTimer <= 0.0f) resetProgress(true); break;
        }
        updateHUD();
    }

private:
    float transTimer = 0.0f, winTimer = 0.0f, shockR = 0.0f;
    float reverseClock = 0.0f, steamAcc = 0.0f;
    bool  reversing = false;
    int   spinDir = 1;

    void updateRotation(float dt) {
        float speed;
        if (phase == BossPhase::Transicion || phase == BossPhase::Ganado) speed = 0.0f;
        else if (phase == BossPhase::Erradicacion) { speed = SPIN_P2; spinDir = 1; reversing = false; }
        else { // SUPRESION: velocidad por tramo de calor + inversion a alto calor
            speed = (heat < HEAT_MID) ? SPIN_LOW : (heat < HEAT_HIGH) ? SPIN_MID : SPIN_HIGH;
            if (heat >= HEAT_HIGH) {
                reverseClock += dt;
                if (!reversing && reverseClock >= REVERSE_PERIOD) { reversing = true;  reverseClock = 0.0f; }
                else if (reversing && reverseClock >= REVERSE_TIME) { reversing = false; reverseClock = 0.0f; }
            } else { reversing = false; reverseClock = 0.0f; }
            spinDir = reversing ? -1 : 1;
        }
        if (coreSpin) coreSpin->degPerSec = speed * spinDir;
    }

    void pushArmState() {
        for (BossArm* a : arms) {
            if (phase == BossPhase::Supresion)      { a->active = true;  a->weapon = a->pairA ? Weapon::Minigun     : Weapon::HMG; }
            else if (phase == BossPhase::Erradicacion){ a->active = true; a->weapon = a->pairA ? Weapon::Flamethrower : Weapon::MineLauncher; }
            else                                    { a->active = false; } // transicion/ganado: brazos retraidos
        }
    }

    void pauseNearestArm(float px, float py) {
        BossArm* best = nullptr; float bestD = 1e18f;
        for (BossArm* a : arms) {
            float dx = a->gameObject->transform->x - px, dy = a->gameObject->transform->y - py;
            float d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; best = a; }
        }
        if (best) best->pause(ARM_PAUSE);
    }

    void emitSteam(float dt) {
        if (!steam || !coreT) return;
        steamAcc += (heat / 100.0f) * 22.0f * dt; // mas vapor cuanto mas calor (GDD 8.1)
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
        if (statusText) statusText->setText("ERRADICACION");
        if (pool) pool->clear();
        if (timer) timer->running = false;
    }
    void enterPhase2() {
        phase = BossPhase::Erradicacion;
        if (flames) flames->enable();
        if (mines)  mines->enable();
    }
    void win() {
        phase = BossPhase::Ganado; winTimer = WIN_TIME; shockR = 0.0f;
        if (hp) hp->setInvulnerable(20.0f);
        if (statusText) statusText->setText("SOBRECALENTADO");
        if (fade) fade->fadeOut(0.9f);
    }

    void resetProgress(bool withFade) {
        heat = 0.0f; phase = BossPhase::Supresion;
        transTimer = 0.0f; winTimer = 0.0f; shockR = 0.0f;
        reverseClock = 0.0f; reversing = false; spinDir = 1;
        if (timer) timer->reset();
        if (flames) flames->reset();
        if (mines)  mines->reset();
        if (pool) pool->clear();
        if (player) {
            player->transform->x = startX; player->transform->y = startY;
            if (auto rb = player->getComponent<RigidBody2D>()) { rb->velocityX = 0.0f; rb->velocityY = 0.0f; }
        }
        if (hp) { hp->reset(); hp->setInvulnerable(0.4f); } // breve gracia al reaparecer
        if (statusText) statusText->setText("SUPRESION");
        if (withFade && fade) fade->fadeIn(0.6f);
    }

    void updateHUD() {
        if (timerText && timer)
            timerText->setText(std::string("TIEMPO ") + std::to_string((int)std::ceil(timer->remaining())));
        if (heatText)
            heatText->setText(std::string("CALOR ") + std::to_string((int)heat) + "%");
    }
};

// ----------------------------------------------------------------------------
//  Onda de choque de victoria (GDD 7.7): anillo no letal que se expande. Objeto
//  aparte, creado al final, para dibujarse POR ENCIMA del combate.
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
//  Placa de presion: trigger que se ACTIVA en pulsos (+8%) mientras la pisas, con
//  un cooldown. En Fase 2 queda BLOQUEADA (GDD 7.5). Filtra por TAG.
// ----------------------------------------------------------------------------
class PressurePlate : public Component {
public:
    BossRoom* state = nullptr;
    float w = 80.0f, h = 80.0f;

    void onCollision(GameObject* other) override { if (other->hasTag("player")) contact = true; }
    void update(float dt) override {
        bool locked = state ? state->platesLocked() : false;
        if (cooldown > 0.0f) cooldown -= dt;
        if (contact && !locked && cooldown <= 0.0f && state) {
            state->onPlateActivated(gameObject->transform->x, gameObject->transform->y);
            cooldown = PLATE_COOLDOWN; flash = 1.0f;
        }
        active = contact && !locked;
        if (flash > 0.0f) flash -= dt * 3.0f;
        contact = false;
    }
    void render() override {
        Scene& s = *gameObject->scene;
        float x = gameObject->transform->x, y = gameObject->transform->y;
        bool locked = state ? state->platesLocked() : false;
        if (locked) { // abrazadera hidraulica: apagada con un aspa
            Shapes::fillRect(s, x, y, w, h, 55, 55, 62, 150);
            Shapes::outlineRect(s, x, y, w, h, 120, 120, 130, 200);
            Shapes::line(s, x - w * 0.4f, y - h * 0.4f, x + w * 0.4f, y + h * 0.4f, 120, 120, 130, 200);
            Shapes::line(s, x - w * 0.4f, y + h * 0.4f, x + w * 0.4f, y - h * 0.4f, 120, 120, 130, 200);
        } else {
            int base = active ? 235 : 110;
            base += (int)(20 * flash);
            Shapes::fillRect(s, x, y, w, h, base, base / 2, 12, 150);
            Shapes::outlineRect(s, x, y, w, h, 255, 200, 60, 220);
        }
    }
private:
    bool contact = false, active = false; float cooldown = 0.0f, flash = 0.0f;
};

// ============================================================================
//  Construccion de la escena
// ============================================================================
void buildBulletHell(Scene& scene) {
    Audio::load("death", "assets/audio/death.wav");
    Audio::load("dash",  "assets/audio/dash.wav");

    scene.createGameObject("MainCamera")->addComponent<Camera>();
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

    // Estado del jefe (temprano: placas y peligros lo referencian).
    GameObject* logicObj = scene.createGameObject("RoomLogic");
    auto state = logicObj->addComponent<BossRoom>();

    // 4 placas en las 4 esquinas (GDD 7.3), dispositivos de suelo: bajo el jugador.
    const float pX = HALF_W - 90.0f, pY = HALF_H - 90.0f;
    const float platePos[4][2] = { { -pX, -pY }, { pX, -pY }, { -pX, pY }, { pX, pY } };
    for (auto& pp : platePos) {
        GameObject* plate = scene.createGameObject("Plate");
        plate->tag = "plate";
        plate->transform->x = pp[0]; plate->transform->y = pp[1];
        auto bc = plate->addComponent<BoxCollider>();
        bc->width = 80.0f; bc->height = 80.0f; bc->isTrigger = true;
        plate->addComponent<PressurePlate>()->state = state;
    }

    // Jugador.
    GameObject* player = scene.createGameObject("Player");
    player->tag = "player";
    player->transform->x = 0.0f; player->transform->y = HALF_H - 160.0f;
    player->transform->scaleX = player->transform->scaleY = 2.0f;
    auto psr = player->addComponent<SpriteRenderer>(SHIPS_SHEET);
    psr->setSourceRect(0, 0, SHIP_CELL, SHIP_CELL);
    auto rb = player->addComponent<RigidBody2D>(); rb->gravityScale = 0.0f;
    auto body = player->addComponent<BoxCollider>(); body->width = body->height = BODY_SIZE;
    auto hurt = player->addComponent<CircleCollider>(); hurt->radius = HURTBOX_RADIUS;
    auto hp = player->addComponent<Health>();
    player->addComponent<BulletHellController>();

    // Gore (rojo) y vapor del jefe (gris), sistemas de particulas.
    auto fx = scene.createGameObject("Gore")->addComponent<ParticleSystem>(512);
    fx->setBlendMode(BlendMode::Add);
    fx->colR = 230; fx->colG = 30; fx->colB = 30;
    fx->speedMin = 120.0f; fx->speedMax = 420.0f;
    fx->lifeMin = 0.35f; fx->lifeMax = 0.9f;
    fx->sizeStart = 10.0f; fx->sizeEnd = 1.0f; fx->gravity = 500.0f;

    auto steam = scene.createGameObject("Steam")->addComponent<ParticleSystem>(256);
    steam->setBlendMode(BlendMode::Alpha);

    // Peligros de Fase 2 (inactivos hasta la Erradicacion).
    auto flames = scene.createGameObject("Flames")->addComponent<FlamePatchField>();
    flames->target = hurt; flames->hp = hp; flames->decals = decals;
    auto mines = scene.createGameObject("Mines")->addComponent<MineField>();
    mines->target = hurt; mines->hp = hp;

    // Chasis central rotatorio (PARENTING: los brazos son hijos).
    GameObject* core = scene.createGameObject("Core");
    core->tag = "boss";
    auto coreSpin = core->addComponent<Spin>();
    core->transform->scaleX = core->transform->scaleY = 3.5f;
    auto csr = core->addComponent<SpriteRenderer>(SHIPS_SHEET);
    csr->setSourceRect(1 * SHIP_CELL, 3 * SHIP_CELL, SHIP_CELL, SHIP_CELL);

    std::vector<BossArm*> arms;
    for (int i = 0; i < ARM_COUNT; ++i) {
        GameObject* arm = scene.createGameObject("Arm");
        arm->transform->scaleX = arm->transform->scaleY = 2.0f;
        auto asr = arm->addComponent<SpriteRenderer>(SHIPS_SHEET);
        // Par A (minigun/fuego) y par B (HMG/minas) con celdas distintas.
        int cell = (i % 2 == 0) ? 2 : 0;
        asr->setSourceRect(cell * SHIP_CELL, 3 * SHIP_CELL, SHIP_CELL, SHIP_CELL);
        auto ch = arm->addComponent<ChildOf>();
        ch->setParent(core);
        float ang = (float)i / ARM_COUNT * 2.0f * PI;
        ch->localX = std::cos(ang) * ARM_OFFSET;
        ch->localY = std::sin(ang) * ARM_OFFSET;
        auto a = arm->addComponent<BossArm>();
        a->coreT = core->transform; a->flames = flames; a->mines = mines;
        a->pairA = (i % 2 == 0);
        arms.push_back(a);
    }

    // Pool de balas.
    auto pool = scene.createGameObject("BulletManager")->addComponent<BulletPool>(1200);
    pool->setColor(255, 120, 40);
    pool->setBounds(-HALF_W, -HALF_H, HALF_W, HALF_H);
    pool->setTarget(hurt);
    for (BossArm* a : arms) a->pool = pool;

    // Onda de choque (encima del combate).
    scene.createGameObject("Shock")->addComponent<ShockwaveView>()->boss = state;

    // Temporizador de Fase 1: cuenta atras que, si se agota, fuerza la transicion.
    auto timer = logicObj->addComponent<PhaseTimer>();
    timer->duration = PHASE1_TIME;
    timer->onComplete = [state]() { state->onPhase1TimerEnd(); };

    // HUD de texto (fuente 5x7 embebida).
    auto mkText = [&scene](const char* name, float x, float y, float size, bool center) {
        GameObject* o = scene.createGameObject(name);
        o->transform->x = x; o->transform->y = y;
        auto t = o->addComponent<TextRenderer>();
        t->screenSpace = true; t->pixelSize = size; t->centered = center;
        return t;
    };
    auto statusText = mkText("Status", 640.0f, 24.0f, 4.0f, true);
    statusText->setText("SUPRESION");
    auto timerText = mkText("Timer", 640.0f, 64.0f, 6.0f, true);
    auto heatText  = mkText("Heat", 24.0f, 24.0f, 3.0f, false);
    heatText->setColor(255, 140, 40);

    // Fundido de entrada (el ULTIMO).
    auto fade = scene.createGameObject("Fade")->addComponent<ScreenFade>();
    fade->fadeIn(0.6f);

    // Cablear el jefe.
    state->timer = timer; state->statusText = statusText; state->timerText = timerText;
    state->heatText = heatText; state->fade = fade; state->player = player; state->hp = hp;
    state->pool = pool; state->decals = decals; state->coreSpin = coreSpin; state->coreT = core->transform;
    state->flames = flames; state->mines = mines; state->steam = steam; state->arms = arms;
    state->startX = player->transform->x; state->startY = player->transform->y;

    // GORE + MUERTE: estalla, mancha permanente y pierde TODO el progreso.
    hp->onDeath = [player, fx, decals, state]() {
        float dx = player->transform->x, dy = player->transform->y;
        fx->emitBurst(dx, dy, 48);
        Audio::play("death");
        for (int i = 0; i < 10; ++i) {
            float ox = frand(-30.0f, 30.0f), oy = frand(-30.0f, 30.0f);
            float s = frand(8.0f, 24.0f);
            decals->stampRect(dx + ox, dy + oy, s, s, 110, 8, 8, 200);
        }
        state->onPlayerDeath();
    };
}
