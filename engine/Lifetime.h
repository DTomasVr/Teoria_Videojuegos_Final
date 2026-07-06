#pragma once
#include "GameObject.h"
#include "Scene.h"

// Destruye su objeto al cabo de 'seconds'. Ideal para balas, efectos o cualquier

class Lifetime : public Component {
public:
    float seconds = 1.0f;

    void update(float dt) override {
        seconds -= dt;
        if (seconds <= 0.0f)
            gameObject->scene->destroy(gameObject);
    }
};
