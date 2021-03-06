#include <core/ui_manager.h>
#include <control/ui_label.h>
#include <control/ui_viewport.h>


#include <interface/ui_default_config.h>
#include <interface/ui_ctrlinfolist.h>
#include "mycontrol.h"

const auto xul = u8R"xml(
<?xml version="1.0"?>
<window clearcolor="white" title="lui demo">
  <spacer flex="1"/>
  <hbox flex="1">
    <spacer flex="1"/>
    <myctrl flex="1" value="cyan"/>
    <spacer flex="1"/>
  </hbox>
  <spacer flex="1"/>
</window>
)xml";

namespace Demo { struct Config final : LongUI::CUIDefaultConfigure {
    // register control
    void RegisterControl(LongUI::ControlInfoList& list) noexcept override {
        // MyControl
        list.push_back(&Demo::MyControl::s_meta);
    }
};}

int main() {
    int code = -1;
    Demo::Config config;
    if (UIManager.Initialize(&config)) {
        {
            LongUI::UIControl::ControlMakingBegin();
            LongUI::UIViewport viewport;
            viewport.SetXul(xul);
            LongUI::UIControl::ControlMakingEnd();

            viewport.GetWindow()->ShowWindow();
            code = LongUI::IntCode(viewport.GetWindow()->Exec());
        }
        UIManager.Uninitialize();
    }
    return code;
}