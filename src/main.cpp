// #ifdef ROO_TESTING

// #include <iostream>

// #include "roo_testing/devices/display/ili9341/ili9341spi.h"
// #include "roo_testing/devices/display/ili9486/ili9486spi.h"
// #include "roo_testing/devices/display/ili9488/ili9488spi.h"
// #include "roo_testing/devices/microcontroller/esp32/fake_esp32.h"
// #include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
// #include "roo_testing/transducers/ui/viewport/flex_viewport.h"
// #include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"
// #include "roo_testing/transducers/voltage/voltage.h"
// #include "roo_testing/transducers/wifi/wifi.h"

// struct Emulator {
//   roo_testing_transducers::FltkViewport viewport;
//   roo_testing_transducers::FlexViewport flexViewport;

//   // FakeIli9486Spi display;
//   FakeIli9341Spi display;
//   // FakeIli9488Spi display;
//   FakeXpt2046Spi touch;

//   Emulator()
//       : viewport(),
//         flexViewport(viewport, 1,
//                      roo_testing_transducers::FlexViewport::kRotationRight),
//         display(flexViewport),
//         touch(flexViewport, FakeXpt2046Spi::Calibration(269, 249, 3829, 3684,
//                                                         true, false, false)) {
//     auto& board = FakeEsp32();
//     board.attachSpiDevice(display, 18, 19, 23);
//     // board.gpio.attachOutput(5, display.cs());
//     // board.gpio.attachOutput(2, display.dc());
//     // board.gpio.attachOutput(4, display.rst());

//     board.gpio.attachOutput(5, display.cs());
//     board.gpio.attachOutput(17, display.dc());
//     board.gpio.attachOutput(27, display.rst());

//     // board.attachSpiDevice(display, 14, 12, 13);
//     // board.gpio.attachOutput(15, display.cs());
//     // board.gpio.attachOutput(33, display.dc());
//     // board.gpio.attachOutput(26, display.rst());

//     // board.attachSpiDevice(touch, 18, 19, 23);
//     board.gpio.attachOutput(2, touch.cs());

//     board.gpio.attachInput(25, roo_testing_transducers::Vcc33());
//   }
// } emulator;

// #endif

// // #include <Wire.h>
// #include <Arduino.h>

// #include "SPI.h"
// #include "roo_display.h"
// #include "roo_display/driver/ili9341.h"
// #include "roo_display/driver/touch_xpt2046.h"
// #include "roo_display/shape/basic.h"

// using namespace roo_display;

// int pinLcdBl = 16;

// // static const int pinLcdDc = 17;
// // static const int pinLcdReset = 27;

// // static const int pinLcdCs = 5;
// // static const int pinTouchCs = 2;
// // static const int pinSdCs = 4;

// Ili9341spi<5, 17, 27> display_device;
// TouchXpt2046<2> touch_device;

// Display display(display_device, touch_device,
//                 TouchCalibration(269, 249, 3829, 3684,
//                                  Orientation::LeftDown()));

// void setup() {
//   SPI.begin();
//   display.init(color::DarkGray);
//   Serial.begin(115200);
//   pinMode(pinLcdBl, OUTPUT);
//   ledcAttachPin(pinLcdBl, 0);
//   ledcSetup(0, 30000, 8);
//   ledcWrite(0, 200);
// }

// int16_t x = -1;
// int16_t y = -1;
// bool was_touched = false;

// void loop(void) {
//   int16_t old_x = x;
//   int16_t old_y = y;
//   bool touched = display.getTouch(x, y);
//   if (touched) {
//     was_touched = true;
//     DrawingContext dc(display);
//     dc.draw(Line(0, y, display.width() - 1, y, color::Red));
//     dc.draw(Line(x, 0, x, display.height() - 1, color::Red));
//     if (x != old_x) {
//       dc.draw(
//           Line(old_x, 0, old_x, display.height() - 1, dc.getBackgroundColor()));
//     }
//     if (y != old_y) {
//       dc.draw(
//           Line(0, old_y, display.width() - 1, old_y, dc.getBackgroundColor()));
//     }
//     dc.draw(Line(0, y, display.width() - 1, y, color::Red));
//     dc.draw(Line(x, 0, x, display.height() - 1, color::Red));
//   } else {
//     if (was_touched) {
//       was_touched = false;
//       DrawingContext dc(display);
//       dc.draw(
//           Line(0, old_y, display.width() - 1, old_y, dc.getBackgroundColor()));
//       dc.draw(
//           Line(old_x, 0, old_x, display.height() - 1, dc.getBackgroundColor()));
//     }
//   }
// }

#include <Arduino.h>

#ifdef ROO_TESTING

#include <iostream>

#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/devices/display/ili9486/ili9486spi.h"
#include "roo_testing/devices/microcontroller/esp32/fake_esp32.h"
#include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"
#include "roo_testing/transducers/voltage/voltage.h"
#include "roo_testing/transducers/wifi/wifi.h"

struct Emulator {
  roo_testing_transducers::FltkViewport viewport;
  roo_testing_transducers::FlexViewport flexViewport;
  roo_testing_transducers::wifi::Environment wifi;

  // FakeIli9486Spi display;
  FakeIli9341Spi display;
  FakeXpt2046Spi touch;

