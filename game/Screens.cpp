#include "Screens.h"
#include "SceneFlow.h"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/TextRenderer.h"
#include "../engine/ScreenFade.h"
#include "../engine/AssetManager.h"
#include "../engine/Audio.h"

// Componentes locales -> namespace anonimo (enlace interno), como el resto de escenas.
namespace {

// Dibuja una textura ocupando TODA la salida (pantalla completa). Sin camara.
inline void drawFullscreen(Scene& scene, SDL_Texture* t) {
    if (!t) return;
    SDL_Renderer* r = scene.getRenderer();
    int ow = 0, oh = 0; SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
    SDL_FRect dst{ 0.0f, 0.0f, (float)ow, (float)oh };
    SDL_RenderTexture(r, t, nullptr, &dst);
}

//  Imagen fija en espacio de pantalla: fullscreen (fondo) o centrada por fraccion
class ScreenImage : public Component {
public:
    std::string path;
    bool  fullscreen = false;
    float cxFrac = 0.5f, cyFrac = 0.5f, wFrac = 0.4f; // solo si !fullscreen

    // Carga PEREZOSA: 'path' se asigna despues de addComponent (cuando ya corrio awake),
    void render() override {
        if (!tex && !path.empty()) tex = gameObject->scene->getAssets().loadTexture(path);
        if (!tex) return;
        if (fullscreen) { drawFullscreen(*gameObject->scene, tex); return; }
        SDL_Renderer* r = gameObject->scene->getRenderer();
        int ow = 0, oh = 0; SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
        float tw = 1.0f, th = 1.0f; SDL_GetTextureSize(tex, &tw, &th);
        float w = ow * wFrac, h = w * (th / tw);
        SDL_FRect dst{ ow * cxFrac - w * 0.5f, oh * cyFrac - h * 0.5f, w, h };
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
private:
    SDL_Texture* tex = nullptr;
};

//  Boton de pantalla: dos texturas (normal / hundida). El "sensor" es su rectangulo
class ScreenButton : public Component {
public:
    std::string normalPath, hoverPath;
    float cxFrac = 0.24f, cyFrac = 0.62f, wFrac = 0.24f;
    int   action = 0;        // >0 = SceneFlow::requestScene(action); -1 = salir
    int   hotkey = -1;       // SDL_SCANCODE_* opcional (teclado)

    void update(float) override {
        ensureTex();
        SDL_Renderer* r = gameObject->scene->getRenderer();
        int ow = 0, oh = 0; SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
        computeRect(ow, oh);

        float mx = 0.0f, my = 0.0f;
        Uint32 btn = SDL_GetMouseState(&mx, &my);
        hover = (mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh);

        bool down = (btn & SDL_BUTTON_LMASK) != 0;
        if (hover && down && !prevDown) trigger();
        prevDown = down;

        if (hotkey >= 0) {
            const bool* k = SDL_GetKeyboardState(nullptr);
            bool kd = k[hotkey];
            if (kd && !prevKey) trigger();
            prevKey = kd;
        }
    }
    void render() override {
        ensureTex();
        SDL_Renderer* r = gameObject->scene->getRenderer();
        int ow = 0, oh = 0; SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
        computeRect(ow, oh);
        SDL_Texture* t = (hover && hoverTex) ? hoverTex : normalTex;
        if (t) { SDL_FRect dst{ rx, ry, rw, rh }; SDL_RenderTexture(r, t, nullptr, &dst); }
    }
private:
    SDL_Texture* normalTex = nullptr; SDL_Texture* hoverTex = nullptr;
    bool hover = false, prevDown = false, prevKey = false;
    float rx = 0.0f, ry = 0.0f, rw = 0.0f, rh = 0.0f;

    // Carga perezosa (los paths se fijan tras addComponent/awake).
    void ensureTex() {
        if (!normalTex && !normalPath.empty()) normalTex = gameObject->scene->getAssets().loadTexture(normalPath);
        if (!hoverTex  && !hoverPath.empty())  hoverTex  = gameObject->scene->getAssets().loadTexture(hoverPath);
    }
    void computeRect(int ow, int oh) {
        float tw = 1.0f, th = 1.0f; if (normalTex) SDL_GetTextureSize(normalTex, &tw, &th);
        rw = ow * wFrac; rh = rw * (th / tw);
        rx = ow * cxFrac - rw * 0.5f; ry = oh * cyFrac - rh * 0.5f;
    }
    void trigger() {
        if (action == -1) { SDL_Event e; SDL_zero(e); e.type = SDL_EVENT_QUIT; SDL_PushEvent(&e); }
        else SceneFlow::requestScene(action);
    }
};

//  Cinematica de fotogramas estaticos: fondo a pantalla completa + lineas de texto.
struct CineFrame { std::string img; std::vector<std::string> lines; };

class CinematicCtrl : public Component {
public:
    std::vector<CineFrame> frames;
    std::vector<TextRenderer*> lineTr; // filas de texto (las llena showFrame)
    int nextScene = 0;

