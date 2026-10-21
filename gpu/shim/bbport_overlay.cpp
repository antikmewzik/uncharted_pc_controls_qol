// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_overlay.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include "bbport_keyicons.h"
#include "bbport_settings.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

// DejaVu Sans (Cyrillic), embedded (third_party/fonts, Bitstream Vera license).
#ifdef _WIN32
asm(".section .rdata,\"dr\"\n"
    ".balign 16\n"
    ".global bb_font_ttf\n"
    "bb_font_ttf:\n"
    ".incbin \"" BB_FONT_PATH "\"\n"
    ".global bb_font_ttf_end\n"
    "bb_font_ttf_end:\n"
    ".text\n");
#else
asm(".section .rodata\n"
    ".balign 16\n"
    ".hidden bb_font_ttf\n"
    ".global bb_font_ttf\n"
    "bb_font_ttf:\n"
    ".incbin \"" BB_FONT_PATH "\"\n"
    ".hidden bb_font_ttf_end\n"
    ".global bb_font_ttf_end\n"
    "bb_font_ttf_end:\n"
    ".previous\n");
#endif
extern "C" const unsigned char bb_font_ttf[];
extern "C" const unsigned char bb_font_ttf_end[];

extern "C" void runtime_restart(void); // bb-probe (probe.c)

namespace BbOverlay {

namespace {

std::mutex imgui_mutex; // the ImGui context: window thread (input) and present thread
bool initialized = false;
std::atomic<bool> menu_open{false};
bool l3_down = false, r3_down = false;
bool dirty = false; // settings changed while open: saved on close
// The game's text dialog (SetTextEntry), guarded by imgui_mutex.
bool text_entry_active = false;
std::string text_entry_prompt, text_entry_text;
float base_scale = 1.0f;

// Present rate for the FPS counter.
std::chrono::steady_clock::time_point last_present{};
float frame_ms_avg = 0.0f, last_present_ms = 0.0f;

// ---- the keyboard and mouse bindings window ("Touches clavier / souris", opened from the game's Commandes menu) ----
// It edits input.ini (BB_INPUT_CONFIG) line by line, keeps every other line, and tells the pad code to read it again.
std::atomic<bool> controls_open{false};
std::atomic<bool> controls_changed{false};
std::vector<std::string> ini_lines;
int capture_index = -1, capture_slot = 0; // the binding being captured: index into ControlActions, slot 0 or 1

// The window speaks the game's language (the four of the dump): 0 English, 1 French, 2 Spanish (Latin America), 3 Brazilian
// Portuguese. src/runtime_pad.c finds it from the text of the "Keyboard/Mouse" row and tells it through bbgpu_set_language.
std::atomic<int> ui_language{1};
const char* T(const char* en, const char* fr, const char* es, const char* pt) {
    switch (ui_language.load()) {
    case 0: return en;
    case 2: return es;
    case 3: return pt;
    default: return fr;
    }
}

struct ControlAction {
    const char* name;     // input.ini: key.<name>
    const char* label[4]; // what the key does in the game (as on the controller diagram), in the four languages: en, fr, es, pt
    const char* fallback; // default keys when the file has none
};
constexpr ControlAction ControlActions[] = {
    {"ls_up", {"Move forward", "Avancer", "Avanzar", "Avançar"}, "W"},
    {"ls_down", {"Move back", "Reculer", "Retroceder", "Recuar"}, "S"},
    {"ls_left", {"Move left", "Aller à gauche", "Ir a la izquierda", "Ir para a esquerda"}, "A"},
    {"ls_right", {"Move right", "Aller à droite", "Ir a la derecha", "Ir para a direita"}, "D"},
    {"rs_up", {"Camera up (without the mouse)", "Caméra vers le haut (sans souris)", "Cámara hacia arriba (sin mouse)",
               "Câmera para cima (sem mouse)"}, "I"},
    {"rs_down", {"Camera down (without the mouse)", "Caméra vers le bas (sans souris)", "Cámara hacia abajo (sin mouse)",
                 "Câmera para baixo (sem mouse)"}, "K"},
    {"rs_left", {"Camera left (without the mouse)", "Caméra vers la gauche (sans souris)", "Cámara hacia la izquierda (sin mouse)",
                 "Câmera para a esquerda (sem mouse)"}, "J"},
    {"rs_right", {"Camera right (without the mouse)", "Caméra vers la droite (sans souris)", "Cámara hacia la derecha (sin mouse)",
                  "Câmera para a direita (sem mouse)"}, "L"},
    {"cross", {"Jump / Climb", "Sauter / Escalader", "Saltar / Escalar", "Pular / Escalar"}, "Space"},
    {"circle", {"Take cover / Roll / Drop down", "À couvert / Rouler / Descendre", "Cubrirse / Rodar / Descender",
                "Abrigar-se / Rolar / Descer"}, "C"},
    {"square", {"Melee", "Combat rapproché", "Combate cuerpo a cuerpo", "Combate corpo a corpo"}, "F"},
    {"triangle", {"Interact / Pick up", "Interagir / Ramasser", "Interactuar / Recoger", "Interagir / Pegar"}, "E"},
    {"l1", {"Grenade / Hold to aim", "Grenade / Maintenir pour viser", "Granada / Mantener para apuntar",
            "Granada / Segurar para mirar"}, "G"},
    {"r1", {"Reload", "Recharger", "Recargar", "Recarregar"}, "R"},
    {"l2", {"Aim weapon", "Viser avec l'arme", "Apuntar con el arma", "Mirar com a arma"}, "Mouse Right"},
    {"r2", {"Fire weapon", "Tirer avec l'arme", "Disparar el arma", "Atirar com a arma"}, "Mouse Left"},
    {"l3", {"Switch shoulder while aiming", "Changer d'épaule en visant", "Cambiar de hombro al apuntar",
            "Trocar de ombro ao mirar"}, "Left Shift"},
    {"r3", {"Reset camera", "Réinitialiser la caméra", "Reiniciar la cámara", "Redefinir a câmera"}, "V"},
    {"walk", {"Walk (hold with the movement keys)", "Marcher (à tenir avec les touches de déplacement)",
              "Caminar (mantener con las teclas de movimiento)", "Andar (segurar com as teclas de movimento)"}, ""},
    {"options", {"Pause", "Pause", "Pausa", "Pausa"}, "Escape|Return"},
    {"touchpad", {"Journal", "Journal", "Diario", "Diário"}, "Tab"},
    {"up", {"Look", "Regarder", "Mirar", "Olhar"}, "Up"},
    {"down", {"D-pad down (photo mode removed)", "Croix du bas (mode photo retiré)", "Cruceta abajo (modo foto retirado)",
              "Direcional para baixo (modo foto removido)"}, "Down"},
    {"left", {"Weapon 1 (sidearm)", "Arme 1 (arme de poing)", "Arma 1 (arma corta)", "Arma 1 (arma secundária)"}, "1|Wheel Up"},
    {"right", {"Weapon 2 (main weapon)", "Arme 2 (arme principale)", "Arma 2 (arma principal)", "Arma 2 (arma principal)"}, "2|Wheel Down"},
};
const char* ActionLabel(const ControlAction& action) {
    return action.label[ui_language.load()];
}
constexpr int ControlActionCount = int(sizeof(ControlActions) / sizeof(ControlActions[0]));

std::string IniPath() {
    const char* env = std::getenv("BB_INPUT_CONFIG");
    return env && env[0] ? env : "";
}

void IniLoad() {
    ini_lines.clear();
    const std::string path = IniPath();
    if (path.empty()) {
        return;
    }
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        ini_lines.push_back(line);
    }
}

std::string IniGet(const std::string& key, const std::string& fallback) {
    const std::string prefix = key + "=";
    for (const std::string& line : ini_lines) {
        if (line.compare(0, prefix.size(), prefix) == 0) {
            return line.substr(prefix.size());
        }
    }
    return fallback;
}

void IniSet(const std::string& key, const std::string& value) {
    const std::string prefix = key + "=";
    for (std::string& line : ini_lines) {
        if (line.compare(0, prefix.size(), prefix) == 0) {
            line = prefix + value;
            return;
        }
    }
    ini_lines.push_back(prefix + value);
}

/// Writes the file (a temporary one, then renamed over it) and tells the others.
void IniSave() {
    const std::string path = IniPath();
    if (path.empty()) {
        std::printf("Overlay: BB_INPUT_CONFIG is not set, the bindings cannot be saved\n");
        return;
    }
    const std::string temporary = path + ".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        for (const std::string& line : ini_lines) {
            out << line << "\n";
        }
    }
    std::remove(path.c_str());
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::printf("Overlay: could not write %s\n", path.c_str());
        return;
    }
    controls_changed = true;
    BbKeyIcons::ForceRepaint();
}

