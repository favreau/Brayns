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

#include <brayns/common/input/KeyboardHandler.h>

#include <brayns/parameters/ParametersManager.h>

#include "SolRCamera.h"
#include "SolREngine.h"
#include "SolRFrameBuffer.h"
#include "SolRMaterial.h"
#include "SolRRenderer.h"
#include "SolRScene.h"

#ifdef USE_CUDA
#include <solr/engines/CudaKernel.h>
#else
#ifdef USE_OPENCL
#include <solr/engines/OpenCLKernel.h>
#endif // USE_OPENCL
#endif // USE_CUDA

namespace brayns
{
SolREngine::SolREngine(ParametersManager& parametersManager)
    : Engine(parametersManager)
{
    if (!_kernel)
    {
#ifdef USE_CUDA
        BRAYNS_INFO << "Initializing SolR with CUDA engine" << std::endl;
        _kernel = new solr::CudaKernel();
#else
#ifdef USE_OPENCL
        BRAYNS_INFO << "Initializing SolR with OpenCL engine" << std::endl;
        _kernel = new solr::OpenCLKernel();
#else
        throw std::runtime_error("Sol-R engine is undefined");
#endif // USE_OPENCL
#endif // USE_CUDA
    }

    BRAYNS_INFO << "Initializing scene" << std::endl;
    _scene = createScene(parametersManager);

    BRAYNS_INFO << "Initializing camera" << std::endl;
    _createCameras();

    BRAYNS_INFO << "Initializing renderers" << std::endl;
    _createRenderers();

    BRAYNS_INFO << "Initializing frame buffer" << std::endl;

    const auto frameSize = getSupportedFrameSize(
        _parametersManager.getApplicationParameters().getWindowSize());

    auto& sceneInfo = _kernel->getSceneInfo();
    sceneInfo.size = {(int)frameSize.x(), (int)frameSize.y()};
    _frameBuffer =
        createFrameBuffer(frameSize, FrameBufferFormat::rgb_i8, true);

    _kernel->initBuffers();
    _kernel->resetAll();
    _kernel->setFrame(0);

    BRAYNS_INFO << "Engine initialization complete" << std::endl;
}

SolREngine::~SolREngine()
{
    if (_kernel)
    {
        _kernel->cleanup();
        delete _kernel;
        _kernel = nullptr;
    }
}

EngineType SolREngine::name() const
{
    return _type;
}

void SolREngine::commit()
{
    Engine::commit();
}

void SolREngine::preRender() {}

void SolREngine::_createRenderers()
{
    auto& rp = _parametersManager.getRenderingParameters();
    auto solrRenderer = std::make_shared<SolRRenderer>(
        _kernel, _parametersManager.getAnimationParameters(), rp);

    for (const auto& renderer : rp.getRenderers())
    {
        PropertyMap properties;
        if (renderer == "basic")
        {
            properties.setProperty(
                {"shadows", "Shadow intensity", 0., {0., 1.}});
            properties.setProperty(
                {"softShadows", "Shadow softness", 0., {0., 1.}});
        }
    }
    _renderer = solrRenderer;
}

FrameBufferPtr SolREngine::createFrameBuffer(
    const Vector2ui& frameSize, const FrameBufferFormat frameBufferFormat,
    const bool accumulation) const
{
    return std::make_shared<SolRFrameBuffer>(_kernel, frameSize,
                                             frameBufferFormat, accumulation);
}

ScenePtr SolREngine::createScene(ParametersManager& parametersManager) const
{
    return std::make_shared<SolRScene>(_kernel, parametersManager);
}

CameraPtr SolREngine::createCamera() const
{
    return std::make_shared<SolRCamera>(_kernel);
}

void SolREngine::_createCameras()
{
    auto solrCamera = std::make_shared<SolRCamera>(_kernel);

    PropertyMap::Property fovy{"fovy", "Field of view", 45., {.1, 360.}};
    PropertyMap::Property aspect{"aspect", "Aspect ratio", 1.};
    aspect.markReadOnly();

    RenderingParameters& rp = _parametersManager.getRenderingParameters();
    for (const auto& camera : rp.getCameras())
    {
        PropertyMap properties;
        properties.setProperty(aspect);
        if (camera == "perspective" || camera == "clippedperspective")
        {
            properties.setProperty(fovy);
            properties.setProperty({"apertureRadius", "Aperture radius", 0.});
            properties.setProperty({"focusDistance", "Focus Distance", 1.});
        }
        if (camera == "orthographic")
        {
            properties.setProperty({"height", "Height", 1.});
        }
        solrCamera->setProperties(camera, properties);
    }
    solrCamera->setCurrentType(rp.getCameraType());
    _camera = solrCamera;
}

RendererPtr SolREngine::createRenderer(
    const AnimationParameters& animationParameters,
    const RenderingParameters& renderingParameters) const
{
    return std::make_shared<SolRRenderer>(_kernel, animationParameters,
                                          renderingParameters);
}

Vector2ui SolREngine::getSupportedFrameSize(const Vector2ui& size) const
{
    return size;
}

Vector2ui SolREngine::getMinimumFrameSize() const
{
    return {64, 64};
}

} // namespace brayns
