#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0700
#define COBJMACROS

#include <windows.h>
#include <initguid.h>
#include <dinput.h>
#include <wchar.h>

#include "padmap.h"
#include "efz_config_api.h"

#define BUTTON(key, fallback, text) \
    { "Buttons", key, NULL, text, EFZ_SETTING_NUMBER, 0, 16, NULL, fallback }

static const char kSlot[] =
    "The button number (1 to 16) the game sees when this is pressed. 0 = off.\n"
    "Pick a number the controller does not use for a button you want to keep.";

static const char kTriggerAxis[] =
    "For controllers whose trigger is an axis of its own and not a button, such as\n"
    "GameCube adapters and some generic pads: the axis that trigger is on.\n"
    "It then presses the LeftTrigger or RightTrigger button. Leave Off for Xbox and\n"
    "PlayStation controllers. An axis chosen here is no longer read as the right stick.";

static const EfzConfigSetting kSettings[] = {
    { "General", "Layout", NULL,
      "How the controller's extra sticks and triggers are laid out.\n"
      "Auto - Xbox-type controllers are recognised, everything else is Classic\n"
      "Xbox - triggers share one axis, the right stick is on the X and Y rotation axes\n"
      "Classic - the right stick is on the Z axis and Z rotation (PlayStation and most other pads)",
      EFZ_SETTING_CHOICE, 0, 0, "Auto|Xbox|Classic", "Auto" },
    { "General", "Threshold", NULL,
      "How far a stick or trigger has to move to count as pressed, in percent.",
      EFZ_SETTING_NUMBER, 10, 90, NULL, "50" },
    { "General", "LeftTriggerAxis", NULL, kTriggerAxis,
      EFZ_SETTING_CHOICE, 0, 0, "Off|Z|RX|RY|RZ|Slider1|Slider2", "Off" },
    { "General", "RightTriggerAxis", NULL, kTriggerAxis,
      EFZ_SETTING_CHOICE, 0, 0, "Off|Z|RX|RY|RZ|Slider1|Slider2", "Off" },
    { "General", "TriggerRest", NULL,
      "Which end a trigger axis chosen above rests at when the trigger is released.\n"
      "Try High if the trigger counts as pressed while you are not touching it.",
      EFZ_SETTING_CHOICE, 0, 0, "Low|High", "Low" },
    { "General", "KeepBindingsOnJoin", NULL,
      "When you join an online match, EfzRevival lets you play with your 1P keys.\n"
      "Newer versions of it lose that again as soon as they re-open the controllers,\n"
      "leaving you on the 2P keys. On keeps you on your 1P keys.",
      EFZ_SETTING_SWITCH, 0, 1, NULL, "1" },
    BUTTON("LeftTrigger", "11", kSlot),
    BUTTON("RightTrigger", "12", kSlot),
    BUTTON("RightStickUp", "13", kSlot),
    BUTTON("RightStickDown", "14", kSlot),
    BUTTON("RightStickLeft", "15", kSlot),
    BUTTON("RightStickRight", "16", kSlot),
    BUTTON("DPadUp", "0", kSlot),
    BUTTON("DPadDown", "0", kSlot),
    BUTTON("DPadLeft", "0", kSlot),
    BUTTON("DPadRight", "0", kSlot),
    BUTTON("Button17", "0", kSlot),
    BUTTON("Button18", "0", kSlot),
    BUTTON("Button19", "0", kSlot),
    BUTTON("Button20", "0", kSlot),
    BUTTON("Button21", "0", kSlot),
    BUTTON("Button22", "0", kSlot),
    BUTTON("Button23", "0", kSlot),
    BUTTON("Button24", "0", kSlot),
    BUTTON("Button25", "0", kSlot),
    BUTTON("Button26", "0", kSlot),
    BUTTON("Button27", "0", kSlot),
    BUTTON("Button28", "0", kSlot),
    BUTTON("Button29", "0", kSlot),
    BUTTON("Button30", "0", kSlot),
    BUTTON("Button31", "0", kSlot),
    BUTTON("Button32", "0", kSlot),
};

EFZ_CONFIG_EXPORT const EfzConfigSchema EFZ_ConfigSchema = {
    EFZ_CONFIG_SCHEMA_VERSION, sizeof(EfzConfigSetting), "ControllsFix.ini",
    sizeof(kSettings) / sizeof(kSettings[0]), kSettings,
};

