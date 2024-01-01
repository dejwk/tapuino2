#ifdef ESP32

#include <iostream>
#include <string>
#include <vector>

#include "FS.h"
#include "core/include/Lang.h"
#include "core/include/Options.h"
// #include "SD_MMC.h"
#include "core/include/OptionEventHandler.h"
#include "core/include/config.h"

using namespace std;
using namespace TapuinoNext;

#include "FS.h"
#include "core/include/ESP32TapLoader.h"
#include "core/include/ESP32TapRecorder.h"
#include "core/include/Updater.h"
#include "core/include/Version.h"

OptionEventHandler optionEventHander;

void testPins() {
  pinMode(C64_SENSE_PIN, OUTPUT);
  digitalWrite(C64_SENSE_PIN, LOW);

  pinMode(C64_MOTOR_PIN, INPUT_PULLDOWN);

  pinMode(C64_READ_PIN, OUTPUT);
  digitalWrite(C64_READ_PIN, LOW);

  pinMode(C64_WRITE_PIN, INPUT_PULLDOWN);

  char buf[51];
  memset(buf, 0, 51);
  while (true) {
    int w = digitalRead(C64_WRITE_PIN);
    int m = digitalRead(C64_MOTOR_PIN);
    int s = digitalRead(C64_SENSE_PIN);
    int r = digitalRead(C64_READ_PIN);

    snprintf(buf, 50, "W-%c M-%c S-%c R-%c", w ? 'H' : 'L', m ? 'H' : 'L',
             s ? 'H' : 'L', r ? 'H' : 'L');
    // lcdUtils.Title(buf);

    delay(500);
  }
}

bool initTapuino() {
  Serial.begin(115200);
  Serial.println(S_INIT);
  // theLCD.Init(I2C_DISP_ADDR);

  // lcdUtils.Title("TapuinoNext");
  Serial.println("TapuinoNext");

  // testEncoderPins();
  //  theInputHandler.Init();

  char version[128];
  memset(version, 0, 128);
  snprintf(version, 128, "%s", FW_VERSION);
  // lcdUtils.Status(version);
  Serial.println(version);
  delay(2000);

  // lcdUtils.Status(S_INIT);
  Serial.println("Starting SD init");

  // if (!theFileLoader.Init())
  // {
  //     Serial.println(S_INIT_FAILED);
  //     // lcdUtils.Status(S_INIT_FAILED);
  //     return false;
  // }
  // Serial.println(S_INIT_OK);
  // // lcdUtils.Status(S_INIT_OK);
  // delay(1000);
  // return (true);
  return true;
}

MenuEntry mainMenuEntries[] = {
    {MenuEntryType::IndexEntry, S_INSERT_TAP, NULL},
    {MenuEntryType::IndexEntry, S_RECORD_TAP, NULL},
    {MenuEntryType::IndexEntry, S_OPTIONS, NULL},
};

TheMenu mainMenu = {S_MAIN_MENU, (MenuEntry*)mainMenuEntries, 3, 0, NULL};

// void setup()
// {
//     if (!initTapuino())
//         return;

//     MenuHandler menu(&lcdUtils, &theInputHandler);
//     Updater updater(&lcdUtils, &theInputHandler, &theFileLoader);

//     Options options(&theFileLoader, &optionEventHander, &menu, &updater);

//     FlipBuffer flipBuffer(4096);
//     ErrorCodes ret = flipBuffer.Init();
//     if (ret != ErrorCodes::OK)
//     {
//         lcdUtils.Error(S_OUT_OF_MEMORY, ret);
//         while (1) {}
//     }

//     UtilityCollection utilityCollection(&lcdUtils, &theInputHandler,
//     &theFileLoader, &options, &flipBuffer); LoadSelector
//     lsel(&utilityCollection); RecordSelector rsel(&utilityCollection);
//     options.LoadOptions();

//     while (true)
//     {
//         int index = menu.Display(&mainMenu);
//         switch (index)
//         {
//             case 0:
//             {
//                 lsel.OnAction();
//                 break;
//             }
//             case 1:
//             {
//                 rsel.OnAction();
//                 break;
//             }
//             case 2:
//             {
//                 options.OnAction();
//                 break;
//             }
//         }
//     }
// }

// void loop()
// {
// }

#endif