/*---------------------------------------------------------*\
| RGBController_KeychronUltra.h                           |
|                                                           |
|   OpenRGB RGBController wrapper for the Keychron V6 Ultra  |
|   8K custom-firmware OpenRGB direct-control support.       |
\*---------------------------------------------------------*/

#pragma once

#include <mutex>
#include <vector>

#include "RGBController.h"
#include "KeychronUltraController.h"
#include "KeychronLayouts.h"

class RGBController_KeychronUltra : public RGBController
{
public:
    RGBController_KeychronUltra(KeychronUltraController* controller_ptr,
                                  const KeychronLayout* layout_ptr);
    ~RGBController_KeychronUltra();

    void        SetupZones();
    void        ResizeZone(int zone, int new_size);

    void        DeviceUpdateLEDs();
    void        UpdateZoneLEDs(int zone);
    void        UpdateSingleLED(int led);

    void        DeviceUpdateMode();
    void        SetCustomMode();
    bool        GetMixedInfo(unsigned int& layers, unsigned int& effects_per_layer);
    bool        GetMixedRegions(std::vector<unsigned char>& masks);
    bool        SetMixedRegions(const std::vector<unsigned char>& masks);
    bool        SetMixedEffect(unsigned int layer, unsigned char effect, unsigned char hue,
                               unsigned char sat, unsigned char speed);
    bool        SetMixedMode(bool enable);
    bool        SaveMixed();
    const KeychronLayout* GetLayout() const { return layout; }

private:
    void        UpdateLEDsLocked();         /* caller must hold controller_mutex */

    KeychronUltraController* controller;
    const KeychronLayout*      layout;
    matrix_map_type            matrix;      /* backs the zone's matrix_map pointer */

    /*-----------------------------------------------------------------------*\
    | Guards `controller` against the base class's DeviceCallThread - see the |
    | destructor for why that thread can outlive the pointer.                 |
    \*-----------------------------------------------------------------------*/
    std::mutex                 controller_mutex;
};
