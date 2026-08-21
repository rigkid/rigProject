file(READ "${CMAKE_CURRENT_LIST_DIR}/pack.json" PACK_JSON)
string(JSON PACK_NAME GET "${PACK_JSON}" name)

if(NOT TARGET rigComponent)
	if(DEFINED RIGKIT_ROOT AND NOT RIGKIT_ROOT STREQUAL "")
		set(_rigComponent_dir "${RIGKIT_ROOT}/packs/rigComponent")
	else()
		set(_rigComponent_dir "${CMAKE_SOURCE_DIR}/packs/rigComponent")
	endif()
	if(EXISTS "${_rigComponent_dir}/CMakeLists.txt")
		add_subdirectory("${_rigComponent_dir}" "${CMAKE_BINARY_DIR}/packs/rigComponent")
	endif()
endif()

add_library(${PACK_NAME} STATIC
	src/rigProject.cpp
	src/rigProject.h
	src/ProjectSerializer.cpp
	src/ProjectSerializer.h
	src/ComponentSerializerRegistry.cpp
	src/ComponentSerializerRegistry.h
	src/AddSerializer.h
	src/CoreSerializers.cpp
	src/ComponentSerializers.cpp
	src/ComponentSerializers.h
	src/EntityIdRemap.cpp
	src/EntityIdRemap.h
	src/ProjectJson.h
	src/CProject.h
	src/CPage.h
	src/rig/pages.h
	src/rig/pagesWindow.h
	src/ContractImport.cpp
	src/ContractImport.h
	src/Rigz.cpp
	src/Rigz.h
)

include(FetchContent)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
set(MINIZ_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MINIZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MINIZ_BUILD_FRAMEWORK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	miniz
	GIT_REPOSITORY https://github.com/richgel999/miniz.git
	GIT_TAG 3.0.2
	GIT_SHALLOW TRUE
	EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(miniz)
if(NOT TARGET miniz)
	# miniz 3.0.2 still declares cmake_minimum_required(3.0); if its project
	# did not create a target, build the amalgamation sources ourselves.
	set(_miniz_export_dir "${CMAKE_CURRENT_BINARY_DIR}/miniz_export")
	file(MAKE_DIRECTORY "${_miniz_export_dir}")
	file(WRITE "${_miniz_export_dir}/miniz_export.h"
		"#pragma once\n#ifndef MINIZ_EXPORT\n#define MINIZ_EXPORT\n#endif\n")
	add_library(miniz STATIC
		"${miniz_SOURCE_DIR}/miniz.c"
		"${miniz_SOURCE_DIR}/miniz_tdef.c"
		"${miniz_SOURCE_DIR}/miniz_tinfl.c"
		"${miniz_SOURCE_DIR}/miniz_zip.c"
	)
	target_include_directories(miniz PUBLIC
		"${miniz_SOURCE_DIR}"
		"${_miniz_export_dir}"
	)
endif()

target_link_libraries(${PACK_NAME} PUBLIC rigkit)
if(TARGET rigComponent)
	target_link_libraries(${PACK_NAME} PUBLIC rigComponent)
endif()
target_link_libraries(${PACK_NAME} PRIVATE miniz)
target_include_directories(${PACK_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
)

# ComponentSerializers.cpp registers every rigComponent codec in one TU; MinGW/
# MSVC Debug hits the PE COFF section limit without big-obj (relocation
# truncated to fit: IMAGE_REL_AMD64_ADDR32NB).
if(MSVC)
	target_compile_options(${PACK_NAME} PRIVATE /bigobj)
elseif(MINGW OR (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
	target_compile_options(${PACK_NAME} PRIVATE -Wa,-mbig-obj)
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	target_compile_options(${PACK_NAME} PRIVATE -bigobj)
endif()
message(STATUS "Configured pack: ${PACK_NAME} (project serialize)")
