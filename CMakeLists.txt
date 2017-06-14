cmake_minimum_required (VERSION 3.1.0)

project(moovoo)


#set(Boost_USE_STATIC_LIBS        ON)
#set(Boost_USE_MULTITHREADED      ON)
#set(Boost_USE_STATIC_RUNTIME    OFF)
find_package(Boost 1.58.0 COMPONENTS python-py35)

find_package(PythonLibs 3.5 REQUIRED)

message(${PYTHON_INCLUDE_DIRS})
 
include_directories(${PROJECT_SOURCE_DIR}/external)
include_directories(${PYTHON_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${PROJECT_SOURCE_DIR}/external/GLFW)
link_directories(${PROJECT_SOURCE_DIR}/external/vulkan)


add_definitions(-DSOURCE_DIR="${CMAKE_SOURCE_DIR}/")
add_definitions(-DBINARY_DIR="${PROJECT_BINARY_DIR}/")

set(CMAKE_CXX_STANDARD 11)

set(shadersrc atoms.vert atoms.frag fount.vert fount.frag conns.vert conns.frag dynamics.comp skybox.vert skybox.frag)

set(shaders "")

foreach(shader ${shadersrc})
	add_custom_command(
		OUTPUT ${shader}.spv
		COMMAND glslangValidator -V ${PROJECT_SOURCE_DIR}/moovoo/${shader} -o ${PROJECT_BINARY_DIR}/${shader}.spv
		MAIN_DEPENDENCY moovoo/${shader}
	)
	list(APPEND shaders "moovoo/${shader}")
endforeach(shader)

add_library(moovoo SHARED moovoo/main.cpp ${shaders} ../external/vku/vku.hpp)
target_link_libraries(moovoo ${Boost_LIBRARIES})
target_link_libraries(moovoo ${PYTHON_LIBRARIES})
target_compile_features(moovoo PRIVATE cxx_range_for)
SET_TARGET_PROPERTIES(moovoo PROPERTIES PREFIX "")

if (WIN32)
	target_link_libraries(moovoo vulkan-1)
endif()

if (UNIX)
	target_link_libraries(moovoo vulkan dl pthread)
endif()


