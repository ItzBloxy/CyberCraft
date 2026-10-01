cmake_minimum_required(VERSION 3.25)

project(BlockRender VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(RED4EXT_SDK_DIR "${REPO_ROOT}/extern/RED4ext.SDK" CACHE PATH "RED4ext.SDK checkout")
set(STREAMLINE_DIR "${REPO_ROOT}/extern/streamline-2.7.2" CACHE PATH "Streamline 2.7.2 headers")
set(CP2077_DIR "" CACHE PATH "Cyberpunk 2077 folder; when set, each build copies the plugin into red4ext/plugins/BlockRender")

# Shared settings. Streamline's headers are SYSTEM, which keeps their warnings out of ours.
add_library(render_settings INTERFACE)
target_compile_options(render_settings INTERFACE
	$<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus>
	$<$<COMPILE_LANGUAGE:C>:/W4 /utf-8>)
target_compile_definitions(render_settings INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(render_settings INTERFACE src)
target_include_directories(render_settings SYSTEM INTERFACE "${STREAMLINE_DIR}/include")

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
)
target_link_libraries(render_core PUBLIC render_settings d3dcompiler dxguid)

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
)
target_link_libraries(render_tests PRIVATE render_core)
add_test(NAME render_tests COMMAND render_tests WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
