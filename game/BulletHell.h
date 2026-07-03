#pragma once

class Scene;

// Escena de PRUEBAS para "REDACTED" (tecla 4). Es un banco de trabajo pensado para
// ir probando las capacidades del motor tier a tier:
//   - Tier 1 (YA): sala cerrada, jugador con movimiento 8-dir + Dash con i-frames,
//     hurtbox circular, muerte instantanea + respawn, y una torreta que escupe un
//     patron en espiral hacia el pool de balas.
//   - Tier 2/3: hay "HOOKS" marcados en el .cpp donde enganchar particulas/gore,
//     decals persistentes, HUD, mas peligros, etc. sin reescribir la escena.
void buildBulletHell(Scene& scene);
