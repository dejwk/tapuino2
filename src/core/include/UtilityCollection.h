#pragma once
#include "InputHandler.h"
#include "LCDUtils.h"
#include "Options.h"
#include "FlipBuffer.h"

namespace TapuinoNext
{
    class UtilityCollection
    {
      public:
        UtilityCollection(InputHandler* inputHandler, Options* options, FlipBuffer* flipBuffer)
            : inputHandler(inputHandler), options(options), flipBuffer(flipBuffer)
        {
        }

        ~UtilityCollection()
        {
        }

        InputHandler* inputHandler;
        Options* options;
        FlipBuffer* flipBuffer;
    };
} // namespace TapuinoNext