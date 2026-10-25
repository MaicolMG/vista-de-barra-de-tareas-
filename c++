// ==WindhawkMod==
// @id            taskbar-icon-bounce
// @name          Taskbar & Start Menu Icon Bounce
// @description   Animacion de rebote y escala en iconos de la taskbar y menu de inicio
// @version       1.0.0
// @author        You
// @github        https://github.com/nat
// @twitter       https://twitter.com/jack
// @homepage      https://your-personal-homepage.example.com/
// @include       explorer.exe
// @include       StartMenuExperienceHost.exe
// @compilerOptions -luser32 -lgdi32 -lcomctl32 -lole32
// @license       MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar & Start Menu Icon Bounce

Anade animaciones fluidas a los iconos de la barra de tareas y el menu de inicio.

- **Bounce (rebote)**: los iconos saltan al hacer clic.
- **Scale hover**: los iconos crecen al pasar el cursor por encima.
- **Spring back**: retorno elastico cuando el cursor se aleja.

## Uso

Simplemente activa el mod. Puedes ajustar los valores en la pestana Settings:

- `bounce_height`: cuanto sube el icono al hacer clic (px).
- `hover_scale`: tamano del icono al hacer hover en % (100 = normal).
- `anim_speed`: velocidad de la animacion (1 lento - 20 rapido).
- `enable_bounce`: activa o desactiva el rebote al clic.
- `enable_hover`: activa o desactiva la escala al hover.

## Requisitos

- Windows 10 o Windows 11
- Windhawk 1.3 o superior
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- bounce_height: 8
  $name: Altura del rebote (px)
  $description: Cuantos pixeles sube el icono al hacer clic (1-30).

- hover_scale: 130
  $name: Escala al pasar el cursor (%)
  $description: Tamano del icono al hacer hover, en porcentaje (100-200).

- anim_speed: 12
  $name: Velocidad de animacion
  $description: Velocidad de las transiciones (1=lento, 20=rapido).

- enable_bounce: true
  $name: Activar rebote al hacer clic
  $description: Los iconos rebotan al hacer clic.

- enable_hover: true
  $name: Activar escala al pasar el cursor
  $description: Los iconos escalan al pasar el cursor.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <unordered_map>

// ─────────────────────────────────────────────
//  Configuracion global leida desde Settings
// ─────────────────────────────────────────────
struct Settings {
    int  bounce_height = 8;
    int  hover_scale   = 130;  // porcentaje, 100 = tamaño original
    int  anim_speed    = 12;
    bool enable_bounce = true;
    bool enable_hover  = true;
} g_settings;

// ─────────────────────────────────────────────
//  Estado de animacion por HWND
// ─────────────────────────────────────────────
struct IconAnim {
    float    offset_y     = 0.0f;
    float    scale        = 1.0f;
    float    target_y     = 0.0f;
    float    target_scale = 1.0f;
    bool     bouncing     = false;
    UINT_PTR timer_id     = 0;
};

static std::unordered_map<HWND, IconAnim> g_anims;
static UINT_PTR g_next_timer = 2000;

// ─────────────────────────────────────────────
//  Timer: interpola offset_y y scale hacia su target (~60 fps)
// ─────────────────────────────────────────────
static void CALLBACK AnimTimerProc(HWND hwnd, UINT, UINT_PTR id, DWORD)
{
    auto it = g_anims.find(hwnd);
    if (it == g_anims.end()) { KillTimer(hwnd, id); return; }

    IconAnim& a   = it->second;
    float     spd = g_settings.anim_speed / 100.0f;

    // Lerp suave hacia el objetivo
    a.offset_y += (a.target_y     - a.offset_y) * spd;
    a.scale    += (a.target_scale - a.scale)     * spd;

    // Detectar cuando la animacion termino
    bool y_done = fabsf(a.offset_y - a.target_y)     < 0.3f;
    bool s_done = fabsf(a.scale    - a.target_scale)  < 0.003f;

    if (y_done && s_done) {
        a.offset_y = a.target_y;
        a.scale    = a.target_scale;
        a.bouncing = false;
        KillTimer(hwnd, id);
        a.timer_id = 0;
    }

    // Repintar el icono
    InvalidateRect(hwnd, nullptr, FALSE);
    HWND parent = GetParent(hwnd);
    if (parent) InvalidateRect(parent, nullptr, FALSE);
}

// ─────────────────────────────────────────────
//  Arrancar el timer de animacion para una ventana
// ─────────────────────────────────────────────
static void StartAnim(HWND hwnd)
{
    auto& a = g_anims[hwnd];
    if (a.timer_id == 0) {
        a.timer_id = g_next_timer++;
        SetTimer(hwnd, a.timer_id, 16, AnimTimerProc);
    }
}

