#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0700
#define COBJMACROS

#include "padmap.h"

#include <string.h>
#include <wchar.h>

typedef struct XState {
    DWORD packet;
    WORD buttons;
    BYTE leftTrigger, rightTrigger;
    SHORT leftX, leftY, rightX, rightY;
} XState;
typedef DWORD (WINAPI *XGetState)(DWORD, XState *);

static const WORD kXButtons[10] = {
    0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0x0020, 0x0010, 0x0040, 0x0080,
};

static XGetState LoadXInput(void)
{
    static const wchar_t *const names[] = { L"\\xinput1_4.dll", L"\\xinput1_3.dll", L"\\xinput9_1_0.dll" };
    static XGetState get;
    static bool tried;
    wchar_t path[MAX_PATH + 32];
    HMODULE module;
    int i;

    if (tried)
        return get;
    tried = true;
    for (i = 0; i < 3 && !get; ++i) {
        if (!GetSystemDirectoryW(path, MAX_PATH))
            break;
        wcscat(path, names[i]);
        module = LoadLibraryW(path);
        if (module)
            get = (XGetState)GetProcAddress(module, "XInputGetState");
    }
    return get;
}

void PadMap_Defaults(PadMapConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->layout = PADMAP_AUTO;
    config->threshold = 50;
    config->leftTrigger = 11;
    config->rightTrigger = 12;
    config->stick[PADMAP_UP] = 13;
    config->stick[PADMAP_DOWN] = 14;
    config->stick[PADMAP_LEFT] = 15;
    config->stick[PADMAP_RIGHT] = 16;
}

static int ReadAxis(const wchar_t *iniPath, const wchar_t *key)
{
    static const wchar_t *const names[PADMAP_AXIS_COUNT] = { L"Z", L"RX", L"RY", L"RZ", L"Slider1", L"Slider2" };
    wchar_t text[16];
    int i;

    GetPrivateProfileStringW(L"General", key, L"Off", text, 16, iniPath);
    for (i = 0; i < PADMAP_AXIS_COUNT; ++i)
        if (_wcsicmp(text, names[i]) == 0)
            return i + 1;
    return 0;
}

static int ReadButton(const wchar_t *iniPath, const wchar_t *key, int fallback)
{
    int value = (int)GetPrivateProfileIntW(L"Buttons", key, fallback, iniPath);
    return value < 0 || value > 16 ? fallback : value;
}

void PadMap_Load(PadMapConfig *config, const wchar_t *iniPath)
{
    static const wchar_t *const directions[4] = { L"Up", L"Down", L"Left", L"Right" };
    wchar_t key[32], text[16];
    int i;

    PadMap_Defaults(config);
    GetPrivateProfileStringW(L"General", L"Layout", L"Auto", text, 16, iniPath);
    config->layout = _wcsicmp(text, L"Xbox") == 0 ? PADMAP_XBOX
                   : _wcsicmp(text, L"Classic") == 0 ? PADMAP_CLASSIC : PADMAP_AUTO;
    config->threshold = (int)GetPrivateProfileIntW(L"General", L"Threshold", 50, iniPath);
    if (config->threshold < 10 || config->threshold > 90)
        config->threshold = 50;
    config->triggerAxis[0] = ReadAxis(iniPath, L"LeftTriggerAxis");
    config->triggerAxis[1] = ReadAxis(iniPath, L"RightTriggerAxis");
    GetPrivateProfileStringW(L"General", L"TriggerRest", L"Low", text, 16, iniPath);
    config->triggerRestHigh = _wcsicmp(text, L"High") == 0;

    config->leftTrigger = ReadButton(iniPath, L"LeftTrigger", config->leftTrigger);
    config->rightTrigger = ReadButton(iniPath, L"RightTrigger", config->rightTrigger);
    for (i = 0; i < 4; ++i) {
        _snwprintf(key, 32, L"RightStick%s", directions[i]);
        config->stick[i] = ReadButton(iniPath, key, config->stick[i]);
        _snwprintf(key, 32, L"DPad%s", directions[i]);
        config->dpad[i] = ReadButton(iniPath, key, config->dpad[i]);
    }
    for (i = 0; i < 16; ++i) {
        _snwprintf(key, 32, L"Button%d", i + 17);
        config->extra[i] = ReadButton(iniPath, key, config->extra[i]);
    }
}

