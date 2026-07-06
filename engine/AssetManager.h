#pragma once
#include <string>
#include <unordered_map>

struct SDL_Renderer;
struct SDL_Texture;

// Carga y guarda en cache las texturas.

class AssetManager {
public:
    explicit AssetManager(SDL_Renderer* renderer);
    ~AssetManager();

    // Es dueno de recursos: no permitimos copiarlo.
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    // Devuelve la textura de esa ruta. La carga SOLO la primera vez;
    SDL_Texture* loadTexture(const std::string& path);

private:
    SDL_Renderer* renderer = nullptr;                       // no somos dueno
    std::unordered_map<std::string, SDL_Texture*> textures; // SI somos dueno
};
