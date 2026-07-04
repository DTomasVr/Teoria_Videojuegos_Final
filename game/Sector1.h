#pragma once

class Scene;

// Sector 1 de "REDACTED": las camaras de prueba que preceden al jefe HERCULES-1
// (GDD 6). Cada camara es una sala cerrada donde solo hay que SOBREVIVIR un tiempo
// esquivando sistemas de armamento que escalan por fases; morir reinicia el intento
// al instante (el gore/crateres del intento quedan en el suelo).
//
//   buildCamara01 -> "El Pozo" (GDD 6.1): 40 s, torretas MK-II + nukes, 2 fases.
//   buildCamara02 -> "La Trinchera" (GDD 6.2): 55 s, torretas rotativas + lanzallamas
//                    + minas + bloque de escombros, 3 fases.
//
// (Camara 03 se agregara aqui reutilizando Turret/Nuke/Flamethrower/Chamber.)
void buildCamara01(Scene& scene);
void buildCamara02(Scene& scene);
