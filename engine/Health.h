#pragma once
#include <functional>
#include "Component.h"

// Vida y muerte del objeto. En "REDACTED" cualquier contacto con un peligro es muerte instantanea (no hay barra de vida).
// la invulnerabilidad es temporal (Dash) o permanente (godMode)

class Health : public Component {
public:
    bool alive = true;

    // MODO DIOS (test de progresion): invulnerabilidad permanente para TODOS los Health
    inline static bool godMode = false;

	// Callback de muerte: lo define el juego (reventar en particulas, reiniciar la sala, etc). Se llama UNA vez al morir (si no esta invulnerable)
    std::function<void()> onDeath;

    // Arranca (o extiende) la ventana de invulnerabilidad. La llama el Dash
    void setInvulnerable(float seconds) {
        if (seconds > invulnTimer) invulnTimer = seconds;
    }
    bool isInvulnerable() const { return godMode || invulnTimer > 0.0f; }

    // Muerte instantanea. No hace nada si esta invulnerable o ya muerto (idempotente)
    void kill() {
        if (!alive || isInvulnerable()) return;
        alive = false;
        if (onDeath) onDeath();
    }

    // Devuelve el objeto a la vida (nuevo clon en la sala): reinicia estado
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
