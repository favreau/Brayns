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

#include "BrickLoader.h"
#include "MorphologyLoader.h"
#include "log.h"
#include "types.h"

#include <brayns/common/material/Material.h>
#include <brayns/common/scene/Model.h>
#include <brayns/common/scene/Scene.h>

#include <fstream>

namespace
{
const size_t CACHE_VERSION = 1;
}

BrickLoader::BrickLoader(brayns::Scene& scene)
    : Loader(scene)
{
}

BrickLoader::~BrickLoader() {}

std::set<std::string> BrickLoader::getSupportedDataTypes()
{
    return {"brayns"};
}

brayns::ModelDescriptorPtr BrickLoader::importFromBlob(
    brayns::Blob&& /*blob*/, const size_t /*index*/,
    const size_t /*materialID*/)
{
    throw std::runtime_error("Loading circuit from blob is not supported");
}

std::string BrickLoader::_readString(std::ifstream& f)
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

brayns::ModelDescriptorPtr BrickLoader::importFromFile(
    const std::string& filename, const size_t /*index*/,
    const size_t /*materialID*/)
{
    PLUGIN_INFO << "Loading model from cache file: " << filename << std::endl;
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file.good())
        PLUGIN_THROW(
            std::runtime_error("Could not open cache file " + filename));

    // File version
    size_t version;
    file.read((char*)&version, sizeof(size_t));

    if (version != CACHE_VERSION)
        PLUGIN_THROW(std::runtime_error(
            "Only version " + std::to_string(CACHE_VERSION) + " is supported"));

    auto model = _scene.createModel();

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
    brayns::ModelMetadata metadata;
    file.read((char*)&nbElements, sizeof(size_t));
    for (size_t i = 0; i < nbElements; ++i)
        metadata[_readString(file)] = _readString(file);

    size_t nbMaterials;
    file.read((char*)&nbMaterials, sizeof(size_t));

    // Materials
    size_t materialId;
    for (size_t i = 0; i < nbMaterials; ++i)
    {
        file.read((char*)&materialId, sizeof(size_t));

        auto material = model->createMaterial(materialId, _readString(file));

        brayns::Vector3f value3f;
        file.read((char*)&value3f, sizeof(brayns::Vector3f));
        material->setDiffuseColor(value3f);
        file.read((char*)&value3f, sizeof(brayns::Vector3f));
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
        material->setShadingMode(
            static_cast<brayns::MaterialShadingMode>(shadingMode));
    }

    uint64_t bufferSize{0};

    // Spheres
    file.read((char*)&nbSpheres, sizeof(size_t));
    for (size_t i = 0; i < nbSpheres; ++i)
    {
        file.read((char*)&materialId, sizeof(size_t));
        file.read((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Sphere);
        auto& spheres = model->getSpheres()[materialId];
        spheres.resize(nbElements);
        file.read((char*)spheres.data(), bufferSize);
    }

    // Cylinders
    file.read((char*)&nbCylinders, sizeof(size_t));
    for (size_t i = 0; i < nbCylinders; ++i)
    {
        file.read((char*)&materialId, sizeof(size_t));
        file.read((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Cylinder);
        auto& cylinders = model->getCylinders()[materialId];
        cylinders.resize(nbElements);
        file.read((char*)cylinders.data(), bufferSize);
    }

    // Cones
    file.read((char*)&nbCones, sizeof(size_t));
    for (size_t i = 0; i < nbCones; ++i)
    {
        file.read((char*)&materialId, sizeof(size_t));
        file.read((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Cone);
        auto& cones = model->getCones()[materialId];
        cones.resize(nbElements);
        file.read((char*)cones.data(), bufferSize);
    }

    // Meshes
    file.read((char*)&nbMeshes, sizeof(size_t));
    for (size_t i = 0; i < nbMeshes; ++i)
    {
        file.read((char*)&materialId, sizeof(size_t));
        auto& meshes = model->getTrianglesMeshes()[materialId];
        // Vertices
        file.read((char*)&nbVertices, sizeof(size_t));
        if (nbVertices != 0)
        {
            meshes.vertices.resize(nbVertices);
            bufferSize = nbVertices * sizeof(brayns::Vector3f);
            file.read((char*)meshes.vertices.data(), bufferSize);
        }

        // Indices
        file.read((char*)&nbIndices, sizeof(size_t));
        if (nbIndices != 0)
        {
            meshes.indices.resize(nbIndices);
            bufferSize = nbIndices * sizeof(brayns::Vector3ui);
            file.read((char*)meshes.indices.data(), bufferSize);
        }

        // Normals
        file.read((char*)&nbNormals, sizeof(size_t));
        if (nbNormals != 0)
        {
            meshes.normals.resize(nbNormals);
            bufferSize = nbNormals * sizeof(brayns::Vector3f);
            file.read((char*)meshes.normals.data(), bufferSize);
        }

        // Texture coordinates
        file.read((char*)&nbTexCoords, sizeof(size_t));
        if (nbTexCoords != 0)
        {
            meshes.textureCoordinates.resize(nbTexCoords);
            bufferSize = nbTexCoords * sizeof(brayns::Vector2f);
            file.read((char*)meshes.textureCoordinates.data(), bufferSize);
        }
    }

    // Streamlines
    size_t nbStreamlines;
    auto& streamlines = model->getStreamlines();
    file.read((char*)&nbStreamlines, sizeof(size_t));
    for (size_t i = 0; i < nbStreamlines; ++i)
    {
        brayns::StreamlinesData streamlineData;
        // Id
        size_t id;
        file.read((char*)&id, sizeof(size_t));

        // Vertex
        file.read((char*)&nbElements, sizeof(size_t));
        streamlineData.vertex.resize(nbElements);
        file.read((char*)streamlineData.vertex.data(),
                  nbElements * sizeof(brayns::Vector4f));

        // Vertex Color
        file.read((char*)&nbElements, sizeof(size_t));
        streamlineData.vertexColor.resize(nbElements);
        file.read((char*)streamlineData.vertexColor.data(),
                  nbElements * sizeof(brayns::Vector4f));

        // Indices
        file.read((char*)&nbElements, sizeof(size_t));
        streamlineData.indices.resize(nbElements);
        file.read((char*)streamlineData.indices.data(),
                  nbElements * sizeof(int32_t));

        streamlines[id] = streamlineData;
    }

    // SDF geometry
    auto& sdfData = model->getSDFGeometryData(true);
    file.read((char*)&nbElements, sizeof(size_t));

    if (nbElements > 0)
    {
        // Geometries
        sdfData.geometries.resize(nbElements);
        bufferSize = nbElements * sizeof(brayns::SDFGeometry);
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

    auto modelDescriptor =
        std::make_shared<brayns::ModelDescriptor>(std::move(model), "Brick",
                                                  filename, metadata);
    return modelDescriptor;
}

void BrickLoader::exportToFile(const brayns::ModelDescriptorPtr modelDescriptor,
                               const std::string& filename)
{
    PLUGIN_INFO << "Saving model to cache file: " << filename << std::endl;
    std::ofstream file(filename, std::ios::out | std::ios::binary);
    if (!file.good())
        PLUGIN_THROW(
            std::runtime_error("Could not open cache file " + filename));

    const size_t version = CACHE_VERSION;
    file.write((char*)&version, sizeof(size_t));

    // Save geometry
    auto& model = modelDescriptor->getModel();
    uint64_t bufferSize{0};

    // Metadata
    auto metadata = modelDescriptor->getMetadata();
    size_t nbElements = metadata.size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (const auto& data : metadata)
    {
        size_t size = data.first.length();
        file.write((char*)&size, sizeof(size_t));
        file.write((char*)data.first.c_str(), size);
        size = data.second.length();
        file.write((char*)&size, sizeof(size_t));
        file.write((char*)data.second.c_str(), size);
    }

    const auto& materials = model.getMaterials();
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

        brayns::Vector3f value3f;
        value3f = material.second->getDiffuseColor();
        file.write((char*)&value3f, sizeof(brayns::Vector3f));
        value3f = material.second->getSpecularColor();
        file.write((char*)&value3f, sizeof(brayns::Vector3f));
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
    nbElements = model.getSpheres().size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (auto& spheres : model.getSpheres())
    {
        const auto materialId = spheres.first;
        file.write((char*)&materialId, sizeof(size_t));

        const auto& data = spheres.second;
        nbElements = data.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Sphere);
        file.write((char*)data.data(), bufferSize);
    }

    // Cylinders
    nbElements = model.getCylinders().size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (auto& cylinders : model.getCylinders())
    {
        const auto materialId = cylinders.first;
        file.write((char*)&materialId, sizeof(size_t));

        const auto& data = cylinders.second;
        nbElements = data.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Cylinder);
        file.write((char*)data.data(), bufferSize);
    }

    // Cones
    nbElements = model.getCones().size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (auto& cones : model.getCones())
    {
        const auto materialId = cones.first;
        file.write((char*)&materialId, sizeof(size_t));

        const auto& data = cones.second;
        nbElements = data.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Cone);
        file.write((char*)data.data(), bufferSize);
    }

    // Meshes
    nbElements = model.getTrianglesMeshes().size();
    file.write((char*)&nbElements, sizeof(size_t));
    for (const auto& meshes : model.getTrianglesMeshes())
    {
        const auto materialId = meshes.first;
        file.write((char*)&materialId, sizeof(size_t));

        const auto& data = meshes.second;

        // Vertices
        nbElements = data.vertices.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Vector3f);
        file.write((char*)data.vertices.data(), bufferSize);

        // Indices
        nbElements = data.indices.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Vector3ui);
        file.write((char*)data.indices.data(), bufferSize);

        // Normals
        nbElements = data.normals.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Vector3f);
        file.write((char*)data.normals.data(), bufferSize);

        // Texture coordinates
        nbElements = data.textureCoordinates.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Vector2f);
        file.write((char*)data.textureCoordinates.data(), bufferSize);
    }

    // Streamlines
    const auto& streamlines = model.getStreamlines();
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
        bufferSize = nbElements * sizeof(brayns::Vector4f);
        file.write((char*)streamlineData.vertex.data(), bufferSize);

        // Vertex Color
        nbElements = streamlineData.vertexColor.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(brayns::Vector4f);
        file.write((char*)streamlineData.vertexColor.data(), bufferSize);

        // Indices
        nbElements = streamlineData.indices.size();
        file.write((char*)&nbElements, sizeof(size_t));
        bufferSize = nbElements * sizeof(int32_t);
        file.write((char*)streamlineData.indices.data(), bufferSize);
    }

    // SDF geometry
    const auto& sdfData = model.getSDFGeometryData(false);
    nbElements = sdfData.geometries.size();
    file.write((char*)&nbElements, sizeof(size_t));

    if (nbElements > 0)
    {
        // Geometries
        bufferSize = nbElements * sizeof(brayns::SDFGeometry);
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
