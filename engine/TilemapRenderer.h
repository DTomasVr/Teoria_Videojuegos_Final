#pragma once
#include <string>
#include <vector>
#include "Component.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el.cpp

// Dibuja una grilla de tiles recortados de un tileset, misma idea que el SpriteRenderer.

class TilemapRenderer : public Component {
public:
    TilemapRenderer() = default; // modo archivo: el tileset lo define loadFromFile
    TilemapRenderer(std::string tilesetPath, int tileW, int tileH, int tilesetColumns);

    // Define el mapa: indices en orden row-major, con su ancho (columnas) y alto (filas).
    void setMap(const std::vector<int>& tiles, int mapWidth, int mapHeight);

    // Carga el mapa COMPLETO desde un archivo de texto (tileset, tile, columns, solid
    bool loadFromFile(const std::string& path);

    // Carga un mapa exportado desde Tiled en formato JSON (capa de tiles + tileset
    bool loadFromTiledJson(const std::string& path);

    // Marca un indice de tile como solido (genera colision). Se puede llamar varias veces.
    void setSolid(int tileIndex);

    void awake() override;   // carga la textura del tileset
    void update(float dt) override; // build perezoso de los colliders la primera vez
    void render() override;  // dibuja solo las celdas visibles (culling)

private:
    bool isSolid(int tileIndex) const;
    void buildColliders();

    std::string path;
    SDL_Texture* texture = nullptr; // prestada por el AssetManager (no somos dueno)

    int tileW = 0, tileH = 0;       // tamano del tile EN LA IMAGEN (para recortar)
    int tilesetColumns = 0;         // columnas del tileset (para mapear indice -> celda)

    std::vector<int> tiles;         // mapa row-major (-1 vacio)
    int mapWidth = 0, mapHeight = 0;

    std::vector<int> solids;        // indices de tile marcados como solidos
    bool built = false;             // ya se generaron los colliders?
};
