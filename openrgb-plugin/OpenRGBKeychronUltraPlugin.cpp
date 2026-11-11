/*---------------------------------------------------------*\
| OpenRGBKeychronUltraPlugin.cpp                          |
\*---------------------------------------------------------*/

#include "OpenRGBKeychronUltraPlugin.h"
#include "KeychronUltraController.h"
#include "KeychronLayouts.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <hidapi.h>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>


/*---------------------------------------------------------------------------*\
| VID and the raw-interface usage filter come from KeychronUltraController.h  |
| so that detection here and the controller's reconnect path can never drift    |
| apart and match different interfaces.                                        |
\*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*\
| The set of supported boards (PID + display name + LED layout) lives in       |
| KEYCHRON_LAYOUTS[] (KeychronLayouts.h). Every board speaks the same 0x16     |
| raw-HID protocol; only the PID, name and per-key layout differ.              |
\*---------------------------------------------------------------------------*/

OpenRGBPluginInfo OpenRGBKeychronUltraPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;
    info.Name          = "Keychron Ultra (OpenRGB + Mixed RGB)";
    info.Description    = "Direct per-key RGB control plus native Mixed RGB mode for Keychron "
                          "V- and Q-series Ultra keyboards running custom ZMK firmware.";
    info.Version        = "0.6.0";
    info.Commit         = "";
    info.URL            = "https://github.com/janjp10101-lab/zmk/tree/feature/cyan-blue-splash-overlay/openrgb-plugin";
    info.Location       = OPENRGB_PLUGIN_LOCATION_SETTINGS;
    info.Label          = "Keychron Ultra";
    info.TabIconString  = "";
    return(info);
}

unsigned int OpenRGBKeychronUltraPlugin::GetPluginAPIVersion()
{
    return(OPENRGB_PLUGIN_API_VERSION);
}

void OpenRGBKeychronUltraPlugin::Load(ResourceManagerInterface* resource_manager)
{
    rm = resource_manager;

    /*-----------------------------------------------------------------------*\
    | Safe to call again, hidapi ignores a second init. There is deliberately  |
    | no matching hid_exit() in Unload(): hidapi state is global and OpenRGB   |
    | owns it, so tearing it down from a plugin would pull it out from under   |
    | the host and every other device it has open.                             |
    \*-----------------------------------------------------------------------*/
    hid_init();

    DetectControllers();

    /*-----------------------------------------------------------------------*\
    | OpenRGB only loads a plugin once per run. Its onDetectionEnded() keeps  |
    | a plugins_loaded flag and skips loading after the first time. But every |
    | rescan calls ResourceManager::Cleanup(), and that deletes everything in |
    | rgb_controllers_hw, which is the same list RegisterRGBController() puts |
    | our devices in.                                                         |
    |                                                                         |
    | So a rescan destroys our keyboard and nobody asks us to make a new one. |
    | It disappears for the rest of the session, and the pointers we kept are |
    | now dangling, ready to be freed a second time in Unload().              |
    |                                                                         |
    | Rebuilding at the end of detection fixes both. Cleanup() has already    |
    | run by then, the device list is finished, and it fires on the first     |
    | detection as well as on every rescan.                                   |
    \*-----------------------------------------------------------------------*/
    rm->RegisterDetectionEndCallback(&OpenRGBKeychronUltraPlugin::OnDetectionEnd, this);
}

void OpenRGBKeychronUltraPlugin::OnDetectionEnd(void* arg)
{
    OpenRGBKeychronUltraPlugin* plugin = (OpenRGBKeychronUltraPlugin*)arg;

    plugin->DropDeletedControllers();
    plugin->DetectControllers();
}

