#pragma once
#include <inttypes.h>
#include <vector>

#include "MenuTypes.h"
#include "OptionTypes.h"

namespace TapuinoNext
{
    class Options
    {
      public:
        Options(IChangeNotify* notify, ActionCallback* updateCallback);
        void LoadOptions();
        void SaveOptions();
        void OnAction();

        ToggleOption ntscPAL;
        ToggleOption autoPlay;
        ToggleOption backlight;
        EnumOption machineType;

      protected:
        const char* TagIdToString(OptionTagId id);
        std::vector<IOptionType*> allOptions;
    };
} // namespace TapuinoNext
