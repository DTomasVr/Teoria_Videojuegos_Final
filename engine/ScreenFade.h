#pragma once
#include <functional>
#include "Component.h"

// Velo de pantalla completa para transiciones (fundido a/desde negro entre salas,

class ScreenFade : public Component {
public:
    int r = 0, g = 0, b = 0; // color del velo (negro por defecto)

    void fadeIn(float seconds);   // de opaco -> transparente
    void fadeOut(float seconds);  // de transparente -> opaco
    void setAlpha(float a01);     // fija la opacidad al instante (0..1)
    bool isFading() const { return active; }

    // Parpadeo: sube a opaco en outSeconds y vuelve a transparente en inSeconds, de
    void blink(float outSeconds, float inSeconds);

    // Se dispara una vez cuando el fundido llega a su destino.
    std::function<void()> onComplete;

    void update(float dt) override;
    void render() override;

private:
    float alpha  = 0.0f; // opacidad actual 0..1
    float target = 0.0f; // opacidad objetivo
    float speed  = 0.0f; // unidades de alpha por segundo
    bool  active = false;

    bool  blinking = false; // parpadeo de respawn en curso
    int   blinkStage = 0;   // 0 = subiendo a negro; 1 = volviendo
    float blinkOut = 0.0f, blinkIn = 0.0f;
};
