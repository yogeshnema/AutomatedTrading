#include "Risk/SignalService.h"

#include <cmath>
#include <numeric>

std::vector<VolSignal> SignalService::findVixDislocations(
    const std::vector<VolSurfacePoint>& surface,
    double indiaVix,
    double zScoreThreshold) const
{
    std::vector<VolSignal> signals;
    if (surface.size() < 2 || indiaVix <= 0.0) {
        return signals;
    }

    std::vector<double> differences;
    differences.reserve(surface.size());

    const double vixVol = indiaVix / 100.0;
    for (const auto& point : surface) {
        differences.push_back(point.impliedVolatility - vixVol);
    }

    const double mean = std::accumulate(differences.begin(), differences.end(), 0.0) /
        static_cast<double>(differences.size());

    double variance = 0.0;
    for (const auto difference : differences) {
        variance += (difference - mean) * (difference - mean);
    }

    const double stdDev = std::sqrt(variance / static_cast<double>(differences.size() - 1));
    if (stdDev == 0.0) {
        return signals;
    }

    for (size_t index = 0; index < surface.size(); ++index) {
        const double zScore = (differences[index] - mean) / stdDev;
        if (std::abs(zScore) < zScoreThreshold) {
            continue;
        }

        const auto& point = surface[index];
        signals.push_back(VolSignal{
            "NIFTY_IV_vs_INDIA_VIX",
            point.expiry,
            point.strike,
            point.optionType,
            point.impliedVolatility,
            vixVol,
            zScore,
            zScore > 0.0 ? "IV_RICH_VS_VIX" : "IV_CHEAP_VS_VIX" });
    }

    return signals;
}
