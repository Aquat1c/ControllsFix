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

enum { GET_DEVICE_STATE_SLOT = 9, DEVICE_CAP = 8 };

static HMODULE g_self;
static GetDeviceStateFn g_original;
static PadMapConfig g_config;
static PadMapDevice g_devices[DEVICE_CAP];
static CRITICAL_SECTION g_lock;

static HRESULT WINAPI HookedGetDeviceState(void *device, DWORD size, void *data)
{
    HRESULT hr = g_original(device, size, data);
    int i;

    if (hr != DI_OK || (size != sizeof(DIJOYSTATE) && size != sizeof(DIJOYSTATE2)))
        return hr;

    EnterCriticalSection(&g_lock);
    for (i = 0; i < DEVICE_CAP && g_devices[i].device && g_devices[i].device != device; ++i)
        ;
    if (i == DEVICE_CAP)
        i = 0;
    if (g_devices[i].device != device)
        PadMap_Describe((LPDIRECTINPUTDEVICE2A)device, &g_devices[i]);
    PadMap_Apply(&g_config, &g_devices[i], (DIJOYSTATE *)data);
    LeaveCriticalSection(&g_lock);
    return hr;
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

    GetSystemDirectoryW(path, MAX_PATH);
    wcscat(path, L"\\dinput.dll");
    module = LoadLibraryW(path);
    create = module ? (DirectInputCreateExFn)GetProcAddress(module, "DirectInputCreateEx") : NULL;
    if (!create || FAILED(create(GetModuleHandleW(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput7A,
                                 (void **)&directInput, NULL)))
        return 0;
    if (SUCCEEDED(IDirectInput7_CreateDeviceEx(directInput, &GUID_SysKeyboard, &IID_IDirectInputDevice7A,
                                               (void **)&keyboard, NULL))) {
        table = *(void ***)keyboard;
        if (VirtualProtect(&table[GET_DEVICE_STATE_SLOT], sizeof(void *), PAGE_READWRITE, &protect)) {
            g_original = (GetDeviceStateFn)table[GET_DEVICE_STATE_SLOT];
            table[GET_DEVICE_STATE_SLOT] = (void *)HookedGetDeviceState;
            VirtualProtect(&table[GET_DEVICE_STATE_SLOT], sizeof(void *), protect, &protect);
        }
        IDirectInputDevice7_Release(keyboard);
    }
    IDirectInput7_Release(directInput);
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
