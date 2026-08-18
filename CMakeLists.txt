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
	src/EntityIdRemap.cpp
	src/EntityIdRemap.h
	src/ProjectJson.h
	src/CProject.h
	src/CPage.h
	src/rig/pages.h
	src/rig/pagesWindow.h
	src/ContractImport.cpp
	src/ContractImport.h
)
target_link_libraries(${PACK_NAME} PUBLIC rigkit)
if(TARGET rigComponent)
	target_link_libraries(${PACK_NAME} PUBLIC rigComponent)
endif()
target_include_directories(${PACK_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
)
message(STATUS "Configured pack: ${PACK_NAME} (project serialize)")
