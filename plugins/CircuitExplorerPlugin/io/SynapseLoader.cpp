/* Copyright (c) 2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
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

#include "SynapseLoader.h"
#include "log.h"

#include <brion/brion.h>

#include <brayns/common/material/Material.h>
#include <brayns/common/scene/Model.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/parameters/ParametersManager.h>

SynapseLoader::SynapseLoader(brayns::Scene& scene,
                             brayns::ParametersManager& parametersManager)
    : Loader(scene)
    , _parametersManager(parametersManager)
{
}

SynapseLoader::~SynapseLoader()
{
}

std::set<std::string> SynapseLoader::getSupportedDataTypes()
{
    return {"json"};
}

brayns::ModelDescriptorPtr SynapseLoader::importFromBlob(
    brayns::Blob&& /*blob*/, const size_t /*index*/,
    const size_t /*materialID*/)
{
    throw std::runtime_error("Loading circuit from blob is not supported");
}

brayns::ModelDescriptorPtr SynapseLoader::importFromFile(
    const std::string& /*filename*/, const size_t /*index*/,
    const size_t /*materialID*/)
{
    throw std::runtime_error("Loading circuit from file is not supported");
}

brayns::ModelDescriptorPtr SynapseLoader::importSynapsesFromGIDs(
    const std::string& circuitConfig, const uint32_t& gid,
    const brayns::Vector3fs& colors, const float lightEmission)
{
    const brion::BlueConfig bc(circuitConfig);
    const brain::Circuit circuit(bc);
    const brain::GIDSet gids = {gid};

    const brain::Synapses& synapses =
        circuit.getAfferentSynapses(gids, brain::SynapsePrefetch::all);

    if (synapses.empty())
        throw std::runtime_error(
            "No synapse could be found for the give GID set");

    // Load synapses
    PLUGIN_DEBUG << "Loading " << synapses.size() << " synapses" << std::endl;
    auto model = _scene.createModel();
    size_t i = 0;
    for (auto synapse : synapses)
    {
        if (i >= colors.size())
            throw std::runtime_error("Invalid number of colors. Expected " +
                                     std::to_string(synapses.size()) +
                                     ", provided: " +
                                     std::to_string(colors.size()));

        auto material = model->createMaterial(i, std::to_string(i));
        material->setDiffuseColor(colors[i]);
        material->setShadingMode(brayns::MaterialShadingMode::none);
        material->setEmission(lightEmission);

        const auto pre = synapse.getPresynapticCenterPosition();
        const auto radius =
            _parametersManager.getGeometryParameters().getRadiusMultiplier();
        model->addSphere(i, {pre, radius});
        ++i;
    }

    // Construct model
    brayns::Transformation transformation;
    transformation.setRotationCenter(model->getBounds().getCenter());
    brayns::ModelMetadata metaData = {{"Circuit", circuitConfig},
                                      {"Number of synapses",
                                       std::to_string(synapses.size())}};
    auto modelDescriptor =
        std::make_shared<brayns::ModelDescriptor>(std::move(model),
                                                  std::to_string(gid),
                                                  metaData);
    modelDescriptor->setTransformation(transformation);
    return modelDescriptor;
}
