#pragma once
#include "Component.h"

class Camera : public Component {
public:
    float zoom = 1.0f; // 1 = normal, 2 = acercado, 0.5 = alejado

    // Si se fijan (>0), el zoom se AJUSTA solo cada frame para que este rectangulo de
    float fitWidth = 0.0f, fitHeight = 0.0f;
    void fitToWorld(float w, float h) { fitWidth = w; fitHeight = h; }

    void awake() override;         // se registra como la camara activa de la escena
    void update(float dt) override; // recalcula el zoom de ajuste

    // Convierte un punto del mundo a su posicion en pantalla (aplica posicion + zoom).
    void worldToScreen(float worldX, float worldY, float& screenX, float& screenY) const;

    float getZoom() const { return zoom; }
};
