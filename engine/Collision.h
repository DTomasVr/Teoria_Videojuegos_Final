#pragma once

// Helpers de colision puramente matematicos

namespace Collision {

// Circulo vs circulo. Cada circulo es (centro, radio) en coordenadas de mundo
inline bool circleVsCircle(float x1, float y1, float r1,
                           float x2, float y2, float r2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float rr = r1 + r2;
    return (dx * dx + dy * dy) <= (rr * rr);
}

// Circulo vs AABB (rectangulo alineado a ejes dado por su centro y sus medios
inline bool circleVsAABB(float cx, float cy, float r,
                         float boxCenterX, float boxCenterY,
                         float halfW, float halfH) {
    // Vector del centro del circulo al centro de la caja, acotado a la caja.
    float dx = cx - boxCenterX;
    float dy = cy - boxCenterY;
    if (dx >  halfW) dx =  halfW;
    if (dx < -halfW) dx = -halfW;
    if (dy >  halfH) dy =  halfH;
    if (dy < -halfH) dy = -halfH;

    // Punto mas cercano de la caja al circulo y su distancia al centro.
    float closestX = boxCenterX + dx;
    float closestY = boxCenterY + dy;
    float ex = cx - closestX;
    float ey = cy - closestY;
    return (ex * ex + ey * ey) <= (r * r);
}

// Punto dentro de un circulo. Util para zonas de peligro (radio de nuke, anillo
inline bool pointInCircle(float px, float py,
                          float cx, float cy, float r) {
    float dx = px - cx;
    float dy = py - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

} // namespace Collision