void PadMap_Describe(LPDIRECTINPUTDEVICE2A device, PadMapDevice *out)
{
    static const DWORD offsets[PADMAP_AXIS_COUNT] = { DIJOFS_Z, DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ,
                                                      DIJOFS_SLIDER(0), DIJOFS_SLIDER(1) };
    DIPROPGUIDANDPATH identity;
    DIPROPRANGE range;
    int i;

    memset(out, 0, sizeof(*out));
    out->device = device;
    out->slot = -1;

    memset(&identity, 0, sizeof(identity));
    identity.diph.dwSize = sizeof(identity);
    identity.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    identity.diph.dwHow = DIPH_DEVICE;
    if (SUCCEEDED(IDirectInputDevice2_GetProperty(device, DIPROP_GUIDANDPATH, &identity.diph))) {
        _wcslwr(identity.wszPath);
        out->xinput = wcsstr(identity.wszPath, L"ig_") != NULL;
    }

    for (i = 0; i < PADMAP_AXIS_COUNT; ++i) {
        memset(&range, 0, sizeof(range));
        range.diph.dwSize = sizeof(range);
        range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        range.diph.dwObj = offsets[i];
        range.diph.dwHow = DIPH_BYOFFSET;
        if (SUCCEEDED(IDirectInputDevice2_GetProperty(device, DIPROP_RANGE, &range.diph))
                && range.lMax > range.lMin) {
            out->present[i] = true;
            out->low[i] = range.lMin;
            out->high[i] = range.lMax;
        }
    }
}

static LONG AxisValue(const DIJOYSTATE *state, int axis)
{
    switch (axis) {
    case PADMAP_AXIS_Z: return state->lZ;
    case PADMAP_AXIS_RX: return state->lRx;
    case PADMAP_AXIS_RY: return state->lRy;
    case PADMAP_AXIS_RZ: return state->lRz;
    case PADMAP_AXIS_SLIDER1: return state->rglSlider[0];
    default: return state->rglSlider[1];
    }
}

static int Side(const PadMapConfig *config, const PadMapDevice *device, int axis, const DIJOYSTATE *state)
{
    LONG middle, limit, value = AxisValue(state, axis);
    int threshold = config->threshold;

    if (!device->present[axis] || config->triggerAxis[0] == axis + 1 || config->triggerAxis[1] == axis + 1)
        return 0;
    middle = device->low[axis] + (device->high[axis] - device->low[axis]) / 2;
    limit = (device->high[axis] - device->low[axis]) / 2 * threshold / 100;
    return value - middle > limit ? 1 : middle - value > limit ? -1 : 0;
}

static bool ReadTriggers(PadMapDevice *device, const DIJOYSTATE *state, int pull, XState *out)
{
    XGetState get = LoadXInput();
    XState pads[4];
    unsigned buttons = 0;
    int i, b, connected = 0, only = -1, matches = 0, match = -1;

    if (!get)
        return false;
    if (device->slot >= 0) {
        if (get((DWORD)device->slot, out) == 0)
            return true;
        device->slot = -1;
    }

    for (b = 0; b < 10; ++b)
        if (state->rgbButtons[b] & 0x80)
            buttons |= 1u << b;
    for (i = 0; i < 4; ++i) {
        unsigned seen = 0;
        int difference;

        if (get((DWORD)i, &pads[i]) != 0)
            continue;
        ++connected;
        only = i;
        for (b = 0; b < 10; ++b)
            if (pads[i].buttons & kXButtons[b])
                seen |= 1u << b;
        difference = (int)pads[i].leftTrigger - (int)pads[i].rightTrigger;
        if (seen == buttons && (pull == 0 || (pull > 0 ? difference > 40 : difference < -40))) {
            ++matches;
            match = i;
        }
    }

    if (connected == 1)
        device->slot = only;
    else if (matches == 1 && (buttons || pull))
        device->slot = match;
    if (device->slot < 0)
        return false;
    *out = pads[device->slot];
    return true;
}

