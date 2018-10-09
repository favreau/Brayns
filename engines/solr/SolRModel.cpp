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

#include "SolRModel.h"
#include "SolREngine.h"
#include "SolRMaterial.h"
#include "SolRVolume.h"

#include <brayns/common/material/Material.h>
#include <brayns/common/scene/Scene.h>

#ifdef USE_CUDA
#include <solr/engines/CudaKernel.h>
#endif
#ifdef USE_OPENCL
#include <solr/engines/OpenCLKernel.h>
#endif

namespace brayns
{
SolRModel::SolRModel(solr::GPUKernel* kernel)
    : _kernel(kernel)
{
}

SolRModel::~SolRModel() {}

void SolRModel::commit()
{
    if (!dirty())
        return;

    // Materials
    for (auto material : _materials)
        material.second->commit();

    // Spheres
    if (_spheresDirty)
    {
        for (const auto& spheres : _spheres)
        {
            const auto material = spheres.first;
            for (const auto& s : spheres.second)
            {
                const auto id = _kernel->addPrimitive(solr::ptSphere, true);
                const auto c = s.center;
                const auto r = s.radius;
                _kernel->setPrimitive(id, c.x(), c.y(), c.z(), r, 0, 0,
                                      material);
            }
        }
        _spheresDirty = false;
    }

    // Cylinders
    if (_cylindersDirty)
    {
        for (const auto& cylinders : _cylinders)
        {
            const auto material = cylinders.first;
            for (const auto& c : cylinders.second)
            {
                const auto id = _kernel->addPrimitive(solr::ptCylinder, true);
                const auto c0 = c.center;
                const auto c1 = c.center + c.up;
                const auto r = c.radius;
                _kernel->setPrimitive(id, c0.x(), c0.y(), c0.z(), c1.x(),
                                      c1.y(), c1.z(), r, 0, 0, material);
            }
        }
        _cylindersDirty = false;
    }

    // Cones
    if (_conesDirty)
    {
        for (const auto& cones : _cones)
        {
            const auto material = cones.first;
            for (const auto& c : cones.second)
            {
                const auto id = _kernel->addPrimitive(solr::ptCone, true);
                const auto c0 = c.center;
                const auto c1 = c.center + c.up;
                const auto r0 = c.centerRadius;
                const auto r1 = c.upRadius;
                _kernel->setPrimitive(id, c0.x(), c0.y(), c0.z(), c1.x(),
                                      c1.y(), c1.z(), r0, r1, 0, material);
            }
        }
        _conesDirty = false;
    }

    _updateBounds();

    // handled by the scene
    _instancesDirty = false;

    _kernel->compactBoxes(true);
}

MaterialPtr SolRModel::createMaterial(const size_t materialId,
                                      const std::string& name)
{
    MaterialPtr material = std::make_shared<SolRMaterial>(_kernel);
    material->setName(name);
    _materials[materialId] = material;
    return material;
}

void SolRModel::buildBoundingBox()
{
    BRAYNS_ERROR << "SolRModel::buildBoundingBox not implemented" << std::endl;
}
} // namespace brayns
