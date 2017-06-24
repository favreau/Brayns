# Copyright (c) 2015-2017, EPFL/Blue Brain Project
# All rights reserved. Do not distribute without permission.
# Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>

set(DEFLECTSTREAMER_LINK_LIBRARIES
  PUBLIC Deflect)

set(DEFLECTSTREAMER_SOURCES
    OSPRayPixelOperations.cpp
    DeflectStreamer.cpp
)

set(DEFLECTSTREAMER_PUBLIC_HEADERS
  OSPRayPixelOperations.h
)

common_library(DeflectStreamer)
