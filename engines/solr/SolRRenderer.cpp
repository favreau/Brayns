/* Copyright (c) 2015-2016, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
 *
 * This file is part of Brayns <https://github.com/BlueBrain/Brayns>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <brayns/common/log.h>

#include "SolRCamera.h"
#include "SolRFrameBuffer.h"
#include "SolRMaterial.h"
#include "SolRRenderer.h"
#include "SolRScene.h"

#ifdef USE_CUDA
#include <solr/engines/CudaKernel.h>
#endif
#ifdef USE_OPENCL
#include <solr/engines/OpenCLKernel.h>
#endif

namespace brayns
{
SolRRenderer::SolRRenderer(solr::GPUKernel* kernel,
                           const AnimationParameters& animationParameters,
                           const RenderingParameters& renderingParameters)
    : Renderer(animationParameters, renderingParameters)
    , _kernel(kernel)
{
}

SolRRenderer::~SolRRenderer() {}

void SolRRenderer::render(FrameBufferPtr frameBuffer)
{
    auto solrFrameBuffer =
        std::static_pointer_cast<SolRFrameBuffer>(frameBuffer);
    auto lock = solrFrameBuffer->getScopeLock();

    _kernel->render_begin(0);
    _kernel->render_end();

    solrFrameBuffer->incrementAccumFrames();
    solrFrameBuffer->markModified();
}

void SolRRenderer::commit()
{
    const auto& color = _renderingParameters.getBackgroundColor();
    auto& sceneInfo = _kernel->getSceneInfo();
    sceneInfo.backgroundColor.x = color.x();
    sceneInfo.backgroundColor.y = color.y();
    sceneInfo.backgroundColor.z = color.z();
    sceneInfo.backgroundColor.w = 0.5f;
    sceneInfo.graphicsLevel = solr::glNoShading;
    sceneInfo.nbRayIterations = 3;
    sceneInfo.transparentColor = 0.f;
    sceneInfo.viewDistance = 50000.f;
    sceneInfo.shadowIntensity = 1.0f;
    sceneInfo.eyeSeparation = 380.f;
    sceneInfo.renderBoxes = 0;
    sceneInfo.maxPathTracingIterations = 10; // solr::NB_MAX_ITERATIONS;
    //        _renderingParameters.getMaxAccumFrames();
    sceneInfo.frameBufferType = solr::ftRGB;
    sceneInfo.timestamp = 0;
    sceneInfo.atmosphericEffect = solr::aeNone;
    sceneInfo.cameraType = solr::ctPerspective;
    sceneInfo.doubleSidedTriangles = false;
    sceneInfo.extendedGeometry = true;
    sceneInfo.advancedIllumination = solr::aiFull;
    sceneInfo.draftMode = false;
    sceneInfo.skyboxRadius = static_cast<int>(sceneInfo.viewDistance * 0.9f);
    sceneInfo.skyboxMaterialId = SKYBOX_SPHERE_MATERIAL;
    sceneInfo.gradientBackground = 0;
    sceneInfo.geometryEpsilon = 0.05f;
    sceneInfo.rayEpsilon = 0.05f;

    auto& postProcessingInfo = _kernel->getPostProcessingInfo();
    postProcessingInfo.type = solr::ppe_none;
    postProcessingInfo.param1 = 0.f;
    postProcessingInfo.param2 = 10.f;
    postProcessingInfo.param3 = 1000;
}

void SolRRenderer::setCamera(CameraPtr camera)
{
    _camera = static_cast<SolRCamera*>(camera.get());
    assert(_camera);
    markModified();
}

Renderer::PickResult SolRRenderer::pick(const Vector2f& pickPos)
{
    PickResult result{false, {0, 0, 0}};
    const auto id = _kernel->getPrimitiveAt(pickPos.x(), pickPos.y());
    if (id != 0)
    {
        result.hit = true;
        result.pos.x() = id;
    }
    return result;
}

} // namespace brayns
