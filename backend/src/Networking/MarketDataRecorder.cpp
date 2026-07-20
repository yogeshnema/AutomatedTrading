#include "Common/MarketDataRecorder.h"

#include "MarketData/MarketDataEvent.h"

#include <fstream>
#include <stdexcept>

std::size_t MarketDataRecorder::run(
    IEventSubscriber& subscriber,
    const std::string& outputPath,
    std::size_t maximumEvents) const
{
    std::ofstream output(outputPath, std::ios::app);
    if (!output) {
        throw std::runtime_error(
            "Unable to open market data recording: " + outputPath);
    }

    std::size_t recorded = 0;
    while (maximumEvents == 0 || recorded < maximumEvents) {
        const auto [topic, payload] = subscriber.receive();

        // Validate the contract before making the event durable.
        MarketDataEvent::fromJson(payload);
        output << payload << '\n';
        output.flush();
        ++recorded;
    }

    return recorded;
}