std::string BindingSlot(const std::string& value, int slot) {
    const auto bar = value.find('|');
    if (slot == 0) {
        return value.substr(0, bar);
    }
    return bar == std::string::npos ? std::string{} : value.substr(bar + 1);
}

void SetBinding(int index, int slot, const std::string& name) {
    const ControlAction& action = ControlActions[index];
    const std::string current = IniGet(std::string("key.") + action.name, action.fallback);
    std::string first = BindingSlot(current, 0), second = BindingSlot(current, 1);
    (slot == 0 ? first : second) = name;
    if (first.empty()) {
        first = second;
        second.clear();
    }
    IniSet(std::string("key.") + action.name, second.empty() ? first : first + "|" + second);
    IniSave();
}

/// The binding name for an SDL mouse button, "" for the ones the game cannot use.
std::string MouseButtonName(int button) {
    switch (button) {
    case SDL_BUTTON_LEFT: return "Mouse Left";
    case SDL_BUTTON_RIGHT: return "Mouse Right";
    case SDL_BUTTON_MIDDLE: return "Mouse Middle";
    case SDL_BUTTON_X1: return "Mouse 4";
    case SDL_BUTTON_X2: return "Mouse 5";
    default: return {};
    }
}

void SetOpen(bool value);

void CloseControls() {
    controls_open = false;
    capture_index = -1;
    SetOpen(false);
}

