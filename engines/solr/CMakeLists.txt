# Copyright (c) 2018, EPFL/Blue Brain Project
# All rights reserved. Do not distribute without permission.
# Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>

set(BRAYNSSOLRENGINEPLUGIN_SOURCES
  SolRCamera.cpp
  SolREngine.cpp
  SolRFrameBuffer.cpp
  SolRMaterial.cpp
  SolRModel.cpp
  SolRRenderer.cpp
  SolRScene.cpp
  SolRVolume.cpp
)

set(BRAYNSSOLRENGINEPLUGIN_HEADERS
  SolRCamera.h
  SolREngine.h
  SolRFrameBuffer.h
  SolRMaterial.h
  SolRModel.h
  SolRRenderer.h
  SolRScene.h
  SolRVolume.h
)

# TODO (Add lib according to engine: Cuda or OpenCL)
set(BRAYNSSOLRENGINEPLUGIN_LINK_LIBRARIES
    PRIVATE vmmlib braynsCommon braynsParameters ${SOLR_LIBRARY} cudart
)

# Compile C++ code
set(BRAYNSSOLRENGINEPLUGIN_OMIT_LIBRARY_HEADER ON)
set(BRAYNSSOLRENGINEPLUGIN_OMIT_VERSION_HEADERS ON)
set(BRAYNSSOLRENGINEPLUGIN_EXPORT ON)

common_library(braynsSolREnginePlugin)
