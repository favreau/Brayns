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

#ifndef OPTIXMODEL_H
#define OPTIXMODEL_H

#include <brayns/common/scene/Model.h>

#include <optixu/optixpp_namespace.h>

#include <map>

namespace brayns
{
class OptiXModel : public Model
{
public:
    OptiXModel() = default;
    ~OptiXModel();

    /** @copydoc Model::commit */
    void commit() final;

    /** @copydoc Model::buildBoundingBox */
    void buildBoundingBox() final;

    /** @copydoc Model::createMaterial */
    MaterialPtr createMaterial(const size_t materialId,
                               const std::string& name) final;

    ::optix::GeometryGroup getGeometryGroup() const { return _geometryGroup; }
    ::optix::GeometryGroup getBoundingBoxGroup() const
    {
        return _boundingBoxGroup;
    }

private:
    void _commitSpheres(const size_t materialId);
    void _commitCylinders(const size_t materialId);
    void _commitCones(const size_t materialId);
    void _commitMeshes(const size_t materialId);
    void _commitMaterials();

    ::optix::GeometryGroup _geometryGroup{nullptr};
    ::optix::GeometryGroup _boundingBoxGroup{nullptr};

    // Material Lookup tables
    ::optix::Buffer _colorMapBuffer{nullptr};
    ::optix::Buffer _emissionIntensityMapBuffer{nullptr};

    // Spheres
    std::map<size_t, ::optix::Buffer> _spheresBuffers;
    std::map<size_t, ::optix::Geometry> _optixSpheres;

    // Cylinders
    std::map<size_t, optix::Buffer> _cylindersBuffers;
    std::map<size_t, optix::Geometry> _optixCylinders;

    // Cones
    std::map<size_t, optix::Buffer> _conesBuffers;
    std::map<size_t, optix::Geometry> _optixCones;

    // Triangle meshes
    ::optix::Geometry _mesh{nullptr};
    ::optix::Buffer _verticesBuffer{nullptr};
    ::optix::Buffer _indicesBuffer{nullptr};
    ::optix::Buffer _normalsBuffer{nullptr};
    ::optix::Buffer _textureCoordsBuffer{nullptr};
    ::optix::Buffer _colorsBuffer{nullptr};

    // Materials and textures
    std::vector<size_t> _optixMaterialsMap;
    std::map<std::string, optix::Buffer> _optixTextures;
    std::map<std::string, optix::TextureSampler> _optixTextureSamplers;
};
} // namespace brayns

#endif // OPTIXMODEL_H
