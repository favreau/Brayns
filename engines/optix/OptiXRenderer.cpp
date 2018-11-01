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

#include "OptiXRenderer.h"
#include "OptiXFrameBuffer.h"
#include "OptiXUtils.h"
#include <chrono>

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

namespace brayns
{
OptiXRenderer::OptiXRenderer(const AnimationParameters& animationParameters,
                             const RenderingParameters& renderingParameters)
    : Renderer(animationParameters, renderingParameters)
{
    auto context = OptiXContext::get().getOptixContext();

    context["max_depth"]->setUint(10);
    context["radiance_ray_type"]->setUint(0);
    context["shadow_ray_type"]->setUint(1);
    context["scene_epsilon"]->setFloat(1.e-5f);
}

void OptiXRenderer::render(FrameBufferPtr frameBuffer)
{
    if (!frameBuffer->getAccumulation())
        return;

    // Provide a random seed to the renderer
    optix::float4 jitter = {(float)rand() / (float)RAND_MAX,
                            (float)rand() / (float)RAND_MAX,
                            (float)rand() / (float)RAND_MAX,
                            (float)rand() / (float)RAND_MAX};
    auto context = OptiXContext::get().getOptixContext();
    context["jitter4"]->setFloat(jitter);

    // Render
    const auto size = frameBuffer->getSize();
    context->launch(0, size.x(), size.y());
}

void OptiXRenderer::commit()
{
    auto context = OptiXContext::get().getOptixContext();
    const auto& properties = _properties["advanced_simulation"];

    setOptiXProperties(context, properties);

    auto color = _renderingParameters.getBackgroundColor();
    context["ambient_light_color"]->setFloat(color.x(), color.y(), color.z());
    context["bg_color"]->setFloat(color.x(), color.y(), color.z());
}

void OptiXRenderer::setCamera(CameraPtr /*camera*/) {}
} // namespace brayns
