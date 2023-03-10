#pragma once

#include "ErrorCodes.h"
// #include "LCD.h"

namespace tapuino {
class PlayerActivity;
}

namespace TapuinoNext
{
    class LCDUtils
    {
      public:
        LCDUtils(tapuino::PlayerActivity& player);

        // LCDUtils(LCD* lcd);
        // LCD* GetLCD();
        // void Title(const char* msg, char indicator);
        // void Title(const char* msg);
        // void Status(const char* msg, char indicator);
        // void Status(const char* msg);
        void Error(const char* title, ErrorCodes errorCode);
        void Error(const char* title, const char* msg);

        void ShowProgress(bool motor, uint16_t counter, uint32_t total, uint32_t read);

        // void PlayUI(bool motor, uint16_t counter, uint32_t tickerTime);
        // void ShowFile(const char* fileName, bool title, bool isDirectory);
        // void ShowFile(const char* fileName, bool title);
        // void ScrollFileName(uint32_t tickerTime, uint32_t tickerHoldTime, bool isDirectory);
        // char Spinner();

      private:
        tapuino::PlayerActivity& player_;
        unsigned long lastTick;
        unsigned long lastHoldTick;
        const char* fileNameScroll;
        char fileNameBuf[256];

        void Line(uint8_t line, const char* msg, char indicator);
    };
} // namespace TapuinoNext