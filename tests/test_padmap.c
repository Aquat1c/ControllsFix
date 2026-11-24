#define DIRECTINPUT_VERSION 0x0700

#include <stdio.h>
#include <string.h>

#include "../padmap.h"

static int g_failures;

static void check(const char *name, bool condition)
{
    printf("%s %s\n", condition ? "ok  " : "FAIL", name);
    g_failures += !condition;
}

static void rest(DIJOYSTATE *state)
{
    memset(state, 0, sizeof(*state));
    state->lZ = state->lRx = state->lRy = state->lRz = 32767;
    state->rgdwPOV[0] = state->rgdwPOV[1] = state->rgdwPOV[2] = state->rgdwPOV[3] = 0xFFFFFFFF;
}

static unsigned bit(int button)
{
    return 1u << (button - 1);
}

int main(void)
{
    PadMapConfig config;
    PadMapDevice pad;
    DIJOYSTATE state;
    wchar_t ini[MAX_PATH];
    FILE *file;
    int i;

    memset(&pad, 0, sizeof(pad));
    pad.slot = -1;
    for (i = 0; i < PADMAP_AXIS_COUNT; ++i) {
        pad.present[i] = true;
        pad.high[i] = 65535;
    }
    PadMap_Defaults(&config);

    config.layout = PADMAP_XBOX;
    rest(&state);
    check("xbox: nothing at rest", PadMap_Apply(&config, &pad, &state) == 0);
    rest(&state);
    state.lZ = 65000;
    check("xbox: left trigger", PadMap_Apply(&config, &pad, &state) == bit(11));
    rest(&state);
    state.lZ = 500;
    check("xbox: right trigger", PadMap_Apply(&config, &pad, &state) == bit(12));
    rest(&state);
    state.lRy = 0;
    state.lRx = 65535;
    check("xbox: right stick up and right", PadMap_Apply(&config, &pad, &state) == (bit(13) | bit(16)));
    rest(&state);
    state.lRx = 40000;
    check("xbox: small movement ignored", PadMap_Apply(&config, &pad, &state) == 0);

    config.layout = PADMAP_AUTO;
    rest(&state);
    state.lRx = state.lRy = 0;
    state.lZ = 0;
    state.lRz = 65535;
    check("classic: stick on z, triggers at rest ignored",
          PadMap_Apply(&config, &pad, &state) == (bit(15) | bit(14)));
    pad.present[PADMAP_AXIS_RZ] = false;
    rest(&state);
    state.lRx = 0;
    check("classic without z rotation: stick on rx", PadMap_Apply(&config, &pad, &state) == bit(15));
    pad.present[PADMAP_AXIS_RZ] = true;

    rest(&state);
    state.rgbButtons[12] = 0x80;
    state.lRz = 0;
    check("already held button is not reported as new",
          PadMap_Apply(&config, &pad, &state) == 0 && (state.rgbButtons[12] & 0x80));

    GetTempPathW(MAX_PATH, ini);
    wcscat(ini, L"efz_padmap_test.ini");
    file = _wfopen(ini, L"w");
    fputs("[General]\nLayout=Classic\nThreshold=30\n[Buttons]\nRightStickUp=0\nDPadLeft=9\nButton17=10\n"
          "LeftTrigger=99\n", file);
    fclose(file);
    PadMap_Load(&config, ini);
    DeleteFileW(ini);
    check("ini", config.layout == PADMAP_CLASSIC && config.threshold == 30 && config.stick[PADMAP_UP] == 0
                 && config.dpad[PADMAP_LEFT] == 9 && config.extra[0] == 10 && config.leftTrigger == 11
                 && config.stick[PADMAP_DOWN] == 14);
    rest(&state);
    state.rgdwPOV[0] = 27000;
    state.rgbButtons[16] = 0x80;
    state.lRz = 0;
    check("d-pad and high buttons", PadMap_Apply(&config, &pad, &state) == (bit(9) | bit(10)));

    printf(g_failures ? "%d FAILED\n" : "all passed\n", g_failures);
    return g_failures != 0;
}