  Emulator()
      : viewport(),
        flexViewport(viewport, 1,
                     roo_testing_transducers::FlexViewport::kRotationRight),
        display(flexViewport),
        touch(flexViewport,
              FakeXpt2046Spi::Calibration(
                  269, 249, 3829, 3684, true, false, false)) {
    auto& board = FakeEsp32();
    board.attachSpiDevice(display, 18, 19, 23);
    board.gpio.attachOutput(5, display.cs());
    board.gpio.attachOutput(17, display.dc());
    board.gpio.attachOutput(27, display.rst());

    board.attachSpiDevice(touch, 18, 19, 23);
    board.gpio.attachOutput(2, touch.cs());

    board.gpio.attachInput(25, roo_testing_transducers::Vcc33());

    auto ap1 = std::unique_ptr<roo_testing_transducers::wifi::AccessPoint>(
        new roo_testing_transducers::wifi::AccessPoint(
            roo_testing_transducers::wifi::MacAddress(1, 1, 1, 1, 1, 1),
            "dawidk"));
    ap1->setAuthMode(roo_testing_transducers::wifi::AUTH_WEP);
    ap1->setPasswd("chomik");
    wifi.addAccessPoint(std::move(ap1));

    auto ap2 = std::unique_ptr<roo_testing_transducers::wifi::AccessPoint>(
        new roo_testing_transducers::wifi::AccessPoint(
            roo_testing_transducers::wifi::MacAddress(1, 1, 1, 1, 1, 2),
            "other 1"));
    wifi.addAccessPoint(std::move(ap2));

    auto ap3 = std::unique_ptr<roo_testing_transducers::wifi::AccessPoint>(
        new roo_testing_transducers::wifi::AccessPoint(
            roo_testing_transducers::wifi::MacAddress(1, 1, 1, 1, 1, 3),
            "oythejr 2"));

    wifi.addAccessPoint(std::move(ap3));

    board.setWifiEnvironment(wifi);
  }
} emulator;

#endif

#include <Arduino.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>

#undef min
#undef max

#include <algorithm>

using std::max;
using std::min;

#include "config.h"
#include "index/mem_index.h"
#include "io/sd.h"
#include "memory/mem_buffer.h"
#include "roo_display.h"
#include "roo_display/core/orientation.h"
#include "roo_scheduler.h"
#include "roo_windows/activities/keyboard.h"
#include "roo_windows/keyboard_layout/en_us.h"
#include "roo_windows/widgets/text_field.h"
#include "ui/tapuino.h"

static const int pinLcdDc = 17;
static const int pinLcdReset = 27;

static const int pinLcdCs = 5;
static const int pinTouchCs = 2;
static const int pinSdCs = 4;

static const int pinLcdBl = 16;

// #include "roo_display/driver/ili9486.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/touch_xpt2046.h"

#include "roo_display/products/makerfabs/esp32_tft_capacitive_35.h"
// #include "roo_display/products/makerfabs/esp32s3_parallel_ips_capacitive_43.h"

roo_display::Orientation orientation =
    roo_display::Orientation::RotatedByCount(-1);

// roo_display::Ili9486spi<pinLcdCs, pinLcdDc, pinLcdReset> display_device(
//     orientation);

roo_display::Ili9341spi<pinLcdCs, pinLcdDc, pinLcdReset> display_device(
    orientation);
roo_display::TouchXpt2046<pinTouchCs> touch_device;

// roo_display::TouchXpt2046<pinTouchCs> touch_device(
//     roo_display::TouchCalibration(269, 249, 3829, 3684,
// roo_display::Orientation::LeftDown()));
// roo_display::TouchCalibration(126, 159, 3918, 3866,
//                               roo_display::Orientation::RightDown()));

tapuino::Sd sd(pinSdCs, SPI);
roo_scheduler::Scheduler scheduler;

using namespace roo_windows;

tapuino::MemIndex mem_index;

// roo_display::products::makerfabs::Esp32s3ParallelIpsCapacitive43 tft(orientation);
// roo_display::products::makerfabs::Esp32TftCapacitive35 tft(orientation);

roo_display::Display display(display_device, touch_device,
    roo_display::TouchCalibration(269, 249, 3829, 3684,
roo_display::Orientation::LeftDown()));

// roo_display::Display display(tft);

Environment env;
roo_windows::Application app(&env, display, scheduler);

Keyboard kb(env, kbEngUS());
TextFieldEditor editor(scheduler, kb);

tapuino::Tapuino tp(env, scheduler, sd, mem_index);

char* __stack_start;

void setup() {
  char stack;
  __stack_start = &stack;
  // memset(buf.idx_buffer, 0, 105 * 1024);
//   heap_caps_print_heap_info(0);
//   tapuino::membuf::Allocate();
//   mem_index.init();
  //heap_caps_print_heap_info(0);
  Serial.begin(115200);
  roo_glog::SetVLOGLevel("*", 1);

  // Serial.printf("free heap: %d", ESP.getFreeHeap());
  // heap_caps_print_heap_info(0);
  pinMode(pinLcdBl, OUTPUT);
  ledcAttachPin(pinLcdBl, 0);
  ledcSetup(0, 30000, 8);
  ledcWrite(0, 200);

  Serial.println("Initializing SPI...");
  SPI.begin(18, 19, 23, -1);
  Serial.println("Initializing the display...");
  // tft.initTransport();
  display.init();

  tp.start(app);
}

long m = 0;

void loop() {
  if (millis() > m) {
    // LOG(INFO) << "free heap: " << ESP.getFreeHeap();
    // LOG(INFO) << "stack high watermark: " <<
    uxTaskGetStackHighWaterMark(nullptr);
    // heap_caps_print_heap_info(0);
    m = millis() + 1000;
  }
  app.tick();
  scheduler.executeEligibleTasks(1);
}