// ─────────────────────────────────────────────
//  Subclass procedure para los botones de la taskbar
// ─────────────────────────────────────────────
static LRESULT CALLBACK TaskbarBtnProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR)
{
    switch (msg)
    {
        case WM_MOUSEMOVE:
        {
            if (g_settings.enable_hover) {
                auto& a      = g_anims[hwnd];
                float target = g_settings.hover_scale / 100.0f;
                if (fabsf(a.target_scale - target) > 0.01f) {
                    a.target_scale = target;
                    StartAnim(hwnd);
                }
            }
            break;
        }

        case WM_MOUSELEAVE:
        {
            auto& a        = g_anims[hwnd];
            a.target_scale = 1.0f;
            a.target_y     = 0.0f;
            StartAnim(hwnd);
            break;
        }

        case WM_LBUTTONDOWN:
        {
            if (g_settings.enable_bounce) {
                auto& a    = g_anims[hwnd];
                a.bouncing = true;
                a.target_y = -(float)g_settings.bounce_height;
                StartAnim(hwnd);
            }
            break;
        }

        case WM_LBUTTONUP:
        {
            if (g_settings.enable_bounce) {
                auto& a    = g_anims[hwnd];
                // Overshoot al soltar para efecto elastico
                a.offset_y = (float)g_settings.bounce_height * 0.3f;
                a.target_y = 0.0f;
                StartAnim(hwnd);
            }
            break;
        }

        case WM_NCDESTROY:
        {
            auto it = g_anims.find(hwnd);
            if (it != g_anims.end()) {
                if (it->second.timer_id)
                    KillTimer(hwnd, it->second.timer_id);
                g_anims.erase(it);
            }
            RemoveWindowSubclass(hwnd, TaskbarBtnProc, uIdSubclass);
            break;
        }
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ─────────────────────────────────────────────
//  Enumerar hijos y subclasificar botones
// ─────────────────────────────────────────────
static BOOL CALLBACK EnumChildren(HWND hwnd, LPARAM)
{
    wchar_t cls[256] = {};
    GetClassNameW(hwnd, cls, 256);

    if (wcscmp(cls, L"Button")           == 0 ||
        wcscmp(cls, L"MSTaskListWClass") == 0)
    {
        SetWindowSubclass(hwnd, TaskbarBtnProc, 0xB0C, 0);

        // Habilitar WM_MOUSELEAVE
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
    }

    EnumChildWindows(hwnd, EnumChildren, 0);
    return TRUE;
}

// ─────────────────────────────────────────────
//  Hookear taskbar y menu de inicio
// ─────────────────────────────────────────────
static void HookAll()
{
    // Taskbar principal
    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (hTray) EnumChildWindows(hTray, EnumChildren, 0);

    // Taskbars secundarias (multi-monitor)
    HWND hSec = nullptr;
    while ((hSec = FindWindowExW(nullptr, hSec,
            L"Shell_SecondaryTrayWnd", nullptr)) != nullptr)
        EnumChildWindows(hSec, EnumChildren, 0);

    // Menu de inicio
    HWND hStart = FindWindowW(L"Windows.UI.Core.CoreWindow", nullptr);
    if (hStart) EnumChildWindows(hStart, EnumChildren, 0);
}

// ─────────────────────────────────────────────
//  Timer para re-hookear si Explorer se reinicia
// ─────────────────────────────────────────────
static UINT_PTR g_rehook_timer = 0;

static void CALLBACK RehookProc(HWND, UINT, UINT_PTR, DWORD) { HookAll(); }

// ─────────────────────────────────────────────
//  Windhawk entry points
// ─────────────────────────────────────────────
void Wh_ModSettingsChanged()
{
    g_settings.bounce_height = Wh_GetIntSetting(L"bounce_height");
    g_settings.hover_scale   = Wh_GetIntSetting(L"hover_scale");
    g_settings.anim_speed    = Wh_GetIntSetting(L"anim_speed");
    g_settings.enable_bounce = Wh_GetIntSetting(L"enable_bounce") != 0;
    g_settings.enable_hover  = Wh_GetIntSetting(L"enable_hover")  != 0;

    // Clamp
    if (g_settings.bounce_height < 1)   g_settings.bounce_height = 1;
    if (g_settings.bounce_height > 30)  g_settings.bounce_height = 30;
    if (g_settings.hover_scale   < 100) g_settings.hover_scale   = 100;
    if (g_settings.hover_scale   > 200) g_settings.hover_scale   = 200;
    if (g_settings.anim_speed    < 1)   g_settings.anim_speed    = 1;
    if (g_settings.anim_speed    > 20)  g_settings.anim_speed    = 20;
}

BOOL Wh_ModInit()
{
    Wh_Log(L"[IconBounce] Iniciando...");
    Wh_ModSettingsChanged();
    HookAll();
    // Re-hookear cada 5 s por si Explorer se reinicia
    g_rehook_timer = SetTimer(nullptr, 0, 5000, RehookProc);
    Wh_Log(L"[IconBounce] Listo.");
    return TRUE;
}

void Wh_ModUninit()
{
    Wh_Log(L"[IconBounce] Descargando...");
    if (g_rehook_timer) { KillTimer(nullptr, g_rehook_timer); g_rehook_timer = 0; }

    for (auto& [hwnd, anim] : g_anims) {
        if (anim.timer_id) KillTimer(hwnd, anim.timer_id);
        RemoveWindowSubclass(hwnd, TaskbarBtnProc, 0xB0C);
    }
    g_anims.clear();
    Wh_Log(L"[IconBounce] Descargado.");
}
