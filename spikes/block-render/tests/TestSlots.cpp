#include "Harness.h"

#include "hooks/Slots.h"

namespace slots = cpmc::slots;

extern "C"
{
size_t SlotsC_Device(int aWhich);
size_t SlotsC_Queue(void);
size_t SlotsC_List(int aWhich);
size_t SlotsC_Dxgi(int aWhich);
}

TEST(SlotsMatchTheSdksDeviceAndQueueLayouts)
{
	CHECK_EQ(SlotsC_Device(0), slots::kCreateRenderTargetView);
	CHECK_EQ(SlotsC_Device(1), slots::kCreateDepthStencilView);
	CHECK_EQ(SlotsC_Device(2), slots::kCopyDescriptors);
	CHECK_EQ(SlotsC_Device(3), slots::kCopyDescriptorsSimple);
	CHECK_EQ(SlotsC_Queue(), slots::kExecuteCommandLists);
}

TEST(SlotsMatchTheSdksCommandListLayout)
{
	const size_t expected[] = { slots::kClose,
		                        slots::kReset,
		                        slots::kDrawInstanced,
		                        slots::kDrawIndexedInstanced,
		                        slots::kDispatch,
		                        slots::kResourceBarrier,
		                        slots::kExecuteBundle,
		                        slots::kOMSetRenderTargets,
		                        slots::kClearDepthStencilView,
		                        slots::kClearRenderTargetView,
		                        slots::kSetMarker,
		                        slots::kBeginEvent,
		                        slots::kEndEvent,
		                        slots::kExecuteIndirect,
		                        slots::kBeginRenderPass,
		                        slots::kEndRenderPass,
		                        slots::kBarrier };
	for (int i = 0; i < int(std::size(expected)); ++i)
	{
		CHECK_EQ(SlotsC_List(i), expected[i]);
	}
}

TEST(SlotsMatchTheSdksDxgiLayouts)
{
	CHECK_EQ(SlotsC_Dxgi(0), slots::kCreateSwapChain);
	CHECK_EQ(SlotsC_Dxgi(1), slots::kCreateSwapChainForHwnd);
	CHECK_EQ(SlotsC_Dxgi(2), slots::kPresent);
	CHECK_EQ(SlotsC_Dxgi(3), slots::kPresent1);
}