static void Press(DIJOYSTATE *state, int button)
{
    if (button >= 1 && button <= 16)
        state->rgbButtons[button - 1] |= 0x80;
}

static unsigned Held(const DIJOYSTATE *state)
{
    unsigned mask = 0;
    int i;

    for (i = 0; i < 16; ++i)
        if (state->rgbButtons[i] & 0x80)
            mask |= 1u << i;
    return mask;
}

unsigned PadMap_Apply(const PadMapConfig *config, PadMapDevice *device, DIJOYSTATE *state)
{
    unsigned before = Held(state);
    bool xbox = config->layout == PADMAP_XBOX || (config->layout == PADMAP_AUTO && device->xinput);
    int horizontal, vertical, i;
    DWORD hat = state->rgdwPOV[0];
    bool pressed[16];

    for (i = 0; i < 16; ++i)
        pressed[i] = (state->rgbButtons[16 + i] & 0x80) != 0;

    if (xbox) {
        int pull = Side(config, device, PADMAP_AXIS_Z, state);
        XState pad;

        if (device->xinput && ReadTriggers(device, state, pull, &pad)) {
            if (pad.leftTrigger * 100 > config->threshold * 255)
                Press(state, config->leftTrigger);
            if (pad.rightTrigger * 100 > config->threshold * 255)
                Press(state, config->rightTrigger);
        } else if (pull) {
            Press(state, pull > 0 ? config->leftTrigger : config->rightTrigger);
        }
        horizontal = Side(config, device, PADMAP_AXIS_RX, state);
        vertical = Side(config, device, PADMAP_AXIS_RY, state);
    } else if (device->present[PADMAP_AXIS_RZ]) {
        horizontal = Side(config, device, PADMAP_AXIS_Z, state);
        vertical = Side(config, device, PADMAP_AXIS_RZ, state);
    } else {
        horizontal = Side(config, device, PADMAP_AXIS_RX, state);
        vertical = Side(config, device, PADMAP_AXIS_RY, state);
    }

    if (vertical)
        Press(state, config->stick[vertical < 0 ? PADMAP_UP : PADMAP_DOWN]);
    if (horizontal)
        Press(state, config->stick[horizontal < 0 ? PADMAP_LEFT : PADMAP_RIGHT]);

    for (i = 0; i < 2; ++i) {
        int axis = config->triggerAxis[i] - 1;
        LONG travel;

        if (axis < 0 || !device->present[axis])
            continue;
        travel = config->triggerRestHigh ? device->high[axis] - AxisValue(state, axis)
                                         : AxisValue(state, axis) - device->low[axis];
        if (travel > (device->high[axis] - device->low[axis]) / 100 * config->threshold)
            Press(state, i == 0 ? config->leftTrigger : config->rightTrigger);
    }

    if (LOWORD(hat) != 0xFFFF) {
        if (hat >= 29250 || hat < 6750)
            Press(state, config->dpad[PADMAP_UP]);
        if (hat >= 11250 && hat < 24750)
            Press(state, config->dpad[PADMAP_DOWN]);
        if (hat >= 20250 && hat < 33750)
            Press(state, config->dpad[PADMAP_LEFT]);
        if (hat >= 2250 && hat < 15750)
            Press(state, config->dpad[PADMAP_RIGHT]);
    }

    for (i = 0; i < 16; ++i)
        if (pressed[i])
            Press(state, config->extra[i]);

    return Held(state) & ~before;
}
