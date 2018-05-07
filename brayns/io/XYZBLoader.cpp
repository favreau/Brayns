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

#include "XYZBLoader.h"

#include <brayns/common/log.h>
#include <brayns/common/scene/Model.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/common/utils/Utils.h>

#include <fstream>

#include <boost/filesystem.hpp>

namespace brayns
{
XYZBLoader::XYZBLoader(const GeometryParameters& geometryParameters)
    : _geometryParameters(geometryParameters)
{
}

std::set<std::string> XYZBLoader::getSupportedDataTypes()
{
    return {"xyz"};
}

void XYZBLoader::importFromBlob(Blob&& blob, Scene& scene,
                                const size_t index BRAYNS_UNUSED,
                                const Transformation& transformation,
                                const size_t defaultMaterialId BRAYNS_UNUSED)
{
    BRAYNS_INFO << "Loading xyz " << blob.name << std::endl;

    std::stringstream stream(blob.data);
    size_t numlines = 0;
    {
        numlines = std::count(std::istreambuf_iterator<char>(stream),
                              std::istreambuf_iterator<char>(), '\n');
    }
    stream.seekg(0);

    auto model = scene.createModel();

    const auto name = boost::filesystem::basename({blob.name});
    const auto materialId =
        (defaultMaterialId == NO_MATERIAL ? 0 : defaultMaterialId);
    model->createMaterial(materialId, name);
    auto& spheres = model->getSpheres()[materialId];

    const size_t startOffset = spheres.size();
    spheres.reserve(spheres.size() + numlines);

    size_t i = 0;
    std::string line;
    std::stringstream msg;
    msg << "Loading " << shortenString(blob.name) << " ..." << std::endl;
    while (std::getline(stream, line))
    {
        const auto materialId =
            (defaultMaterialId == NO_MATERIAL ? 0 : defaultMaterialId);
        model.createMaterial(materialId, name);
        auto& spheres = model.getSpheres()[materialId];

        const size_t startOffset = spheres.size();
        spheres.reserve(spheres.size() + numlines);

        size_t i = 0;
        std::string line;
        std::stringstream msg;
        msg << "Loading " << shortenString(blob.name) << " ..." << std::endl;
        while (std::getline(stream, line))
        {
        case 3:
        {
            const float newRadius = maxDim / 100.f;
            BRAYNS_WARN << "Given radius "
                        << _geometryParameters.getRadiusMultiplier()
                        << " is too big for this scene, using radius "
                        << newRadius << " now" << std::endl;

            for (i = 0; i < numlines; ++i)
                spheres[i + startOffset].radius = newRadius;
        }
        default:
            throw std::runtime_error("Invalid content in line " +
                                     std::to_string(i + 1) + ": " + line);
        }
        updateProgress(msg.str(), i++, numlines);
    }

    const float maxDim = model->getBounds().getSize().find_max();
    if (maxDim < 100 * _geometryParameters.getRadiusMultiplier())
    {
        const float newRadius = maxDim / 100.f;
        BRAYNS_WARN << "Given radius "
                    << _geometryParameters.getRadiusMultiplier()
                    << " is too big for this scene, using radius " << newRadius
                    << " now" << std::endl;

        for (i = 0; i < numlines; ++i)
            spheres[i + startOffset].radius = newRadius;
    }

    scene.addModel(std::move(model), name, blob.name);
}

void XYZBLoader::importFromFile(const std::string& filename, Scene& scene,
                                const size_t index,
                                const Transformation& transformation,
                                const size_t defaultMaterialId)
{
    std::ifstream file(filename);
    if (!file.good())
        throw std::runtime_error("Could not open file " + filename);
    importFromBlob({"xyz",
                    filename,
                    {std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>()}},
                   scene, index, transformation, defaultMaterialId);
}
}
