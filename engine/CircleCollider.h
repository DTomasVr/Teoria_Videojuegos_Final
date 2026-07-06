#pragma once
#include "GameObject.h"
#include "Transform.h"

// Colisionador CIRCULAR anclado al centro del Transform.

class CircleCollider : public Component {
public:
    float radius  = 4.0f;    // radio en unidades de mundo
    float offsetX = 0.0f;    // corrimiento respecto al centro del objeto
    float offsetY = 0.0f;
    bool  isTrigger = true;  // por ahora informativo; la hurtbox no separa, solo avisa

    // Centro del circulo en coordenadas de mundo (lo consulta la deteccion de balas).
    float centerX() const { return gameObject->transform->x + offsetX; }
    float centerY() const { return gameObject->transform->y + offsetY; }
};
