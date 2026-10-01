#pragma once

#include "Coords.h"
#include "sl/Sl.h"
#include "track/StateTracker.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cpmc
{
// What a frame log holds (the spec's "The frame log"). The plugin fills it in while it watches two frames; the main
// thread then writes it out as frame.json.

// A resource's description, from ID3D12Resource::GetDesc.
struct ResourceInfo
{
	bool     known = false;
	uint32_t dimension = 0;  // D3D12_RESOURCE_DIMENSION
	uint32_t format = 0;     // DXGI_FORMAT
	uint64_t width = 0;
	uint32_t height = 0;
	uint32_t depthOrArraySize = 0;
	uint32_t mips = 0;
	uint32_t flags = 0;  // D3D12_RESOURCE_FLAGS
};

struct LogEvent
{
	double      ms = 0.0;  // since the log began
	uint32_t    frame = 0;
	uint32_t    thread = 0;
	std::string what;    // frameToken, setConstants, setTag, evaluate, submit or present
	std::string detail;
};

struct LogConstants
{
	uint32_t    frame = 0;
	uint32_t    viewport = 0;
	SlConstants constants;
};

struct LogTag
{
	uint32_t     frame = 0;
	uint32_t     viewport = 0;
	SlTag        tag;
	ResourceInfo resource;
	uint64_t     list = 0;  // the command list passed with it, if any
};

struct LogEvaluate
{
	uint32_t frame = 0;
	uint32_t feature = 0;
	uint32_t viewport = 0;
	uint64_t list = 0;        // as the game passed it
	uint64_t nativeList = 0;  // the D3D12 list underneath, if it was a Streamline proxy
};

struct LogList
{
	uint64_t                list = 0;
	std::string             name;  // the game's name for it, if found
	uint32_t                frame = 0;
	std::vector<PassRecord> passes;
};

struct LogSubmission
{
	double                ms = 0.0;
	uint32_t              frame = 0;
	uint64_t              queue = 0;
	std::vector<uint64_t> lists;
	uint32_t              untracked = 0;  // lists the tracker hadn't followed from their Reset
};

struct LogDevice
{
	bool        seenAtCreation = false;
	uint64_t    device = 0;
	std::string gpuApiDevice;  // native, proxy, other, or missing
	bool        gpuApiMatches = false;
	std::string gpuApiQueue;
	std::string gpuApiSwapChain;
	std::string streamline;  // proxies, manual or unknown
	size_t      rtvs = 0;
	size_t      dsvs = 0;
	size_t      unknownHandles = 0;
	size_t      vtableEntries = 0;
	size_t      imports = 0;
};

struct LogScriptCamera
{
	Vec3d position;
	Vec3f forward;
	float fov = 0.0f;
};

struct LogCandidate
{
	uint64_t list = 0;
	size_t   pass = 0;
};

struct FrameLogData
{
	std::string                    pluginVersion;
	std::string                    time;
	uint32_t                       firstFrame = 0;
	uint32_t                       lastFrame = 0;
	LogDevice                      device;
	std::optional<LogScriptCamera> scriptCamera;
	std::vector<LogConstants>      constants;
	std::vector<LogTag>            tags;  // global (slSetTag) and local (slEvaluateFeature) tags
	std::vector<LogEvaluate>       evaluates;
	std::vector<LogEvent>          events;
	std::vector<LogSubmission>     submissions;
	std::vector<LogList>           lists;
	std::vector<LogCandidate>      gbufferCandidates;
};

// The name dxgiformat.h gives a format, without the DXGI_FORMAT_ prefix, or "format <n>".
std::string DxgiFormatName(uint32_t aFormat);

// frame.json, pretty-printed.
std::string FrameLogJson(const FrameLogData& aLog);
}
