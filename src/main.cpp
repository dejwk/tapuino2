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
        flexViewport(viewport, 2,
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

#include "roo_collections/flat_small_hash_set.h"


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
  // roo_logging::SetVLOGLevel("*", 1);

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
