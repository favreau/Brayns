/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
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

#include "OptiXCamera.h"
#include "OptiXEngine.h"
#include "OptiXFrameBuffer.h"
#include "OptiXRenderer.h"
#include "OptiXScene.h"
#include "OptiXUtils.h"

namespace brayns
{
OptiXEngine::OptiXEngine(ParametersManager& parametersManager)
    : Engine(parametersManager)
{
    BRAYNS_INFO << "Initializing OptiX" << std::endl;
    _initializeContext();

    BRAYNS_INFO << "Initializing renderers" << std::endl;
    _createRenderers();

    BRAYNS_INFO << "Initializing scene" << std::endl;
    _scene.reset(new OptiXScene(_parametersManager));

    BRAYNS_INFO << "Initializing frame buffer" << std::endl;
    _frameSize = _parametersManager.getApplicationParameters().getWindowSize();

    const auto& renderParams = _parametersManager.getRenderingParameters();
    _frameBuffer = createFrameBuffer(_frameSize, FrameBufferFormat::rgba_i8,
                                     renderParams.getAccumulation());

    BRAYNS_INFO << "Initializing cameras" << std::endl;
    _createCameras();

    BRAYNS_INFO << "Engine initialization complete" << std::endl;
}

OptiXEngine::~OptiXEngine()
{
    _scene.reset();
    _frameBuffer.reset();
    _renderer.reset();
    _camera.reset();

    auto context = OptiXContext::get().getOptixContext();
    if (context)
        context->destroy();
}

void OptiXEngine::_initializeContext()
{
    // Set up context
    auto context = OptiXContext::get().getOptixContext();
    if (!context)
        BRAYNS_THROW(std::runtime_error("Failed to initialize OptiX"));
}

void OptiXEngine::_createCameras()
{
    auto optixCamera = createCamera();

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
        optixCamera->setProperties(camera, properties);
    }
    optixCamera->setCurrentType(rp.getCameraType());
    _camera = optixCamera;
}

void OptiXEngine::_createRenderers()
{
    auto& rp = _parametersManager.getRenderingParameters();
    auto optixRenderer =
        createRenderer(_parametersManager.getAnimationParameters(), rp);

    for (const auto& renderer : rp.getRenderers())
    {
        PropertyMap properties;
        if (renderer == "pathtracing")
        {
            properties.setProperty(
                {"shadows", "Shadow intensity", 0., {0., 1.}});
            properties.setProperty(
                {"softShadows", "Shadow softness", 0., {0., 1.}});
        }
        if (renderer == "proximity")
        {
            properties.setProperty(
                {"alphaCorrection", "Alpha correction", 0.5, {0.001, 1.}});
            properties.setProperty(
                {"detectionDistance", "Detection distance", 1.});
            properties.setProperty({"detectionFarColor", "Detection far color",
                                    std::array<double, 3>{{1., 0., 0.}}});
            properties.setProperty({"detectionNearColor",
                                    "Detection near color",
                                    std::array<double, 3>{{0., 1., 0.}}});
            properties.setProperty({"detectionOnDifferentMaterial",
                                    "Detection on different material", false});
            properties.setProperty(
                {"surfaceShadingEnabled", "Surface shading", true});
        }
        if (renderer == "basic_simulation")
        {
            properties.setProperty(
                {"alphaCorrection", "Alpha correction", 0.5, {0.001, 1.}});
        }
        if (renderer == "advanced_simulation")
        {
            properties.setProperty(
                {"aoDistance", "Ambient occlusion distance", 10000.});
            properties.setProperty(
                {"aoWeight", "Ambient occlusion weight", 0., {0., 1.}});
            properties.setProperty(
                {"detectionDistance", "Detection distance", 15.});
            properties.setProperty(
                {"shadows", "Shadow intensity", 0., {0., 1.}});
            properties.setProperty(
                {"softShadows", "Shadow softness", 0., {0., 1.}});
            properties.setProperty({"samplingThreshold",
                                    "Threshold under which sampling is ignored",
                                    0.001,
                                    {0.001, 1.}});
            properties.setProperty({"volumeSpecularExponent",
                                    "Volume specular exponent",
                                    20.,
                                    {1., 100.}});
            properties.setProperty({"volumeAlphaCorrection",
                                    "Volume alpha correction",
                                    0.5,
                                    {0.001, 1.}});
        }
        if (renderer == "scivis")
        {
            properties.setProperty(
                {"aoDistance", "Ambient occlusion distance", 10000.});
            properties.setProperty(
                {"aoSamples", "Ambient occlusion samples", 1, {0, 128}});
            properties.setProperty({"aoTransparencyEnabled",
                                    "Ambient occlusion transparency", true});
            properties.setProperty(
                {"aoWeight", "Ambient occlusion weight", 0., {0., 1.}});
            properties.setProperty(
                {"oneSidedLighting", "One-sided lighting", true});
            properties.setProperty({"shadowsEnabled", "Shadows", false});
        }
        optixRenderer->setProperties(renderer, properties);
    }
    optixRenderer->setCurrentType(rp.getCurrentRenderer());
    _renderer = optixRenderer;
}

ScenePtr OptiXEngine::createScene(ParametersManager& parametersManager) const
{
    return std::make_shared<OptiXScene>(parametersManager);
}

FrameBufferPtr OptiXEngine::createFrameBuffer(
    const Vector2ui& frameSize, FrameBufferFormat frameBufferFormat,
    const bool /*accumulation*/) const
{
    const auto& renderParams = _parametersManager.getRenderingParameters();
    return std::make_shared<OptiXFrameBuffer>(frameSize, frameBufferFormat,
                                              renderParams);
}

RendererPtr OptiXEngine::createRenderer(
    const AnimationParameters& animationParameters,
    const RenderingParameters& renderingParameters) const
{
    return std::make_shared<OptiXRenderer>(animationParameters,
                                           renderingParameters);
}

CameraPtr OptiXEngine::createCamera() const
{
    const auto environmentMap =
        !_parametersManager.getSceneParameters().getEnvironmentMap().empty();
    return std::make_shared<OptiXCamera>(environmentMap);
}

void OptiXEngine::commit()
{
    Engine::commit();
}

void OptiXEngine::preRender()
{
    const auto& renderParams = _parametersManager.getRenderingParameters();
    if (renderParams.getAccumulation() != _frameBuffer->getAccumulation())
        _frameBuffer->setAccumulation(renderParams.getAccumulation());
}

Vector2ui OptiXEngine::getSupportedFrameSize(const Vector2ui& size) const
{
    return size;
}

Vector2ui OptiXEngine::getMinimumFrameSize() const
{
    return {1, 1};
}
} // namespace brayns
