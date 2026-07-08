#include "VolSurfaceService.h"

void VolSurfaceService::rebuild(const std::vector<ImpliedVolPoint>& impliedVols)
{
    points_.clear();
    points_.reserve(impliedVols.size());

    for (const auto& point : impliedVols) {
        points_.push_back(VolSurfacePoint{
            point.instrument.expiry,
            point.instrument.strike,
            point.instrument.optionType,
            point.impliedVolatility });
    }
}
const std::vector<VolSurfacePoint>& VolSurfaceService::points() const
{
    return points_;
}

std::vector<VolSurfacePoint> VolSurfaceService::pointsForExpiry(const std::string& expiry) const
{
    std::vector<VolSurfacePoint> result;

    for (const auto& point : points_) {
        if (point.expiry == expiry) {
            result.push_back(point);
        }
    }

    return result;
}