void ControlsWindow() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(780.0f * base_scale, std::min(720.0f * base_scale, viewport->WorkSize.y * 0.92f)),
                             ImGuiCond_Appearing);
    bool keep_open = true;
    const std::string window_title = std::string(T("Keyboard / Mouse", "Touches clavier / souris", "Teclado / Mouse", "Teclado / Mouse")) + "###keysmouse";
    if (!ImGui::Begin(window_title.c_str(), &keep_open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!keep_open) {
            CloseControls();
        }
        return;
    }
    ImGui::TextWrapped("%s", T("Click a key, then press the new key, a mouse button or the wheel. Esc cancels. The × button clears a key. Changes are saved at once.",
                               "Cliquez sur une touche, puis appuyez sur la nouvelle touche, un bouton de la souris ou la molette. Échap annule. Le bouton × efface une touche. Les changements sont enregistrés tout de suite.",
                               "Haz clic en una tecla y luego pulsa la nueva tecla, un botón del mouse o la rueda. Esc cancela. El botón × borra una tecla. Los cambios se guardan al instante.",
                               "Clique em uma tecla e depois pressione a nova tecla, um botão do mouse ou a roda. Esc cancela. O botão × apaga uma tecla. As alterações são salvas na hora."));
    if (IniPath().empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", T("The keys file was not found: nothing will be saved.", "Le fichier des touches est introuvable : rien ne sera enregistré.", "No se encuentra el archivo de teclas: no se guardará nada.", "O arquivo das teclas não foi encontrado: nada será salvo."));
    }
    ImGui::Separator();
    if (ImGui::BeginTable("##bindings", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH,
                          ImVec2(0.0f, ImGui::GetContentRegionAvail().y - 150.0f * base_scale))) {
        ImGui::TableSetupColumn(T("Action", "Action", "Acción", "Ação"), ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableSetupColumn(T("Key 1", "Touche 1", "Tecla 1", "Tecla 1"), ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn(T("Key 2", "Touche 2", "Tecla 2", "Tecla 2"), ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (int i = 0; i < ControlActionCount; ++i) {
            const ControlAction& action = ControlActions[i];
            const std::string value = IniGet(std::string("key.") + action.name, action.fallback);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(ActionLabel(action));
            for (int slot = 0; slot < 2; ++slot) {
                ImGui::TableSetColumnIndex(1 + slot);
                ImGui::PushID(i * 2 + slot);
                const bool capturing = capture_index == i && capture_slot == slot;
                const std::string name = BindingSlot(value, slot);
                const std::string text = capturing ? std::string(T("Press a key…", "Appuyez sur une touche…", "Pulsa una tecla…", "Pressione uma tecla…"))
                                                   : name.empty() ? std::string(T("(none)", "(aucune)", "(ninguna)", "(nenhuma)")) : name;
                const float clear_width = ImGui::GetFrameHeight();
                if (ImGui::Button((text + "###bind").c_str(), ImVec2(ImGui::GetContentRegionAvail().x - clear_width - 4.0f, 0.0f))) {
                    capture_index = i;
                    capture_slot = slot;
                }
                ImGui::SameLine();
                if (ImGui::Button("×", ImVec2(clear_width, 0.0f)) && !name.empty()) {
                    SetBinding(i, slot, "");
                    capture_index = -1;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText(T("Mouse", "Souris", "Mouse", "Mouse"));
    float sensitivity = float(std::atof(IniGet("mouse_sensitivity", "1.0").c_str()));
    float aim_sensitivity = float(std::atof(IniGet("mouse_aim_sensitivity", "1.0").c_str()));
    bool invert_y = std::atoi(IniGet("mouse_invert_y", "0").c_str()) != 0;
    bool mouse_look = std::atoi(IniGet("mouse_look", "1").c_str()) != 0;
    bool free_smoothing = std::atoi(IniGet("free_view_smoothing", "0").c_str()) != 0;
    bool aim_raw = std::atoi(IniGet("aim_raw", "0").c_str()) != 0;
    bool changed = false;
    if (ImGui::SliderFloat(T("Sensitivity", "Sensibilité", "Sensibilidad", "Sensibilidade"), &sensitivity, 0.1f, 5.0f, "%.2f")) {
        char text[32];
        std::snprintf(text, sizeof(text), "%.2f", sensitivity);
        IniSet("mouse_sensitivity", text);
        changed = true;
    }
    if (ImGui::SliderFloat(T("Sensitivity while aiming", "Sensibilité en visant", "Sensibilidad al apuntar", "Sensibilidade ao mirar"), &aim_sensitivity, 0.1f, 5.0f, "%.2f")) {
        char text[32];
        std::snprintf(text, sizeof(text), "%.2f", aim_sensitivity);
        IniSet("mouse_aim_sensitivity", text);
        changed = true;
    }
    if (ImGui::Checkbox(T("Invert the mouse vertical axis", "Inverser l'axe vertical de la souris", "Invertir el eje vertical del mouse", "Inverter o eixo vertical do mouse"), &invert_y)) {
        IniSet("mouse_invert_y", invert_y ? "1" : "0");
        changed = true;
    }
    if (ImGui::Checkbox(T("Look with the mouse", "Regarder avec la souris", "Mirar con el mouse", "Olhar com o mouse"), &mouse_look)) {
        IniSet("mouse_look", mouse_look ? "1" : "0");
        changed = true;
    }
    if (ImGui::Checkbox(T("Smooth the free camera (game's own smoothing)", "Lisser la vue libre (lissage du jeu)",
                          "Suavizar la cámara libre (suavizado del juego)", "Suavizar a câmera livre (suavização do jogo)"), &free_smoothing)) {
        IniSet("free_view_smoothing", free_smoothing ? "1" : "0");
        changed = true;
    }
    if (ImGui::Checkbox(T("Raw aiming (no smoothing)", "Visée brute (sans lissage)", "Apuntado sin procesar (sin suavizado)",
                          "Mira bruta (sem suavização)"), &aim_raw)) {
        IniSet("aim_raw", aim_raw ? "1" : "0");
        changed = true;
    }
    ImGui::TextDisabled("%s", T("Raw aiming removes the dynamic smoothing: slow mouse movements will show stepping.",
                                "La visée brute supprime le lissage dynamique : les mouvements lents de la souris feront du stepping (paliers).",
                                "El apuntado sin procesar quita el suavizado dinámico: los movimientos lentos del mouse irán a saltos.",
                                "A mira bruta remove a suavização dinâmica: movimentos lentos do mouse ficarão em degraus."));
    if (changed) {
        IniSave();
    }
    ImGui::Separator();
    if (ImGui::Button(T("Defaults", "Valeurs par défaut", "Valores predeterminados", "Padrões"))) {
        for (const ControlAction& action : ControlActions) {
            IniSet(std::string("key.") + action.name, action.fallback);
        }
        IniSave();
        capture_index = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button(T("Close", "Fermer", "Cerrar", "Fechar")) || !keep_open) {
        ImGui::End();
        CloseControls();
        return;
    }
    ImGui::End();
}

void SetOpen(bool value) {
    if (menu_open.exchange(value) == value) {
        return;
    }
    if (!value) {
        controls_open = false;
        capture_index = -1;
    }
    ImGui::GetIO().MouseDrawCursor = value;
    if (!value && dirty) {
        dirty = false;
        BbSettings::Save();
    }
}

ImGuiKey KeyFromSdl(SDL_Keycode key) {
    switch (key) {
    case SDLK_TAB: return ImGuiKey_Tab;
    case SDLK_LEFT: return ImGuiKey_LeftArrow;
    case SDLK_RIGHT: return ImGuiKey_RightArrow;
    case SDLK_UP: return ImGuiKey_UpArrow;
    case SDLK_DOWN: return ImGuiKey_DownArrow;
    case SDLK_PAGEUP: return ImGuiKey_PageUp;
    case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
    case SDLK_HOME: return ImGuiKey_Home;
    case SDLK_END: return ImGuiKey_End;
    case SDLK_DELETE: return ImGuiKey_Delete;
    case SDLK_BACKSPACE: return ImGuiKey_Backspace;
    case SDLK_SPACE: return ImGuiKey_Space;
    case SDLK_RETURN: return ImGuiKey_Enter;
    case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
    case SDLK_ESCAPE: return ImGuiKey_Escape;
    case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
    case SDLK_RCTRL: return ImGuiKey_RightCtrl;
    case SDLK_LSHIFT: return ImGuiKey_LeftShift;
    case SDLK_RSHIFT: return ImGuiKey_RightShift;
    case SDLK_LALT: return ImGuiKey_LeftAlt;
    case SDLK_RALT: return ImGuiKey_RightAlt;
    default: return ImGuiKey_None;
    }
}

ImGuiKey KeyFromGamepad(u8 button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return ImGuiKey_GamepadFaceDown;
    case SDL_GAMEPAD_BUTTON_EAST: return ImGuiKey_GamepadFaceRight;
    case SDL_GAMEPAD_BUTTON_WEST: return ImGuiKey_GamepadFaceLeft;
    case SDL_GAMEPAD_BUTTON_NORTH: return ImGuiKey_GamepadFaceUp;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return ImGuiKey_GamepadDpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return ImGuiKey_GamepadDpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return ImGuiKey_GamepadDpadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return ImGuiKey_GamepadDpadRight;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return ImGuiKey_GamepadL1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return ImGuiKey_GamepadR1;
    case SDL_GAMEPAD_BUTTON_START: return ImGuiKey_GamepadStart;
    case SDL_GAMEPAD_BUTTON_BACK: return ImGuiKey_GamepadBack;
    default: return ImGuiKey_None;
    }
}

float PixelDensity(SDL_WindowID id) {
    SDL_Window* window = SDL_GetWindowFromID(id);
    const float density = window ? SDL_GetWindowPixelDensity(window) : 1.0f;
    return density > 0.0f ? density : 1.0f;
}

// Marks the settings dirty when a widget changed them.
template <typename T>
void Store(std::atomic<T>& target, T value, bool changed) {
    if (changed) {
        target = value;
        dirty = true;
    }
}

void Checkbox(const char* label, std::atomic<bool>& value) {
    bool v = value;
    Store(value, v, ImGui::Checkbox(label, &v));
}

void Slider(const char* label, std::atomic<float>& value, float lo, float hi) {
    float v = value;
    Store(value, v, ImGui::SliderFloat(label, &v, lo, hi, "%.2f"));
}

void Hint(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void Menu() {
    if (controls_open) {
        ControlsWindow();
        return;
    }
    auto& s = BbSettings::Get();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 40.0f * base_scale,
                                   viewport->WorkPos.y + 40.0f * base_scale),
                            ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(620.0f * base_scale, 0.0f), ImGuiCond_Appearing);
    bool keep_open = true;
    if (!ImGui::Begin("Bloodborne — настройки  (Insert / L3+R3)", &keep_open,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGui::Text("%.0f FPS  (%.1f мс)", frame_ms_avg > 0.0f ? 1000.0f / frame_ms_avg : 0.0f,
                frame_ms_avg);

    ImGui::SeparatorText("Временной апскейлер");
    static const char* upscalers[] = {"Выкл", "FSR 3.1", "FSR 4 (INT8)", "FSR 4.1.1 (INT8)",
                                     "TAA (нативное сглаживание)", "DLSS (NVIDIA RTX)"};
    static const char* later[] = {"XeSS"};
    int upscaler = s.upscaler;
    if (ImGui::BeginCombo("Апскейлер", upscalers[upscaler])) {
        for (int i = 0; i < BbSettings::UpscalerCount; ++i) {
            const bool supported = i == BbSettings::UpscalerFsr4 ? s.fsr4_supported.load()
                : i == BbSettings::UpscalerFsr411 ? s.fsr411_supported.load()
                : i == BbSettings::UpscalerDlss   ? s.dlss_supported.load() : true;
            ImGui::BeginDisabled(!supported);
            if (ImGui::Selectable(upscalers[i], i == upscaler)) {
                Store(s.upscaler, i, true);
            }
            ImGui::EndDisabled();
            if (!supported) {
                ImGui::SameLine();
                ImGui::TextDisabled("— не поддерживается видеокартой");
            }
        }
        for (const char* name : later) {
            ImGui::BeginDisabled();
            ImGui::Selectable(name, false);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("— в работе");
        }
        ImGui::EndCombo();
    }
    if (const char* problem = s.dlss_problem.load(); problem && upscaler == BbSettings::UpscalerDlss) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "DLSS: %s", problem);
    }
    if (const char* problem = s.fsr4_problem.load()) {
        ImGui::PushTextWrapPos();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "FSR 4 недоступен: %s", problem);
        if (!BbSettings::IsFsr4(s.upscaler))
            ImGui::TextUnformatted("Активен режим, выбранный выше. FSR 4 можно выбрать снова.");
        ImGui::PopTextWrapPos();
    }
    if (BbSettings::IsFsr4(s.upscaler)) {
        if (s.upscaler == BbSettings::UpscalerFsr411) {
            Hint("FSR 4.1.1 в режиме INT8: модель из DLL AMD 4.1.1, воспроизведённая в Vulkan "
                 "(результат совпадает с DLL). Одна модель для Native..Performance и отдельная "
                 "для Ultra Performance. Ассеты: tools/fsr4cap/build_assets.sh (нужны DLL и Proton).");
        } else {
            Hint("FSR 4 в режиме INT8 (модель v07 из исходников AMD FidelityFX SDK). Качество выше, "
                 "чем у FSR 3.1, но проход тяжелее. Смена пресета пересобирает модель (короткая "
                 "пауза). Ассеты: tools/fetch_fsr4_assets.sh.");
        }
        Checkbox("FSR 4: авто-экспозиция", s.fsr4_auto_exposure);
        Checkbox("FSR 4: обратный знак jitter", s.fsr4_invert_jitter);
        Hint("Проверка при гостинге: сеть FSR 4 нормирует цвет по экспозиции и по ней решает, "
             "когда отбросить прошлые кадры. Меняются сразу, без перезапуска.");
    }
    const bool upscaler_on = s.upscaler != BbSettings::UpscalerOff;
    const bool taa = s.upscaler == BbSettings::UpscalerTaa;
    ImGui::BeginDisabled(!upscaler_on);
    ImGui::BeginDisabled(taa);
    int preset = taa ? BbSettings::NativeAA : s.preset.load();
    char preset_label[64];
    std::snprintf(preset_label, sizeof(preset_label), "%s (x%.1f)", BbSettings::PresetName(preset),
                  BbSettings::PresetScale(preset));
    if (ImGui::BeginCombo("Пресет", preset_label)) {
        for (int i = 0; i < BbSettings::PresetCount; ++i) {
            char label[64];
            const float scale = BbSettings::PresetScale(i);
            const int output = s.output_res;
            std::snprintf(label, sizeof(label), "%s (x%.1f, рендер %dx%d)",
                          BbSettings::PresetName(i), scale,
                          int(std::lround(BbSettings::OutputWidths[output] / scale / 2) * 2),
                          int(std::lround(BbSettings::OutputHeights[output] / scale / 2) * 2));
            if (ImGui::Selectable(label, i == preset)) {
                Store(s.preset, i, true);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (taa) {
        ImGui::TextWrapped("TAA сглаживает сцену в разрешении вывода, без модели FSR и апскейлинга. "
                           "Сохранённый пресет FSR восстановится при выборе FSR.");
    }
    ImGui::Text("Активный рендер сцены: %d x %d", s.active_render_width.load(),
                s.active_render_height.load());
    if (BbSettings::FixedRenderSession()) {
        ImGui::Text("Пресет при запуске: %s", BbSettings::PresetName(s.startup_preset));
        if (const char* automatic = std::getenv("BB_AUTO_RENDER_RES");
            automatic && automatic[0] == '1') {
            Hint("При выводе не 1080p вся игра рисуется в разрешении пресета (патч при запуске): "
                 "это быстрее всего на Steam Deck и слабых GPU. Смена пресета или разрешения "
                 "вывода — после перезапуска. Пункт «Смена разрешения на лету» ниже включает "
                 "смену без перезапуска (постобработка тогда остаётся в 1080p, медленнее).");
        } else {
            Hint("BB_RENDER_RES фиксирует размер сцены при запуске. Уберите эту явную переменную "
                 "для смены разрешения и пресетов без перезапуска игры.");
        }
    } else {
        Hint("Native AA: FSR работает как сглаживание. Остальные пресеты уменьшают разрешение "
             "отрисовки сцены относительно вывода. Интерфейс рисуется в разрешении вывода. "
             "Пресет применяется со следующего кадра без перезапуска игры.");
    }
    Checkbox("Резкость (RCAS)", s.sharpen);
    ImGui::BeginDisabled(!s.sharpen);
    Slider("Сила резкости", s.sharpness, 0.0f, 2.0f);
    Hint("До 1 — резкость самого апскейлера (RCAS). Выше 1 добавляется ещё один проход RCAS. "
         "Ctrl+клик по ползунку — ввести точное значение.");
    ImGui::EndDisabled();
    Checkbox("Субпиксельный сдвиг (jitter)", s.jitter);
    Hint("Каждый кадр сцена сдвигается на долю пикселя, и апскейлер собирает из нескольких "
         "кадров больше деталей. Без него получается только сглаживание по истории.");

    ImGui::SeparatorText("Маска реактивности");
    ImGui::BeginDisabled(taa);
    Checkbox("Включить маску", s.reactive);
    Hint("Помечает прозрачные эффекты (частицы, дымку), чтобы апскейлер меньше опирался на "
         "прошлые кадры. Меньше шлейфов за эффектами, но под ними возвращается дрожание.");
    ImGui::BeginDisabled(!s.reactive);
    Slider("Масштаб", s.reactive_scale, 0.0f, 4.0f);
    Slider("Порог", s.reactive_threshold, 0.0f, 1.0f);
    Slider("Максимум", s.reactive_max, 0.0f, 1.0f);
    bool show_mask = s.debug_view == BbSettings::DebugReactive;
    if (ImGui::Checkbox("Показать маску (отладка)", &show_mask)) {
        s.debug_view = show_mask ? BbSettings::DebugReactive : BbSettings::DebugNone;
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    Checkbox("Векторы движения персонажей", s.object_motion);
    Hint("Точные векторы для анимированных объектов: одежда и оружие меньше рассыпаются "
         "при движении. Статичная сцена не получает дополнительный проход. "
         "Изменение применяется после перезапуска игры.");
    bool show_motion = s.debug_view == BbSettings::DebugMotion;
    if (ImGui::Checkbox("Показать векторы движения (отладка)", &show_motion)) {
        s.debug_view = show_motion ? BbSettings::DebugMotion : BbSettings::DebugNone;
    }
    Hint("Красный/зелёный: движение по горизонтали/вертикали (8 пикселей = полная яркость). "
         "Синий: пиксель получил точный вектор объекта, а не только движение камеры. "
         "Движущийся предмет без синего и без красного/зелёного апскейлер считает "
         "неподвижным, отсюда шлейф.");
    ImGui::EndDisabled(); // upscaler off

    ImGui::SeparatorText("Генерация кадров");
    static const char* frame_gens[] = {"Выкл", "FSR 3 (любая видеокарта)",
                                       "DLSS (NVIDIA RTX 40/50)"};
    const int frame_gen = s.frame_gen;
    if (ImGui::BeginCombo("Генерация кадров", frame_gens[frame_gen])) {
        for (int i = 0; i < BbSettings::FrameGenCount; ++i) {
            const bool supported = i != BbSettings::FrameGenDlss || s.dlssg_max_multiplier > 1;
            ImGui::BeginDisabled(!supported);
            if (ImGui::Selectable(frame_gens[i], i == frame_gen)) {
                Store(s.frame_gen, i, true);
            }
            ImGui::EndDisabled();
            if (!supported) {
                ImGui::SameLine();
                ImGui::TextDisabled("— не поддерживается видеокартой");
            }
        }
        ImGui::EndCombo();
    }
    Hint("Кадры между двумя кадрами игры: 60 FPS на экране становятся 120. Нужны представление "
         "Mailbox или Immediate и монитор с такой частотой. Немного добавляет задержку ввода. "
         "Меню и видеоролики не генерируются.");
    const int max_multiplier = s.frame_gen == BbSettings::FrameGenDlss ? s.dlssg_max_multiplier.load() : 2;
    ImGui::BeginDisabled(s.frame_gen == BbSettings::FrameGenOff || max_multiplier <= 2);
    int multiplier = std::min(s.frame_gen_multiplier.load(), max_multiplier);
    if (ImGui::SliderInt("Множитель", &multiplier, 2, std::max(max_multiplier, 2), "%dx")) {
        Store(s.frame_gen_multiplier, multiplier, true);
    }
    ImGui::EndDisabled();
    if (const char* problem = s.frame_gen_problem.load()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", problem);
    }

    ImGui::SeparatorText("Разрешение вывода");
    static const char* outputs[] = {"1280 x 720", "1920 x 1080", "2560 x 1440", "3840 x 2160"};
    int output = s.output_res;
    if (ImGui::BeginCombo("Разрешение вывода", outputs[output])) {
        for (int i = 0; i < BbSettings::OutputCount; ++i) {
            if (ImGui::Selectable(outputs[i], i == output)) {
                Store(s.output_res, i, true);
            }
        }
        ImGui::EndCombo();
    }
    if (BbSettings::FixedRenderSession()) {
        Hint("Размер готового кадра и интерфейса. Пресет задаёт размер сцены относительно "
             "вывода: 4K Performance = 1920x1080. Применяется после перезапуска игры.");
    } else {
        Hint("Размер готового кадра и интерфейса меняется на границе следующего кадра. "
             "Пресет задаёт размер сцены относительно вывода: 4K Performance = 1920x1080. "
             "Смена размера сбрасывает историю FSR и может вызвать короткую паузу.");
    }
    static const char* live_modes[] = {"Авто (по видеокарте)", "Выключена (быстрее)", "Включена"};
    int live = s.live_resolution + 1;
    if (ImGui::BeginCombo("Смена разрешения на лету", live_modes[live])) {
        for (int i = 0; i < 3; ++i) {
            if (ImGui::Selectable(live_modes[i], i == live)) {
                Store(s.live_resolution, i - 1, true);
            }
        }
        ImGui::EndCombo();
    }
    Hint("Включена: разрешение вывода и пресет меняются без перезапуска, но постобработка игры "
         "остаётся в 1080p — на Steam Deck и старых видеокартах это заметно медленнее. "
         "Выключена: всё рисуется в разрешении пресета, смена — через перезапуск. Авто включает "
         "её на мощных дискретных видеокартах. Применяется после перезапуска игры.");
    // No game-effect, cheat or model-detail toggles here: those were Bloodborne patches
    // (scripts/patches.py, patches/Bloodborne.xml), which the Uncharted launch never applies.
    bool restart = s.object_motion != s.startup_object_motion ||
                   s.live_resolution != s.startup_live_resolution ||
                   BbSettings::ResolutionNeedsRestart();
    if (restart) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f),
                           "Изменения применятся после перезапуска игры");
        if (ImGui::Button("Применить и перезапустить игру")) {
            BbSettings::Save();
            runtime_restart();
        }
    }

    ImGui::SeparatorText("Прочее");
    Checkbox("Счётчик FPS в углу", s.show_fps);

    ImGui::Spacing();
    if (ImGui::Button("Закрыть")) {
        keep_open = false;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Настройки сохраняются в bbport.ini");
    ImGui::End();
    if (!keep_open) {
        SetOpen(false);
    }
}

// The game's text dialog: what is typed, and how to finish (keyboard or controller).
void TextEntryBox() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.42f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(viewport->WorkSize.x * 0.32f, 0.0f),
                                        ImVec2(viewport->WorkSize.x * 0.8f, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("##text_entry", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::TextColored(ImVec4(0.85f, 0.72f, 0.45f, 1.0f), "%s", text_entry_prompt.c_str());
    ImGui::Separator();
    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("%s_", text_entry_text.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::TextDisabled("Keyboard: type, Backspace to erase, Enter = OK, Esc = cancel");
    ImGui::TextDisabled("Controller: Cross (A) = OK, Circle (B) = cancel");
    ImGui::End();
}

void FpsCounter() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float pad = 12.0f * base_scale;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - pad,
                                   viewport->WorkPos.y + pad),
                            ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.5f);
    ImGui::Begin("##fps", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    const auto& s = BbSettings::Get();
    ImGui::Text("%.0f FPS  %.1f мс  %s", frame_ms_avg > 0.0f ? 1000.0f / frame_ms_avg : 0.0f,
                frame_ms_avg,
                s.upscaler == BbSettings::UpscalerFsr3   ? "FSR 3.1"
                : s.upscaler == BbSettings::UpscalerFsr4 ? "FSR 4"
                : s.upscaler == BbSettings::UpscalerFsr411 ? "FSR 4.1.1"
                : s.upscaler == BbSettings::UpscalerTaa ? "TAA"
                : s.upscaler == BbSettings::UpscalerDlss ? "DLSS"
                                                         : "");
    ImGui::End();
}

} // namespace

void Init(const Vulkan::Instance& instance, vk::Format format, u32 image_count) {
    std::scoped_lock lock{imgui_mutex};
    if (initialized) {
        return;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // window positions are not kept
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.BackendPlatformName = "bbport";

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg].w = 0.92f;

    ImFontConfig font_config;
    font_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(bb_font_ttf),
                                   int(bb_font_ttf_end - bb_font_ttf), 18.0f, &font_config);

    const vk::Instance vk_instance = instance.GetInstance();
    ImGui_ImplVulkan_LoadFunctions(
        instance.ApiVersion(),
        [](const char* name, void* user) {
            return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
                *static_cast<const vk::Instance*>(user), name);
        },
        const_cast<vk::Instance*>(&vk_instance));

    const VkFormat color_format = static_cast<VkFormat>(format);
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = instance.ApiVersion();
    info.Instance = vk_instance;
    info.PhysicalDevice = instance.GetPhysicalDevice();
    info.Device = instance.GetDevice();
    info.QueueFamily = instance.GetGraphicsQueueFamilyIndex();
    info.Queue = instance.GetGraphicsQueue();
    info.DescriptorPoolSize = 16;
    info.MinImageCount = std::max(image_count, 2u);
    info.ImageCount = std::max(image_count, 2u);
    info.UseDynamicRendering = true;
    info.PipelineInfoMain.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
    };
    if (!ImGui_ImplVulkan_Init(&info)) {
        std::printf("Overlay: ImGui Vulkan backend init failed\n");
        ImGui::DestroyContext();
        return;
    }
    initialized = true;
    std::printf("Overlay: menu ready (Insert or L3+R3)\n");
}

void UpdateTextInput(SDL_Window* window) {
    bool want = false;
    {
        std::scoped_lock lock{imgui_mutex};
        want = initialized && menu_open && ImGui::GetIO().WantTextInput;
    }
    if (want != SDL_TextInputActive(window)) {
        if (want) {
            SDL_StartTextInput(window);
        } else {
            SDL_StopTextInput(window);
        }
    }
}

bool HandleEvent(const SDL_Event& event) {
    std::scoped_lock lock{imgui_mutex};
    if (!initialized) {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    const bool is_open = menu_open;
    // The bindings window waiting for a key: the next key, mouse button or wheel step is the new binding.
    if (controls_open && capture_index >= 0) {
        std::string name;
        bool cancel = false;
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            if (event.key.key == SDLK_ESCAPE) {
                cancel = true;
            } else if (event.key.key != SDLK_INSERT && event.key.key != SDLK_LALT) { // Insert: the port's menu; left Alt: frees the cursor
                name = SDL_GetScancodeName(event.key.scancode);
            }
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            name = MouseButtonName(event.button.button);
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL && event.wheel.y != 0.0f) {
            name = event.wheel.y > 0.0f ? "Wheel Up" : "Wheel Down";
        }
        if (cancel) {
            capture_index = -1;
            return true;
        }
        if (!name.empty()) { // (the click that started the capture was taken before: this one is the choice)
            SetBinding(capture_index, capture_slot, name);
            capture_index = -1;
            return true;
        }
        if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
            return true; // Insert, left Alt and the releases: nothing
        }
    }
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        const bool down = event.type == SDL_EVENT_KEY_DOWN;
        if (down && !event.key.repeat &&
            (event.key.key == SDLK_INSERT || (is_open && event.key.key == SDLK_ESCAPE))) {
            SetOpen(event.key.key == SDLK_INSERT ? !is_open : false);
            return true;
        }
        if (!is_open) {
            return false;
        }
        io.AddKeyEvent(ImGuiMod_Ctrl, (event.key.mod & SDL_KMOD_CTRL) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (event.key.mod & SDL_KMOD_SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (event.key.mod & SDL_KMOD_ALT) != 0);
        if (const ImGuiKey key = KeyFromSdl(event.key.key); key != ImGuiKey_None) {
            io.AddKeyEvent(key, down);
        }
        return true;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        const u8 button = event.gbutton.button;
        if (button == SDL_GAMEPAD_BUTTON_LEFT_STICK) {
            l3_down = down;
        } else if (button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
            r3_down = down;
        }
        if (down && l3_down && r3_down) {
            SetOpen(!is_open);
            return true;
        }
        if (!is_open) {
            return false;
        }
        if (const ImGuiKey key = KeyFromGamepad(button); key != ImGuiKey_None) {
            io.AddKeyEvent(key, down);
        }
        return true;
    }
    case SDL_EVENT_TEXT_INPUT: {
        // Typed characters (Ctrl+click on a slider, a text field): key events alone erase but
        // do not type. SDL sends them while text input is on (UpdateTextInput).
        if (!is_open) {
            return false;
        }
        io.AddInputCharactersUTF8(event.text.text);
        return true;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        if (!is_open) {
            return false;
        }
        const float density = PixelDensity(event.motion.windowID);
        io.AddMousePosEvent(event.motion.x * density, event.motion.y * density);
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!is_open) {
            return false;
        }
        const int button = event.button.button == SDL_BUTTON_LEFT    ? 0
                           : event.button.button == SDL_BUTTON_RIGHT  ? 1
                           : event.button.button == SDL_BUTTON_MIDDLE ? 2
                                                                      : -1;
        if (button >= 0) {
            io.AddMouseButtonEvent(button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        }
        return true;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        if (!is_open) {
            return false;
        }
        io.AddMouseWheelEvent(event.wheel.x, event.wheel.y);
        return true;
    default:
        return false;
    }
}

