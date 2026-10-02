#include "Status.h"

#include <format>

namespace cpmc
{
namespace
{
std::string Ms(double aAverage, double aWorst)
{
	if (aAverage < 0.0)
	{
		return "not measured";
	}
	return std::format("{:.3f} ms average, {:.3f} ms worst", aAverage, aWorst);
}

std::string OneLine(std::string aText)
{
	for (char& ch : aText)
	{
		if (ch == '\n' || ch == '\r')
		{
			ch = ' ';
		}
	}
	return aText;
}
}

std::string FormatStatus(const StatusReport& aReport)
{
	std::string out;
	out += std::format("mode={}\n", aReport.mode);
	out += std::format("frames={} drawn{}, {} skipped{}, {} with a late camera (last second)\n", aReport.framesDrawn,
	                   aReport.framesPartial == 0 ? "" : std::format(" ({} without the tagged depth)", aReport.framesPartial),
	                   aReport.framesSkipped, aReport.lastSkip.empty() ? "" : " (" + OneLine(aReport.lastSkip) + ")",
	                   aReport.lateCamera);
	out += std::format("blocks={} in {} sections, {} vertices\n", aReport.blocks, aReport.sections, aReport.vertices);
	out += std::format("gpu={}\n", Ms(aReport.gpuMsAverage, aReport.gpuMsWorst));
	out += std::format("frame={}\n", Ms(aReport.frameMsAverage, aReport.frameMsWorst));
	out += std::format("streamline={} frames in the last second\n", aReport.streamlineFrames);
	out += std::format("hooks={}\n", OneLine(aReport.hooks));
	out += std::format("request={}\n", OneLine(aReport.lastRequest));
	for (const StatusReport::Structure& structure : aReport.structures)
	{
		out += std::format("structure={} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f}\n", structure.name, structure.worldMin.x,
		                   structure.worldMin.y, structure.worldMin.z, structure.worldMax.x, structure.worldMax.y,
		                   structure.worldMax.z);
	}
	return out;
}
}
