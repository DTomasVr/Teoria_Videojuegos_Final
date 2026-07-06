#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <memory>

#include "engine/Scene.h"
#include "engine/Debugger.h"
#include "engine/Audio.h"
#include "engine/Health.h"

#include "game/Platformer.h"
#include "game/TopDown.h"
#include "game/Shooter.h"
#include "game/BulletHell.h"
#include "game/Sector1.h"
#include "game/Screens.h"
#include "game/SceneFlow.h"

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Error al inicializar SDL: %s", SDL_GetError());
        return 1;
    }
    Audio::init(); // audio opcional: si falla, el juego sigue sin sonido
    SDL_Window* window = SDL_CreateWindow("REDACTED", 1280, 720, 0);
    if (!window) { SDL_Quit(); return 1; }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    SDL_SetWindowFullscreen(window, true); // pantalla completa (borde negro = fuera de la sala)

    const double FIXED_DT = 1.0 / 60.0; // 60 Hz de simulacion
    std::unique_ptr<Scene> scene;
    int current = 0;
    double accumulator = 0.0;

    // Construye la escena 'sel' (1-7) y ajusta el titulo. Lo usan tanto las teclas de
    // depuracion como la PROGRESION entre camaras (una camara pide la siguiente al ganar).
    auto loadScene = [&](int sel) {
        scene = std::make_unique<Scene>(renderer);
        accumulator = 0.0; // arrancar la nueva escena con el reloj limpio
        current = sel;
        const char* title = "REDACTED  (0=menu, 1-7 cambia, F1 debug)";
        switch (sel) {
            // Flujo real del juego: menu -> cinematicas -> gameplay.
            case 0:  buildMainMenu(*scene);        title = "REDACTED - Menu"; break;
            case 10: buildIntroCinematic(*scene);  title = "REDACTED - Intro"; break;
            case 11: buildChamberCinematic(*scene, 1); title = "REDACTED - Camara 01"; break;
            case 12: buildChamberCinematic(*scene, 2); title = "REDACTED - Camara 02"; break;
            case 13: buildChamberCinematic(*scene, 3); title = "REDACTED - Camara 03"; break;
            case 14: buildBossCinematic(*scene);   title = "REDACTED - HERCULES-1"; break;
            case 16: buildVictoryCinematic(*scene); title = "REDACTED - Victoria"; break;
            // Gameplay / demos (accesibles con teclas de depuracion 1-7).
            case 1: buildPlatformer(*scene); title = "Ejemplo 1: Platformer  (0=menu, 1-7 cambia, F1 debug)"; break;
            case 2: buildTopDown(*scene);    title = "Ejemplo 2: Top-down  (0=menu, 1-7 cambia, F1 debug)"; break;
            case 3: buildShooter(*scene);    title = "Ejemplo 3: Shooter  (0=menu, 1-7 cambia, F1 debug)"; break;
            case 4: buildBulletHell(*scene); title = "Ejemplo 4: Boss HERCULES-1  (0=menu, 1-7 cambia, F1 debug)"; break;
            case 5: buildCamara01(*scene);   title = "Sector 1 - Camara 01: El Pozo  (0=menu, 1-7 cambia, F1 debug)"; break;
            case 6: buildCamara02(*scene);   title = "Sector 1 - Camara 02: La Trinchera  (0=menu, 1-7 cambia, F1 debug)"; break;
            case 7: buildCamara03(*scene);   title = "Sector 1 - Camara 03: El Suelo de Matanza  (0=menu, 1-7 cambia, F1 debug)"; break;
        }
        SDL_SetWindowTitle(window, title);
    };
    loadScene(0); // escena inicial: menu principal

    // Bucle de PASO DE TIEMPO FIJO (GDD 9.1): la logica se actualiza siempre con el
    // mismo dt, sin importar el hardware, para que fisica y patrones de bala sean
    // deterministas (mismo resultado en cualquier PC). El tiempo real de cada frame
    // se acumula y se consume en pasos de FIXED_DT; el render ocurre una vez por frame.
    bool running = true;
    Uint64 currentTime = SDL_GetTicks();

    while (running) {
        Uint64 newTime = SDL_GetTicks();
        double frameTime = (newTime - currentTime) / 1000.0;
        currentTime = newTime;
        if (frameTime > 0.25) frameTime = 0.25; // tope: evita la "espiral de la muerte"
        accumulator += frameTime;

        // Eventos: una vez por frame (los controladores leen el estado del teclado
        // con SDL_GetKeyboardState dentro de su update, asi que aqui solo atendemos
        // quit y flancos de tecla).
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;

            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) running = false; // salir de pantalla completa
                if (event.key.scancode == SDL_SCANCODE_F1) Debug::toggle();
                if (event.key.scancode == SDL_SCANCODE_G) Health::godMode = !Health::godMode; // modo dios (test)

                // Tecla 0: volver al menu principal.
                if (event.key.scancode == SDL_SCANCODE_0 && current != 0) loadScene(0);

                // Teclas de depuracion: saltar directamente a cualquier escena (1-7).
                int sel = 0;
                if (event.key.scancode == SDL_SCANCODE_1) sel = 1;
                if (event.key.scancode == SDL_SCANCODE_2) sel = 2;
                if (event.key.scancode == SDL_SCANCODE_3) sel = 3;
                if (event.key.scancode == SDL_SCANCODE_4) sel = 4;
                if (event.key.scancode == SDL_SCANCODE_5) sel = 5;
                if (event.key.scancode == SDL_SCANCODE_6) sel = 6;
                if (event.key.scancode == SDL_SCANCODE_7) sel = 7;
                if (sel != 0 && sel != current) loadScene(sel);
            }
        }

        // Actualizacion a paso fijo: puede correr 0, 1 o varias veces por frame
        // segun el tiempo acumulado.
        while (accumulator >= FIXED_DT) {
            scene->update((float)FIXED_DT);
            accumulator -= FIXED_DT;
        }

        // Progresion entre camaras: si la escena pidio avanzar (al superarse), la
        // reconstruimos aqui, fuera del update (las teclas 1-7 siguen disponibles).
        int requested = SceneFlow::takeRequested();
        if (requested >= 0 && requested != current) loadScene(requested);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // fondo negro: los bordes fuera de la sala
        SDL_RenderClear(renderer);
        scene->render();
        Debug::drawColliders(*scene);
        SDL_RenderPresent(renderer);
    }

    Audio::shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
