cmake_minimum_required(VERSION 3.25)

project(BlockRender VERSION 0.19.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(RED4EXT_SDK_DIR "${REPO_ROOT}/extern/RED4ext.SDK" CACHE PATH "RED4ext.SDK checkout")
set(STREAMLINE_DIR "${REPO_ROOT}/extern/streamline-2.7.2" CACHE PATH "Streamline 2.7.2 headers")
set(SKYCRAFT_DIR "${REPO_ROOT}/extern/skycraft" CACHE PATH "SkyCraft's protocol header")
set(PHYSX_DIR "${REPO_ROOT}/extern/physx-3.4.2" CACHE PATH "PhysX 3.4.2 headers (PhysX_3.4/Include, PxShared/include)")
set(CP2077_DIR "" CACHE PATH "Cyberpunk 2077 folder; when set, each build copies the plugin into red4ext/plugins/BlockRender")

# Shared settings. Streamline's, SkyCraft's and PhysX's headers are SYSTEM, which keeps their warnings out of ours.
# PX_PHYSX_STATIC_LIB empties PhysX's import macro: PhysX is called only through its virtual tables and inline header
# code (spike 1's rule), so nothing is imported from its DLLs.
add_library(render_settings INTERFACE)
target_compile_options(render_settings INTERFACE
	$<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus>
	$<$<COMPILE_LANGUAGE:C>:/W4 /utf-8>)
target_compile_definitions(render_settings INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX PX_PHYSX_STATIC_LIB)
target_include_directories(render_settings INTERFACE src)
target_include_directories(render_settings SYSTEM INTERFACE "${STREAMLINE_DIR}/include" "${SKYCRAFT_DIR}"
	"${PHYSX_DIR}/PhysX_3.4/Include" "${PHYSX_DIR}/PxShared/include")

# Everything that doesn't need RED4ext or the running game.
add_library(render_core STATIC
	src/Json.cpp
	src/Readable.cpp
	src/Requests.cpp
	src/LogQueue.cpp
	src/sl/Sl.cpp
	src/hooks/ImportPatch.cpp
	src/hooks/VtablePatch.cpp
	src/track/ViewRegistry.cpp
	src/track/StateTracker.cpp
	src/render/Camera.cpp
	src/render/Atlas.cpp
	src/world/BlockWorld.cpp
	src/world/Mesher.cpp
	src/world/Structures.cpp
	src/log/DdsWriter.cpp
	src/log/FrameLogJson.cpp
	src/render/Shaders.cpp
	src/Status.cpp
	src/log/EventNames.cpp
	src/render/Pipeline.cpp
	src/hooks/Submission.cpp
	src/render/Predlss.cpp
	src/hooks/HookGroups.cpp
	src/render/Gbuffer.cpp
	src/link/Rings.cpp
	src/link/Messages.cpp
	src/link/Overlay.cpp
	src/link/Floor.cpp
	src/link/Look.cpp
	src/link/Commands.cpp
	src/link/Mapping.cpp
	src/link/Link.cpp
	src/link/Ticks.cpp
	src/link/Voxels.cpp
	src/link/Ground.cpp
	src/link/GroundWorker.cpp
	src/link/Solids.cpp
	src/link/Actors.cpp
	src/link/Combat.cpp
	src/link/WorldTable.cpp
	src/entities/WorldShapes.cpp
	src/entities/Entities.cpp
	src/physx/GameAbi.cpp
	src/physx/ShapeTriangles.cpp
	src/physx/OriginWatch.cpp
	src/physx/SceneWatch.cpp
	src/physx/Session.cpp
	src/physx/Export.cpp
	src/physx/BoxMerge.cpp
	src/physx/BodyPlan.cpp
	src/physx/Characters.cpp
	src/link/Drive.cpp
	src/link/Turn.cpp
	src/log/DriveLog.cpp
	src/render/PendingSections.cpp
	src/render/Hud.cpp
	src/Measurements.cpp
	src/ProcessUsage.cpp
	src/input/Scancodes.cpp
	src/input/BuildMode.cpp
	src/input/FocusWatch.cpp
	src/input/Router.cpp
	src/NpcRegistry.cpp
	src/CombatInbox.cpp
)
target_link_libraries(render_core PUBLIC render_settings d3dcompiler dxguid advapi32)

# The RED4ext SDK, header-only. SYSTEM keeps its warnings out of ours.
set(RED4EXT_HEADER_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory("${RED4EXT_SDK_DIR}" red4ext-sdk EXCLUDE_FROM_ALL SYSTEM)

# What runs inside the game: the hooks and the features they feed. Only the running game can test it.
add_library(render_runtime STATIC
	src/hooks/D3D12Hooks.cpp
	src/hooks/StreamlineHooks.cpp
	src/track/GpuApi.cpp
	src/render/Gpu.cpp
	src/log/FrameLog.cpp
	src/log/Dumps.cpp
	src/render/Renderer.cpp
	src/input/RawInputHook.cpp
	src/physx/SceneHooks.cpp
	src/physx/Bodies.cpp
)
target_link_libraries(render_runtime PUBLIC render_core RED4ext::SDK d3d12)

# The RED4ext plugin: entry points, requests, the status file and finding V.
add_library(BlockRender SHARED
	src/Plugin.cpp
	src/PlayerLocator.cpp
	src/Puppet.cpp
	src/ScriptTypes.cpp
	src/GameScript.cpp
	src/Natives.cpp
)
target_link_libraries(BlockRender PRIVATE render_runtime)

if(CP2077_DIR)
	set(PLUGIN_OUT "${CP2077_DIR}/red4ext/plugins/BlockRender")
	set(HUD_OUT "${CP2077_DIR}/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud")
	add_custom_command(TARGET BlockRender POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_OUT}/shaders" "${PLUGIN_OUT}/Scripts" "${HUD_OUT}"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:BlockRender>" "$<TARGET_PDB_FILE:BlockRender>" "${PLUGIN_OUT}/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/shaders/blocks.hlsl" "${PLUGIN_OUT}/shaders/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/scripts/BlockRender.reds" "${PLUGIN_OUT}/Scripts/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/tools/blockrender_hud/init.lua" "${HUD_OUT}/"
		VERBATIM)
endif()

# mclink: a console stand-in for the game's side of SkyCraft's protocol, run against the real Minecraft.
add_executable(mclink
	tools/mclink/Main.cpp
	tools/mclink/DumpObj.cpp
	tools/mclink/McLinkScript.cpp
)
target_include_directories(mclink PRIVATE tools/mclink)
target_link_libraries(mclink PRIVATE render_core winmm)

# Unit tests: no game needed. They run in this folder, so tests can find shaders/.
enable_testing()
add_executable(render_tests
	tests/TestMain.cpp
	tests/TestCoords.cpp
	tests/TestJson.cpp
	tests/TestReadable.cpp
	tests/TestRequests.cpp
	tests/TestLogQueue.cpp
	tests/TestSl.cpp
	tests/TestPatches.cpp
	tests/TestSlots.cpp
	tests/SlotsC.c
	tests/TestViewRegistry.cpp
	tests/TestStateTracker.cpp
	tests/TestCamera.cpp
	tests/TestAtlas.cpp
	tests/TestBlockWorld.cpp
	tests/TestMesher.cpp
	tests/TestStructures.cpp
	tests/TestDds.cpp
	tests/TestFrameLogJson.cpp
	tests/TestShaders.cpp
	tests/TestWinding.cpp
	tests/TestStatus.cpp
	tests/TestEventNames.cpp
	tests/TestSubmission.cpp
	tests/TestPredlss.cpp
	tests/TestHookGroups.cpp
	tests/TestGbuffer.cpp
	tests/TestRings.cpp
	tests/TestMessages.cpp
	tests/TestOverlay.cpp
	tests/TestFloor.cpp
	tests/TestLook.cpp
	tests/TestCommands.cpp
	tests/TestMapping.cpp
	tests/TestLink.cpp
	tests/TestMcLinkScript.cpp
	tests/TestPendingSections.cpp
	tests/TestRetired.cpp
	tests/TestHud.cpp
	tests/TestMeasurements.cpp
	tests/TestProcessUsage.cpp
	tests/TestScancodes.cpp
	tests/TestBuildMode.cpp
	tests/TestFocusWatch.cpp
	tests/TestRouter.cpp
	tests/TestTicks.cpp
	tests/TestVoxels.cpp
	tests/TestGround.cpp
	tests/TestGroundWorker.cpp
	tests/TestSolids.cpp
	tests/TestAudit.cpp
	tests/TestGameAbi.cpp
	tests/TestPrimitives.cpp
	tests/TestMeshes.cpp
	tests/TestHeightfield.cpp
	tests/TestOriginWatch.cpp
	tests/TestSceneWatch.cpp
	tests/TestPhysxSession.cpp
	tests/TestExport.cpp
	tests/TestBoxMerge.cpp
	tests/TestBodyPlan.cpp
	tests/TestDrive.cpp
	tests/TestTurn.cpp
	tests/TestDriveLog.cpp
	tests/TestDumpObj.cpp
	tests/TestActors.cpp
	tests/TestCharacters.cpp
	tests/TestNpcRegistry.cpp
	tests/TestCombat.cpp
	tests/TestCombatInbox.cpp
	tests/TestWorldTable.cpp
	tests/TestWorldShapes.cpp
	tests/TestEntities.cpp
	tools/mclink/DumpObj.cpp
	tools/mclink/McLinkScript.cpp
)
target_include_directories(render_tests PRIVATE tools/mclink)
target_link_libraries(render_tests PRIVATE render_core)
add_test(NAME render_tests COMMAND render_tests WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
