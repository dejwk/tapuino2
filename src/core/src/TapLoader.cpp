#include "core/include/TapLoader.h"
#include "FS.h"
#include "core/include/Lang.h"
#include <HardwareSerial.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "io/sd.h"

using namespace std;

using namespace TapuinoNext;

#define OUT_OF_FILE_MARKER 0xFFFFFFFF

TapLoader::TapLoader(UtilityCollection* utilityCollection,
                     roo_scheduler::Scheduler& scheduler)
    : TapBase(utilityCollection),
      scheduler(scheduler),
      playTick(scheduler, [this](){PlayTick(); }, roo_time::Millis(200)),
      input(),
      playFinishedCb(nullptr)
{
    isTiming = false;
    tapInfo.position = 0;
}

TapLoader::~TapLoader()
{
}

inline uint32_t TapLoader::ReadNextByte()
{
    uint32_t nextByte = flipBuffer->ReadByte();
    tapInfo.position++;
    return (nextByte);
}

uint32_t TapLoader::CalcSignalTime()
{
    if (tapInfo.position >= tapInfo.length)
    {
        return (OUT_OF_FILE_MARKER);
    }

    uint32_t signalTime = ReadNextByte();
    if (signalTime != 0)
    {
        signalTime = (uint32_t) ((double) signalTime * cycleMult8);
    }
    else
    {
        if (tapInfo.version == 0)
        {
            // in version 0 TAP files a zero length signal indicates an
            // overflow, so the maximum value is returned.
            signalTime = 256;
        }
        else
        {
            signalTime = ReadNextByte();
            signalTime |= ReadNextByte() << 8;
            signalTime |= ReadNextByte() << 16;
            signalTime = (uint32_t) ((double) signalTime * cycleMultRaw);
        }
    }
    if (tapInfo.version == 2)
    {
        // for TAP format 2 (half-wave) each half of the signal is represented
        // indivudually and so is returned as double the length of a version 0
        // or 1 value so that the 2 Mhz counter doesn't have to do the work. The
        // alternative would be to halve the version 0 or 1 values.
        signalTime <<= 1;
    }
    return (signalTime);
}

// bool TapLoader::SeekToCounter(File tapFile, uint16_t targetCounter)
// {
//     // save everything!
//     uint64_t saveFilePos = tapFile.position();
//     TAP_INFO saveTapInfo;
//     memcpy(&saveTapInfo, &tapInfo, sizeof(TAP_INFO));

//     if (tapInfo.counterActual > targetCounter)
//     {
//         // only start from the begining of the tap if we are seeking backwards
//         // get to the first byte of actual tap data
//         tapFile.seek(TAP_HEADER_LENGTH);
//         flipBuffer->FillWholeBuffer(tapFile);
//         tapInfo.cycles = 0;
//         tapInfo.position = 0;
//         tapInfo.counterActual = 0;
//     }

//     lcdUtils->Title(S_SEEKING);
//     lcdUtils->Status("");

//     // TODO: there is a chance that we may slightly overrun the desired value
//     // at this point we accept that, but we may want to restore and let the user
//     // choose a lower counter value
//     while (tapInfo.counterActual < targetCounter)
//     {
//         uint32_t signalTime = CalcSignalTime();
//         flipBuffer->FillBufferIfNeeded(tapFile);

//         if (signalTime == OUT_OF_FILE_MARKER)
//         {
//             // Ran out of file!
//             tapFile.seek(saveFilePos);
//             memcpy(&tapInfo, &saveTapInfo, sizeof(TAP_INFO));
//             lcdUtils->Error(S_COUNTER_GR_SIZE, "");
//             lcdUtils->PlayUI(false, tapInfo.counterActual, 0);
//             return (false);
//         }
//         else
//         {
//             tapInfo.cycles += signalTime;
//             // (uint16_t) (DS_G * (sqrt((tapInfo.cycles / 1000000.0 * (DS_V_PLAY / DS_D / PI)) + ((DS_R * DS_R) / (DS_D * DS_D))) - (DS_R / DS_D)));
//             tapInfo.counterActual = CYCLES_TO_COUNTER(tapInfo.cycles);
//             lcdUtils->PlayUI(true, tapInfo.counterActual, 1000);
//         }
//     }
//     return (true);
// }

