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

#include "MolecularSystemReader.h"

#include <brayns/common/Transformation.h>
#include <brayns/common/log.h>
#include <brayns/common/scene/Model.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/common/utils/Utils.h>
#include <brayns/io/MeshLoader.h>
#include <brayns/io/ProteinLoader.h>
#include <brayns/io/simulation/CADiffusionSimulationHandler.h>

#include <fstream>

namespace brayns
{
MolecularSystemReader::MolecularSystemReader(
    const GeometryParameters& geometryParameters)
    : _geometryParameters(geometryParameters)
{
}

void MolecularSystemReader::importFromFile(
    const std::string& fileName, Scene& scene, const size_t index BRAYNS_UNUSED,
    const Transformation& transformation BRAYNS_UNUSED,
    const size_t defaultMaterialId BRAYNS_UNUSED)
{
    _nbProteins = 0;
    if (!_loadConfiguration(fileName))
        throw std::runtime_error("Failed to load " + fileName);
    if (!_loadProteins())
        throw std::runtime_error("Failed to load proteins");
    if (!_loadPositions())
        throw std::runtime_error("Failed to load positions");
    if (!_createScene(scene))
        throw std::runtime_error("Failed to load scene");
    if (!_loadEnvironmentMesh(scene))
        throw std::runtime_error("Failed to load environment mesh");

    if (!_calciumSimulationFolder.empty())
    {
        CADiffusionSimulationHandlerPtr handler(
            new CADiffusionSimulationHandler(_calciumSimulationFolder));
        handler->setFrame(scene, 0);
        scene.setCADiffusionSimulationHandler(handler);
    }
    BRAYNS_INFO << "Total number of different proteins: " << _proteins.size()
                << std::endl;
    BRAYNS_INFO << "Total number of proteins          : " << _nbProteins
                << std::endl;
}

bool MolecularSystemReader::_createScene(Scene& scene)
{
    const size_t step = 100 / _density;
    for (size_t p = 0; p < _nbProteins; p += step)
    {
        const auto& proteinPosition = _proteinPositions[p];
        const auto& protein = _proteins.find(p);
        size_t instanceCount = 0;
        // Scale mesh to match PDB units. PDB are in angstrom, and positions are
        // in micrometers
        const float scale = _scale * 0.0001f;
        for (size_t pos = 0; pos < proteinPosition.size(); pos += step)
        {
            const auto position = _scale * proteinPosition[pos];
            Transformation transformation;
            transformation.setTranslation(position);
            transformation.setScale({scale, scale, scale});

            if (!_proteinFolder.empty())
            {
                // Load PDB files
                try
                {
                    if (instanceCount == 0)
                    {
                        const auto pdbFilename =
                            _proteinFolder + '/' + protein->second + ".pdb";
                        ProteinLoader loader(_geometryParameters);
                        loader.importFromFile(pdbFilename, scene, p,
                                              Transformation(), 0);
                    }
                    auto& modelDescriptors = scene.getModelDescriptors();
                    auto& modelDescriptor =
                        modelDescriptors[modelDescriptors.size() - 1];
                    modelDescriptor.addInstance(transformation);
                }
                catch (const std::runtime_error& e)
                {
                    BRAYNS_ERROR << e.what() << std::endl;
                    break;
                }
            }
            else if (!_meshFolder.empty())
            {
                // Load meshes
                try
                {
                    if (instanceCount == 0)
                    {
                        const std::string fileName =
                            _meshFolder + '/' + protein->second + ".obj";
                        MeshLoader meshLoader(_geometryParameters);
                        meshLoader.importFromFile(fileName, scene, p,
                                                  Transformation(),
                                                  NO_MATERIAL);
                    }
                    auto& modelDescriptors = scene.getModelDescriptors();
                    auto& modelDescriptor =
                        modelDescriptors[modelDescriptors.size() - 1];
                    modelDescriptor.addInstance(transformation);
                }
                catch (const std::runtime_error& e)
                {
                    BRAYNS_ERROR << e.what() << std::endl;
                    break;
                }
            }
            ++instanceCount;
        }
        updateProgress("Loading proteins...", p, _nbProteins);
    }
    return true;
}

bool MolecularSystemReader::_loadConfiguration(const std::string& fileName)
{
    // Load molecular system configuration
    std::ifstream configurationFile(fileName, std::ios::in);
    if (!configurationFile.good())
    {
        BRAYNS_ERROR << "Could not open file " << fileName << std::endl;
        return false;
    }

    std::map<std::string, std::string> parameters;
    std::string line;
    while (std::getline(configurationFile, line))
    {
        std::stringstream lineStream(line);
        std::string key, value;
        lineStream >> key >> value;
        parameters[key] = value;
    }
    configurationFile.close();

    _density = std::stof(parameters["SystemDensity"]);
    _proteinFolder = parameters["ProteinFolder"];
    _meshFolder = parameters["MeshFolder"];
    _descriptorFilename = parameters["SystemDescriptor"];
    _positionsFilename = parameters["ProteinPositions"];
    _calciumSimulationFolder = parameters["CalciumPositions"];
    _environmentMesh = parameters["EnvironmentMesh"];
    _scale = std::stof(parameters["SystemScale"]);

    BRAYNS_DEBUG << "Loading molecular system" << std::endl;
    BRAYNS_DEBUG << "  Density           : " << _density << std::endl;
    BRAYNS_DEBUG << "  Scale             : " << _scale << std::endl;
    BRAYNS_DEBUG << "  Protein folder    : " << _proteinFolder << std::endl;
    BRAYNS_DEBUG << "  Mesh folder       : " << _meshFolder << std::endl;
    BRAYNS_DEBUG << "  System descriptor : " << _descriptorFilename
                 << std::endl;
    BRAYNS_DEBUG << "  Protein positions : " << _positionsFilename << std::endl;
    BRAYNS_DEBUG << "  Calcium positions : " << _calciumSimulationFolder
                 << std::endl;
    BRAYNS_DEBUG << "  Environment mesh  : " << _environmentMesh << std::endl;
    return true;
}

bool MolecularSystemReader::_loadEnvironmentMesh(Scene& scene)
{
    if (_environmentMesh.empty())
        return true;

    try
    {
        MeshLoader meshLoader(_geometryParameters);
        Transformation transformation;
        transformation.setScale({_scale, _scale, _scale});
        meshLoader.importFromFile(_environmentMesh, scene, 0, transformation);
    }
    catch (const std::runtime_error& e)
    {
        BRAYNS_ERROR << e.what() << std::endl;
        return false;
    }
    return true;
}

bool MolecularSystemReader::_loadProteins()
{
    std::ifstream descriptorFile(_descriptorFilename, std::ios::in);
    if (!descriptorFile.good())
    {
        BRAYNS_ERROR << "Could not open file " << _descriptorFilename
                     << std::endl;
        return false;
    }

    // Load list of proteins
    std::string line;
    while (descriptorFile.good() && std::getline(descriptorFile, line))
    {
        std::stringstream lineStream(line);
        std::string protein;
        size_t id;
        size_t instances;
        lineStream >> protein >> id >> instances;
        if (protein.empty())
            continue;

        bool add = false;
        if (!_proteinFolder.empty())
        {
            const auto pdbFilename(_proteinFolder + '/' + protein + ".pdb");
            std::ifstream pdbFile(pdbFilename, std::ios::in);
            if (pdbFile.good())
            {
                pdbFile.close();
                add = true;
            }
#if 0
            else
            {
                BRAYNS_WARN << pdbFilename << " needs to be downloaded"
                            << std::endl;
                // PDB file not present in folder, download it from RCSB.org
                std::string command;
                command = "wget http://www.rcsb.org/pdb/files/";
                command += protein;
                command += ".pdb";
                command += " -P ";
                command += _proteinFolder;
                int status = system(command.c_str());
                BRAYNS_INFO << command << ": " << status << std::endl;
                add = (status == 0);
            }
#endif
        }
        else if (!_meshFolder.empty())
        {
            const auto meshFilename(_meshFolder + '/' + protein + ".obj");
            std::ifstream meshFile(meshFilename, std::ios::in);
            if (meshFile.good())
            {
                meshFile.close();
                add = true;
            }
        }

        if (add)
            _proteins[id] = protein;
    }
    descriptorFile.close();
    return true;
}

bool MolecularSystemReader::_loadPositions()
{
    // Load proteins according to specified positions
    std::ifstream filePositions(_positionsFilename, std::ios::in);
    if (!filePositions.good())
    {
        BRAYNS_ERROR << "Could not open file " << _positionsFilename
                     << std::endl;
        return false;
    }

    // Load protein positions
    _nbProteins = 0;
    std::string line;
    while (filePositions.good() && std::getline(filePositions, line))
    {
        std::stringstream lineStream(line);
        size_t id;
        Vector3f position;
        lineStream >> id >> position.x() >> position.y() >> position.z();

        if (_proteins.find(id) != _proteins.end())
        {
            auto& proteinPosition = _proteinPositions[id];
            proteinPosition.push_back(position);
            ++_nbProteins;
        }
    }
    filePositions.close();
    return true;
}

void MolecularSystemReader::_writePositionstoFile(const std::string& filename)
{
    std::ofstream outfile(filename, std::ios::binary);
    for (const auto& position : _proteinPositions)
    {
        for (const auto& element : position.second)
        {
            const float radius = 1.f;
            const float value = 1.f;
            outfile.write((char*)&element.x(), sizeof(float));
            outfile.write((char*)&element.y(), sizeof(float));
            outfile.write((char*)&element.z(), sizeof(float));
            outfile.write((char*)&radius, sizeof(float));
            outfile.write((char*)&value, sizeof(float));
        }
    }
    outfile.close();
}
}
