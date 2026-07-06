#pragma once

// Pantallas de flujo (no-gameplay) de "REDACTED": menu principal y cinematicas
// estaticas (intro, entrada a camaras, jefe, victoria). Son escenas de imagen fija
// dibujadas en espacio de PANTALLA (sin camara), con texto en la fuente 5x7 del motor.
// Al terminar, cada una pide (via SceneFlow) la siguiente escena del flujo.
//
// Indices de escena del flujo (ver main.cpp / SceneFlow):
//   0 = menu | 10 = intro | 11/12/13 = entrada Camara 01/02/03 | 14 = intro jefe
//   16 = victoria | 4 = jefe, 5/6/7 = gameplay Camaras 01/02/03

class Scene;

void buildMainMenu(Scene& scene);
void buildIntroCinematic(Scene& scene);              // 3 paneles -> Camara 01 (11)
void buildChamberCinematic(Scene& scene, int chamber); // 1/2/3 -> gameplay 5/6/7
void buildBossCinematic(Scene& scene);               // -> jefe (4)
void buildVictoryCinematic(Scene& scene);            // 3 paneles -> menu (0)
