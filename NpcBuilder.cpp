// NpcBuilder.cpp — конструктор НПС из готовых частей для RDR1 + RedHook v0.8
// Плагин: собрать в DLL x64, переименовать в NpcBuilder.red, положить в папку с RDR.exe.
//
// F6 — вкл/выкл редактор (сообщение в консоли RedHook, клавиша F8).
// Стрелки вверх/вниз — слот, влево/вправо — вариант, PgUp/PgDn — текстура,
// F7 — спавн НПС, F8 занят консолью RedHook, поэтому сохранение — F10, загрузка — F11.
//
// Имена нативов берутся из NpcBuilder.ini ([natives]), чтобы поправить их без перекомпиляции.
#include "RedHookApi.h"
#include <array>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <initializer_list>
#include <cstring>

enum Slot { HEAD, TORSO, LEGS, HANDS, HAT, ACCESSORY, SLOT_COUNT };
static const char* SLOT_NAMES[SLOT_COUNT] = { "head", "torso", "legs", "hands", "hat", "accessory" };
struct Part { int variation = 0, texture = 0; };
struct Recipe { std::string model; std::array<Part, SLOT_COUNT> parts{}; };

static Recipe g_recipe;
static bool g_open = false;
static int  g_slot = 0;
static char g_ini[MAX_PATH] = ".\\NpcBuilder.ini";

// флаги, выставляемые обработчиком клавиатуры, обрабатываются в потоке скрипта
static volatile LONG g_pending[256];

static void LogF(const char* fmt, ...) {
    char b[256]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
    RH::Log(b);
}

struct NativeName { char name[96]; };
static NativeName N_REQUEST, N_HASLOADED, N_CREATE, N_PLAYER, N_SETCOMP, N_COMPCOUNT;

static void LoadConfig() {
    char b[96];
    auto rd = [&](const char* key, NativeName& out) {
        GetPrivateProfileStringA("natives", key, "", b, sizeof b, g_ini);
        strcpy_s(out.name, b);
    };
    rd("REQUEST_MODEL", N_REQUEST);       rd("HAS_MODEL_LOADED", N_HASLOADED);
    rd("CREATE_PED", N_CREATE);           rd("GET_PLAYER_PED", N_PLAYER);
    rd("SET_COMPONENT", N_SETCOMP);       rd("GET_COMPONENT_COUNT", N_COMPCOUNT);
    GetPrivateProfileStringA("recipe", "model", "", b, sizeof b, g_ini);
    g_recipe.model = b;
    for (int i = 0; i < SLOT_COUNT; ++i) {
        char k[48];
        sprintf_s(k, "%s_var", SLOT_NAMES[i]); g_recipe.parts[i].variation = GetPrivateProfileIntA("recipe", k, 0, g_ini);
        sprintf_s(k, "%s_tex", SLOT_NAMES[i]); g_recipe.parts[i].texture   = GetPrivateProfileIntA("recipe", k, 0, g_ini);
    }
}
static void SaveRecipe() {
    WritePrivateProfileStringA("recipe", "model", g_recipe.model.c_str(), g_ini);
    for (int i = 0; i < SLOT_COUNT; ++i) {
        char k[48], v[16];
        sprintf_s(k, "%s_var", SLOT_NAMES[i]); sprintf_s(v, "%d", g_recipe.parts[i].variation); WritePrivateProfileStringA("recipe", k, v, g_ini);
        sprintf_s(k, "%s_tex", SLOT_NAMES[i]); sprintf_s(v, "%d", g_recipe.parts[i].texture);   WritePrivateProfileStringA("recipe", k, v, g_ini);
    }
    LogF("NpcBuilder: recipe saved");
}

// Вызов натива по имени. Возвращает указатель на результат или nullptr, если имя не задано.
static uint64_t* Call(const NativeName& n, std::initializer_list<uint64_t> args) {
    if (!n.name[0]) { LogF("NpcBuilder: native name not set in ini"); return nullptr; }
    RH::NativeInit(RH::Joaat(n.name));
    for (uint64_t a : args) RH::NativePush(a);
    return RH::NativeCall();
}

static void Apply(uint64_t ped) {
    for (int i = 0; i < SLOT_COUNT; ++i)
        Call(N_SETCOMP, { ped, (uint64_t)i, (uint64_t)g_recipe.parts[i].variation, (uint64_t)g_recipe.parts[i].texture });
}

static uint64_t g_preview = 0;
static uint64_t Spawn() {
    if (g_recipe.model.empty()) { LogF("NpcBuilder: set [recipe] model in ini"); return 0; }
    // TODO: загрузка модели и CREATE_PED — аргументы зависят от реальных нативов RDR1
    LogF("NpcBuilder: spawn requested (natives not configured yet)");
    return 0;
}
static int Wrap(int v, int c) { return c <= 0 ? 0 : ((v % c) + c) % c; }

static void Step(int key) {
    switch (key) {
    case VK_F6: g_open = !g_open; LogF(g_open ? "NpcBuilder: ON" : "NpcBuilder: OFF"); return;
    }
    if (!g_open) return;
    switch (key) {
    case VK_UP:    g_slot = Wrap(g_slot - 1, SLOT_COUNT); LogF("slot: %s", SLOT_NAMES[g_slot]); break;
    case VK_DOWN:  g_slot = Wrap(g_slot + 1, SLOT_COUNT); LogF("slot: %s", SLOT_NAMES[g_slot]); break;
    case VK_LEFT:  g_recipe.parts[g_slot].variation--; g_recipe.parts[g_slot].texture = 0; if (g_preview) Apply(g_preview); break;
    case VK_RIGHT: g_recipe.parts[g_slot].variation++; g_recipe.parts[g_slot].texture = 0; if (g_preview) Apply(g_preview); break;
    case VK_PRIOR: g_recipe.parts[g_slot].texture++; if (g_preview) Apply(g_preview); break;
    case VK_NEXT:  g_recipe.parts[g_slot].texture--; if (g_preview) Apply(g_preview); break;
    case VK_F7:    g_preview = Spawn(); break;
    case VK_F10:   SaveRecipe(); break;
    case VK_F11:   LoadConfig(); LogF("NpcBuilder: config reloaded"); break;
    }
    if (key == VK_LEFT || key == VK_RIGHT || key == VK_PRIOR || key == VK_NEXT)
        LogF("%s: var %d tex %d", SLOT_NAMES[g_slot], g_recipe.parts[g_slot].variation, g_recipe.parts[g_slot].texture);
}

static void OnKey(DWORD key, WORD, BYTE, BOOL, BOOL, BOOL wasDown, BOOL isUp) {
    if (!isUp && !wasDown && key < 256) InterlockedExchange(&g_pending[key], 1);
}

static void ScriptMain() {
    LoadConfig();
    LogF("NpcBuilder loaded. F6 = toggle");
    RH::KeyboardRegister(OnKey);
    for (;;) {
        for (int k = 0; k < 256; ++k)
            if (InterlockedExchange(&g_pending[k], 0)) Step(k);
        RH::ScriptWait(0);
    }
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(mod);
        char p[MAX_PATH]; GetModuleFileNameA(mod, p, MAX_PATH);
        char* s = strrchr(p, '\\'); if (s) { strcpy_s(s + 1, MAX_PATH - (s + 1 - p), "NpcBuilder.ini"); strcpy_s(g_ini, p); }
        RH::ScriptRegister(mod, ScriptMain);
    } else if (reason == DLL_PROCESS_DETACH) {
        RH::KeyboardUnregister(OnKey);
        RH::ScriptUnregister(mod);
    }
    return TRUE;
}
