#pragma once
#include <string>
#include "Component.h"

struct SDL_Texture; 

// Capa de DECALS PERSISTENTES. Es el sistema clave del gore de "REDACTED"

class DecalLayer : public Component {
public:
    // Crea el render target: esquina superior izquierda en mundo (worldOriginX,Y)
    bool setup(float worldOriginX, float worldOriginY, int pixelW, int pixelH);

    // Estampa un rectangulo de color solido (mancha de sangre, crater, parche).
    void stampRect(float wx, float wy, float w, float h, int r, int g, int b, int a);

    // Estampa un sprite (con rotacion y tinte) para marcas con forma real si se
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
