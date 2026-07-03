#pragma once
#include <cmath>
#include "GameObject.h"
#include "Transform.h"

// Parenting de Transform como COMPONENTE (el motor no invierte la jerarquia: en vez
// de tocar Transform y todos los renderers, este componente coloca a su objeto
// respecto a un Transform "padre" cada frame). Util para el jefe HERCULES-1: un
// chasis central que rota y 4 brazos articulados anclados a el (GDD 7.1).
//
// Coloca el objeto en: padre + (offset local ROTADO por la rotacion del padre), y
// opcionalmente hereda la rotacion del padre (para que el brazo gire con el chasis).
// Es logica pura: no toca SDL.
//
// ORDEN: el padre debe actualizarse ANTES que el hijo. Como la Scene actualiza en
// orden de creacion, crea el padre primero y luego los hijos.

class ChildOf : public Component {
public:
    Transform* parent = nullptr;
    float localX = 0.0f, localY = 0.0f; // offset en el espacio local del padre
    float localRotation = 0.0f;         // rotacion extra (grados) sobre la del padre
    bool  inheritRotation = true;       // false = solo hereda posicion

    void setParent(GameObject* p) { parent = p ? p->transform : nullptr; }

    void update(float) override {
        if (!parent) return;
        float rad = parent->rotation * 0.01745329f; // grados -> radianes
        float c = std::cos(rad), s = std::sin(rad);

        Transform* t = gameObject->transform;
        t->x = parent->x + (localX * c - localY * s);
        t->y = parent->y + (localX * s + localY * c);
        t->rotation = inheritRotation ? (parent->rotation + localRotation) : localRotation;
    }
};
