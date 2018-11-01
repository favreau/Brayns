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

#include "OptiXModel.h"
#include "OptiXContext.h"
#include "OptiXMaterial.h"
#include "OptiXUtils.h"

#include <brayns/common/log.h>

#include <brayns/common/material/Material.h>

namespace brayns
{
OptiXModel::~OptiXModel()
{
    for (auto& spheres : _spheresBuffers)
        spheres.second->destroy();
    for (auto& cones : _conesBuffers)
        cones.second->destroy();
    for (auto& cylinders : _cylindersBuffers)
        cylinders.second->destroy();

    RT_DESTROY(_verticesBuffer);
    RT_DESTROY(_indicesBuffer);
    RT_DESTROY(_normalsBuffer);
    RT_DESTROY(_colorsBuffer);
    RT_DESTROY(_textureCoordsBuffer);
    RT_DESTROY(_geometryGroup);
    RT_DESTROY(_boundingBoxGroup);
}

void OptiXModel::commit()
{
    // Materials
    _commitMaterials();

    // Geometry group
    RT_DESTROY(_geometryGroup);
    _geometryGroup = OptiXContext::get().createGeometryGroup();

    size_t nbSpheres = 0;
    size_t nbCylinders = 0;
    size_t nbCones = 0;
    if (_spheresDirty)
        for (const auto& spheres : _spheres)
        {
            nbSpheres += spheres.second.size();
            _commitSpheres(spheres.first);
        }

    if (_cylindersDirty)
        for (const auto& cylinders : _cylinders)
        {
            nbCylinders += cylinders.second.size();
            _commitCylinders(cylinders.first);
        }

    if (_conesDirty)
        for (const auto& cones : _cones)
        {
            nbCones += cones.second.size();
            _commitCones(cones.first);
        }

    if (_trianglesMeshesDirty)
        for (const auto& meshes : _trianglesMeshes)
            _commitMeshes(meshes.first);

    _updateBounds();

    // handled by the scene
    _instancesDirty = false;

    BRAYNS_DEBUG << nbSpheres << " spheres" << std::endl;
    BRAYNS_DEBUG << nbCylinders << " cylinders" << std::endl;
    BRAYNS_DEBUG << nbCones << " cones" << std::endl;
    BRAYNS_DEBUG << "Geometry group has " << _geometryGroup->getChildCount()
                 << " children instances" << std::endl;
    BRAYNS_DEBUG << "Bounding box group has "
                 << _boundingBoxGroup->getChildCount() << " children instances"
                 << std::endl;
}

void OptiXModel::_commitSpheres(const size_t materialId)
{
    if (_spheres.find(materialId) == _spheres.end())
        return;

    auto context = OptiXContext::get().getOptixContext();
    const auto& spheres = _spheres[materialId];
    const auto bufferSize = spheres.size() * sizeof(Sphere);
    context["sphere_size"]->setUint(sizeof(Sphere) / sizeof(float));

    // Geometry
    _optixSpheres[materialId] =
        OptiXContext::get().createGeometry(OptixGeometryType::sphere);
    _optixSpheres[materialId]->setPrimitiveCount(spheres.size());

    // Buffer
    if (!_spheresBuffers[materialId])
        _spheresBuffers[materialId] =
            context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT, bufferSize);
    memcpy(_spheresBuffers[materialId]->map(), spheres.data(), bufferSize);
    _spheresBuffers[materialId]->unmap();
    _optixSpheres[materialId]["spheres"]->setBuffer(
        _spheresBuffers[materialId]);

    // Material
    auto& mat = static_cast<OptiXMaterial&>(*_materials[materialId]);
    const auto material = mat.getOptixMaterial();
    if (!material)
        BRAYNS_THROW(std::runtime_error("Material is not defined"));

    // Instance
    auto instance = context->createGeometryInstance();
    instance->setGeometry(_optixSpheres[materialId]);
    instance->setMaterialCount(1);
    instance->setMaterial(0, material);
    if (materialId == BOUNDINGBOX_MATERIAL_ID)
        _boundingBoxGroup->addChild(instance);
    else
        _geometryGroup->addChild(instance);
}

