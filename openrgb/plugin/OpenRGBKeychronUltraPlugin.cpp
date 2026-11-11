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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QSlider>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QMessageBox>


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
    info.Name          = "Keychron Ultra Mixed RGB";
    info.Description    = "Three-layer firmware-rendered Mixed RGB control for Keychron Ultra keyboards "
                          "keyboards running custom ZMK firmware (issue #893).";
    info.Version        = "0.1.0";
    info.Commit         = "";
    info.URL            = "https://github.com/janjp10101-lab/zmk/tree/mixed-rgb-overlay-test/openrgb/plugin";
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
            if(proto < KEYCHRON_ULTRA_PROTOCOL_VERSION)
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
    QLabel* title = new QLabel("Keychron Mixed RGB — 3 layers");
    title->setStyleSheet("font-weight:600;font-size:16px;");
    root->addWidget(title);

    RGBController_KeychronUltra* target = nullptr;
    {
        std::lock_guard<std::mutex> lock(registered_mutex);
        if(!registered.empty()) target = registered.front();
    }
    if(target == nullptr)
    {
        root->addWidget(new QLabel("Connect your V1 Ultra and run Rescan Devices."));
        root->addStretch();
        return(panel);
    }

    const KeychronLayout* layout = target->GetLayout();
    std::vector<unsigned char> masks;
    if(!target->GetMixedRegions(masks))
    {
        root->addWidget(new QLabel("Firmware did not answer the Mixed RGB configuration query. Install protocol v2 firmware and rescan."));
        root->addStretch();
        return(panel);
    }

    QCheckBox* enabled[3];
    QSlider* speed[3];
    const char* layerNames[3] = {
        "Layer 1 — static orange (23 keys)",
        "Layer 2 — cyan-blue spiral",
        "Layer 3 — yellow reactive splash"
    };
    const unsigned char effects[3] = {23, 3, 22};
    const unsigned char hues[3] = {21, 137, 43};

    for(int i = 0; i < 3; ++i)
    {
        QGroupBox* group = new QGroupBox(layerNames[i]);
        QHBoxLayout* row = new QHBoxLayout(group);
        enabled[i] = new QCheckBox("Enabled");
        enabled[i]->setChecked(true);
        speed[i] = new QSlider(Qt::Horizontal);
        speed[i]->setRange(1, 255);
        speed[i]->setValue(127);
        row->addWidget(enabled[i]);
        row->addWidget(new QLabel("Speed"));
        row->addWidget(speed[i], 1);
        root->addWidget(group);
    }

    root->addWidget(new QLabel("Select exactly 23 keys for the orange layer. The physical keycaps remain grey."));
    QListWidget* keyList = new QListWidget();
    keyList->setMinimumHeight(220);
    for(unsigned int i = 0; i < layout->led_count; ++i)
    {
        QListWidgetItem* item = new QListWidgetItem(QString::fromUtf8(layout->led_names[i]), keyList);
        item->setData(Qt::UserRole, (int)i);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState((masks[i] & 0x01) ? Qt::Checked : Qt::Unchecked);
    }
    root->addWidget(keyList, 1);

    QLabel* countLabel = new QLabel();
    auto updateCount = [keyList, countLabel]() {
        unsigned int count = 0;
        for(int i = 0; i < keyList->count(); ++i)
            if(keyList->item(i)->checkState() == Qt::Checked) ++count;
        countLabel->setText(QString("%1 / 23 orange keys selected").arg(count));
    };
    QObject::connect(keyList, &QListWidget::itemChanged, panel,
        [updateCount](QListWidgetItem*) { updateCount(); });
    updateCount();
    root->addWidget(countLabel);

    QPushButton* apply = new QPushButton("Apply and save Mixed RGB");
    root->addWidget(apply);
    QObject::connect(apply, &QPushButton::clicked, panel,
        [target, keyList, enabled0 = enabled[0], enabled1 = enabled[1], enabled2 = enabled[2],
         speed0 = speed[0], speed1 = speed[1], speed2 = speed[2]]() {
            unsigned int selected = 0;
            std::vector<unsigned char> newMasks((size_t)keyList->count(), 0x06);
            for(int i = 0; i < keyList->count(); ++i)
            {
                if(keyList->item(i)->checkState() == Qt::Checked)
                {
                    newMasks[(size_t)i] = enabled0->isChecked() ? 0x01 : 0x06;
                    ++selected;
                }
            }
            if(selected != 23)
            {
                QMessageBox::warning(nullptr, "Mixed RGB", "Select exactly 23 orange keys before applying.");
                return;
            }

            const bool layerEnabled[3] = {
                enabled0->isChecked(), enabled1->isChecked(), enabled2->isChecked()
            };
            const int layerSpeed[3] = {
                speed0->value(), speed1->value(), speed2->value()
            };
            const unsigned char effects[3] = {23, 3, 22};
            const unsigned char hues[3] = {21, 137, 43};

            if(!target->SetMixedRegions(newMasks))
            {
                QMessageBox::critical(nullptr, "Mixed RGB", "Failed to write the LED layer masks.");
                return;
            }
            for(unsigned int layer = 0; layer < 3; ++layer)
            {
                if(!target->SetMixedEffect(layer,
                    layerEnabled[layer] ? effects[layer] : 0,
                    hues[layer], 255, (unsigned char)layerSpeed[layer]))
                {
                    QMessageBox::critical(nullptr, "Mixed RGB", "Failed to update a layer. No save was requested.");
                    return;
                }
            }
            if(!target->SetMixedMode(true) || !target->SaveMixed())
            {
                QMessageBox::critical(nullptr, "Mixed RGB", "Could not activate or save Mixed RGB.");
                return;
            }
            QMessageBox::information(nullptr, "Mixed RGB", "Three-layer configuration sent and saved to the keyboard.");
        });
    return(panel);
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
