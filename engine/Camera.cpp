#include "Camera.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"

#include <SDL3/SDL.h>

void Camera::awake() {
    // Al agregarse, esta camara pasa a ser la activa de la escena.
    gameObject->scene->setActiveCamera(this);
}

void Camera::update(float /*dt*/) {
    // Zoom de AJUSTE: encaja el rectangulo [fitWidth x fitHeight] en la pantalla
    // manteniendo proporcion (el lado que sobra queda en negro).
    if (fitWidth > 0.0f && fitHeight > 0.0f) {
        SDL_Renderer* renderer = gameObject->scene->getRenderer();
        int w = 0, h = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
        float zx = (float)w / fitWidth, zy = (float)h / fitHeight;
        zoom = (zx < zy) ? zx : zy;
    }
}

void Camera::worldToScreen(float worldX, float worldY,
                           float& screenX, float& screenY) const {
    SDL_Renderer* renderer = gameObject->scene->getRenderer();

    // Tamano actual de la ventana (se adapta solo si el alumno la redimensiona).
    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);

    Transform* cam = gameObject->transform;

    // El punto que mira la camara queda en el centro; el resto se desplaza segun el zoom.
    screenX = (worldX - cam->x) * zoom + w * 0.5f;
    screenY = (worldY - cam->y) * zoom + h * 0.5f;
}
