#pragma once
#include "InputHandler.h"
#include "LCDUtils.h"
#include "MenuTypes.h"

namespace TapuinoNext
{
    class Updater : public ActionCallback
    {
      public:
        Updater(LCDUtils* lcdUtils);
        void OnAction();

      private:
        LCDUtils* lcdUtils;

        void PerformUpdate();
        void OnProgress(size_t progress, size_t size);
    };
} // namespace TapuinoNext