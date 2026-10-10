#include "splitter_bounds.h"

#include <algorithm>
#include <cstddef>

namespace peles
{
namespace core
{

int DraggedPanelExtent (int startExtent, int pointerDelta, SplitterSide side)
{
	return side == SplitterSide::PanelBefore ? startExtent + pointerDelta
	                                         : startExtent - pointerDelta;
}

int ClampPanelExtent (int requested, int sharedExtent)
{
	const int upper  = sharedExtent - kMinViewportExtent;
	const int extent = std::min (std::max (requested, kMinPanelExtent), upper);
	return std::max (extent, 0);
}

std::vector<int> ClampAxisExtents (const std::vector<AxisPanel>& panels, int sharedExtent)
{
	std::vector<int> targets;
	targets.reserve (panels.size ());
	int total = 0;
	for (const AxisPanel& panel : panels)
	{
		const int target =
		    panel.requested >= 0 ? std::max (panel.requested, kMinPanelExtent) : panel.extent;
		targets.push_back (target);
		total += target;
	}

	int missing = total + kMinViewportExtent - sharedExtent;

	// 1. Au-dessus du minimum, dans l'ordre.
	for (std::size_t i = 0; i < panels.size () && missing > 0; ++i)
	{
		if (panels[i].requested < 0)
			continue;
		const int given = std::min (missing, std::max (targets[i] - kMinPanelExtent, 0));
		targets[i] -= given;
		missing -= given;
	}

	// 2. Sous le minimum, jusqu'a zero, dans le meme ordre.
	for (std::size_t i = 0; i < panels.size () && missing > 0; ++i)
	{
		if (panels[i].requested < 0)
			continue;
		const int given = std::min (missing, targets[i]);
		targets[i] -= given;
		missing -= given;
	}
	return targets;
}

int SharedAxisExtent (int panelsExtent, int viewportExtent, int occupied, int available)
{
	const int overflow = occupied > available ? occupied - available : 0;
	return panelsExtent + viewportExtent - overflow;
}

} // namespace core
} // namespace peles