typedef HRESULT (WINAPI *GetDeviceStateFn)(void *, DWORD, void *);
typedef HRESULT (WINAPI *DirectInputCreateExFn)(HINSTANCE, DWORD, REFIID, void **, LPUNKNOWN);

typedef ULONG (WINAPI *ReleaseFn)(void *);

enum { RELEASE_SLOT = 2, GET_DEVICE_STATE_SLOT = 9, DEVICE_CAP = 8 };

static HMODULE g_self;
static GetDeviceStateFn g_original;
static ReleaseFn g_release;
static PadMapConfig g_config;
static PadMapDevice g_devices[DEVICE_CAP];
static CRITICAL_SECTION g_lock;
static BYTE *g_bindings;
static BYTE g_bindingsBefore[32];
static bool g_reopened;

#ifdef CONTROLLS_FIX_LOG
#include <stdarg.h>
#include <stdio.h>
#include <intrin.h>

static FILE *g_log;
static CRITICAL_SECTION g_logLock;
static HRESULT WINAPI HookedGetDeviceState(void *device, DWORD size, void *data);

static void Log(const char *format, ...)
{
    va_list args;

    if (!g_log)
        return;
    EnterCriticalSection(&g_logLock);
    fprintf(g_log, "%10lu t%-6lu ", GetTickCount(), GetCurrentThreadId());
    va_start(args, format);
    vfprintf(g_log, format, args);
    va_end(args);
    fputc('\n', g_log);
    fflush(g_log);
    LeaveCriticalSection(&g_logLock);
}

static const char *ModuleOf(void *address)
{
    static char names[4][MAX_PATH];
    static LONG turn;
    char *name = names[InterlockedIncrement(&turn) & 3], *slash;
    HMODULE module = NULL;

    name[0] = 0;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)address, &module) && module)
        GetModuleFileNameA(module, name, MAX_PATH);
    slash = strrchr(name, '\\');
    return slash ? slash + 1 : name[0] ? name : "?";
}

static unsigned Buttons(const DIJOYSTATE *state)
{
    unsigned mask = 0;
    int i;

    for (i = 0; i < 32; ++i)
        if (state->rgbButtons[i] & 0x80)
            mask |= 1u << i;
    return mask;
}

static void LogDevice(const PadMapDevice *pad)
{
    LPDIRECTINPUTDEVICE2A device = (LPDIRECTINPUTDEVICE2A)pad->device;
    DIPROPGUIDANDPATH identity;
    DIPROPSTRING name;
    int i;

    memset(&identity, 0, sizeof(identity));
    identity.diph.dwSize = sizeof(identity);
    identity.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    identity.diph.dwHow = DIPH_DEVICE;
    IDirectInputDevice2_GetProperty(device, DIPROP_GUIDANDPATH, &identity.diph);
    memset(&name, 0, sizeof(name));
    name.diph.dwSize = sizeof(name);
    name.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    name.diph.dwHow = DIPH_DEVICE;
    IDirectInputDevice2_GetProperty(device, DIPROP_PRODUCTNAME, &name.diph);
    Log("device %p described: xinput=%d name=\"%ls\" path=%ls", pad->device, pad->xinput, name.wsz, identity.wszPath);
    for (i = 0; i < PADMAP_AXIS_COUNT; ++i)
        Log("  axis %d present=%d range %ld..%ld", i, pad->present[i], pad->low[i], pad->high[i]);
}

static void LogState(int index, const PadMapDevice *pad, DWORD size, const DIJOYSTATE *state,
                     unsigned before, unsigned added, void *caller)
{
    static unsigned lastBefore[8], lastAdded[8], lastZones[8];
    static DWORD lastHat[8];
    static int lastSlot[8];
    LONG values[PADMAP_AXIS_COUNT];
    unsigned zones = 0;
    int i;

    values[0] = state->lZ;
    values[1] = state->lRx;
    values[2] = state->lRy;
    values[3] = state->lRz;
    values[4] = state->rglSlider[0];
    values[5] = state->rglSlider[1];
    for (i = 0; i < PADMAP_AXIS_COUNT; ++i)
        if (pad->present[i])
            zones |= (unsigned)((values[i] - pad->low[i]) * 4 / (pad->high[i] - pad->low[i] + 1)) << (i * 4);
    if (before == lastBefore[index] && added == lastAdded[index] && zones == lastZones[index]
            && state->rgdwPOV[0] == lastHat[index] && pad->slot == lastSlot[index])
        return;
    lastBefore[index] = before;
    lastAdded[index] = added;
    lastZones[index] = zones;
    lastHat[index] = state->rgdwPOV[0];
    lastSlot[index] = pad->slot;
    Log("state %p size=%lu caller=%s+%p buttons=%08X added=%04X x=%ld y=%ld z=%ld rx=%ld ry=%ld rz=%ld s1=%ld s2=%ld "
        "pov=%lX xslot=%d",
        pad->device, size, ModuleOf(caller), caller, before, added, state->lX, state->lY, values[0], values[1],
        values[2], values[3], values[4], values[5], state->rgdwPOV[0], pad->slot);
}

