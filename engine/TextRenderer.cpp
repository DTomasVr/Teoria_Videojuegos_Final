#include "TextRenderer.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"

#include <SDL3/SDL.h>
#include <cctype>

// --- Fuente 5x7 embebida ----------------------------------------------------
// Cada glifo son 7 filas; en cada fila los 5 bits bajos son los pixeles
// (bit 4 = columna izquierda). 1 = pixel encendido.
namespace {
struct Glyph { char c; unsigned char rows[7]; };

const Glyph FONT[] = {
    {'0',{0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110}},
    {'1',{0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110}},
    {'2',{0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111}},
    {'3',{0b11111,0b00010,0b00100,0b00010,0b00001,0b10001,0b01110}},
    {'4',{0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010}},
    {'5',{0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110}},
    {'6',{0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110}},
    {'7',{0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000}},
    {'8',{0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110}},
    {'9',{0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100}},
    {'A',{0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}},
    {'B',{0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110}},
    {'C',{0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110}},
    {'D',{0b11100,0b10010,0b10001,0b10001,0b10001,0b10010,0b11100}},
    {'E',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111}},
    {'F',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000}},
    {'G',{0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01111}},
    {'H',{0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}},
    {'I',{0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110}},
    {'J',{0b00111,0b00010,0b00010,0b00010,0b00010,0b10010,0b01100}},
    {'K',{0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001}},
    {'L',{0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111}},
    {'M',{0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001}},
    {'N',{0b10001,0b10001,0b11001,0b10101,0b10011,0b10001,0b10001}},
    {'O',{0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}},
    {'P',{0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000}},
    {'Q',{0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101}},
    {'R',{0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001}},
    {'S',{0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110}},
    {'T',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100}},
    {'U',{0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}},
    {'V',{0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100}},
    {'W',{0b10001,0b10001,0b10001,0b10101,0b10101,0b10101,0b01010}},
    {'X',{0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001}},
    {'Y',{0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100}},
    {'Z',{0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111}},
    {':',{0b00000,0b00100,0b00100,0b00000,0b00100,0b00100,0b00000}},
    {'%',{0b11000,0b11001,0b00010,0b00100,0b01000,0b10011,0b00011}},
    {'.',{0b00000,0b00000,0b00000,0b00000,0b00000,0b00110,0b00110}},
    {'-',{0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000}},
    {'/',{0b00001,0b00010,0b00010,0b00100,0b01000,0b01000,0b10000}},
};

// Devuelve las 7 filas del glifo, o nullptr si es espacio/no soportado.
const unsigned char* glyphRows(char c) {
    c = (char)std::toupper((unsigned char)c);
    for (const Glyph& gl : FONT) if (gl.c == c) return gl.rows;
    return nullptr; // espacio o desconocido
}
} // namespace

void TextRenderer::render() {
    if (text.empty()) return;
    SDL_Renderer* ren = gameObject->scene->getRenderer();
    Transform* t = gameObject->transform;

    // Punto de inicio y escala del pixel de fuente segun el espacio de coordenadas.
    float startX, startY, px;
    if (screenSpace) {
        int ow = 0, oh = 0;
        SDL_GetCurrentRenderOutputSize(ren, &ow, &oh); // ancla al tamano real (pantalla completa)
        startX = ow * anchorX + t->x; startY = oh * anchorY + t->y; px = pixelSize;
    } else {
        Camera* cam = gameObject->scene->getActiveCamera();
        float zoom = cam ? cam->getZoom() : 1.0f;
        if (cam) cam->worldToScreen(t->x, t->y, startX, startY);
        else { startX = t->x; startY = t->y; }
        px = pixelSize * zoom;
    }

    const float advance = (5.0f + spacing) * px; // ancho de un glifo + separacion
    if (centered) {
        float total = text.size() * advance - spacing * px;
        startX -= total * 0.5f;
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, (Uint8)colR, (Uint8)colG, (Uint8)colB, (Uint8)colA);

    float cursorX = startX;
    for (char c : text) {
        const unsigned char* rows = glyphRows(c);
        if (rows) {
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (rows[row] & (1 << (4 - col))) {
                        SDL_FRect p{ cursorX + col * px, startY + row * px, px, px };
                        SDL_RenderFillRect(ren, &p);
                    }
                }
            }
        }
        cursorX += advance;
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}