void OptiXModel::_commitCylinders(const size_t materialId)
{
    if (_cylinders.find(materialId) == _cylinders.end())
        return;

    auto context = OptiXContext::get().getOptixContext();
    const auto& cylinders = _cylinders[materialId];
    const auto bufferSize = cylinders.size() * sizeof(Cylinder);
    context["cylinder_size"]->setUint(sizeof(Cylinder) / sizeof(float));
    _optixCylinders[materialId] =
        OptiXContext::get().createGeometry(OptixGeometryType::cylinder);

    auto& optixCylinders = _optixCylinders[materialId];
    _optixCylinders[materialId]->setPrimitiveCount(cylinders.size());
    if (!_cylindersBuffers[materialId])
        _cylindersBuffers[materialId] =
            context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT, bufferSize);
    memcpy(_cylindersBuffers[materialId]->map(), cylinders.data(), bufferSize);
    _cylindersBuffers[materialId]->unmap();
    optixCylinders["cylinders"]->setBuffer(_cylindersBuffers[materialId]);

    auto& mat = static_cast<OptiXMaterial&>(*_materials[materialId]);
    const auto material = mat.getOptixMaterial();
    if (!material)
        BRAYNS_THROW(std::runtime_error("Material is not defined"));

    auto instance = context->createGeometryInstance();
    instance->setGeometry(optixCylinders);
    instance->setMaterialCount(1);
    instance->setMaterial(0, material);
    if (materialId == BOUNDINGBOX_MATERIAL_ID)
        _boundingBoxGroup->addChild(instance);
    else
        _geometryGroup->addChild(instance);
}

void OptiXModel::_commitCones(const size_t materialId)
{
    if (_cones.find(materialId) == _cones.end())
        return;

    auto context = OptiXContext::get().getOptixContext();
    const auto& cones = _cones[materialId];
    const auto bufferSize = cones.size() * sizeof(Cone);
    context["cone_size"]->setUint(sizeof(Cone) / sizeof(float));
    _optixCones[materialId] =
        OptiXContext::get().createGeometry(OptixGeometryType::cone);

    auto& optixCones = _optixCones[materialId];
    optixCones->setPrimitiveCount(cones.size());
    if (!_conesBuffers[materialId])
        _conesBuffers[materialId] =
            context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT, bufferSize);
    memcpy(_conesBuffers[materialId]->map(), cones.data(), bufferSize);
    _conesBuffers[materialId]->unmap();
    optixCones["cones"]->setBuffer(_conesBuffers[materialId]);

    auto& mat = static_cast<OptiXMaterial&>(*_materials[materialId]);
    auto material = mat.getOptixMaterial();
    if (!material)
        BRAYNS_THROW(std::runtime_error("Material is not defined"));

    auto instance = context->createGeometryInstance();
    instance->setGeometry(optixCones);
    instance->setMaterialCount(1);
    instance->setMaterial(0, material);
    if (materialId == BOUNDINGBOX_MATERIAL_ID)
        _boundingBoxGroup->addChild(instance);
    else
        _geometryGroup->addChild(instance);
}