static void LogBindings(const char *label, const BYTE *bindings)
{
    char text[160];
    int i, at = 0;

    for (i = 0; i < 8; ++i)
        at += sprintf(text + at, " (%u,%u)", bindings[i * 2 + 1], bindings[i * 2]);
    Log("%s:%s", label, text);
}

typedef struct LogXState {
    DWORD packet;
    WORD buttons;
    BYTE leftTrigger, rightTrigger;
    SHORT sticks[4];
} LogXState;

static void LogXInput(void)
{
    typedef DWORD (WINAPI *GetState)(DWORD, LogXState *);
    static const char *const names[3] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    static unsigned last[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
    GetState get = NULL;
    LogXState state;
    unsigned now;
    int i;

    for (i = 0; i < 3 && !get; ++i)
        if (GetModuleHandleA(names[i]))
            get = (GetState)GetProcAddress(GetModuleHandleA(names[i]), "XInputGetState");
    for (i = 0; get && i < 4; ++i) {
        memset(&state, 0, sizeof(state));
        now = get((DWORD)i, &state) != 0 ? 0xFFFFFFFE
            : (unsigned)state.buttons | (unsigned)(state.leftTrigger >> 5) << 16 | (unsigned)(state.rightTrigger >> 5) << 20;
        if (now == last[i])
            continue;
        last[i] = now;
        if (now == 0xFFFFFFFE)
            Log("xinput slot %d: not connected", i);
        else
            Log("xinput slot %d: buttons=%04X lt=%u rt=%u", i, state.buttons, state.leftTrigger, state.rightTrigger);
    }
}

typedef HRESULT (WINAPI *DeviceCallFn)(void *);
static DeviceCallFn g_acquire, g_unacquire, g_poll;

static void LogCall(const char *name, void *device, HRESULT hr, void *caller)
{
    static struct { void *device; const char *name; HRESULT hr; } seen[24];
    int i;

    EnterCriticalSection(&g_logLock);
    for (i = 0; i < 24 && seen[i].name && (seen[i].device != device || seen[i].name != name); ++i)
        ;
    if (i == 24)
        i = 0;
    if (seen[i].name && seen[i].hr == hr) {
        LeaveCriticalSection(&g_logLock);
        return;
    }
    seen[i].device = device;
    seen[i].name = name;
    seen[i].hr = hr;
    LeaveCriticalSection(&g_logLock);
    Log("%s %p -> %08lX caller=%s", name, device, (unsigned long)hr, ModuleOf(caller));
}

static HRESULT WINAPI LoggedAcquire(void *device)
{
    HRESULT hr = g_acquire(device);
    LogCall("acquire", device, hr, _ReturnAddress());
    return hr;
}

static HRESULT WINAPI LoggedUnacquire(void *device)
{
    HRESULT hr = g_unacquire(device);
    Log("unacquire %p -> %08lX caller=%s", device, (unsigned long)hr, ModuleOf(_ReturnAddress()));
    return hr;
}

static HRESULT WINAPI LoggedPoll(void *device)
{
    HRESULT hr = g_poll(device);
    LogCall("poll", device, hr, _ReturnAddress());
    return hr;
}

static void LogHookCalls(void **table)
{
    DWORD protect;

    if (!VirtualProtect(table, 27 * sizeof(void *), PAGE_READWRITE, &protect))
        return;
    g_acquire = (DeviceCallFn)table[7];
    g_unacquire = (DeviceCallFn)table[8];
    g_poll = (DeviceCallFn)table[25];
    table[7] = (void *)LoggedAcquire;
    table[8] = (void *)LoggedUnacquire;
    table[25] = (void *)LoggedPoll;
    VirtualProtect(table, 27 * sizeof(void *), protect, &protect);
    Log("log hooks on acquire, unacquire and poll installed; each is logged when its result changes");
}

static void LogKeyFile(void)
{
    static FILETIME last;
    static bool missing;
    wchar_t path[MAX_PATH + 16], *slash;
    BYTE data[512];
    FILETIME written;
    SYSTEMTIME when;
    DWORD size = 0;
    HANDLE file;
    char text[200];
    int preset, player, i, at;

    GetModuleFileNameW(NULL, path, MAX_PATH);
    slash = wcsrchr(path, L'\\');
    wcscpy(slash ? slash + 1 : path, L"KEY.ini");
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                       OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        if (!missing)
            Log("KEY.ini cannot be opened: error %lu", GetLastError());
        missing = true;
        return;
    }
    missing = false;
    if (!GetFileTime(file, NULL, NULL, &written) || CompareFileTime(&written, &last) == 0) {
        CloseHandle(file);
        return;
    }
    last = written;
    memset(data, 0, sizeof(data));
    ReadFile(file, data, sizeof(data), &size, NULL);
    CloseHandle(file);
    FileTimeToLocalFileTime(&written, &written);
    FileTimeToSystemTime(&written, &when);
    Log("KEY.ini on disk: %lu bytes, written %02u:%02u:%02u.%03u, selected preset %u (0 = first)", size,
        when.wHour, when.wMinute, when.wSecond, when.wMilliseconds, size > 156 ? data[156] : 0);
    Log("  pairs are (device,code) for up down left right A B C D; device 0 keyboard, 1 PAD1, 2 PAD2;"
        " a pad code is the button number, 17-20 the stick; the game keeps pad codes minus one");
    for (preset = 0; preset < 4 && (DWORD)(preset * 39 + 39) <= size; ++preset) {
        const BYTE *block = data + preset * 39;

        for (player = 0; player < 2; ++player) {
            for (i = 0, at = 0; i < 8; ++i)
                at += sprintf(text + at, " (%u,%u)", block[player * 16 + i * 2], block[player * 16 + i * 2 + 1]);
            Log("  preset %d player %d:%s", preset + 1, player + 1, text);
        }
        Log("  preset %d options: %u %u %u %u %u %u %u", preset + 1, block[32], block[33], block[34], block[35],
            block[36], block[37], block[38]);
    }
}