void TapLoader::StartTimer()
{
    if (!isTiming)
    {
        isTiming = true;
        // prep the read signal to start HIGH for first high / low transition
        digitalWrite(C64_READ_PIN, HIGH);
        // tell the C64 that play has been pressed
        digitalWrite(C64_SENSE_PIN, LOW);
        processSignal = true;
        HWStartTimer();
    }
    else
    {
        Serial.println("TapLoader::StartTimer() called while already running!");
    }
}

void TapLoader::StopTimer()
{
    if (isTiming)
    {
        isTiming = false;
        // prevent any further buffer processing
        processSignal = false;
        // shutdown the hardware timer
        HWStopTimer();

        // reset the read signal to initial state
        digitalWrite(C64_READ_PIN, LOW);
        // tell the C64 that stop has been pressed
        digitalWrite(C64_SENSE_PIN, HIGH);
    }
    else
    {
        Serial.println("TapLoader::StopTimer() called, but it already stopped!");
    }
}

ErrorCodes TapLoader::ReadTapHeader(tapuino::InputStream& input)
{
    uint32_t size;
    // byte wise header magic is read into 32bit values for easy comparison
    // below
    uint32_t tap_magic[TAP_HEADER_MAGIC_LENGTH / 4];
    memset(&tapInfo, 0, sizeof(tapInfo));

    flipBuffer->Reset();
    size = input.size();

    // safety first! minimum possible TAP size?
    if (size < TAP_HEADER_LENGTH + 1)
    {
        return ErrorCodes::INVALID_TAP_FILE;
    }

    if (input.read((uint8_t*) &tap_magic, TAP_HEADER_MAGIC_LENGTH) != TAP_HEADER_MAGIC_LENGTH)
        return ErrorCodes::INVALID_TAP_FILE;

    if (input.read((uint8_t*) &tapInfo, TAP_HEADER_DATA_LENGTH) != TAP_HEADER_DATA_LENGTH)
    {
        return ErrorCodes::INVALID_TAP_FILE;
    }

    Serial.printf("Length: %d, %d, %d\n", tapInfo.length, size, TAP_HEADER_LENGTH);
    // check size first
    if (tapInfo.length != (size - TAP_HEADER_LENGTH))
    {
        // the header is bad... we can try and proceed.
        tapInfo.length = size - TAP_HEADER_LENGTH;
        Serial.printf("Corrected Length: %d, %d, %d\n", tapInfo.length, size, TAP_HEADER_LENGTH);
    }

    // check the post fix for "TAPE-RAW", use a 4-byte magic trick
    if (tap_magic[1] != TAP_MAGIC_POSTFIX1 || tap_magic[2] != TAP_MAGIC_POSTFIX2)
    {
        return ErrorCodes::INVALID_TAP_FILE;
    }

    // now check type: C16 or C64, use a 4-byte magic trick
    if (tap_magic[0] != TAP_MAGIC_C64 && tap_magic[0] != TAP_MAGIC_C16)
    {
        return ErrorCodes::UNKNOWN_TAP_FORMAT;
    }

    SetupCycleTiming();
    // tapInfo.length += 1024;

    return ErrorCodes::OK;
}

// bool TapLoader::InPlayMenu(File tapFile)
// {
//     MenuHandler menu(lcdUtils, inputHandler);

//     MenuEntry inPlayEntries[] = {
//         {MenuEntryType::IndexEntry, S_PLAY, NULL},
//         {MenuEntryType::IndexEntry, S_SEEK, NULL},
//         {MenuEntryType::IndexEntry, S_EXIT, NULL},
//     };
//     TheMenu inPlayMenu = {S_PLAY_MENU, (MenuEntry*) inPlayEntries, 3, 0, NULL};

//     while (true)
//     {
//         switch (menu.Display(&inPlayMenu))
//         {
//             // Play
//             case 0:
//             {
//                 return (true);
//             }
//             // Seek
//             case 1:
//             {
//                 ValueOption seekPos(OptionTagId::LAST, NULL, tapInfo.counterActual, 0, 999, 1);
//                 if (menu.DisplayValue(S_SET_COUNTER, &seekPos) == InputResponse::Abort)
//                 {
//                     break;
//                 }

