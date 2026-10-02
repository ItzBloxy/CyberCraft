#include "link/Mapping.h"

#include <skycraft_protocol.h>

#include <sddl.h>

#include <atomic>
#include <cstring>
#include <format>
#include <vector>

namespace cpmc::link
{
namespace proto = skycraft::proto;

namespace
{
template<class T>
std::atomic_ref<T> Atomic(T& aValue)
{
	return std::atomic_ref<T>(aValue);
}

// SkyCraft's SharedWithThisUser: this user, the system and administrators, at medium integrity. Null (the default
// security) if it can't be built. Free with LocalFree.
PSECURITY_DESCRIPTOR SharedWithThisUser()
{
	std::wstring sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)";
	HANDLE       token = nullptr;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
	{
		DWORD size = 0;
		GetTokenInformation(token, TokenUser, nullptr, 0, &size);
		std::vector<uint8_t> buffer(size);
		if (size && GetTokenInformation(token, TokenUser, buffer.data(), size, &size))
		{
			LPWSTR sid = nullptr;
			if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid))
			{
				sddl += std::wstring(L"(A;;GA;;;") + sid + L")";
				LocalFree(sid);
			}
		}
		CloseHandle(token);
	}
	sddl += L"S:(ML;;NW;;;ME)";
	PSECURITY_DESCRIPTOR descriptor = nullptr;
	if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr))
	{
		return nullptr;
	}
	return descriptor;
}

bool ProcessAlive(uint32_t aPid)
{
	HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, aPid);
	if (!process)
	{
		return false;
	}
	const bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
	CloseHandle(process);
	return alive;
}
}

Mapping::~Mapping()
{
	Close();
}

Mapping::Opened Mapping::Open(const wchar_t* aName, uint32_t aOurPid, uint64_t aNowMs)
{
	Close();
	constexpr uint64_t  size = proto::kMappingBytes;
	SECURITY_ATTRIBUTES access{ sizeof access, SharedWithThisUser(), FALSE };
	handle_ = CreateFileMappingW(INVALID_HANDLE_VALUE, access.lpSecurityDescriptor ? &access : nullptr, PAGE_READWRITE, DWORD(size >> 32),
	                             DWORD(size & 0xFFFFFFFF), aName);
	const DWORD created = GetLastError();
	if (access.lpSecurityDescriptor)
	{
		LocalFree(access.lpSecurityDescriptor);
	}
	if (!handle_)
	{
		return { Outcome::Failed, std::format("CreateFileMapping failed ({})", created) };
	}
	const bool existed = created == ERROR_ALREADY_EXISTS;
	base_ = static_cast<uint8_t*>(MapViewOfFile(handle_, FILE_MAP_ALL_ACCESS, 0, 0, 0));
	if (!base_)
	{
		const DWORD error = GetLastError();
		Close();
		return { Outcome::Failed, std::format("MapViewOfFile failed ({})", error) };
	}
	MEMORY_BASIC_INFORMATION region{};
	if (VirtualQuery(base_, &region, sizeof region) == 0 || region.RegionSize < size)
	{
		const uint64_t found = region.RegionSize;
		Close();
		return { Outcome::Refused, std::format("the mapping already there is {} bytes, smaller than the protocol's {}", found, size) };
	}
	auto* header = reinterpret_cast<proto::Header*>(base_);
	if (existed)
	{
		const uint32_t pid = Atomic(header->skyrimPid).load(std::memory_order_acquire);
		const uint64_t beat = Atomic(header->skyrimHeartbeatMs).load(std::memory_order_acquire);
		const bool     fresh = beat >= aNowMs || aNowMs - beat < kAliveMs;
		if (pid != 0 && pid != aOurPid && fresh && ProcessAlive(pid))
		{
			Close();
			return { Outcome::Refused, std::format("another game side (process {}) is using it", pid) };
		}
	}
	// As SkyCraft does: everything the game side owns starts from a known state, rings and the overlay's swap included.
	std::memset(base_ + proto::kOffSkyState, 0, sizeof(proto::SkyState));
	std::memset(base_ + proto::kOffOverlayCtl, 0, 0x100);
	std::memset(base_ + proto::kOffInputRing, 0, proto::kInputRingDataOff);
	std::memset(base_ + proto::kOffCollisionRing, 0, proto::kColRingDataOff);
	std::memset(base_ + proto::kOffActorTable, 0, sizeof(proto::ActorTable));
	std::memset(base_ + proto::kOffEventRing, 0, proto::kEventRingDataOff);
	std::memset(base_ + proto::kOffWorldEntities, 0, sizeof(proto::WorldEntities));
	std::memset(base_ + proto::kOffRenderRing, 0, proto::kRenRingDataOff);
	Atomic(header->version).store(proto::kVersion, std::memory_order_relaxed);
	Atomic(header->skyrimPid).store(aOurPid, std::memory_order_relaxed);
	Atomic(header->skyrimHeartbeatMs).store(aNowMs, std::memory_order_relaxed);
	Atomic(header->magic).store(proto::kMagic, std::memory_order_release);
	return { existed ? Outcome::Reused : Outcome::Created, existed ? "reused" : "created" };
}

void Mapping::Close()
{
	if (base_)
	{
		UnmapViewOfFile(base_);
		base_ = nullptr;
	}
	if (handle_)
	{
		CloseHandle(handle_);
		handle_ = nullptr;
	}
}

void Mapping::Heartbeat(uint64_t aNowMs)
{
	if (base_)
	{
		Atomic(reinterpret_cast<proto::Header*>(base_)->skyrimHeartbeatMs).store(aNowMs, std::memory_order_release);
	}
}

uint32_t Mapping::McPid() const
{
	return base_ ? Atomic(reinterpret_cast<proto::Header*>(base_)->mcPid).load(std::memory_order_acquire) : 0;
}

uint64_t Mapping::McHeartbeatMs() const
{
	return base_ ? Atomic(reinterpret_cast<proto::Header*>(base_)->mcHeartbeatMs).load(std::memory_order_acquire) : 0;
}
}
