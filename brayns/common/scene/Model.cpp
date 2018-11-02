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

#include "Model.h"

#include <brayns/common/Transformation.h>
#include <brayns/common/log.h>
#include <brayns/common/material/Material.h>
#include <brayns/common/material/Texture2D.h>
#include <brayns/common/volume/Volume.h>

#include <boost/filesystem.hpp>

#include <fstream>
#include <set>

namespace
{
const size_t CACHE_VERSION = 1;
}

namespace brayns
{
ModelParams::ModelParams(const std::string& path)
    : _name(boost::filesystem::basename(path))
    , _path(path)
{
}

ModelParams::ModelParams(const std::string& name, const std::string& path)
    : _name(name)
    , _path(path)
{
}

ModelDescriptor::ModelDescriptor(ModelPtr model, const std::string& path)
    : ModelParams(path)
    , _model(std::move(model))
{
}

ModelDescriptor::ModelDescriptor(ModelPtr model, const std::string& path,
                                 const ModelMetadata& metadata)
    : ModelParams(path)
    , _metadata(metadata)
    , _model(std::move(model))
{
}

ModelDescriptor::ModelDescriptor(ModelPtr model, const std::string& name,
                                 const std::string& path,
                                 const ModelMetadata& metadata)
    : ModelParams(name, path)
    , _metadata(metadata)
    , _model(std::move(model))
{
}

ModelDescriptor& ModelDescriptor::operator=(const ModelParams& rhs)
{
    if (this == &rhs)
        return *this;
    _updateValue(_boundingBox, rhs.getBoundingBox());
    if (rhs.getName().empty())
        _updateValue(_name, boost::filesystem::basename(rhs.getPath()));
    else
        _updateValue(_name, rhs.getName());
    _updateValue(_path, rhs.getPath());
    _updateValue(_visible, rhs.getVisible());

    // Transformation
    const auto oldRotationCenter = _transformation.getRotationCenter();
    const auto newRotationCenter = rhs.getTransformation().getRotationCenter();
    _updateValue(_transformation, rhs.getTransformation());
    if (newRotationCenter == Vector3f())
        // If no rotation center is specified in the model params, the one set
        // by the model loader is used
        _transformation.setRotationCenter(oldRotationCenter);

    return *this;
}

void ModelDescriptor::addInstance(const ModelInstance& instance)
{
    _instances.push_back(instance);
    _instances.rbegin()->setInstanceID(_nextInstanceID++);
    if (_model)
        _model->markInstancesDirty();
}

void ModelDescriptor::removeInstance(const size_t id)
{
    auto i = std::remove_if(_instances.begin(), _instances.end(),
                            [id](const auto& instance) {
                                return id == instance.getInstanceID();
                            });
    if (i == _instances.end())
        return;

    _instances.erase(i, _instances.end());

    if (_model)
        _model->markInstancesDirty();
}

ModelInstance* ModelDescriptor::getInstance(const size_t id)
{
    auto i = std::find_if(_instances.begin(), _instances.end(),
                          [id](const auto& instance) {
                              return id == instance.getInstanceID();
                          });
    return i == _instances.end() ? nullptr : &(*i);
}

void ModelDescriptor::computeBounds()
{
    _bounds.reset();
    for (const auto& instance : getInstances())
    {
        if (!instance.getVisible() || !_model)
            continue;

        _bounds.merge(
            transformBox(getModel().getBounds(),
                         getTransformation() * instance.getTransformation()));
    }
}

void ModelDescriptor::save(const std::string& filename)
{
    if (!_model)
        return;

    BRAYNS_INFO << "Saving model to cache file: " << filename << std::endl;
    std::ofstream file(filename, std::ios::out | std::ios::binary);
    if (!file.good())
        BRAYNS_THROW(
            std::runtime_error("Could not open cache file " + filename));

    const size_t version = CACHE_VERSION;
    file.write((char*)&version, sizeof(size_t));

    // Save geometry
    uint64_t bufferSize{0};

    // Metadata
    size_t nbElements = _metadata.size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (const auto& data : _metadata)
    {
        size_t size = data.first.length();
        file.write((char*)&size, sizeof(size_t));
        file.write((char*)data.first.c_str(), size);
        size = data.second.length();
        file.write((char*)&size, sizeof(size_t));
        file.write((char*)data.second.c_str(), size);
    }

    const auto& materials = _model->getMaterials();
    const auto nbMaterials = materials.size();
    file.write((char*)&nbMaterials, sizeof(size_t));

    // Save materials
    for (const auto& material : materials)
    {
        file.write((char*)&material.first, sizeof(size_t));

        auto name = material.second->getName();
        size_t size = name.length();
        file.write((char*)&size, sizeof(size_t));
        file.write((char*)name.c_str(), size);

        Vector3f value3f;
        value3f = material.second->getDiffuseColor();
        file.write((char*)&value3f, sizeof(Vector3f));
        value3f = material.second->getSpecularColor();
        file.write((char*)&value3f, sizeof(Vector3f));
        float value = material.second->getSpecularExponent();
        file.write((char*)&value, sizeof(float));
        value = material.second->getReflectionIndex();
        file.write((char*)&value, sizeof(float));
        value = material.second->getOpacity();
        file.write((char*)&value, sizeof(float));
        value = material.second->getRefractionIndex();
        file.write((char*)&value, sizeof(float));
        value = material.second->getEmission();
        file.write((char*)&value, sizeof(float));
        value = material.second->getGlossiness();
        file.write((char*)&value, sizeof(float));
        const bool boolean = material.second->getCastSimulationData();
        file.write((char*)&boolean, sizeof(bool));
        const size_t shadingMode = material.second->getShadingMode();
        file.write((char*)&shadingMode, sizeof(size_t));
    }

    // Spheres
    nbElements = _model->getSpheres().size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (auto& spheres : _model->getSpheres())
    {
        const auto materialId = spheres.first;
        file.write((char*)&materialId, sizeof(size_t));

        const auto& data = spheres.second;
        nbElements = data.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Sphere);
        file.write((char*)data.data(), bufferSize);
    }

