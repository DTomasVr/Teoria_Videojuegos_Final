#pragma once
#include <functional>
#include "Component.h"

// Velo de pantalla completa para transiciones (fundido a/desde negro entre salas,
// DOCX). Dibuja un rectangulo que cubre toda la ventana en espacio de PANTALLA
// (ignora la camara) con mezcla alpha; su opacidad se anima entre 0 (transparente)
// y 1 (opaco). Para que quede POR ENCIMA de todo, agregarlo el ULTIMO en la escena.
//
// Uso tipico de transicion entre niveles:
//   fade->onComplete = []{ cargarSiguienteSala(); };
//   fade->fadeOut(0.4f);   // se pone negro y luego dispara onComplete
// y al entrar a la sala nueva: fade->fadeIn(0.4f) (arranca en negro y aclara).

class ScreenFade : public Component {
public:
    int r = 0, g = 0, b = 0; // color del velo (negro por defecto)

    void fadeIn(float seconds);   // de opaco -> transparente
    void fadeOut(float seconds);  // de transparente -> opaco
    void setAlpha(float a01);     // fija la opacidad al instante (0..1)
    bool isFading() const { return active; }

    // Se dispara una vez cuando el fundido llega a su destino.
    std::function<void()> onComplete;

    void update(float dt) override;
    void render() override;

private:
    float alpha  = 0.0f; // opacidad actual 0..1
    float target = 0.0f; // opacidad objetivo
    float speed  = 0.0f; // unidades de alpha por segundo
    bool  active = false;
};
