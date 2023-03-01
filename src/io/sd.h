#pragma once

#include "SD.h"
#include "SPI.h"
#include "roo_scheduler.h"

namespace tapuino {

// Uses the global SD singleton.
class Sd {
 public:
  Sd(const Sd&) = delete;
  Sd(int sd_cs_pin = SS, decltype(SPI)& spi = SPI, uint32_t freq = 800000)
      : spi_(spi), sd_cs_pin_(sd_cs_pin), freq_(freq), fs_(SD), refcount_(0) {
    pinMode(sd_cs_pin, OUTPUT);
    digitalWrite(sd_cs_pin, HIGH);
  }

  FS& fs() const { return fs_; }

  bool mount() {
    if (refcount_ == 0) {
      Serial.println("Mounting the card");
      digitalWrite(sd_cs_pin_, LOW);
  Serial.printf("free heap: %d", ESP.getFreeHeap());
      bool ok = fs_.begin(sd_cs_pin_, spi_);//, freq_);
  Serial.printf("free heap: %d", ESP.getFreeHeap());
      digitalWrite(sd_cs_pin_, HIGH);
      if (!ok) return false;
      
    }
    ++refcount_;
    Serial.printf("Refcount: %d\n", refcount_);
    return true;
  }

  void unmount() {
    --refcount_;
    Serial.printf("Refcount: %d\n", refcount_);
    if (refcount_ > 0) return;
    Serial.println("Unmounting the card");
    digitalWrite(sd_cs_pin_, LOW);
    fs_.end();
    digitalWrite(sd_cs_pin_, HIGH);
  }

  bool is_mounted() const { return refcount_ > 0; }

 private:
  decltype(SPI)& spi_;
  int sd_cs_pin_;
  uint32_t freq_;

  SDFS& fs_;
  int refcount_;
};

// class SdMount {
//  public:
//   SdMount() : sd_(nullptr), ok_(false) {}

//   SdMount(Sd& sd) : sd_(&sd), ok_(sd.mount()) {}

//   SdMount(const SdMount& other) : sd_(other.sd_), ok_(other.ok_) {
//     if (ok_) sd_->mount();
//   }

//   SdMount(SdMount&& other)
//       : sd_(std::move(other.sd_)), ok_(std::move(other.ok_)) {
//     other.sd_ = nullptr;
//     other.ok_ = false;
//   }

//   void close() {
//     if (!ok_) return;
//     ok_ = false;
//     sd_->unmount();
//   }

//   //
//   SdMount& operator=(SdMount&& other) {
//     if (sd_ != other.sd_ || !other.ok_) {
//       close();
//     }
//     sd_ = other.sd_;
//     ok_ = other.ok_;
//     other.sd_ = nullptr;
//     other.ok_ = false;
//     return *this;
//   }

//   SdMount& operator=(const SdMount& other) {
//     if (sd_ == other.sd_) {
//       if (ok_ != other.ok_) {
//         if (other.ok_) {
//           ok_ = sd_->mount();
//         } else {
//           close();
//         }
//       }
//     } else {
//       close();
//       sd_ = other.sd_;
//       if (other.ok_) {
//         ok_ = sd_->mount();
//       }
//     }
//     return *this;
//   }

//   ~SdMount() { close(); }

//   bool ok() const { return ok_; }

//  private:
//   Sd* sd_;
//   bool ok_;
// };

}  // namespace tapuino
