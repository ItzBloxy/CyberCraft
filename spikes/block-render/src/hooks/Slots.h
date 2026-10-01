#pragma once

#include <cstddef>

// The vtable slot of every method the plugin patches, counted from 0 (IUnknown::QueryInterface). The interfaces
// derive in a straight line, so each method keeps its slot in every later version of its interface.
// tests/SlotsC.c reads the same numbers from the Windows SDK's C layouts, and tests/TestSlots.cpp compares them.
namespace cpmc::slots
{
// ID3D12Device
constexpr size_t kCreateRenderTargetView = 20;
constexpr size_t kCreateDepthStencilView = 21;
constexpr size_t kCopyDescriptors = 23;
constexpr size_t kCopyDescriptorsSimple = 24;

// ID3D12CommandQueue
constexpr size_t kExecuteCommandLists = 10;

// ID3D12GraphicsCommandList, then ...List4 and ...List7
constexpr size_t kClose = 9;
constexpr size_t kReset = 10;
constexpr size_t kDrawInstanced = 12;
constexpr size_t kDrawIndexedInstanced = 13;
constexpr size_t kDispatch = 14;
constexpr size_t kResourceBarrier = 26;
constexpr size_t kExecuteBundle = 27;
constexpr size_t kOMSetRenderTargets = 46;
constexpr size_t kClearDepthStencilView = 47;
constexpr size_t kClearRenderTargetView = 48;
constexpr size_t kSetMarker = 56;
constexpr size_t kBeginEvent = 57;
constexpr size_t kEndEvent = 58;
constexpr size_t kExecuteIndirect = 59;
constexpr size_t kBeginRenderPass = 68;
constexpr size_t kEndRenderPass = 69;
constexpr size_t kBarrier = 80;

// IDXGIFactory and IDXGIFactory2
constexpr size_t kCreateSwapChain = 10;
constexpr size_t kCreateSwapChainForHwnd = 15;

// IDXGISwapChain and IDXGISwapChain1
constexpr size_t kPresent = 8;
constexpr size_t kPresent1 = 22;
}