/// The game's Commandes menu asks for the keyboard and mouse bindings window.
void SetLanguage(int language) {
    ui_language = language >= 0 && language <= 3 ? language : 1;
}

void OpenControls() {
    std::scoped_lock lock{imgui_mutex};
    if (!initialized) {
        return;
    }
    IniLoad();
    capture_index = -1;
    controls_open = true;
    SetOpen(true);
}

/// True once after the bindings file was written: the pad code reads it again.
bool ControlsChanged() {
    return controls_changed.exchange(false);
}

bool Visible() {
    return initialized && (menu_open || text_entry_active || BbSettings::Get().show_fps);
}

bool CapturesInput() {
    return menu_open || text_entry_active;
}

void SetTextEntry(bool active, const std::string& prompt, const std::string& text) {
    std::scoped_lock lock{imgui_mutex};
    text_entry_active = active;
    text_entry_prompt = prompt;
    text_entry_text = text;
}

// Called for every presented frame: the interval for the FPS readout and BB_FPS_LOG.
void NotePresent() {
    const auto now = std::chrono::steady_clock::now();
    const float ms = std::chrono::duration<float, std::milli>(now - last_present).count();
    last_present = now;
    last_present_ms = ms;
    if (ms > 0.0f && ms < 1000.0f) {
        frame_ms_avg = frame_ms_avg == 0.0f ? ms : frame_ms_avg * 0.95f + ms * 0.05f;
    }
    // BB_FPS_LOG=1: presents per second and the slowest frame, every 5 seconds (test runs).
    static const bool fps_log = [] {
        const char* env = std::getenv("BB_FPS_LOG");
        return env && env[0] == '1';
    }();
    if (fps_log) {
        static auto window_start = now;
        static int frames = 0;
        static float worst = 0.0f;
        ++frames;
        worst = std::max(worst, ms < 1000.0f ? ms : 0.0f);
        const float elapsed = std::chrono::duration<float>(now - window_start).count();
        if (elapsed >= 5.0f) {
            std::printf("Frame rate: %.1f FPS, slowest frame %.1f ms\n", frames / elapsed, worst);
            window_start = now;
            frames = 0;
            worst = 0.0f;
        }
    }
}

