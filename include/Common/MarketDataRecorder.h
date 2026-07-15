#pragma once

#include "Common/EventMessaging.h"

#include <cstddef>
#include <string>

class MarketDataRecorder
{
public:
    std::size_t run(
        IEventSubscriber& subscriber,
        const std::string& outputPath,
        std::size_t maximumEvents) const;
};
