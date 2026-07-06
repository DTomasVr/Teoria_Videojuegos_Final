#pragma once
#include <string>
#include "Component.h"
#include "BlendMode.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el.cpp

// Dibuja una imagen segun el Transform. Puede dibujar la imagen completa o,

class SpriteRenderer : public Component {
public:
    SpriteRenderer() = default;               // sin textura inicial: la asigna el animator
    explicit SpriteRenderer(std::string imagePath);

    bool flipX = false; // espejo horizontal (mirar a izquierda/derecha)
    bool flipY = false; // espejo vertical

    void awake() override;
    void render() override;

    // Cambia que textura se dibuja (la usa el SpriteAnimator al cambiar de animacion).
    void setTexture(SDL_Texture* tex);

    // Dibujar solo una porcion de la imagen (un frame del spritesheet).
    void setSourceRect(float x, float y, float w, float h) {
        srcX = x; srcY = y; srcW = w; srcH = h; useSrcRect = true;
    }

    // Tinte y transparencia (modulacion de color). r,g,b,a en 0..255. 255,255,255,255
    void setColor(int r, int g, int b, int a = 255) {
        colR = r; colG = g; colB = b; colA = a;
    }
    void setBlendMode(BlendMode m) { blend = m; }

private:
    std::string path;
    SDL_Texture* texture = nullptr; // prestada por el AssetManager (no somos dueno)
    float width = 0.0f;             // tamano de la imagen COMPLETA
    float height = 0.0f;

    bool  useSrcRect = false;       // false = imagen completa; true = solo el recorte
    float srcX = 0.0f, srcY = 0.0f, srcW = 0.0f, srcH = 0.0f;

    int   colR = 255, colG = 255, colB = 255, colA = 255; // modulacion de color
    BlendMode blend = BlendMode::Alpha;
};
