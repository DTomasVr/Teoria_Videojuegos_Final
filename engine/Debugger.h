#pragma once

class Scene;
class FollowCamera;

// Herramientas de depuracion OPCIONALES.

namespace Debug {
    void setEnabled(bool on);
    void toggle();
    bool isEnabled();

    // Todas reciben la escena para tomar el renderer y la camara activa,
    void drawColliders(Scene& scene);                    // AABB de cada collider
    void drawDeadZone(Scene& scene, FollowCamera* cam);  // zona muerta de la camara
    void drawRect(Scene& scene, float x, float y, float w, float h);
    void drawCircle(Scene& scene, float x, float y, float radius); // hurtbox / zonas radiales
    void drawLine(Scene& scene, float x1, float y1, float x2, float y2);
    void drawPoint(Scene& scene, float x, float y);
}
