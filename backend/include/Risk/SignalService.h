#pragma once

#include "Common/VolArbitrageModels.h"

#include <vector>

class SignalService
{
public:
    std::vector<VolSignal> findVixDislocations(
        const std::vector<VolSurfacePoint>& surface,
        double indiaVix,
        double zScoreThreshold) const;
};
