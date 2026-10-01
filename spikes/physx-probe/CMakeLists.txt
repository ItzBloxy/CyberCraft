cmake_minimum_required(VERSION 3.25)

project(PhysXProbe VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(RED4EXT_SDK_DIR "${REPO_ROOT}/extern/RED4ext.SDK" CACHE PATH "RED4ext.SDK checkout")
set(PHYSX_DIR "${REPO_ROOT}/extern/physx-3.4.2" CACHE PATH "PhysX 3.4.2 headers (PhysX_3.4/Include, PxShared/include)")
set(CP2077_DIR "" CACHE PATH "Cyberpunk 2077 folder; when set, each build copies the plugin into red4ext/plugins/PhysXProbe")

# Shared settings.
add_library(probe_settings INTERFACE)
target_compile_options(probe_settings INTERFACE /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus)
target_compile_definitions(probe_settings INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX PX_PHYSX_STATIC_LIB)
target_include_directories(probe_settings INTERFACE src)
target_include_directories(probe_settings SYSTEM INTERFACE "${PHYSX_DIR}/PhysX_3.4/Include" "${PHYSX_DIR}/PxShared/include")

# Everything that doesn't need RED4ext: geometry, checks, writers, the PhysX session and one dump.
add_library(probe_core STATIC
	src/Json.cpp
	src/physx/ShapeTriangles.cpp
	src/physx/Checks.cpp
	src/physx/Readable.cpp
	src/physx/GameAbi.cpp
	src/ObjWriter.cpp
	src/ReportJson.cpp
	src/DumpWriter.cpp
	src/DumpRequest.cpp
	src/physx/PhysxSession.cpp
	src/Probe.cpp
)
target_link_libraries(probe_core PUBLIC probe_settings version)

# The RED4ext SDK, header-only. SYSTEM keeps its warnings out of ours.
set(RED4EXT_HEADER_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory("${RED4EXT_SDK_DIR}" red4ext-sdk EXCLUDE_FROM_ALL SYSTEM)

# The RED4ext plugin.
add_library(PhysXProbe SHARED
	src/Plugin.cpp
	src/PlayerLocator.cpp
)
target_link_libraries(PhysXProbe PRIVATE probe_core RED4ext::SDK)

if(CP2077_DIR)
	add_custom_command(TARGET PhysXProbe POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CP2077_DIR}/red4ext/plugins/PhysXProbe"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:PhysXProbe>" "$<TARGET_PDB_FILE:PhysXProbe>" "${CP2077_DIR}/red4ext/plugins/PhysXProbe/"
		VERBATIM)
endif()

# Unit tests: no game needed.
enable_testing()
add_executable(probe_tests
	tests/TestMain.cpp
	tests/TestCoords.cpp
	tests/TestJson.cpp
	tests/TestPrimitives.cpp
	tests/TestMeshes.cpp
	tests/TestHeightfield.cpp
	tests/TestChecks.cpp
	tests/TestObjWriter.cpp
	tests/TestReportJson.cpp
	tests/TestDumpWriter.cpp
	tests/TestFileVersions.cpp
	tests/TestAudit.cpp
	tests/TestReadable.cpp
	tests/TestGameAbi.cpp
	tests/TestDumpRequest.cpp
	tests/TestProbe.cpp
)
target_link_libraries(probe_tests PRIVATE probe_core)
add_test(NAME probe_tests COMMAND probe_tests)
