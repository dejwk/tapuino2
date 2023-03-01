#ifdef ESP32
#include "core/include/ESP32TapLoader.h"

using namespace TapuinoNext;

ESP32TapLoader* ESP32TapLoader::internalClass = NULL;

ESP32TapLoader::ESP32TapLoader(
    UtilityCollection* utilityCollection,
    roo_scheduler::Scheduler& scheduler)
#ifdef ROO_TESTING
    : TapLoader(utilityCollection, scheduler),
      scheduler_(scheduler),
      task_(&scheduler, [this]() { ESP32TapLoader::TapSignalTimerStatic(); })
#else
    : TapLoader(utilityCollection, scheduler)
#endif
{
    ESP32TapLoader::internalClass = this;
    tapSignalTimer = NULL;
    stopping = false;
    stopped = false;
}

ESP32TapLoader::~ESP32TapLoader()
{
}

#define IDLE_TIMER_EXECUTE 1000

void ESP32TapLoader::HWStartTimer()
{
    Serial.printf("HWStartTimer: Stopping: %d, Stopped: %d\n", stopping, stopped);
    if (tapSignalTimer == NULL)
    {
        stopping = false;
        stopped = false;
        // set up 2Mhz timer
        tapSignalTimer = timerBegin(0, 40, true);
        timerAttachInterrupt(tapSignalTimer, &ESP32TapLoader::TapSignalTimerStatic, true);
        timerWrite(tapSignalTimer, 0);
        timerAlarmWrite(tapSignalTimer, IDLE_TIMER_EXECUTE, true);
        timerAlarmEnable(tapSignalTimer);
    }
    else
    {
        Serial.printf("timer WAS NOT was null!\n");
    }
}

void ESP32TapLoader::HWStopTimer()
{
    Serial.printf("HWStopTimer: Stopping: %d, Stopped: %d\n", stopping, stopped);
    if (tapSignalTimer != NULL)
    {
        stopping = true;
        while (!stopped)
        {
            Serial.println("waiting for: stopped");
            delay(10);
        };
        timerAlarmDisable(tapSignalTimer);
        timerDetachInterrupt(tapSignalTimer);
        timerEnd(tapSignalTimer);
        tapSignalTimer = NULL;
        stopping = false;
        stopped = false;
    }
    else
    {
        Serial.printf("timer WAS null!\n");
    }
}

void IRAM_ATTR ESP32TapLoader::TapSignalTimerStatic()
{
    ESP32TapLoader::internalClass->TapSignalTimer();
}

void IRAM_ATTR ESP32TapLoader::TapSignalTimer()
{
    // default to an idle mode that keeps the timer ticking while not processing any signals
    uint32_t signalTime = IDLE_TIMER_EXECUTE;
    motorOn = digitalRead(C64_MOTOR_PIN);

    if (processSignal && motorOn)
    {
        if (signal1stHalf)
        {
            digitalWrite(C64_READ_PIN, LOW);
            tapInfo.cycles += (lastSignalTime >> 1);
            lastSignalTime = signalTime = CalcSignalTime();
            // special marker indicating that the end of the TAP has been reached.
            if (lastSignalTime == 0xFFFFFFFF)
            {
                processSignal = false;
                stopping = true;
                stopped = true;
                return;
            }
            // half-wave format, point to the next signal for the 2nd half value
            if (tapInfo.version == 2)
            {
                lastSignalTime = CalcSignalTime();
            if (lastSignalTime == 0xFFFFFFFF)
            {
                processSignal = false;
                stopping = true;
                stopped = true;
                return;
            }
            }
        }
        else
        {
            digitalWrite(C64_READ_PIN, HIGH);
            tapInfo.cycles += (lastSignalTime >> 1);
            signalTime = lastSignalTime;
        }

        signal1stHalf = !signal1stHalf;
    }

    if (!stopping)
    {
#ifdef ROO_TESTING
        task_.scheduleAfter(roo_time::Micros(signalTime / 10));

#else
        timerAlarmWrite(tapSignalTimer, signalTime, true);
#endif
    }
    else
    {
        stopped = true;
    }
}

#endif
