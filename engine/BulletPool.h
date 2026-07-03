#pragma once
#include <string>
#include <vector>
#include <functional>
#include "Component.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el .cpp
class CircleCollider;

// Pool ESTATICO de proyectiles para bullet hell. Reserva de una vez 'capacity'
// balas (GDD 9.2: 1000 concurrentes) y las reutiliza, para NO pedir/soltar memoria
// en runtime (la causa tipica de stuttering). Cada bala es un struct plano, NO un
// GameObject: mover cientos de GameObjects con su unordered_map de componentes por
// frame seria carisimo.
//
// COLISION EN O(n): las balas NO pasan por la fase O(n^2) de la Scene. El pool
// testea sus balas activas contra UNA sola hurtbox (el CircleCollider del jugador,
// via setTarget): 1 contra N, no N contra N. Ademas, si el objetivo tiene un Health
// invulnerable (Dash), se SALTA el bucle de colision entero (i-frames baratos).
//
// Es un Component: se agrega a un GameObject "manager" de la sala y la Scene ya lo
// actualiza y dibuja como a cualquier otro.

class BulletPool : public Component {
public:
    struct Bullet {
        float x = 0.0f, y = 0.0f;   // posicion (centro) en el mundo
        float vx = 0.0f, vy = 0.0f; // velocidad en px/seg
        float radius = 4.0f;        // radio de colision
        float life = 0.0f;          // seg restantes; <= 0 = sin limite (culla por bordes)
        int   type = 0;             // variante libre (color/sprite), a gusto del juego
        bool  active = false;
    };

    explicit BulletPool(int capacity = 1000);

    // --- Aspecto -------------------------------------------------------------
    // Sprite opcional para dibujar cada bala. Sin textura se dibuja un cuadrado de
    // color (fallback util para prototipar sin assets).
    void setSprite(const std::string& path);
    void setSourceRect(float x, float y, float w, float h); // recorte del sprite
    void setColor(int r, int g, int b, int a = 255);        // tinte / color del fallback
    void setDrawScale(float s) { drawScale = s; }           // tamano visual = 2*radius*s

    // --- Configuracion de simulacion ----------------------------------------
    // Rectangulo de mundo fuera del cual una bala se desactiva (culling de sala).
    void setBounds(float minX, float minY, float maxX, float maxY);

    // Hurtbox que las balas pueden impactar (normalmente la del jugador).
    void setTarget(CircleCollider* hurtbox) { target = hurtbox; }

    // Se dispara cuando una bala toca el objetivo (y este no es invulnerable). Es
    // opcional: si el objetivo tiene Health, el pool ya llama a kill() por su cuenta.
    std::function<void()> onHit;

    // --- Uso -----------------------------------------------------------------
    // Toma una bala libre y la activa. Devuelve nullptr si el pool esta lleno.
    Bullet* spawn(float x, float y, float vx, float vy,
                  float radius, float life = 0.0f, int type = 0);

    void clear();                 // desactiva todas (reiniciar la sala al reaparecer)
    int  activeCount() const;     // balas activas (util para debug/HUD)
    int  capacity() const { return (int)pool.size(); }

    void update(float dt) override;
    void render() override;

private:
    std::vector<Bullet> pool;
    int nextFree = 0;             // pista de busqueda para el siguiente slot libre

    SDL_Texture* texture = nullptr; // prestada por el AssetManager (no somos dueno)
    std::string  path;
    bool  useSrc = false;
    float srcX = 0.0f, srcY = 0.0f, srcW = 0.0f, srcH = 0.0f;
    int   colR = 255, colG = 80, colB = 80, colA = 255;
    float drawScale = 1.0f;

    bool  hasBounds = false;
    float bMinX = 0.0f, bMinY = 0.0f, bMaxX = 0.0f, bMaxY = 0.0f;

    CircleCollider* target = nullptr; // no somos dueno
};
