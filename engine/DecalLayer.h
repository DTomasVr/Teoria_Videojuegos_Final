#pragma once
#include <string>
#include "Component.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el .cpp

// Capa de DECALS PERSISTENTES. Es el sistema clave del gore de "REDACTED": la
// sangre, los crateres y los parches de fuego deben QUEDARSE en la sala y
// ACUMULARSE entre intentos (GDD 2.3 / 5.2 / 8.1). El truco: mantener una textura
// fuera de pantalla (render target) sobre la que se "estampan" marcas UNA vez y
// se quedan ahi. El respawn NO la limpia, asi que las marcas persisten entre
// muertes; solo clear() la borra.
//
// Es barato: cientos de marcas cuestan un solo dibujo por frame (la textura
// entera), no cientos de objetos.
//
// COORDENADAS: la textura cubre un rectangulo de mundo [origin, origin+pixel] a
// escala 1:1 (1 unidad de mundo = 1 pixel de la textura). Estampar en mundo (wx,wy)
// escribe en el texel (wx-originX, wy-originY). En render() se dibuja la textura
// completa en el mundo con la camara activa (como un sprite gigante anclado al origen).

class DecalLayer : public Component {
public:
    // Crea el render target: esquina superior izquierda en mundo (worldOriginX,Y)
    // y resolucion pixelW x pixelH (= tamano en mundo a escala 1:1). Llamar una vez
    // tras addComponent. Devuelve false (y hace SDL_Log) si el target no se pudo crear.
    bool setup(float worldOriginX, float worldOriginY, int pixelW, int pixelH);

    // Estampa un rectangulo de color solido (mancha de sangre, crater, parche).
    void stampRect(float wx, float wy, float w, float h, int r, int g, int b, int a);

    // Estampa un sprite (con rotacion y tinte) para marcas con forma real si se
    // dispone de un asset (p.ej. una textura de salpicadura).
    void stampSprite(const std::string& path, float wx, float wy,
                     float w, float h, float rotation, int r, int g, int b, int a);

    void clear();            // borra TODOS los decals (NO se llama en el respawn)

    void render() override;  // dibuja la capa entera en el mundo
    ~DecalLayer();

private:
    void beginStamp();       // fija el render target en la textura de decals
    void endStamp();         // restaura el render target por defecto (pantalla)

    SDL_Texture* target = nullptr; // somos DUENOS (no lo posee el AssetManager)
    float originX = 0.0f, originY = 0.0f;
    int   texW = 0, texH = 0;
};
