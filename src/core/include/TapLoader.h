#pragma once

#include <functional>

#include "ErrorCodes.h"
#include "TapBase.h"

#include "io/tap_file.h"
#include "io/input_stream.h"
#include "roo_scheduler.h"

namespace TapuinoNext
{
    class TapLoader : public TapBase
    {
      public:
        TapLoader(UtilityCollection* utilityCollection, roo_scheduler::Scheduler& scheduler);
        ~TapLoader();
        ErrorCodes Play(tapuino::TapFile& tapFile,
                        std::function<void(ErrorCodes)> playFinishedCb);
        bool IsPlaying() const { return isTiming; }
        void Stop();
        void Reset();
        
        const TAP_INFO* GetTapInfo()
        {
            return &tapInfo;
        }

      protected:
        // Interface for the derrived class that implemtents the hardware interface
        /******************************************************/
        virtual void HWStartTimer() = 0;
        virtual void HWStopTimer() = 0;
        /******************************************************/
        uint32_t CalcSignalTime();

      private:
        uint32_t ReadNextByte();
        ErrorCodes ReadTapHeader(tapuino::InputStream& input);
        void StartTimer();
        void StopTimer();

        // Called from the playTick task.
        void PlayTick();

        roo_scheduler::Scheduler& scheduler;

        // When playing, periodically refills the flip buffer with data from the input
        // stream.
        roo_scheduler::RepetitiveTask playTick;

        // When playing, the source input stream. Otherwise, a null input.
        tapuino::InputStream input;

        std::function<void(ErrorCodes status)> playFinishedCb;

        // bool InPlayMenu(File tapFile);
        // bool SeekToCounter(File tapFile, uint16_t targetCounter);

        bool isTiming;

        ErrorCodes loadingStatus;
    };
} // namespace TapuinoNext