    // Cylinders
    nbElements = _model->getCylinders().size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (auto& cylinders : _model->getCylinders())
    {
        const auto materialId = cylinders.first;
        file.write((char*)&materialId, sizeof(size_t));

        const auto& data = cylinders.second;
        nbElements = data.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Cylinder);
        file.write((char*)data.data(), bufferSize);
    }

    // Cones
    nbElements = _model->getCones().size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (auto& cones : _model->getCones())
    {
        const auto materialId = cones.first;
        file.write((char*)&materialId, sizeof(size_t));

        const auto& data = cones.second;
        nbElements = data.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Cone);
        file.write((char*)data.data(), bufferSize);
    }

    // Meshes
    nbElements = _model->getTrianglesMeshes().size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (const auto& meshes : _model->getTrianglesMeshes())
    {
        const auto materialId = meshes.first;
        file.write((char*)&materialId, sizeof(size_t));

        const auto& data = meshes.second;

        // Vertices
        nbElements = data.vertices.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Vector3f);
        file.write((char*)data.vertices.data(), bufferSize);

        // Indices
        nbElements = data.indices.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Vector3ui);
        file.write((char*)data.indices.data(), bufferSize);

        // Normals
        nbElements = data.normals.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Vector3f);
        file.write((char*)data.normals.data(), bufferSize);

        // Texture coordinates
        nbElements = data.textureCoordinates.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Vector2f);
        file.write((char*)data.textureCoordinates.data(), bufferSize);
    }

    // Streamlines
    const auto& streamlines = _model->getStreamlines();
    nbElements = streamlines.size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (const auto& streamline : streamlines)
    {
        const auto& streamlineData = streamline.second;
        // Id
        size_t id = streamline.first;
        file.write((char*)&id, sizeof(size_t));

        // Vertex
        nbElements = streamlineData.vertex.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Vector4f);
        file.write((char*)streamlineData.vertex.data(), bufferSize);

        // Vertex Color
        nbElements = streamlineData.vertexColor.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Vector4f);
        file.write((char*)streamlineData.vertexColor.data(), bufferSize);

        // Indices
        nbElements = streamlineData.indices.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(int32_t);
        file.write((char*)streamlineData.indices.data(), bufferSize);
    }

    // SDF geometry
    const SDFGeometryData& sdfData = _model->getSDFGeometryData(false);
    nbElements = sdfData.geometries.size();
    file.write((char*)&nbElements, sizeof(size_t));

    if (nbElements > 0)
    {
        // Geometries
        bufferSize = nbElements * sizeof(SDFGeometry);
        file.write((char*)sdfData.geometries.data(), bufferSize);

        // SDF indices
        nbElements = sdfData.geometryIndices.size();
        file.write((char*)&nbElements, sizeof(size_t));
        for (const auto& geometryIndex : sdfData.geometryIndices)
        {
            size_t id = geometryIndex.first;
            file.write((char*)&id, sizeof(size_t));
            nbElements = geometryIndex.second.size();
            file.write((char*)&nbElements, sizeof(size_t));
            bufferSize = nbElements * sizeof(uint64_t);
            file.write((char*)geometryIndex.second.data(), bufferSize);
        }

        // Neighbours
        nbElements = sdfData.neighbours.size();
        file.write((char*)&nbElements, sizeof(size_t));
        for (const auto& neighbour : sdfData.neighbours)
        {
            nbElements = neighbour.size();
            file.write((char*)&nbElements, sizeof(size_t));
            bufferSize = nbElements * sizeof(size_t);
            file.write((char*)neighbour.data(), bufferSize);
        }

        // Neighbours flat
        nbElements = sdfData.neighboursFlat.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(uint64_t);
        file.write((char*)sdfData.neighboursFlat.data(), bufferSize);
    }

    file.close();
}

