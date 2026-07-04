#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <memory>

#include "engine/Scene.h"
#include "engine/Debugger.h"
#include "engine/Audio.h"

#include "game/Platformer.h"
#include "game/TopDown.h"
#include "game/Shooter.h"
#include "game/BulletHell.h"
#include "game/Sector1.h"

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Error al inicializar SDL: %s", SDL_GetError());
        return 1;
    }
    Audio::init(); // audio opcional: si falla, el juego sigue sin sonido
    SDL_Window* window = SDL_CreateWindow("Ejemplo 1: Platformer  (1/2/3/4 cambia, F1 debug)", 1280, 720, 0);
    if (!window) { SDL_Quit(); return 1; }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }

    auto scene = std::make_unique<Scene>(renderer);
    buildPlatformer(*scene);
    int current = 1;

    // Bucle de PASO DE TIEMPO FIJO (GDD 9.1): la logica se actualiza siempre con el
    // mismo dt, sin importar el hardware, para que fisica y patrones de bala sean
    // deterministas (mismo resultado en cualquier PC). El tiempo real de cada frame
    // se acumula y se consume en pasos de FIXED_DT; el render ocurre una vez por frame.
    const double FIXED_DT = 1.0 / 60.0; // 60 Hz de simulacion
    bool running = true;
    Uint64 currentTime = SDL_GetTicks();
    double accumulator = 0.0;

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
                if (event.key.scancode == SDL_SCANCODE_F1) Debug::toggle();

                int sel = 0;
                if (event.key.scancode == SDL_SCANCODE_1) sel = 1;
                if (event.key.scancode == SDL_SCANCODE_2) sel = 2;
                if (event.key.scancode == SDL_SCANCODE_3) sel = 3;
                if (event.key.scancode == SDL_SCANCODE_4) sel = 4;
                if (event.key.scancode == SDL_SCANCODE_5) sel = 5;
                if (event.key.scancode == SDL_SCANCODE_6) sel = 6;

                if (sel != 0 && sel != current) {
                    current = sel;
                    scene = std::make_unique<Scene>(renderer);
                    accumulator = 0.0; // arrancar la nueva escena con el reloj limpio
                    if (sel == 1) { buildPlatformer(*scene); SDL_SetWindowTitle(window, "Ejemplo 1: Platformer  (1-6 cambia, F1 debug)"); }
                    if (sel == 2) { buildTopDown(*scene);    SDL_SetWindowTitle(window, "Ejemplo 2: Top-down  (1-6 cambia, F1 debug)"); }
                    if (sel == 3) { buildShooter(*scene);    SDL_SetWindowTitle(window, "Ejemplo 3: Shooter  (1-6 cambia, F1 debug)"); }
                    if (sel == 4) { buildBulletHell(*scene); SDL_SetWindowTitle(window, "Ejemplo 4: Boss HERCULES-1  (1-6 cambia, F1 debug)"); }
                    if (sel == 5) { buildCamara01(*scene);   SDL_SetWindowTitle(window, "Sector 1 - Camara 01: El Pozo  (1-6 cambia, F1 debug)"); }
                    if (sel == 6) { buildCamara02(*scene);   SDL_SetWindowTitle(window, "Sector 1 - Camara 02: La Trinchera  (1-6 cambia, F1 debug)"); }
                }
            }
        }

        // Actualizacion a paso fijo: puede correr 0, 1 o varias veces por frame
        // segun el tiempo acumulado.
        while (accumulator >= FIXED_DT) {
            scene->update((float)FIXED_DT);
            accumulator -= FIXED_DT;
        }

        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
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