void Render(vk::CommandBuffer cmdbuf, vk::ImageView view, vk::Extent2D extent) {
    const float ms = last_present_ms;
    if (!Visible()) {
        return;
    }
    std::scoped_lock lock{imgui_mutex};
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(extent.width), float(extent.height));
    io.DeltaTime = ms > 0.0f && ms < 1000.0f ? ms / 1000.0f : 1.0f / 60.0f;
    // UI scale follows the display height (1080p = 1).
    const float scale = std::max(float(extent.height) / 1080.0f, 0.75f);
    if (std::abs(scale - base_scale) > 0.01f) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(scale / base_scale);
        style.FontScaleMain = scale;
        base_scale = scale;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    if (menu_open) {
        Menu();
    }
    if (BbSettings::Get().show_fps && !menu_open) {
        FpsCounter();
    }
    if (text_entry_active) {
        TextEntryBox();
    }
    ImGui::Render();

    const vk::RenderingAttachmentInfo attachment{
        .imageView = view,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
    };
    cmdbuf.beginRendering(vk::RenderingInfo{
        .renderArea = {{0, 0}, extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment,
    });
    {
        // Font atlas uploads submit to the graphics queue themselves.
        std::scoped_lock submit_lock{Vulkan::Scheduler::submit_mutex};
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuf);
    }
    cmdbuf.endRendering();
}

} // namespace BbOverlay
