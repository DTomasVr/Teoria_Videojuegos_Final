#pragma once
#include <string>
#include "Component.h"

// Dibuja texto con una fuente de mapa de bits 5x7 EMBEBIDA en el motor (sin
// dependencias externas: no necesita SDL3_ttf ni ningun asset de fuente). Cada
// pixel de la fuente se dibuja como un rectangulo, asi que el estilo pixel-art es
// coherente con el resto del juego. Suficiente para el HUD minimo de "REDACTED":
// el temporizador gigante, "CALOR: 47%", lecturas de NEXUS-9, etc.
//
// Soporta A-Z, 0-9 y algunos simbolos ( espacio : % . - / ). Las minusculas se
// pasan a mayusculas; lo no soportado se dibuja como espacio.
//
// ESPACIO: por defecto es HUD (screenSpace=true): el Transform se interpreta como
// pixeles de PANTALLA (ignora la camara). Con screenSpace=false el texto vive en el
// MUNDO (usa la camara, util para el temporizador proyectado en el suelo).

class TextRenderer : public Component {
public:
    std::string text;
    int   colR = 255, colG = 255, colB = 255, colA = 255;
    float pixelSize = 3.0f;   // lado de cada pixel de la fuente (px)
    float spacing   = 1.0f;   // columnas de separacion entre glifos (en pixeles de fuente)
    bool  centered  = false;  // centrar horizontalmente respecto al punto
    bool  screenSpace = true; // true = HUD (pantalla); false = mundo (con camara)
    // Ancla de pantalla (solo HUD): fraccion 0..1 del tamano de la ventana a la que
    // se suma el Transform como offset. anchorX=0.5 => centrado en X sea cual sea la
    // resolucion (util en pantalla completa). Por defecto 0 = comportamiento absoluto.
    float anchorX = 0.0f, anchorY = 0.0f;

    void setText(const std::string& t) { text = t; }
    void setColor(int r, int g, int b, int a = 255) { colR = r; colG = g; colB = b; colA = a; }

    void render() override;
};
