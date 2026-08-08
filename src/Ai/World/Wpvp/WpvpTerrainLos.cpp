#include "WpvpTerrainLos.h"

#include "Map.h"
#include "Unit.h"

namespace
{
// Spacing of ground-height samples along the sight line. WpvpVisionDistance
// (100yd) is the longest line this ever tests, so ~20 heightmap lookups worst
// case — cheap next to the vmap ray test that already ran.
constexpr float SAMPLE_SPACING = 5.0f;

// How far above the sight line the ground must reach before it counts as
// cover, so grazing terrain (gentle slopes the client renders past) doesn't
// false-positive.
constexpr float OCCLUSION_CLEARANCE = 0.75f;
}  // namespace

bool WpvpTerrainOccludes(Unit* viewer, Unit* target)
{
    Map const* map = viewer->GetMap();
    if (!map || map != target->GetMap())
        return false;

    // Indoors means WMO geometry, which the vmap ray test already handles; the
    // outdoor heightmap above a cave or building would read as solid cover.
    if (!viewer->IsOutdoors() || !target->IsOutdoors())
        return false;

    float const x0 = viewer->GetPositionX();
    float const y0 = viewer->GetPositionY();
    float const z0 = viewer->GetPositionZ() + viewer->GetCollisionHeight();
    float const x1 = target->GetPositionX();
    float const y1 = target->GetPositionY();
    float const z1 = target->GetPositionZ() + target->GetCollisionHeight();

    float const dist = viewer->GetExactDist2d(target);
    if (dist <= SAMPLE_SPACING)
        return false;

    uint32 const steps = static_cast<uint32>(dist / SAMPLE_SPACING);
    for (uint32 i = 1; i <= steps; ++i)
    {
        float const t = float(i) / float(steps + 1);
        float const ground = map->GetGridHeight(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
        if (ground > z0 + (z1 - z0) * t + OCCLUSION_CLEARANCE)
            return true;
    }

    return false;
}
