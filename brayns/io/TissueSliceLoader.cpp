/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
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

#include "TissueSliceLoader.h"

#include <brayns/common/log.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/io/MeshLoader.h>
#include <brayns/io/MorphologyLoader.h>
#include <fstream>

#include <boost/filesystem.hpp>

namespace brayns
{
TissueSliceLoader::TissueSliceLoader(
    const GeometryParameters& geometryParameters)
    : _geometryParameters(geometryParameters)
{
}

bool TissueSliceLoader::_parsePositions(const std::string& filename)
{
    _positions.clear();

    BRAYNS_INFO << "Loading xyz positions from " << filename << std::endl;
    std::ifstream file(filename, std::ios::in);
    if (!file.good())
    {
        BRAYNS_ERROR << "Could not open file " << filename << std::endl;
        return false;
    }
    bool validParsing = true;
    std::string line;
    while (validParsing && std::getline(file, line))
    {
        std::vector<float> lineData;
        while (line.find(",") != std::string::npos)
            line.replace(line.find(","), 1, " ");
        std::stringstream lineStream(line);

        float value;
        while (lineStream >> value)
            lineData.push_back(value);

        switch (lineData.size())
        {
        case 3:
        {
            _positions.push_back(
                Vector3f(lineData[0], lineData[1], lineData[2]));
            break;
        }
        default:
            BRAYNS_ERROR << "Invalid line: " << line << std::endl;
            validParsing = false;
            break;
        }
    }
    file.close();
    BRAYNS_INFO << _positions.size() << " positions loaded" << std::endl;
    return true;
}

bool TissueSliceLoader::_getGIDs(const std::string& circuitConfig,
                                 const size_t neuronCriteria)
{
    _availableGids.clear();
    BRAYNS_INFO << "Opening circuit configuration: " << circuitConfig
                << std::endl;
    const brion::BlueConfig bc(circuitConfig);
    const brain::Circuit circuit(bc);
    const brain::GIDSet& gids = circuit.getGIDs();
    BRAYNS_INFO << "Circuit contains " << gids.size() << " neurons"
                << std::endl;

    strings neuronMatrix;
    try
    {
        brion::Circuit brionCircuit(bc.getCircuitSource());
        for (const auto& neuronAttributes :
             brionCircuit.get(gids, brion::NEURON_LAYER))
            neuronMatrix.push_back(neuronAttributes[0]);
    }
    catch (...)
    {
        BRAYNS_ERROR << "Only MVD2 format is currently supported by Brion "
                        "circuits. Color scheme by layer, e-type or m-type is "
                        "not available for this circuit"
                     << std::endl;
        return false;
    }

    // Filter gids on m-types
    BRAYNS_INFO << "Loading neuron attributes (" << neuronMatrix.size() << ")"
                << std::endl;
    size_t count = 0;
    for (const auto& gid : gids)
    {
        try
        {
            if (boost::lexical_cast<size_t>(neuronMatrix[count]) ==
                neuronCriteria)
                _availableGids.insert(gid);
            ++count;
        }
        catch (boost::bad_lexical_cast& e)
        {
            BRAYNS_ERROR << e.what() << std::endl;
        }
    }

    if (_availableGids.empty())
    {
        BRAYNS_ERROR << "Circuit does not contain any cells" << std::endl;
        return false;
    }

    // Save transformations for later use
    _transforms = circuit.getTransforms(_availableGids);

    return true;
}

void TissueSliceLoader::_filterOutMeshes()
{
    _meshesFilenames.clear();
    _meshesPositions.clear();

    BRAYNS_INFO << "Finding available meshes for " << _availableGids.size()
                << " identified cells" << std::endl;
    size_t count = 0;
    Progress progressMeshFiles("Checking mesh files...", _positions.size());
    for (const auto& gid : _availableGids)
    {
        std::stringstream gidAsString;
        gidAsString << gid;
        const std::string GID = "{gid}";
        auto meshFilePattern = _geometryParameters.getMeshFilePattern();
        meshFilePattern.replace(meshFilePattern.find(GID), GID.length(),
                                gidAsString.str());
        const auto& meshFolder =
            _geometryParameters.getMeshedMorphologiesFolder();
        const std::string meshFilename = meshFolder + "/" + meshFilePattern;

        if (boost::filesystem::exists(meshFilename))
        {
            ++progressMeshFiles;
            _meshesFilenames.push_back(meshFilename);
            _meshesPositions.push_back(_transforms[count].inverse());
        }
        ++count;

        if (_meshesPositions.size() >= _positions.size())
            break;
    }
    BRAYNS_INFO << "Found " << _meshesPositions.size() << " different meshes"
                << std::endl;
}

void TissueSliceLoader::_importMeshes(Scene& scene, MeshLoader& meshLoader)
{
    // Load mesh at specified positions
    size_t count = 0;
    Progress progress("Loading meshes...", _positions.size());
    // SpheresMap& spheres = scene.getSpheres();
    for (const auto& position : _positions)
    {
        ++progress;
        //////////// TO REMOVE
        // spheres[0].push_back(SpherePtr(new Sphere(position, 10.f, 0.f,
        // 0.f)));
        // scene.getWorldBounds().merge(position);
        //////////// TO REMOVE

        const size_t index = count % _meshesFilenames.size();
        Matrix4f idMatrix;
        idMatrix.setTranslation(position);
        Matrix4f matrix = idMatrix * _meshesPositions[index];
        if (!meshLoader.importMeshFromFile(
                _meshesFilenames[index], scene,
                _geometryParameters.getGeometryQuality(), matrix,
                NB_SYSTEM_MATERIALS + count))
            BRAYNS_DEBUG << "Failed to load " << _meshesFilenames[index]
                         << std::endl;

        //////////// TO REMOVE
        // MorphologyLoader morphologyLoader(_geometryParameters);
        // morphologyLoader.importMorphology(uris[index], count, scene,
        // idMatrix);
        //////////// TO REMOVE
        ++count;
    }
    BRAYNS_INFO << "Loaded " << count << " meshes" << std::endl;
}

bool TissueSliceLoader::importFromFile(const std::string& filename,
                                       const std::string& circuitConfig,
                                       Scene& scene, MeshLoader& meshLoader)
{
    const size_t neuronCriteria = 3;
    if (_parsePositions(filename))
        if (_getGIDs(circuitConfig, neuronCriteria))
        {
            _filterOutMeshes();
            _importMeshes(scene, meshLoader);
        }
        else
            return false;
    else
        return false;

    return true;
}
}
