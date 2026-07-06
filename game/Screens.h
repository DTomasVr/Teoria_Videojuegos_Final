#pragma once

// Pantallas de flujo (no-gameplay) de "REDACTED": menu principal y cinematicas

class Scene;

void buildMainMenu(Scene& scene);
void buildIntroCinematic(Scene& scene);              // 3 paneles -> Camara 01 (11)
void buildChamberCinematic(Scene& scene, int chamber); // 1/2/3 -> gameplay 5/6/7
void buildBossCinematic(Scene& scene);               // -> jefe (4)
void buildVictoryCinematic(Scene& scene);            // 3 paneles -> menu (0)
