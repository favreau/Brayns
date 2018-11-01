# Locate SolR SDK
#
# This module defines
# SolR_LIBRARY
# SolR_FOUND, if false, do not try to link to leap
# SolR_INCLUDE_DIRS, where to find the headers
#
# $SolRSDK_DIR is an environment variable that may be
# used to locate the Leap Motion SDK directory
#
# Created by Cyrille Favreau.

FIND_PATH(SolR_INCLUDE_DIRS solr.h
    $ENV{SolRSDK_DIR}/include/solr
    ${CMAKE_PREFIX}/include/solr
)

FIND_LIBRARY(SolR_LIBRARY
    NAMES solr
    PATHS
    $ENV{SolRSDK_DIR}/lib
    $ENV{SolRSDK_DIR}
    ${CMAKE_INSTALL_PREFIX}/lib
)

SET(SolR_FOUND "NO")
IF(SolR_LIBRARY AND SolR_INCLUDE_DIRS)
    SET(SolR_FOUND "YES")
ENDIF(SolR_LIBRARY AND SolR_INCLUDE_DIRS)

# TODO: OpenCL and CUDA
list(APPEND SolR_LIBRARY GL OpenCL)
