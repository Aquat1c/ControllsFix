#ifndef CONTROLLS_FIX_PADMAP_H
#define CONTROLLS_FIX_PADMAP_H

#include <windows.h>
#include <dinput.h>
#include <stdbool.h>

enum { PADMAP_AUTO, PADMAP_XBOX, PADMAP_CLASSIC };
enum { PADMAP_AXIS_Z, PADMAP_AXIS_RX, PADMAP_AXIS_RY, PADMAP_AXIS_RZ, PADMAP_AXIS_COUNT };
enum { PADMAP_UP, PADMAP_DOWN, PADMAP_LEFT, PADMAP_RIGHT };

typedef struct PadMapConfig {
    int layout;
    int threshold;
    int leftTrigger, rightTrigger;
    int stick[4];
    int dpad[4];
    int extra[16];
} PadMapConfig;

typedef struct PadMapDevice {
    void *device;
    bool xinput;
    bool present[PADMAP_AXIS_COUNT];
    LONG low[PADMAP_AXIS_COUNT], high[PADMAP_AXIS_COUNT];
    int slot;
} PadMapDevice;

void PadMap_Defaults(PadMapConfig *config);
void PadMap_Load(PadMapConfig *config, const wchar_t *iniPath);
void PadMap_Describe(LPDIRECTINPUTDEVICE2A device, PadMapDevice *out);
unsigned PadMap_Apply(const PadMapConfig *config, PadMapDevice *device, DIJOYSTATE *state);

#endif
