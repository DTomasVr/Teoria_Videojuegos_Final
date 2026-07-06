#pragma once
#include <cmath>
#include "GameObject.h"
#include "Transform.h"



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