static void LogGame(const BYTE *input)
{
    static int lastScene = -1, lastCount = -1, lastFront = -1;
    int scene = *(const BYTE *)0x790148;
    int count = *(const char *)0x7b128c;
    int front = GetForegroundWindow() == *(HWND *)(input + 0x1e0);

    if (scene != lastScene)
        Log("game scene %d -> %d", lastScene, scene);
    if (count != lastCount)
        Log("game controller count %d -> %d", lastCount, count);
    if (front != lastFront)
        Log("game window in front: %d", front);
    lastScene = scene;
    lastCount = count;
    lastFront = front;
}

static DWORD WINAPI Watch(LPVOID unused)
{
    BYTE *input = (BYTE *)0x7b0968;
    BYTE bindings[32];
    void *devices[3];
    unsigned held[2] = { 0, 0 };
    int i;
    (void)unused;

    if ((DWORD_PTR)GetModuleHandleW(NULL) != 0x400000 || IsBadReadPtr(input, 0x1f8)) {
        Log("watch: this is not the expected game exe, not watching its memory");
        for (;;) {
            LogKeyFile();
            LogXInput();
            Sleep(15);
        }
    }
    memset(bindings, 0xEE, sizeof(bindings));
    memset(devices, 0xEE, sizeof(devices));
    for (;;) {
        LogKeyFile();
        LogGame(input);
        if (memcmp(bindings, input + 0x1c0, 32) != 0) {
            memcpy(bindings, input + 0x1c0, 32);
            Log("game bindings in memory changed, pairs are (device,code) for right left down up A B C D;"
                " pad codes here are the button number minus one, 16-19 the stick");
            LogBindings("  player 1", bindings);
            LogBindings("  player 2", bindings + 16);
        }
        if (memcmp(devices, input + 0x1e8, sizeof(devices)) != 0) {
            memcpy(devices, input + 0x1e8, sizeof(devices));
            for (i = 0; i < 3; ++i) {
                void **table = devices[i] && !IsBadReadPtr(devices[i], 4) ? *(void ***)devices[i] : NULL;
                Log("game device %d = %p table=%p getstate=%p hooked=%d", i, devices[i], (void *)table,
                    table ? table[GET_DEVICE_STATE_SLOT] : NULL,
                    table && table[GET_DEVICE_STATE_SLOT] == (void *)HookedGetDeviceState);
            }
        }
        for (i = 0; i < 2; ++i) {
            unsigned now = Buttons((const DIJOYSTATE *)(input + 0x120 + i * sizeof(DIJOYSTATE)));
            if (now != held[i]) {
                held[i] = now;
                Log("game buffer of pad %d: buttons=%08X", i + 1, now);
            }
        }
        LogXInput();
        Sleep(15);
    }
}
#else
#define Log(...) ((void)0)
#endif