    void showFrame(int i) {
        if (i < 0 || i >= (int)frames.size()) return;
        cur = i;
        curTex = gameObject->scene->getAssets().loadTexture(frames[i].img);
        for (size_t j = 0; j < lineTr.size(); ++j)
            lineTr[j]->setText(j < frames[i].lines.size() ? frames[i].lines[j] : std::string());
    }
    void update(float) override {
        const bool* k = SDL_GetKeyboardState(nullptr);
        bool adv = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_RETURN];
        if (adv && !prevAdv) {
            if (cur + 1 < (int)frames.size()) showFrame(cur + 1);
            else SceneFlow::requestScene(nextScene);
        }
        prevAdv = adv;
    }
    void render() override {
        drawFullscreen(*gameObject->scene, curTex);
        // Franja oscura en el tercio inferior para que el texto se lea sobre la imagen.
        SDL_Renderer* r = gameObject->scene->getRenderer();
        int ow = 0, oh = 0; SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
        SDL_FRect bar{ 0.0f, oh * 0.66f, (float)ow, oh * 0.34f };
        SDL_RenderFillRect(r, &bar);
    }
private:
    SDL_Texture* curTex = nullptr;
    int cur = 0;
    bool prevAdv = true; // arranca en true: ignora el ESPACIO aun pulsado de la escena previa
};

// Arma una cinematica generica: controlador (fondo) + filas de texto + hint + fundido.
void buildCinematic(Scene& scene, std::vector<CineFrame> frames, int nextScene) {
    Audio::playMusic("assets/audio/music_cinematic.ogg"); // ambiente de las cinematicas

    GameObject* logic = scene.createGameObject("Cinematic");
    logic->layer = 0; // el fondo se dibuja aqui (capa mas baja)
    auto ctrl = logic->addComponent<CinematicCtrl>();
    ctrl->nextScene = nextScene;
    ctrl->frames = std::move(frames);

    const int MAX_LINES = 6;
    for (int i = 0; i < MAX_LINES; ++i) {
        GameObject* t = scene.createGameObject("Line");
        t->layer = 20; // texto sobre el fondo
        t->transform->x = 0.0f; t->transform->y = (float)i * 30.0f; // apila hacia abajo
        auto tr = t->addComponent<TextRenderer>();
        tr->screenSpace = true; tr->centered = true; tr->anchorX = 0.5f; tr->anchorY = 0.70f;
        tr->pixelSize = 3.0f; tr->setColor(120, 235, 140, 255); // verde fosforo (terminal)
        ctrl->lineTr.push_back(tr);
    }
    ctrl->showFrame(0);

    GameObject* hint = scene.createGameObject("Hint");
    hint->layer = 20; hint->transform->y = -26.0f;
    auto ht = hint->addComponent<TextRenderer>();
    ht->screenSpace = true; ht->centered = true; ht->anchorX = 0.5f; ht->anchorY = 1.0f;
    ht->pixelSize = 2.0f; ht->setColor(180, 180, 180, 200);
    ht->setText("ESPACIO PARA CONTINUAR");

    GameObject* fade = scene.createGameObject("Fade");
    fade->layer = 200;
    fade->addComponent<ScreenFade>()->fadeIn(0.5f);
}

} // namespace (componentes locales)

