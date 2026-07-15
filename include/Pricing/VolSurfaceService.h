#pragma once

#include "Common/VolArbitrageModels.h"

#include <vector>

class VolSurfaceService
{
public:
    void rebuild(const std::vector<ImpliedVolPoint>& impliedVols);

    const std::vector<VolSurfacePoint>& points() const;

    std::vector<VolSurfacePoint> pointsForExpiry(const std::string& expiry) const;

private:
    std::vector<VolSurfacePoint> points_;
};
