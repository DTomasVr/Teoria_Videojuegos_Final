#pragma once
#include <vector>
#include <functional>
#include "Component.h"

// Temporizador de sala con FASES. Modela la estructura de una camara de "REDACTED"
// (GDD 5.2): se sobrevive durante 'duration' segundos y cada cierto tiempo el
// patron de peligros escala de fase (mas densidad/velocidad). Es logica pura: no
// toca SDL.
//
//   auto pt = obj->addComponent<PhaseTimer>();
//   pt->duration = 40.0f;
//   pt->addPhase(20.0f, []{ subirDificultad(); }); // a los 20 s
//   pt->onComplete = []{ ganar(); };               // al sobrevivir
//   ... y para el HUD: pt->remaining()
// Al reaparecer el clon NO se toca; para reiniciar la sala entera: pt->reset().

class PhaseTimer : public Component {
public:
    float duration = 40.0f;   // segundos totales a sobrevivir
    bool  running  = true;

    std::function<void()> onComplete; // se dispara una vez al llegar a 'duration'

    // Agrega una fase que se dispara cuando el tiempo transcurrido alcanza 'atElapsed'.
    void addPhase(float atElapsed, std::function<void()> onEnter) {
        phases.push_back(Phase{ atElapsed, std::move(onEnter), false });
    }

    float elapsed()   const { return elapsed_; }
    float remaining() const { float r = duration - elapsed_; return r < 0.0f ? 0.0f : r; }
    bool  finished()  const { return done_; }

    void reset() {
        elapsed_ = 0.0f; done_ = false; running = true;
        for (Phase& p : phases) p.fired = false;
    }

    void update(float dt) override {
        if (!running || done_) return;
        elapsed_ += dt;
        for (Phase& p : phases)
            if (!p.fired && elapsed_ >= p.atElapsed) { p.fired = true; if (p.onEnter) p.onEnter(); }
        if (elapsed_ >= duration) {
            done_ = true; running = false;
            if (onComplete) onComplete();
        }
    }

private:
    struct Phase { float atElapsed; std::function<void()> onEnter; bool fired; };
    std::vector<Phase> phases;
    float elapsed_ = 0.0f;
    bool  done_ = false;
};