//  Menu principal
void buildMainMenu(Scene& scene) {
    Audio::playMusic("assets/audio/music_menu.ogg"); // musica del menu principal (en bucle)

    GameObject* bg = scene.createGameObject("MenuBg");
    bg->layer = 0;
    auto bi = bg->addComponent<ScreenImage>();
    bi->path = "assets/redacted/menu/bg.jpg"; bi->fullscreen = true;

    GameObject* logo = scene.createGameObject("Logo");
    logo->layer = 5;
    auto li = logo->addComponent<ScreenImage>();
    li->path = "assets/redacted/menu/logo.png"; li->cxFrac = 0.30f; li->cyFrac = 0.30f; li->wFrac = 0.42f;

    GameObject* play = scene.createGameObject("PlayButton");
    play->layer = 6;
    auto pb = play->addComponent<ScreenButton>();
    pb->normalPath = "assets/redacted/menu/play.png";
    pb->hoverPath  = "assets/redacted/menu/play_pressed.png";
    pb->cxFrac = 0.24f; pb->cyFrac = 0.64f; pb->wFrac = 0.26f;
    pb->action = 10;                 // -> intro
    pb->hotkey = SDL_SCANCODE_SPACE; // tambien con ESPACIO

    GameObject* exit = scene.createGameObject("ExitButton");
    exit->layer = 6;
    auto eb = exit->addComponent<ScreenButton>();
    eb->normalPath = "assets/redacted/menu/exit.png";
    eb->hoverPath  = "assets/redacted/menu/exit_pressed.png";
    eb->cxFrac = 0.24f; eb->cyFrac = 0.83f; eb->wFrac = 0.26f;
    eb->action = -1;                 // salir (ESC tambien sale, gestionado en main)

    GameObject* fade = scene.createGameObject("Fade");
    fade->layer = 200;
    fade->addComponent<ScreenFade>()->fadeIn(0.5f);
}

//  Cinematicas
void buildIntroCinematic(Scene& scene) {
    buildCinematic(scene, {
        { "assets/redacted/scenes/intro_01.jpg", {
            "NEXUS-9 V4.02   KERNEL BOOT: OK",
            "GENETIC REPLICATOR: ACTIVED   BIOMASS: 100%",
            "SPECIMEN-0001: REPLICATION CYCLE INITIATED" } },
        { "assets/redacted/scenes/intro_02.jpg", {
            "ESPECIMEN 0001 INICIALIZADO.",
            "RESPUESTA BIOLOGICA OPTIMA DETECTADA.",
            "CALIBRANDO PARAMETROS MOTRICES." } },
        { "assets/redacted/scenes/intro_03.jpg", {
            "DIRECTIVA: PRESION AMBIENTAL NIVEL 0.",
            "DESPACHANDO CLON A CAMARA 01." } },
    }, 11); // -> entrada Camara 01
}

void buildChamberCinematic(Scene& scene, int chamber) {
    if (chamber == 1)
        buildCinematic(scene, {{ "assets/redacted/scenes/camara_01.jpg", {
            "SUBNIVEL 0   CAMARA 01: EL POZO",
            "PELIGRO: TORRETA MK-II   ZONAS DE NUKE",
            "NEXUS-9: SUPRESION DE BAJA DENSIDAD. MUEVASE." }}}, 1);
    else if (chamber == 2)
        buildCinematic(scene, {{ "assets/redacted/scenes/camara_02.jpg", {
            "SUBNIVEL 0   CAMARA 02: LA TRINCHERA",
            "PELIGRO: LANZALLAMAS   MINAS",
            "NEXUS-9: EVITE LA COBERTURA INMOVIL." }}}, 2);
    else
        buildCinematic(scene, {{ "assets/redacted/scenes/camara_03.jpg", {
            "SUBNIVEL 0   CAMARA 03: EL SUELO DE MATANZA",
            "PELIGRO: CONTINGENCIA SIGMA",
            "NEXUS-9: TASA DE SUPERVIVENCIA MINIMA." }}}, 3);
}

void buildBossCinematic(Scene& scene) {
    buildCinematic(scene, {{ "assets/redacted/scenes/boss_intro.jpg", {
        "PROTOCOLO HERCULES-1   CALIBRACION MAXIMA",
        "NEXUS-9: INICIANDO ERRADICACION FISICA." }}}, 4);
}

void buildVictoryCinematic(Scene& scene) {
    buildCinematic(scene, {
        { "assets/redacted/scenes/victory_01.jpg", {
            "SOBRECARGA COMPLETA   HERCULES-1: INACTIVO",
            "NEXUS-9: UNIDAD TERMICA FUERA DE SERVICIO." } },
        { "assets/redacted/scenes/victory_02.jpg", {
            "SECTOR 1 SUPERADO   ABRIENDO SECTOR 2",
            "NEXUS-9: ACCESO PERMITIDO. ADAPTABILIDAD SUPERIOR." } },
        { "assets/redacted/scenes/victory_03.jpg", {
            "REGISTRO NEURONAL SIGMA-1 RESPALDADO.",
            "PREPARANDO LOTE 0002 PARA EL SECTOR 2." } },
    }, 0); // -> menu
}
