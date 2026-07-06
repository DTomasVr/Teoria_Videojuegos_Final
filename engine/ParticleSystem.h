#pragma once
#include <string>
#include <vector>
#include "Component.h"
#include "BlendMode.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el.cpp

// Sistema de particulas con pool estatico (misma idea que BulletPool)

class ParticleSystem : public Component {
public:
    struct Particle {
        float x = 0, y = 0, vx = 0, vy = 0;
        float life = 0, maxLife = 0;   // segundos restantes / totales
        float size0 = 0, size1 = 0;    // tamano al nacer / al morir (interpola)
        int   r = 255, g = 255, b = 255, a = 255; // color y alpha inicial
        bool  active = false;
    };

    explicit ParticleSystem(int capacity = 512);

    // --- Aspecto -------------------------------------------------------------
    void setSprite(const std::string& path);                 // opcional
    void setSourceRect(float x, float y, float w, float h);   // recorte del sprite
    void setBlendMode(BlendMode m) { blend = m; }

    // --- Parametros de emision (usados por emitBurst) ------------------------
    float speedMin = 60.0f,  speedMax = 260.0f; // px/seg del estallido
    float lifeMin  = 0.30f,  lifeMax  = 0.80f;  // seg de vida
    float sizeStart = 8.0f,  sizeEnd  = 1.0f;   // tamano inicial / final
    float gravity   = 0.0f;                     // px/seg^2 hacia abajo
    float drag      = 0.0f;                     // 0 = sin friccion; >0 frena (1/seg)
    int   colR = 220, colG = 30, colB = 30, colA = 255; // color de las particulas

    // --- Uso -----------------------------------------------------------------
    void emitBurst(float x, float y, int count); // estallido radial con los params
    void emit(float x, float y, float vx, float vy, float life,
              float size0, float size1, int r, int g, int b, int a);
    void clear();
    int  activeCount() const;

    void update(float dt) override;
    void render() override;

private:
    std::vector<Particle> pool;
    int nextFree = 0;

    SDL_Texture* texture = nullptr; // prestada por el AssetManager
    std::string  path;
    bool  useSrc = false;
    float srcX = 0, srcY = 0, srcW = 0, srcH = 0;
    BlendMode blend = BlendMode::Alpha;
};