/*---------------------------------------------------------------------------*\
| Drop the controllers OpenRGB has already destroyed. Do not delete them here, |
| Cleanup() owns them and has freed them already. Keeping a stale one would    |
| hand it to UnregisterRGBController(), which reads its name for the log and   |
| calls a virtual on it, and then we would free it a second time.              |
|                                                                              |
| We decide what is still alive by comparing pointers against the resource     |
| manager's own list, which Cleanup() removes ours from. Comparing a pointer   |
| is safe, so nothing freed is ever read.                                      |
\*---------------------------------------------------------------------------*/
void OpenRGBKeychronUltraPlugin::DropDeletedControllers()
{
    if(rm == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(registered_mutex);

    const std::vector<RGBController*>&          live = rm->GetRGBControllers();
    std::vector<RGBController_KeychronUltra*> alive;

    for(RGBController_KeychronUltra* rgb : registered)
    {
        if(std::find(live.begin(), live.end(), (RGBController*)rgb) != live.end())
        {
            alive.push_back(rgb);
        }
    }

    registered = alive;
}

void OpenRGBKeychronUltraPlugin::DetectControllers()
{
    std::lock_guard<std::mutex> lock(registered_mutex);

    /*-----------------------------------------------------------------------*\
    | Our devices came through the detection cycle alive, which happens when  |
    | Cleanup() did not run at all. Registering again here would show the     |
    | keyboard twice and open a second handle to it.                          |
    \*-----------------------------------------------------------------------*/
    if(!registered.empty())
    {
        return;
    }

    for(unsigned int i = 0; i < KEYCHRON_LAYOUT_COUNT; i++)
    {
        const KeychronLayout* layout = &KEYCHRON_LAYOUTS[i];

        hid_device_info* devs = hid_enumerate(KEYCHRON_ULTRA_VID, layout->pid);
        for(hid_device_info* cur = devs; cur != nullptr; cur = cur->next)
        {
            if(cur->usage_page != KEYCHRON_ULTRA_RAW_USAGE_PAGE
            || cur->usage      != KEYCHRON_ULTRA_RAW_USAGE)
            {
                continue;                               /* only the raw command interface */
            }

            hid_device* dev = hid_open_path(cur->path);
            if(dev == nullptr)
            {
                continue;
            }

            KeychronUltraController* ctrl = new KeychronUltraController(dev, cur->path,
                                                                           layout->pid,
                                                                           layout->led_count);

            /*---------------------------------------------------------------*\
            | Stock firmware does not answer this at all and returns 0, so one |
            | call rules out both stock firmware and a PID whose layout says a |
            | different number of LEDs.                                        |
            \*---------------------------------------------------------------*/
            if(ctrl->GetLEDCount() != layout->led_count)
            {
                delete ctrl;
                continue;
            }

            /*---------------------------------------------------------------*\
            | Turn away firmware newer than we know how to talk to. A device   |
            | that does not answer reports 0, and we let that through, because |
            | the LED count above already proved it is our firmware and one    |
            | lost reply should not cost the user their keyboard.              |
            \*---------------------------------------------------------------*/
            unsigned int proto = ctrl->GetProtocolVersion();
            if(proto > KEYCHRON_ULTRA_PROTOCOL_VERSION)
            {
                delete ctrl;
                continue;
            }

            RGBController_KeychronUltra* rgb = new RGBController_KeychronUltra(ctrl, layout);
            rm->RegisterRGBController(rgb);
            registered.push_back(rgb);

            /*---------------------------------------------------------------*\
            | If a rescan just destroyed the previous controller, put the      |
            | colours back. Does nothing on the first detection of the run.    |
            \*---------------------------------------------------------------*/
            ctrl->RestoreLastFrame();
        }
        hid_free_enumeration(devs);
    }
}

QWidget* OpenRGBKeychronUltraPlugin::GetWidget()
{
    QWidget* panel = new QWidget();
    QVBoxLayout* root = new QVBoxLayout(panel);

    QLabel* intro = new QLabel(
        "Keychron Ultra Mixed RGB\\n"
        "Configure the three firmware-rendered layers. The orange mask is the "
        "preconfigured 23-key layout; the remaining 59 keys carry the cyan spiral "
        "and yellow reactive splash.");
    intro->setWordWrap(true);
    root->addWidget(intro);

    QGroupBox* orange_box = new QGroupBox("Layer 1 — fixed orange");
    QVBoxLayout* orange_layout = new QVBoxLayout(orange_box);
    QComboBox* orange_mode = new QComboBox();
    orange_mode->addItem("Static orange (23 keys)", 1);
    orange_mode->addItem("Off", 0);
    orange_layout->addWidget(orange_mode);
    root->addWidget(orange_box);

    QGroupBox* cyan_box = new QGroupBox("Layer 2 — cyan-blue spiral");
    QFormLayout* cyan_layout = new QFormLayout(cyan_box);
    QComboBox* cyan_mode = new QComboBox();
    cyan_mode->addItem("Cyan-blue spiral", 1);
    cyan_mode->addItem("Off", 0);
    QSlider* cyan_speed = new QSlider(Qt::Horizontal);
    cyan_speed->setRange(1, 255);
    cyan_speed->setValue(127);
    QLabel* cyan_speed_value = new QLabel("127");
    QHBoxLayout* cyan_speed_row = new QHBoxLayout();
    cyan_speed_row->addWidget(cyan_speed, 1);
    cyan_speed_row->addWidget(cyan_speed_value);
    cyan_layout->addRow("Effect", cyan_mode);
    cyan_layout->addRow("Speed", cyan_speed_row);
    QObject::connect(cyan_speed, &QSlider::valueChanged, [cyan_speed_value](int value) {
        cyan_speed_value->setText(QString::number(value));
    });
    root->addWidget(cyan_box);

    QGroupBox* yellow_box = new QGroupBox("Layer 3 — yellow reactive splash");
    QFormLayout* yellow_layout = new QFormLayout(yellow_box);
    QComboBox* yellow_mode = new QComboBox();
    yellow_mode->addItem("Yellow reactive splash", 1);
    yellow_mode->addItem("Off", 0);
    QSlider* yellow_speed = new QSlider(Qt::Horizontal);
    yellow_speed->setRange(1, 255);
    yellow_speed->setValue(127);
    QLabel* yellow_speed_value = new QLabel("127");
    QHBoxLayout* yellow_speed_row = new QHBoxLayout();
    yellow_speed_row->addWidget(yellow_speed, 1);
    yellow_speed_row->addWidget(yellow_speed_value);
    yellow_layout->addRow("Effect", yellow_mode);
    yellow_layout->addRow("Speed", yellow_speed_row);
    QObject::connect(yellow_speed, &QSlider::valueChanged, [yellow_speed_value](int value) {
        yellow_speed_value->setText(QString::number(value));
    });
    root->addWidget(yellow_box);

    QHBoxLayout* actions = new QHBoxLayout();
    QPushButton* apply = new QPushButton("Apply layers");
    QPushButton* save = new QPushButton("Save to keyboard");
    actions->addWidget(apply);
    actions->addWidget(save);
    root->addLayout(actions);

    QLabel* status = new QLabel("Select Mixed RGB mode on the Keychron device, then apply.");
    status->setWordWrap(true);
    root->addWidget(status);
    root->addStretch(1);

    QObject::connect(apply, &QPushButton::clicked, [this, orange_mode, cyan_mode,
                                                    yellow_mode, cyan_speed,
                                                    yellow_speed, status]() {
        const bool ok = ApplyMixedLayers(orange_mode->currentData().toInt() != 0,
                                         cyan_mode->currentData().toInt() != 0,
                                         yellow_mode->currentData().toInt() != 0,
                                         (unsigned char)cyan_speed->value(),
                                         (unsigned char)yellow_speed->value());
        status->setText(ok ? "Mixed RGB layers applied" :
                             "Could not apply layers — check that the V1 firmware is updated");
    });

    QObject::connect(save, &QPushButton::clicked, [this, status]() {
        status->setText(SaveMixedLayers() ? "Settings saved to keyboard" :
                                            "Save failed — apply the layers first");
    });

    return panel;
}

bool OpenRGBKeychronUltraPlugin::ApplyMixedLayers(bool orange, bool cyan, bool yellow,
                                                   unsigned char cyan_speed,
                                                   unsigned char yellow_speed)
{
    std::lock_guard<std::mutex> lock(registered_mutex);
    if(rm == nullptr || registered.empty())
        return false;

    const std::vector<RGBController*>& live = rm->GetRGBControllers();
    bool applied = false;
    bool success = true;

    for(RGBController_KeychronUltra* rgb : registered)
    {
        if(std::find(live.begin(), live.end(), (RGBController*)rgb) == live.end())
            continue;

        applied = true;
        success = rgb->ConfigureMixedLayer(0, orange ? 1 : 0, 20, 255, 127) && success;
        success = rgb->ConfigureMixedLayer(1, cyan ? 3 : 0, 138, 255, cyan_speed) && success;
        success = rgb->ConfigureMixedLayer(2, yellow ? 25 : 0, 43, 255, yellow_speed) && success;
        rgb->ActivateMixedRGB();
    }

    return applied && success;
}

bool OpenRGBKeychronUltraPlugin::SaveMixedLayers()
{
    std::lock_guard<std::mutex> lock(registered_mutex);
    if(rm == nullptr || registered.empty())
        return false;

    const std::vector<RGBController*>& live = rm->GetRGBControllers();
    bool saved = false;
    bool success = true;

    for(RGBController_KeychronUltra* rgb : registered)
    {
        if(std::find(live.begin(), live.end(), (RGBController*)rgb) == live.end())
            continue;

        saved = true;
        success = rgb->SaveMixedRGB() && success;
    }

    return saved && success;
}

QMenu* OpenRGBKeychronUltraPlugin::GetTrayMenu()
{
    return(nullptr);
}

void OpenRGBKeychronUltraPlugin::Unload()
{
    /*-----------------------------------------------------------------------*\
    | Stop the callback first. UnloadPlugins() can dlclose this plugin, and a |
    | detection cycle afterwards would call into unmapped code.               |
    \*-----------------------------------------------------------------------*/
    if(rm != nullptr)
    {
        rm->UnregisterDetectionEndCallback(&OpenRGBKeychronUltraPlugin::OnDetectionEnd, this);
    }

    /*-----------------------------------------------------------------------*\
    | Anything a rescan already destroyed is not ours to unregister or free.  |
    | Called before taking the lock below - it takes the same one.            |
    \*-----------------------------------------------------------------------*/
    DropDeletedControllers();

    std::lock_guard<std::mutex> lock(registered_mutex);

    /*-----------------------------------------------------------------------*\
    | Take every device out of the list before freeing any of them.           |
    |                                                                         |
    | OpenRGB's SDK server keeps a reference to the same vector the resource  |
    | manager stores controllers in, and SendReply_ControllerData() calls     |
    | GetDeviceDescription() on whatever it finds there without taking any    |
    | lock. If a client such as Artemis asks for device data while we are     |
    | deleting, the server can read an object we have already freed. That is  |
    | a real crash and we have seen it, a SIGSEGV inside memcpy under         |
    | GetDeviceDescription during shutdown.                                   |
    |                                                                         |
    | Unregistering first means no new request can find these devices.        |
    \*-----------------------------------------------------------------------*/
    for(RGBController_KeychronUltra* rgb : registered)
    {
        if(rm != nullptr)
        {
            rm->UnregisterRGBController(rgb);
        }
    }

    /*-----------------------------------------------------------------------*\
    | A request that was already running when we unregistered still holds its |
    | pointer, and there is no way for a plugin to wait for it: the server    |
    | offers nothing to synchronise on. Give it a moment to finish instead.   |
    | This only narrows the race, it does not close it, and the only real fix |
    | belongs upstream. Unload runs on shutdown, so the wait costs nothing.   |
    \*-----------------------------------------------------------------------*/
    if(!registered.empty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for(RGBController_KeychronUltra* rgb : registered)
    {
        delete rgb;                                     /* also closes the HID handle */
    }
    registered.clear();
}