static bool MatchesKeyFile(const BYTE *bindings)
{
    static const int order[8] = { 3, 2, 1, 0, 4, 5, 6, 7 };
    wchar_t path[MAX_PATH + 16], *slash;
    BYTE data[160], expected[32];
    DWORD size = 0;
    HANDLE file;
    int preset, player, i;

    GetModuleFileNameW(NULL, path, MAX_PATH);
    slash = wcsrchr(path, L'\\');
    wcscpy(slash ? slash + 1 : path, L"KEY.ini");
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                       OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    ReadFile(file, data, sizeof(data), &size, NULL);
    CloseHandle(file);
    for (preset = 0; preset < 4 && (DWORD)(preset * 39 + 32) <= size; ++preset) {
        for (player = 0; player < 2; ++player) {
            for (i = 0; i < 8; ++i) {
                const BYTE *stored = data + preset * 39 + player * 16 + i * 2;
                WORD value = (WORD)(stored[1] + (stored[0] << 8) - (stored[0] > 0));
                memcpy(expected + player * 16 + order[i] * 2, &value, 2);
            }
        }
        if (memcmp(expected, bindings, 32) == 0)
            return true;
    }
    return false;
}

static bool InPractice(void)
{
    const BYTE *game = *(const BYTE **)0x79010C;

    return game && !IsBadReadPtr(game + 0x1364, 1) && game[0x1364] == 1;
}

static void KeepBindings(void)
{
    if (!g_bindings)
        return;
    EnterCriticalSection(&g_lock);
    if (g_reopened && memcmp(g_bindings, g_bindingsBefore + 16, 16) == 0
            && memcmp(g_bindings + 16, g_bindingsBefore, 16) == 0
            && memcmp(g_bindingsBefore, g_bindingsBefore + 16, 16) != 0 && !InPractice()
            && MatchesKeyFile(g_bindings)) {
        memcpy(g_bindings, g_bindingsBefore, 32);
        Log("bindings were swapped while the controllers were re-opened, swapped back");
    }
    g_reopened = false;
    memcpy(g_bindingsBefore, g_bindings, 32);
    LeaveCriticalSection(&g_lock);
}

static HRESULT WINAPI HookedGetDeviceState(void *device, DWORD size, void *data)
{
    HRESULT hr = g_original(device, size, data);
    int i;

    KeepBindings();
    if (size != sizeof(DIJOYSTATE) && size != sizeof(DIJOYSTATE2))
        return hr;
#ifdef CONTROLLS_FIX_LOG
    LogCall("read", device, hr, _ReturnAddress());
#endif
    if (hr != DI_OK)
        return hr;

    EnterCriticalSection(&g_lock);
    for (i = 0; i < DEVICE_CAP && g_devices[i].device && g_devices[i].device != device; ++i)
        ;
    if (i == DEVICE_CAP)
        i = 0;
    if (g_devices[i].device != device) {
        PadMap_Describe((LPDIRECTINPUTDEVICE2A)device, &g_devices[i]);
#ifdef CONTROLLS_FIX_LOG
        LogDevice(&g_devices[i]);
#endif
    }
#ifdef CONTROLLS_FIX_LOG
    {
        unsigned before = Buttons((const DIJOYSTATE *)data);
        unsigned added = PadMap_Apply(&g_config, &g_devices[i], (DIJOYSTATE *)data);
        LogState(i, &g_devices[i], size, (const DIJOYSTATE *)data, before, added, _ReturnAddress());
    }
#else
    PadMap_Apply(&g_config, &g_devices[i], (DIJOYSTATE *)data);
#endif
    LeaveCriticalSection(&g_lock);
    return hr;
}

