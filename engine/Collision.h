#pragma once

// Helpers de colision puramente matematicos: NO tocan SDL ni el sistema de
// componentes, son funciones libres reutilizables. Pensados para bullet hell,
// donde hay que testear cientos de proyectiles por frame: todas comparan
// distancias AL CUADRADO para evitar la raiz cuadrada (operacion cara), tal como
// pide el GDD (deteccion circular sin sqrt).

namespace Collision {

// Circulo vs circulo. Cada circulo es (centro, radio) en coordenadas de mundo.
// Hay contacto si la distancia entre centros <= suma de radios, comparado al
// cuadrado: dx*dx + dy*dy <= (r1+r2)^2.
inline bool circleVsCircle(float x1, float y1, float r1,
                           float x2, float y2, float r2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float rr = r1 + r2;
    return (dx * dx + dy * dy) <= (rr * rr);
}

// Circulo vs AABB (rectangulo alineado a ejes dado por su centro y sus medios
// lados). Se toma el punto del rectangulo mas cercano al centro del circulo
// (clamp) y se mide la distancia a ese punto, tambien al cuadrado. Sirve para
// la hurtbox circular del jugador contra muros/tiles solidos (AABB).
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
// de mina) evaluadas contra el centro de la hurtbox del jugador.
inline bool pointInCircle(float px, float py,
                          float cx, float cy, float r) {
    float dx = px - cx;
    float dy = py - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

} // namespace Collision
