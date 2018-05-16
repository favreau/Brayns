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

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <fstream>

namespace brayns
{
MolecularSystemReader::MolecularSystemReader(
    const GeometryParameters& geometryParameters, Scene& scene)
    : _geometryParameters(geometryParameters)
    , _scene(scene)
{
}

void MolecularSystemReader::importFromFile(
    const std::string& fileName, Model& model BRAYNS_UNUSED,
    const size_t index BRAYNS_UNUSED,
    const size_t defaultMaterialId BRAYNS_UNUSED)
{
    _nbProteins = 0;
    _loadConfiguration(fileName);
    _loadProteins();
    _loadPositions();
    _createSystem();
    _loadEnvironmentMesh();
    model.getBounds().merge(_bounds);

    if (!_calciumSimulationFolder.empty())
    {
        CADiffusionSimulationHandlerPtr handler(
            new CADiffusionSimulationHandler(_calciumSimulationFolder));
        handler->setFrame(_scene, 0);
        _scene.setCADiffusionSimulationHandler(handler);
    }
    BRAYNS_INFO << "Total number of different proteins: " << _proteins.size()
                << std::endl;
    BRAYNS_INFO << "Total number of proteins          : " << _nbProteins
                << std::endl;
}

void MolecularSystemReader::importFromBlob(Blob&&, Model&, const size_t,
                                           const size_t)
{
    throw std::runtime_error(
        "Importing molecular systems from a blob is not supported");
}

void MolecularSystemReader::_createSystem()
{
    if (_proteinFolder.empty() && _meshFolder.empty())
        throw std::runtime_error("No input folder specified");

    const size_t step = 100 / _density;
    size_t proteinCount = 0;
    for (const auto& protein : _proteins)
    {
        // Scale mesh to match PDB units. PDB are in angstrom, and positions are
        // in micrometers
        const float scale = _scale * 0.01f;
        if (proteinCount % step == 0)
            try
            {
                auto model = _scene.createModel();

                std::string fileName;
                if (!_proteinFolder.empty())
                {
                    fileName = _proteinFolder + '/' + protein.second + ".pdb";
                    ProteinLoader loader(_geometryParameters);
                    loader.importFromFile(fileName, *model, proteinCount,
                                          NO_MATERIAL);
                }
                else if (!_meshFolder.empty())
                {
                    fileName = _meshFolder + '/' + protein.second + ".obj";
                    MeshLoader loader(_geometryParameters);
                    loader.importFromFile(fileName, *model, proteinCount,
                                          NO_MATERIAL);
                }

                _scene.addModel(std::move(model), fs::basename(fileName),
                                fileName);

                const auto& proteinPositions = _proteinPositions[protein.first];
                auto& modelDescriptors = _scene.getModelDescriptors();
                auto& modelDescriptor =
                    modelDescriptors[modelDescriptors.size() - 1];
                auto& transformations = modelDescriptor->getTransformations();
                transformations.resize(proteinPositions.size());

                size_t i = 0;
                for (const auto position : proteinPositions)
                {
                    const auto p = _scale * position;
                    auto& transformation = transformations[i];
                    transformation.setTranslation(p);
                    transformation.setScale({scale, scale, scale});
                    _bounds.merge(p);
                    ++i;
                }
            }
            catch (const std::runtime_error& e)
            {
                BRAYNS_ERROR << e.what() << std::endl;
            }

        updateProgress("Loading proteins...", proteinCount, _nbProteins);
        ++proteinCount;
    }
}

void MolecularSystemReader::_loadConfiguration(const std::string& fileName)
{
    // Load molecular system configuration
    std::ifstream configurationFile(fileName, std::ios::in);
    if (!configurationFile.good())
        throw std::runtime_error("Could not open file " + fileName);

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
}

void MolecularSystemReader::_loadEnvironmentMesh()
{
    if (_environmentMesh.empty())
        return;

    MeshLoader meshLoader(_geometryParameters);
    Transformation transformation;
    transformation.setScale({_scale, _scale, _scale});
    auto model = _scene.createModel();
    meshLoader.importFromFile(_environmentMesh, *model);
    _scene.addModel(std::move(model), fs::basename(_environmentMesh),
                    _environmentMesh, {}, transformation);
}

void MolecularSystemReader::_loadProteins()
{
    std::ifstream descriptorFile(_descriptorFilename, std::ios::in);
    if (!descriptorFile.good())
        throw std::runtime_error("Could not open file " + _descriptorFilename);

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
}

void MolecularSystemReader::_loadPositions()
{
    // Load proteins according to specified positions
    std::ifstream filePositions(_positionsFilename, std::ios::in);
    if (!filePositions.good())
        throw std::runtime_error("Could not open file " + _positionsFilename);

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
