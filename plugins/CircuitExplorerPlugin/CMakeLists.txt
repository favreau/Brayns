cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
set(PLUGIN_NAME CIRCUITEXPLORER)
project(${PLUGIN_NAME} VERSION 0.1.0)
set(${PLUGIN_NAME}_VERSION_ABI 1)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake
                              ${CMAKE_SOURCE_DIR}/CMake/common)
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/CMake/common/Common.cmake)
  message(FATAL_ERROR "CMake/common missing, run: git submodule update --init")
endif()

include(Common)

common_find_package(Brayns REQUIRED)
common_find_package(Brion REQUIRED)
common_find_package_post()

set(${PLUGIN_NAME}_SOURCES
  api/CircuitExplorerParams.cpp
  io/SynapseLoader.cpp
  CircuitExplorerPlugin.cpp
)

set(${PLUGIN_NAME}_PUBLIC_HEADERS
  api/CircuitExplorerParams.h
  io/SynapseLoader.h
  CircuitExplorerPlugin.h
)

set(${PLUGIN_NAME}_LINK_LIBRARIES
  braynsParameters braynsPluginAPI vmmlib Brion Brain ${FREEIMAGE_LIBRARIES} ${HDF5_LIBRARIES})

common_library(circuitExplorer)
