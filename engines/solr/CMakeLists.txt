# Copyright (c) 2018, EPFL/Blue Brain Project
# All rights reserved. Do not distribute without permission.
# Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>

set(BRAYNS_SOLR_ENGINE_SOURCES
  SolRCamera.cpp
  SolREngine.cpp
  SolRFrameBuffer.cpp
  SolRMaterial.cpp
  SolRModel.cpp
  SolRRenderer.cpp
  SolRScene.cpp
  SolRVolume.cpp
)

set(BRAYNS_SOLR_ENGINE_HEADERS
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
set(BRAYNS_SOLR_ENGINE_LINK_LIBRARIES
    PRIVATE vmmlib braynsCommon braynsParameters ${SolR_LIBRARY} cudart
)

# Compile C++ code
set(BRAYNS_SOLR_ENGINE_OMIT_LIBRARY_HEADER ON)
set(BRAYNS_SOLR_ENGINE_OMIT_VERSION_HEADERS ON)
set(BRAYNS_SOLR_ENGINE_EXPORT ON)

common_library(brayns_solr_engine)
