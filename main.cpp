#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <memory>

#include "engine/Scene.h"
#include "engine/Debugger.h"
#include "engine/Audio.h"
#include "engine/Health.h"

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
    auto loadScene = [&](int sel) {
        scene = std::make_unique<Scene>(renderer);
        accumulator = 0.0; // arrancar la nueva escena con el reloj limpio
        current = sel;
        const char* title = "REDACTED  (0=menu, 1-4 escenas, F1 debug)";
        switch (sel) {
            // Flujo real del juego: menu -> cinematicas -> gameplay.
            case 0:  buildMainMenu(*scene);        title = "REDACTED - Menu"; break;
            case 10: buildIntroCinematic(*scene);  title = "REDACTED - Intro"; break;
            case 11: buildChamberCinematic(*scene, 1); title = "REDACTED - Camara 01"; break;
            case 12: buildChamberCinematic(*scene, 2); title = "REDACTED - Camara 02"; break;
            case 13: buildChamberCinematic(*scene, 3); title = "REDACTED - Camara 03"; break;
            case 14: buildBossCinematic(*scene);   title = "REDACTED - HERCULES-1"; break;
            case 16: buildVictoryCinematic(*scene); title = "REDACTED - Victoria"; break;
            // Gameplay (teclas de depuracion 1-4 saltan directamente aqui).
            case 1: buildCamara01(*scene);   title = "Camara 01: El Pozo  (0=menu, 1-4 escenas, F1 debug)"; break;
            case 2: buildCamara02(*scene);   title = "Camara 02: La Trinchera  (0=menu, 1-4 escenas, F1 debug)"; break;
            case 3: buildCamara03(*scene);   title = "Camara 03: El Suelo de Matanza  (0=menu, 1-4 escenas, F1 debug)"; break;
            case 4: buildBulletHell(*scene); title = "Boss HERCULES-1  (0=menu, 1-4 escenas, F1 debug)"; break;
        }
        SDL_SetWindowTitle(window, title);
    };
    loadScene(0); // escena inicial: menu principal

    // Bucle de PASO DE TIEMPO FIJO: la logica se actualiza siempre con el
    bool running = true;
    Uint64 currentTime = SDL_GetTicks();

    while (running) {
        Uint64 newTime = SDL_GetTicks();
        double frameTime = (newTime - currentTime) / 1000.0;
        currentTime = newTime;
        if (frameTime > 0.25) frameTime = 0.25; // tope: evita la "espiral de la muerte"
        accumulator += frameTime;

        // Eventos: una vez por frame (los controladores leen el estado del teclado
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;

            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) running = false; // salir de pantalla completa
                if (event.key.scancode == SDL_SCANCODE_F1) Debug::toggle();
                if (event.key.scancode == SDL_SCANCODE_G) Health::godMode = !Health::godMode; // modo dios (test)

                // Tecla 0: volver al menu principal.
                if (event.key.scancode == SDL_SCANCODE_0 && current != 0) loadScene(0);

                // Teclas de depuracion: saltar directamente al gameplay (1=Camara01,
                int sel = 0;
                if (event.key.scancode == SDL_SCANCODE_1) sel = 1;
                if (event.key.scancode == SDL_SCANCODE_2) sel = 2;
                if (event.key.scancode == SDL_SCANCODE_3) sel = 3;
                if (event.key.scancode == SDL_SCANCODE_4) sel = 4;
                if (sel != 0 && sel != current) loadScene(sel);
            }
        }

        // Actualizacion a paso fijo: puede correr 0, 1 o varias veces por frame
        while (accumulator >= FIXED_DT) {
            scene->update((float)FIXED_DT);
            accumulator -= FIXED_DT;
        }

        // Progresion entre camaras: si la escena pidio avanzar (al superarse), la
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