static ULONG WINAPI HookedRelease(void *device)
{
    ULONG left = g_release(device);
    int i, last;

    if (left != 0)
        return left;
    EnterCriticalSection(&g_lock);
    for (i = 0; i < DEVICE_CAP && g_devices[i].device && g_devices[i].device != device; ++i)
        ;
    if (i < DEVICE_CAP && g_devices[i].device) {
        for (last = i; last + 1 < DEVICE_CAP && g_devices[last + 1].device; ++last)
            ;
        g_devices[i] = g_devices[last];
        memset(&g_devices[last], 0, sizeof(g_devices[last]));
        g_reopened = true;
        Log("device %p released, forgotten", device);
    }
    LeaveCriticalSection(&g_lock);
    return left;
}

static DWORD WINAPI Install(LPVOID unused)
{
    wchar_t path[MAX_PATH + 16], *dot;
    LPDIRECTINPUT7A directInput = NULL;
    LPDIRECTINPUTDEVICE7A keyboard = NULL;
    DirectInputCreateExFn create;
    HMODULE module;
    void **table;
    DWORD protect;
    (void)unused;

    GetModuleFileNameW(g_self, path, MAX_PATH);
    dot = wcsrchr(path, L'.');
    if (dot)
        wcscpy(dot, L".ini");
    PadMap_Load(&g_config, path);
    if (GetPrivateProfileIntW(L"General", L"KeepBindingsOnJoin", 1, path)
            && (DWORD_PTR)GetModuleHandleW(NULL) == 0x400000 && !IsBadWritePtr((void *)0x7b0b28, 32))
        g_bindings = (BYTE *)0x7b0b28;
#ifdef CONTROLLS_FIX_LOG
    wcscpy(wcsrchr(path, L'.'), L".log");
    InitializeCriticalSection(&g_logLock);
    g_log = _wfopen(path, L"w");
    Log("ControllsFix log build, exe=%s", ModuleOf(GetModuleHandleW(NULL)));
    Log("config: layout=%d threshold=%d triggers=%d,%d stick=%d,%d,%d,%d dpad=%d,%d,%d,%d axes=%d,%d resthigh=%d",
        g_config.layout, g_config.threshold, g_config.leftTrigger, g_config.rightTrigger, g_config.stick[0],
        g_config.stick[1], g_config.stick[2], g_config.stick[3], g_config.dpad[0], g_config.dpad[1],
        g_config.dpad[2], g_config.dpad[3], g_config.triggerAxis[0], g_config.triggerAxis[1],
        g_config.triggerRestHigh);
#endif

    GetSystemDirectoryW(path, MAX_PATH);
    wcscat(path, L"\\dinput.dll");
    module = LoadLibraryW(path);
    create = module ? (DirectInputCreateExFn)GetProcAddress(module, "DirectInputCreateEx") : NULL;
    if (!create || FAILED(create(GetModuleHandleW(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput7A,
                                 (void **)&directInput, NULL))) {
        Log("hook: DirectInput could not be created, module=%p", (void *)module);
        return 0;
    }
    if (SUCCEEDED(IDirectInput7_CreateDeviceEx(directInput, &GUID_SysKeyboard, &IID_IDirectInputDevice7A,
                                               (void **)&keyboard, NULL))) {
        table = *(void ***)keyboard;
        if (VirtualProtect(table, (GET_DEVICE_STATE_SLOT + 1) * sizeof(void *), PAGE_READWRITE, &protect)) {
            g_original = (GetDeviceStateFn)table[GET_DEVICE_STATE_SLOT];
            g_release = (ReleaseFn)table[RELEASE_SLOT];
            table[GET_DEVICE_STATE_SLOT] = (void *)HookedGetDeviceState;
            table[RELEASE_SLOT] = (void *)HookedRelease;
            VirtualProtect(table, (GET_DEVICE_STATE_SLOT + 1) * sizeof(void *), protect, &protect);
        }
        Log("hook: table=%p original=%p (%s) now=%p", (void *)table, (void *)g_original,
            ModuleOf((void *)g_original), table[GET_DEVICE_STATE_SLOT]);
#ifdef CONTROLLS_FIX_LOG
        LogHookCalls(table);
#endif
        IDirectInputDevice7_Release(keyboard);
    }
    IDirectInput7_Release(directInput);
#ifdef CONTROLLS_FIX_LOG
    Watch(NULL);
#endif
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    HANDLE thread;
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        g_self = instance;
        DisableThreadLibraryCalls(instance);
        InitializeCriticalSection(&g_lock);
        PadMap_Defaults(&g_config);
        thread = CreateThread(NULL, 0, Install, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
