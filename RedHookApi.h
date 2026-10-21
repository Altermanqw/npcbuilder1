// RedHookApi.h — интерфейс RedHook v0.8, восстановленный по экспортам RedHook.dll
// (SDK в архиве нет). Функции берутся через GetProcAddress, поэтому .lib не нужен.
// Сигнатуры выведены из декорированных имён; не проверены на практике.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace RH {
using KeyHandler = void (*)(DWORD key, WORD repeats, BYTE scan, BOOL ext, BOOL alt, BOOL wasDown, BOOL isUp);

inline FARPROC Sym(const char* name) {
    static HMODULE h = GetModuleHandleA("RedHook.dll");
    return h ? GetProcAddress(h, name) : nullptr;
}
inline void ScriptRegister(HINSTANCE m, void (*fn)()) {
    auto f = (void (*)(HINSTANCE, void (*)()))Sym("?ScriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z");
    if (f) f(m, fn);
}
inline void ScriptUnregister(HINSTANCE m) {
    auto f = (void (*)(HINSTANCE))Sym("?ScriptUnregister@@YAXPEAUHINSTANCE__@@@Z");
    if (f) f(m);
}
inline void ScriptWait(uint64_t ms) {
    auto f = (void (*)(uint64_t))Sym("?ScriptWait@@YAX_K@Z");
    if (f) f(ms);
}
inline void KeyboardRegister(KeyHandler h) {
    auto f = (void (*)(KeyHandler))Sym("?KeyboardHandlerRegister@@YAXP6AXKGEHHHH@Z@Z");
    if (f) f(h);
}
inline void KeyboardUnregister(KeyHandler h) {
    auto f = (void (*)(KeyHandler))Sym("?KeyboardHandlerUnregister@@YAXP6AXKGEHHHH@Z@Z");
    if (f) f(h);
}
// Print(LogType, fmt, ...) — значение LogType неизвестно, 0 = предположение
inline void Log(const char* msg) {
    auto f = (void (*)(int, const char*, ...))Sym("?Print@@YAXW4LogType@@PEBDZZ");
    if (f) f(0, "%s", msg);
}

// ---- вызов нативов по хэшу ----
inline uint32_t Joaat(const char* s) {            // ПРЕДПОЛОЖЕНИЕ: хэш = joaat от имени в нижнем регистре
    uint32_t h = 0;
    for (; *s; ++s) { char c = (*s >= 'A' && *s <= 'Z') ? *s + 32 : *s; h += (uint8_t)c; h += h << 10; h ^= h >> 6; }
    h += h << 3; h ^= h >> 11; h += h << 15;
    return h;
}
inline void     NativeInit(uint32_t hash) { auto f = (void (*)(uint32_t))Sym("?NativeInit@@YAXI@Z"); if (f) f(hash); }
inline void     NativePush(uint64_t v)    { auto f = (void (*)(uint64_t))Sym("?NativePush64@@YAX_K@Z"); if (f) f(v); }
inline uint64_t* NativeCall()             { auto f = (uint64_t* (*)())Sym("?NativeCall@@YAPEA_KXZ"); return f ? f() : nullptr; }

inline uint64_t F(float v) { uint32_t b; memcpy(&b, &v, 4); return b; }
} // namespace RH
