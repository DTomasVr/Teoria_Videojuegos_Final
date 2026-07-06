#pragma once
#include <functional>
#include "Component.h"

// Vida y muerte del objeto. En "REDACTED" cualquier contacto con un peligro es
// LETAL (GDD 3): no hay puntos de vida, la muerte es instantanea. Este componente
// modela justo eso, mas la ventana de invulnerabilidad del Dash ("Adrenaline
// Rush", ~0.1 s de i-frames, GDD 4.1).
//
// Quien detecta el impacto (p.ej. el BulletPool) llama a kill(): si el objeto esta
// invulnerable, no pasa nada; si no, muere UNA sola vez y dispara onDeath (el juego
// engancha ahi el gore, el respawn del clon, etc.). Es logica pura: no toca SDL.

class Health : public Component {
public:
    bool alive = true;

    // MODO DIOS (test de progresion): invulnerabilidad permanente para TODOS los Health.
    // Es global (static) y sobrevive a los cambios de escena, asi se puede recorrer el
    // flujo sin morir. Se alterna con una tecla en main.cpp.
    inline static bool godMode = false;

    // Callback de muerte: lo define el juego (reventar en particulas, reiniciar la
    // sala y reaparecer al siguiente clon). Se dispara exactamente una vez por muerte.
    std::function<void()> onDeath;

    // Arranca (o extiende) la ventana de invulnerabilidad. La llama el Dash.
    void setInvulnerable(float seconds) {
        if (seconds > invulnTimer) invulnTimer = seconds;
    }
    bool isInvulnerable() const { return godMode || invulnTimer > 0.0f; }

    // Muerte instantanea. No hace nada si esta invulnerable o ya muerto (idempotente).
    void kill() {
        if (!alive || isInvulnerable()) return;
        alive = false;
        if (onDeath) onDeath();
    }

    // Devuelve el objeto a la vida (nuevo clon en la sala): reinicia estado.
    void reset() {
        alive = true;
        invulnTimer = 0.0f;
    }

    void update(float dt) override {
        if (invulnTimer > 0.0f) {
            invulnTimer -= dt;
            if (invulnTimer < 0.0f) invulnTimer = 0.0f;
        }
    }

private:
    float invulnTimer = 0.0f; // segundos restantes de invulnerabilidad
};
