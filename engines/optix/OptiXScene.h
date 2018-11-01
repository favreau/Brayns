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

#ifndef OPTIXSCENE_H
#define OPTIXSCENE_H

#include <brayns/common/scene/Scene.h>
#include <brayns/common/types.h>

#include <optixu/optixpp_namespace.h>

#include "OptiXVolume.h"

namespace brayns
{
struct BasicLight
{
    optix::float3 pos;
    optix::float3 color;
    int casts_shadow;
    int padding; // make this structure 32 bytes -- powers of two are your
                 // friend!
};

/**

   OptiX specific scene

   This object is the OptiX specific implementation of a scene

*/
class OptiXScene : public Scene
{
public:
    OptiXScene(ParametersManager& parametersManager);
    ~OptiXScene();

    /** @copydoc Scene::commit */
    void commit() final;

    /** @copydoc Scene::commitLights */
    bool commitLights() final;

    /** @copydoc Scene::commitTransferFunctionData */
    bool commitTransferFunctionData() final;

    SharedDataVolumePtr createSharedDataVolume(const Vector3ui& dimensions,
                                               const Vector3f& spacing,
                                               const DataType type) const final;

    BrickedVolumePtr createBrickedVolume(const Vector3ui& dimensions,
                                         const Vector3f& spacing,
                                         const DataType type) const final;
    ModelPtr createModel() const final;

private:
    void _commitSimulationData();
    bool _commitVolumeData();

    optix::Buffer _lightBuffer{nullptr};
    std::vector<BasicLight> _optixLights;
    ::optix::Group _rootGroup{nullptr};

    // Material Lookup tables
    optix::Buffer _colorMapBuffer{nullptr};
    optix::Buffer _emissionIntensityMapBuffer{nullptr};
    ::optix::TextureSampler _backgroundTextureSampler{nullptr};

    // Volumes
    SharedDataVolumePtr _optixVolume{nullptr};

    ModelDescriptors _activeModels;
};
} // namespace brayns
#endif // OPTIXSCENE_H