void OptiXModel::_commitMeshes(const size_t materialId)
{
    const auto& trianglesMesh = _trianglesMeshes[materialId];
    auto context = OptiXContext::get().getOptixContext();

    // Geometry
    auto& mat = static_cast<OptiXMaterial&>(*_materials[materialId]);
    auto material = mat.getOptixMaterial();
    if (!material)
        BRAYNS_THROW(std::runtime_error("Material is not defined"));
    auto geometry =
        OptiXContext::get().createGeometry(OptixGeometryType::trianglesMesh);

    // Vertices
    const auto& vertices = trianglesMesh.vertices;
    _verticesBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3,
                                            vertices.size());
    auto bufferSize = sizeof(Vector3f) * vertices.size();
    memcpy(_verticesBuffer->map(), vertices.data(), bufferSize);
    geometry["vertices_buffer"]->setBuffer(_verticesBuffer);
    _verticesBuffer->unmap();

    // Indices
    const auto& indices = trianglesMesh.indices;
    geometry->setPrimitiveCount(indices.size());
    _indicesBuffer =
        context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_INT3,
                              indices.size());
    bufferSize = sizeof(Vector3ui) * indices.size();
    memcpy(_indicesBuffer->map(), indices.data(), bufferSize);
    geometry["indices_buffer"]->setBuffer(_indicesBuffer);
    _indicesBuffer->unmap();

    // Normals
    const auto& normals = trianglesMesh.normals;
    bufferSize = sizeof(Vector3f) * normals.size();
    _normalsBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3,
                                           normals.size());
    memcpy(_normalsBuffer->map(), normals.data(), bufferSize);
    _normalsBuffer->unmap();
    geometry["normal_buffer"]->setBuffer(_normalsBuffer);

    // Colors
    const auto& colors = trianglesMesh.colors;
    _colorsBuffer =
        context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, colors.size());
    bufferSize = sizeof(Vector4f) * colors.size();
    memcpy(_colorsBuffer->map(), colors.data(), bufferSize);
    geometry["colors_buffer"]->setBuffer(_colorsBuffer);
    _colorsBuffer->unmap();

    // Texture coordinates
    const auto& texCoords = trianglesMesh.textureCoordinates;
    _textureCoordsBuffer =
        context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT2,
                              texCoords.size());
    bufferSize = sizeof(Vector2f) * texCoords.size();
    memcpy(_textureCoordsBuffer->map(), texCoords.data(), bufferSize);
    geometry["texcoord_buffer"]->setBuffer(_textureCoordsBuffer);
    _textureCoordsBuffer->unmap();

    auto instance = context->createGeometryInstance();
    instance->setGeometry(geometry);
    instance->setMaterialCount(1);
    instance->setMaterial(0, material);
    if (materialId == BOUNDINGBOX_MATERIAL_ID)
        _boundingBoxGroup->addChild(instance);
    else
        _geometryGroup->addChild(instance);

    BRAYNS_DEBUG << "Mesh " << materialId << ": " << vertices.size()
                 << " vertices, " << indices.size() << " indices, "
                 << normals.size() << " normals, " << colors.size()
                 << " colors and " << texCoords.size() << " texture coordinates"
                 << std::endl;
}

void OptiXModel::_commitMaterials()
{
    BRAYNS_INFO << "Committing " << _materials.size() << " OptiX materials"
                << std::endl;

    for (auto& material : _materials)
        material.second->commit();
}

void OptiXModel::buildBoundingBox()
{
    if (_boundingBoxGroup)
        return;

    _boundingBoxGroup = OptiXContext::get().createGeometryGroup();

    auto material = createMaterial(BOUNDINGBOX_MATERIAL_ID, "bounding_box");
    material->setDiffuseColor({1, 1, 1});
    material->setEmission(1.f);

    const Vector3f s(0.5f);
    const Vector3f c(0.5f);
    const float radius = 0.005f;
    const Vector3f positions[8] = {
        {c.x() - s.x(), c.y() - s.y(), c.z() - s.z()},
        {c.x() + s.x(), c.y() - s.y(), c.z() - s.z()}, //    6--------7
        {c.x() - s.x(), c.y() + s.y(), c.z() - s.z()}, //   /|       /|
        {c.x() + s.x(), c.y() + s.y(), c.z() - s.z()}, //  2--------3 |
        {c.x() - s.x(), c.y() - s.y(), c.z() + s.z()}, //  | |      | |
        {c.x() + s.x(), c.y() - s.y(), c.z() + s.z()}, //  | 4------|-5
        {c.x() - s.x(), c.y() + s.y(), c.z() + s.z()}, //  |/       |/
        {c.x() + s.x(), c.y() + s.y(), c.z() + s.z()}  //  0--------1
    };

    for (size_t i = 0; i < 8; ++i)
        addSphere(BOUNDINGBOX_MATERIAL_ID, Sphere(positions[i], radius));

    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[0], positions[1], radius});
    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[2], positions[3], radius});
    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[4], positions[5], radius});
    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[6], positions[7], radius});

    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[0], positions[2], radius});
    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[1], positions[3], radius});
    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[4], positions[6], radius});
    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[5], positions[7], radius});

    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[0], positions[4], radius});
    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[1], positions[5], radius});
    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[2], positions[6], radius});
    addCylinder(BOUNDINGBOX_MATERIAL_ID, {positions[3], positions[7], radius});
}

MaterialPtr OptiXModel::createMaterial(const size_t materialId,
                                       const std::string& name)

{
    auto material = std::make_shared<OptiXMaterial>();
    if (!material)
        BRAYNS_THROW(std::runtime_error("Failed to create material"));
    material->setName(name);
    _materials[materialId] = material;
    return material;
}
} // namespace brayns
