#pragma once
#include <string>
#include "Component.h"

// Dibuja texto con una fuente de mapa de bits 5x7 EMBEBIDA en el motor

class TextRenderer : public Component {
public:
    std::string text;
    int   colR = 255, colG = 255, colB = 255, colA = 255;
    float pixelSize = 3.0f;   // lado de cada pixel de la fuente (px)
    float spacing   = 1.0f;   // columnas de separacion entre glifos (en pixeles de fuente)
    bool  centered  = false;  // centrar horizontalmente respecto al punto
    bool  screenSpace = true; // true = HUD (pantalla); false = mundo (con camara)
    // Ancla de pantalla (solo HUD): fraccion 0..1 del tamano de la ventana a la que
    float anchorX = 0.0f, anchorY = 0.0f;

    void setText(const std::string& t) { text = t; }
    void setColor(int r, int g, int b, int a = 255) { colR = r; colG = g; colB = b; colA = a; }

    void render() override;
};