std::string readString(std::ifstream& f)
{
    size_t size;
    f.read((char*)&size, sizeof(size_t));
    char* str = new char[size + 1];
    f.read(str, size);
    str[size] = 0;
    std::string s{str};
    delete[] str;
    return s;
}

void ModelDescriptor::load(const std::string& filename)
{
    BRAYNS_INFO << "Loading model from cache file: " << filename << std::endl;
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file.good())
        BRAYNS_THROW(
            std::runtime_error("Could not open cache file " + filename));

    // File version
    size_t version;
    file.read((char*)&version, sizeof(size_t));

    if (version != CACHE_VERSION)
        BRAYNS_THROW(std::runtime_error(
            "Only version " + std::to_string(CACHE_VERSION) + " is supported"));

    // Geometry
    size_t nbSpheres = 0;
    size_t nbCylinders = 0;
    size_t nbCones = 0;
    size_t nbMeshes = 0;
    size_t nbVertices = 0;
    size_t nbIndices = 0;
    size_t nbNormals = 0;
    size_t nbTexCoords = 0;

    // Metadata
    size_t nbElements;
    file.read((char*)&nbElements, sizeof(size_t));
    for (size_t i = 0; i < nbElements; ++i)
        _metadata[readString(file)] = readString(file);

    size_t nbMaterials;
    file.read((char*)&nbMaterials, sizeof(size_t));

    // Materials
    size_t materialId;
    for (size_t i = 0; i < nbMaterials; ++i)
    {
        file.read((char*)&materialId, sizeof(size_t));

        auto material = _model->createMaterial(materialId, readString(file));

        Vector3f value3f;
        file.read((char*)&value3f, sizeof(Vector3f));
        material->setDiffuseColor(value3f);
        file.read((char*)&value3f, sizeof(Vector3f));
        material->setSpecularColor(value3f);
        float value;
        file.read((char*)&value, sizeof(float));
        material->setSpecularExponent(value);
        file.read((char*)&value, sizeof(float));
        material->setReflectionIndex(value);
        file.read((char*)&value, sizeof(float));
        material->setOpacity(value);
        file.read((char*)&value, sizeof(float));
        material->setRefractionIndex(value);
        file.read((char*)&value, sizeof(float));
        material->setEmission(value);
        file.read((char*)&value, sizeof(float));
        material->setGlossiness(value);
        bool boolean;
        file.read((char*)&boolean, sizeof(bool));
        material->setCastSimulationData(boolean);
        size_t shadingMode;
        file.read((char*)&shadingMode, sizeof(size_t));
        material->setShadingMode(static_cast<MaterialShadingMode>(shadingMode));
    }

    uint64_t bufferSize{0};

    // Spheres
    file.read((char*)&nbSpheres, sizeof(size_t));
    for (size_t i = 0; i < nbSpheres; ++i)
    {
        file.read((char*)&materialId, sizeof(size_t));
        file.read((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Sphere);
        auto& spheres = _model->getSpheres()[materialId];
        spheres.resize(nbElements);
        file.read((char*)spheres.data(), bufferSize);
    }

    // Cylinders
    file.read((char*)&nbCylinders, sizeof(size_t));
    for (size_t i = 0; i < nbCylinders; ++i)
    {
        file.read((char*)&materialId, sizeof(size_t));
        file.read((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Cylinder);
        auto& cylinders = _model->getCylinders()[materialId];
        cylinders.resize(nbElements);
        file.read((char*)cylinders.data(), bufferSize);
    }

    // Cones
    file.read((char*)&nbCones, sizeof(size_t));
    for (size_t i = 0; i < nbCones; ++i)
    {
        file.read((char*)&materialId, sizeof(size_t));
        file.read((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(Cone);
        auto& cones = _model->getCones()[materialId];
        cones.resize(nbElements);
        file.read((char*)cones.data(), bufferSize);
    }

    // Meshes
    file.read((char*)&nbMeshes, sizeof(size_t));
    for (size_t i = 0; i < nbMeshes; ++i)
    {
        file.read((char*)&materialId, sizeof(size_t));
        auto& meshes = _model->getTrianglesMeshes()[materialId];
        // Vertices
        file.read((char*)&nbVertices, sizeof(size_t));
        if (nbVertices != 0)
        {
            meshes.vertices.resize(nbVertices);
            bufferSize = nbVertices * sizeof(Vector3f);
            file.read((char*)meshes.vertices.data(), bufferSize);
        }

        // Indices
        file.read((char*)&nbIndices, sizeof(size_t));
        if (nbIndices != 0)
        {
            meshes.indices.resize(nbIndices);
            bufferSize = nbIndices * sizeof(Vector3ui);
            file.read((char*)meshes.indices.data(), bufferSize);
        }

        // Normals
        file.read((char*)&nbNormals, sizeof(size_t));
        if (nbNormals != 0)
        {
            meshes.normals.resize(nbNormals);
            bufferSize = nbNormals * sizeof(Vector3f);
            file.read((char*)meshes.normals.data(), bufferSize);
        }

        // Texture coordinates
        file.read((char*)&nbTexCoords, sizeof(size_t));
        if (nbTexCoords != 0)
        {
            meshes.textureCoordinates.resize(nbTexCoords);
            bufferSize = nbTexCoords * sizeof(Vector2f);
            file.read((char*)meshes.textureCoordinates.data(), bufferSize);
        }
    }

    // Streamlines
    size_t nbStreamlines;
    auto& streamlines = _model->getStreamlines();
    file.read((char*)&nbStreamlines, sizeof(size_t));
    for (size_t i = 0; i < nbStreamlines; ++i)
    {
        StreamlinesData streamlineData;
        // Id
        size_t id;
        file.read((char*)&id, sizeof(size_t));

        // Vertex
        file.read((char*)&nbElements, sizeof(size_t));
        streamlineData.vertex.resize(nbElements);
        file.read((char*)streamlineData.vertex.data(),
                  nbElements * sizeof(Vector4f));

        // Vertex Color
        file.read((char*)&nbElements, sizeof(size_t));
        streamlineData.vertexColor.resize(nbElements);
        file.read((char*)streamlineData.vertexColor.data(),
                  nbElements * sizeof(Vector4f));

        // Indices
        file.read((char*)&nbElements, sizeof(size_t));
        streamlineData.indices.resize(nbElements);
        file.read((char*)streamlineData.indices.data(),
                  nbElements * sizeof(int32_t));

        streamlines[id] = streamlineData;
    }

    // SDF geometry
    auto& sdfData = _model->getSDFGeometryData(true);
    file.read((char*)&nbElements, sizeof(size_t));

    if (nbElements > 0)
    {
        // Geometries
        sdfData.geometries.resize(nbElements);
        bufferSize = nbElements * sizeof(SDFGeometry);
        file.read((char*)sdfData.geometries.data(), bufferSize);

        // SDF Indices
        file.read((char*)&nbElements, sizeof(size_t));
        for (size_t j = 0; j < nbElements; ++j)
        {
            size_t id;
            file.read((char*)&id, sizeof(size_t));
            size_t size;
            file.read((char*)&size, sizeof(size_t));
            sdfData.geometryIndices[id].resize(size);
            bufferSize = size * sizeof(uint64_t);
            file.read((char*)sdfData.geometryIndices[id].data(), bufferSize);
        }

        // Neighbours
        file.read((char*)&nbElements, sizeof(size_t));
        sdfData.neighbours.resize(nbElements);
        for (size_t j = 0; j < nbElements; ++j)
        {
            size_t size;
            file.read((char*)&size, sizeof(size_t));
            sdfData.neighbours[j].resize(size);
            bufferSize = size * sizeof(uint64_t);
            file.read((char*)sdfData.neighbours[j].data(), bufferSize);
        }

        // Neighbours flat
        file.read((char*)&nbElements, sizeof(size_t));
        sdfData.neighboursFlat.resize(nbElements);
        bufferSize = nbElements * sizeof(uint64_t);
        file.read((char*)sdfData.neighboursFlat.data(), bufferSize);
    }

    file.close();
}

bool Model::empty() const
{
    return _spheres.empty() && _cylinders.empty() && _cones.empty() &&
           _trianglesMeshes.empty() && _sdf.geometries.empty() &&
           _streamlines.empty() && _volumes.empty() && _bounds.isEmpty();
}

uint64_t Model::addSphere(const size_t materialId, const Sphere& sphere)
{
    _spheresDirty = true;
    _spheres[materialId].push_back(sphere);
    return _spheres[materialId].size() - 1;
}

uint64_t Model::addCylinder(const size_t materialId, const Cylinder& cylinder)
{
    _cylindersDirty = true;
    _cylinders[materialId].push_back(cylinder);
    return _cylinders[materialId].size() - 1;
}

uint64_t Model::addCone(const size_t materialId, const Cone& cone)
{
    _conesDirty = true;
    _cones[materialId].push_back(cone);
    return _cones[materialId].size() - 1;
}

void Model::addStreamline(const size_t materialId, const Streamline& streamline)
{
    if (streamline.position.size() < 2)
        throw std::runtime_error(
            "Number of vertices is less than two which is minimum needed for a "
            "streamline.");

    if (streamline.position.size() != streamline.color.size())
        throw std::runtime_error("Number of vertices and colors do not match.");

    if (streamline.position.size() != streamline.radius.size())
        throw std::runtime_error("Number of vertices and radii do not match.");

    auto& streamlinesData = _streamlines[materialId];

    const size_t startIndex = streamlinesData.vertex.size();
    const size_t endIndex = startIndex + streamline.position.size() - 1;

    for (size_t index = startIndex; index < endIndex; ++index)
        streamlinesData.indices.push_back(index);

    for (size_t i = 0; i < streamline.position.size(); i++)
        streamlinesData.vertex.push_back(
            Vector4f(streamline.position[i], streamline.radius[i]));

    for (const auto& color : streamline.color)
        streamlinesData.vertexColor.push_back(color);

    _streamlinesDirty = true;
}

uint64_t Model::addSDFGeometry(const size_t materialId, const SDFGeometry& geom,
                               const std::vector<size_t>& neighbourIndices)
{
    const uint64_t geomIdx = _sdf.geometries.size();
    _sdf.geometryIndices[materialId].push_back(geomIdx);
    _sdf.neighbours.push_back(neighbourIndices);
    _sdf.geometries.push_back(geom);
    _sdfGeometriesDirty = true;
    return geomIdx;
}

void Model::updateSDFGeometryNeighbours(
    size_t geometryIdx, const std::vector<size_t>& neighbourIndices)
{
    _sdf.neighbours[geometryIdx] = neighbourIndices;
    _sdfGeometriesDirty = true;
}

void Model::addVolume(VolumePtr volume)
{
    _volumes.push_back(volume);
    _volumesDirty = true;
}

void Model::removeVolume(VolumePtr volume)
{
    auto i = std::find(_volumes.begin(), _volumes.end(), volume);
    if (i == _volumes.end())
        return;

    _volumes.erase(i);
    _volumesDirty = true;
}

bool Model::dirty() const
{
    return _spheresDirty || _cylindersDirty || _conesDirty ||
           _trianglesMeshesDirty || _sdfGeometriesDirty || _instancesDirty;
}

void Model::setMaterialsColorMap(const MaterialsColorMap colorMap)
{
    size_t index = 0;
    for (auto material : _materials)
    {
        material.second->setSpecularColor(Vector3f(0.f));
        material.second->setOpacity(1.f);
        material.second->setReflectionIndex(0.f);
        material.second->setEmission(0.f);

        switch (colorMap)
        {
        case MaterialsColorMap::none:
            switch (index)
            {
            case 0: // Default
            case 1: // Soma
                material.second->setDiffuseColor(Vector3f(0.9f, 0.9f, 0.9f));
                break;
            case 2: // Axon
                material.second->setDiffuseColor(Vector3f(0.2f, 0.2f, 0.8f));
                break;
            case 3: // Dendrite
                material.second->setDiffuseColor(Vector3f(0.8f, 0.2f, 0.2f));
                break;
            case 4: // Apical dendrite
                material.second->setDiffuseColor(Vector3f(0.8f, 0.2f, 0.8f));
                break;
            default:
                material.second->setDiffuseColor(
                    Vector3f(float(std::rand() % 255) / 255.f,
                             float(std::rand() % 255) / 255.f,
                             float(std::rand() % 255) / 255.f));
            }
            break;
        case MaterialsColorMap::gradient:
        {
            const float a = float(index) / float(_materials.size());
            material.second->setDiffuseColor(Vector3f(a, 0.f, 1.f - a));
            break;
        }
        case MaterialsColorMap::pastel:
            material.second->setDiffuseColor(
                Vector3f(0.5f + float(std::rand() % 127) / 255.f,
                         0.5f + float(std::rand() % 127) / 255.f,
                         0.5f + float(std::rand() % 127) / 255.f));
            break;
        case MaterialsColorMap::random:
            material.second->setDiffuseColor(
                Vector3f(float(rand() % 255) / 255.f,
                         float(rand() % 255) / 255.f,
                         float(rand() % 255) / 255.f));
            switch (rand() % 10)
            {
            case 0:
                // Transparency only
                material.second->setOpacity(float(std::rand() % 100) / 100.f);
                material.second->setRefractionIndex(1.2f);
                material.second->setSpecularColor(Vector3f(1.f));
                material.second->setSpecularExponent(10.f);
                break;
            case 1:
                // Light emission
                material.second->setEmission(std::rand() % 20);
                break;
            case 2:
                // Reflection only
                material.second->setReflectionIndex(float(std::rand() % 100) /
                                                    100.f);
                material.second->setSpecularColor(Vector3f(1.f));
                material.second->setSpecularExponent(10.f);
                break;
            case 3:
                // Reflection and refraction
                material.second->setReflectionIndex(float(std::rand() % 100) /
                                                    100.f);
                material.second->setOpacity(float(std::rand() % 100) / 100.f);
                material.second->setRefractionIndex(1.2f);
                material.second->setSpecularColor(Vector3f(1.f));
                material.second->setSpecularExponent(10.f);
                break;
            case 4:
                // Reflection and glossiness
                material.second->setReflectionIndex(float(std::rand() % 100) /
                                                    100.f);
                material.second->setSpecularColor(Vector3f(1.f));
                material.second->setSpecularExponent(10.f);
                material.second->setGlossiness(float(std::rand() % 100) /
                                               100.f);
                break;
            case 5:
                // Transparency and glossiness
                material.second->setOpacity(float(std::rand() % 100) / 100.f);
                material.second->setRefractionIndex(1.2f);
                material.second->setSpecularColor(Vector3f(1.f));
                material.second->setSpecularExponent(10.f);
                material.second->setGlossiness(float(std::rand() % 100) /
                                               100.f);
                break;
            }
            break;
        case MaterialsColorMap::shades_of_grey:
            float value = float(std::rand() % 255) / 255.f;
            material.second->setDiffuseColor(Vector3f(value, value, value));
            break;
        }
        material.second->commit();
        ++index;
    }
}

void Model::logInformation()
{
    updateSizeInBytes();

    uint64_t nbSpheres = 0;
    uint64_t nbCylinders = 0;
    uint64_t nbCones = 0;
    uint64_t nbMeshes = _trianglesMeshes.size();
    for (const auto& spheres : _spheres)
        nbSpheres += spheres.second.size();
    for (const auto& cylinders : _cylinders)
        nbCylinders += cylinders.second.size();
    for (const auto& cones : _cones)
        nbCones += cones.second.size();

    BRAYNS_DEBUG << "Spheres: " << nbSpheres << ", Cylinders: " << nbCylinders
                 << ", Cones: " << nbCones << ", Meshes: " << nbMeshes
                 << ", Memory: " << _sizeInBytes << " bytes ("
                 << _sizeInBytes / 1048576 << " MB), Bounds: " << _bounds
                 << std::endl;
}

MaterialPtr Model::getMaterial(const size_t materialId) const
{
    const auto it = _materials.find(materialId);
    if (it == _materials.end())
        throw std::runtime_error("Material " + std::to_string(materialId) +
                                 " is not registered in the model");
    return it->second;
}

void Model::updateSizeInBytes()
{
    _sizeInBytes = 0;
    for (const auto& spheres : _spheres)
        _sizeInBytes += spheres.second.size() * sizeof(Sphere);
    for (const auto& cylinders : _cylinders)
        _sizeInBytes += cylinders.second.size() * sizeof(Cylinder);
    for (const auto& cones : _cones)
        _sizeInBytes += cones.second.size() * sizeof(Cones);
    for (const auto& trianglesMesh : _trianglesMeshes)
    {
        const auto& mesh = trianglesMesh.second;
        _sizeInBytes += mesh.indices.size() * sizeof(Vector3f);
        _sizeInBytes += mesh.normals.size() * sizeof(Vector3f);
        _sizeInBytes += mesh.colors.size() * sizeof(Vector4f);
        _sizeInBytes += mesh.indices.size() * sizeof(Vector3ui);
        _sizeInBytes += mesh.textureCoordinates.size() * sizeof(Vector2f);
    }
    for (const auto& volume : _volumes)
        _sizeInBytes += volume->getSizeInBytes();
}

void Model::_updateBounds()
{
    if (_spheresDirty)
    {
        _spheresDirty = false;
        _sphereBounds.reset();
        for (const auto& spheres : _spheres)
            if (spheres.first != BOUNDINGBOX_MATERIAL_ID)
                for (const auto& sphere : spheres.second)
                {
                    _sphereBounds.merge(sphere.center + sphere.radius);
                    _sphereBounds.merge(sphere.center - sphere.radius);
                }
    }

    if (_cylindersDirty)
    {
        _cylindersDirty = false;
        _cylindersBounds.reset();
        for (const auto& cylinders : _cylinders)
            if (cylinders.first != BOUNDINGBOX_MATERIAL_ID)
                for (const auto& cylinder : cylinders.second)
                {
                    _cylindersBounds.merge(cylinder.center);
                    _cylindersBounds.merge(cylinder.up);
                }
    }

    if (_conesDirty)
    {
        _conesDirty = false;
        _conesBounds.reset();
        for (const auto& cones : _cones)
            if (cones.first != BOUNDINGBOX_MATERIAL_ID)
                for (const auto& cone : cones.second)
                {
                    _conesBounds.merge(cone.center);
                    _conesBounds.merge(cone.up);
                }
    }

    if (_trianglesMeshesDirty)
    {
        _trianglesMeshesDirty = false;
        _trianglesMeshesBounds.reset();
        for (const auto& mesh : _trianglesMeshes)
            if (mesh.first != BOUNDINGBOX_MATERIAL_ID)
                for (const auto& vertex : mesh.second.vertices)
                    _trianglesMeshesBounds.merge(vertex);
    }

    if (_streamlinesDirty)
    {
        _streamlinesDirty = false;
        _streamlinesBounds.reset();
        for (const auto& streamline : _streamlines)
            for (size_t index = 0; index < streamline.second.vertex.size();
                 ++index)
            {
                const auto& pos =
                    streamline.second.vertex[index].get_sub_vector<3, 0>();
                const float radius = streamline.second.vertex[index][3];
                const auto radiusVec = Vector3f(radius, radius, radius);
                _streamlinesBounds.merge(pos + radiusVec);
                _streamlinesBounds.merge(pos - radiusVec);
            }
    }

    if (_sdfGeometriesDirty)
    {
        _sdfGeometriesDirty = false;
        _sdfGeometriesBounds.reset();
        for (const auto& geom : _sdf.geometries)
            _sdfGeometriesBounds.merge(getSDFBoundingBox(geom));
    }

    if (_volumesDirty)
    {
        _volumesDirty = false;
        _volumesBounds.reset();
        for (const auto& volume : _volumes)
            _volumesBounds.merge(volume->getBounds());
    }

    _bounds.reset();
    _bounds.merge(_sphereBounds);
    _bounds.merge(_cylindersBounds);
    _bounds.merge(_conesBounds);
    _bounds.merge(_trianglesMeshesBounds);
    _bounds.merge(_streamlinesBounds);
    _bounds.merge(_sdfGeometriesBounds);
    _bounds.merge(_volumesBounds);
}

void Model::createMissingMaterials(const bool castSimulationData)
{
    std::set<size_t> materialIds;
    for (auto& spheres : _spheres)
        materialIds.insert(spheres.first);
    for (auto& cylinders : _cylinders)
        materialIds.insert(cylinders.first);
    for (auto& cones : _cones)
        materialIds.insert(cones.first);
    for (auto& meshes : _trianglesMeshes)
        materialIds.insert(meshes.first);
    for (auto& sdfGeometries : _sdf.geometryIndices)
        materialIds.insert(sdfGeometries.first);

    for (const auto materialId : materialIds)
    {
        const auto it = _materials.find(materialId);
        if (it == _materials.end())
        {
            auto material =
                createMaterial(materialId, std::to_string(materialId));
            material->setCastSimulationData(castSimulationData);
        }
    }
}
} // namespace brayns