//                 if (seekPos.IsDirty())
//                 {
//                     return (SeekToCounter(tapFile, seekPos.GetValue()));
//                 }
//                 return (true);
//             }
//             // Exit (2) or Abort (-1)
//             default:
//                 return (false);
//         }
//     }
// }

ErrorCodes TapLoader::Play(tapuino::TapFile& tapFile, 
                           std::function<void(ErrorCodes)> playFinishedCb)
{
    if (IsPlaying()) return ErrorCodes::OK;
    if (!input || tapInfo.position == 0) {
        input = tapFile.open();
        ErrorCodes ret = ReadTapHeader(input);
        if (ret != ErrorCodes::OK)
        {
            Reset();
            return ret;
        }

        // lcdUtils->Title(S_TAP_OK);
        // delay(1000);

        // Pre-fill the entire buffer. When the buffer index (bufferPos) crosses the
        // half way point (bufferSwitchPos) bufferSwitchFlag will be set and the
        // first half of the buffer will be filled with new data. this will toggle
        // with the code below in the while loop, filling the second half of the
        // buffer as the index resets to zero etc.
        tapInfo.position = 0;
        ret = flipBuffer->FillWholeBuffer(input);
        if (ret != ErrorCodes::OK) {
            input.close();
            input = tapuino::InputStream();
            return ret;
        }
    }

    loadingStatus = ErrorCodes::OK;
    this->playFinishedCb = playFinishedCb;
    playTick.start();

    // // skip the menu if auto play is set.
    // if (!options->autoPlay.GetValue())
    // {
    //     if (!InPlayMenu(tapFile))
    //     {
    //         return;
    //     }
    // }

    // lcdUtils->Title(S_LOADING);
    // lcdUtils->ShowFile(tapFile.name(), false);
    // uint32_t tickerTime = options->tickerTime.GetValue();
    
    StartTimer();
    return ErrorCodes::OK;
}

void TapLoader::Reset() {
    Stop();
    input.close();
    input = tapuino::InputStream();
    tapInfo.position = 0;
}

void TapLoader::Stop() {
    if (!IsPlaying()) return;
    // If no error and we're seeing stop, it implies explicit abort.
    if (loadingStatus == ErrorCodes::OK) {
        loadingStatus = ErrorCodes::OPERATION_ABORTED;
    }
    StopTimer();
    playTick.stop();
    playFinishedCb(loadingStatus);
    return;
}

void TapLoader::PlayTick() {
    if (!motorOn) {
      Serial.println("Motor has stopped");
    }
    if (!processSignal)
    {
        Serial.println("Signal not processed anymore; calling Stop()");
        // ensure we clean up the timer if we ran out of TAP file, as triggered by CalcSignalTime() returning 0xFFFFFFFF
        // TODO: investigate a possibly cleaner way to handle all of this instead of relying on magic values, processSignal should be owned by Stop/StartTimer
        // Stop();
        Stop();
        return;
    }

    loadingStatus = flipBuffer->FillBufferIfNeeded(input);
    if (loadingStatus != ErrorCodes::OK) {
      Stop();
    }

    // (uint16_t) (DS_G * (sqrt((tapInfo.cycles / 1000000.0 * (DS_V_PLAY / DS_D / PI)) + ((DS_R * DS_R) / (DS_D * DS_D))) - (DS_R / DS_D)));
    tapInfo.counterActual = CYCLES_TO_COUNTER(tapInfo.cycles);
    //lcdUtils->PlayUI(motorOn, tapInfo.counterActual, tickerTime);
    // Serial.printf("progress: %d %d\n", is.size(), flipBuffer->counter() - count);
    // InputResponse resp = inputHandler->GetInput();

    // if (resp == InputResponse::Select || resp == InputResponse::Abort)
    // {
    //     StopTimer();
    //     if (resp == InputResponse::Abort)
    //     {
    //         return;
    //     }
    //     // if (!InPlayMenu(tapFile))
    //     // {
    //     //     Serial.println("exiting playback");
    //     //     return;
    //     // }
    //     // lcdUtils->Title(S_LOADING);
    //     // lcdUtils->ShowFile(tapFile.name(), false);
    //     StartTimer();
    // }
    Serial.println("Finishing playTick");
    Serial.println(playTick.is_active());
}

