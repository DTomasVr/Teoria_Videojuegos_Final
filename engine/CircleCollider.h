#pragma once
#include "GameObject.h"
#include "Transform.h"

// Colisionador CIRCULAR anclado al centro del Transform. Es la "hurtbox" tipica
// de un bullet hell: el sprite del jugador mide ~32x32 px pero su caja real es un
// circulo diminuto (~4 px de radio) en el centro del pecho, para permitir esquives
// milimetricos (ver GDD 4.1).
//
// A DIFERENCIA de BoxCollider, este NO se registra en la fase de fisica O(n^2) de
// la Scene: el bullet hell testea las balas contra la hurtbox aparte, en O(n)
// (una hurtbox contra N balas), que es donde el pool de balas lo consulta. El
// jugador puede tener AL MISMO TIEMPO un BoxCollider (choque con muros) y este
// CircleCollider (impactos de balas/peligros), porque son componentes de tipo
// distinto. Es solo datos: no toca SDL.

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
