# Locate SolR SDK
#
# This module defines
# SOLR_LIBRARY
# SOLR_FOUND, if false, do not try to link to leap
# SOLR_INCLUDE_DIR, where to find the headers
#
# $SOLRSDK_DIR is an environment variable that may be
# used to locate the Leap Motion SDK directory
#
# Created by Cyrille Favreau.

FIND_PATH(SOLR_INCLUDE_DIRS solr.h
    $ENV{SOLRSDK_DIR}/include/solr
    ${CMAKE_INSTALL_PREFIX}/include/solr
)

FIND_LIBRARY(SOLR_LIBRARY
    NAMES solr
    PATHS
    $ENV{SOLRSDK_DIR}/lib
    $ENV{SOLRSDK_DIR}
    ${CMAKE_INSTALL_PREFIX}/lib
)

SET(SOLR_FOUND "NO")
IF(SOLR_LIBRARY AND SOLR_INCLUDE_DIRS)
    SET(SOLR_FOUND "YES")
ENDIF(SOLR_LIBRARY AND SOLR_INCLUDE_DIRS)

# TODO: OpenCL and CUDA
list(APPEND SOLR_LIBRARY GL OpenCL)
