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

#ifndef MEMBRANELESS_ORGANELLES_PLUGIN_H
#define MEMBRANELESS_ORGANELLES_PLUGIN_H

#include <api/CircuitExplorerParams.h>

#include <array>
#include <brayns/common/types.h>
#include <brayns/pluginapi/ExtensionPlugin.h>
#include <vector>

/**
 * @brief The CircuitExplorerPlugin class manages the loading and visualization
 * of the Blue Brain Project micro-circuits, and allows visualisation of voltage
 * simulations
 */
class CircuitExplorerPlugin : public brayns::ExtensionPlugin
{
public:
    CircuitExplorerPlugin(brayns::Scene& scene,
                          brayns::ParametersManager& parametersManager,
                          brayns::ActionInterface* actionInterface, int argc,
                          char** argv);

    /**
     * @brief preRender Updates the scene according to latest data load
     */
    void preRender() final;

private:
    void _setMaterial(const MaterialDescriptor&);
    void _setMaterials(const MaterialsDescriptor&);
    void _setSynapseAttributes(const SynapseAttributes&);
    void _setCircuitAttributes(const CircuitAttributes&);
    void _setConnectionsPerValue(const ConnectionsPerValue&);
    void _setStepsGeometry(const StepsGeometry&);
    void _setMetaballsPerSimulationValue(const MetaballsFromSimulationValue&);

    void _setMorphologyAttributes(const MorphologyAttributes&);
    void _loadModelFromCache(const LoadModelFromCache&);
    void _saveModelToCache(const SaveModelToCache&);

    brayns::Scene& _scene;
    brayns::ParametersManager& _parametersManager;

    SynapseAttributes _synapseAttributes;
    MorphologyAttributes _morphologyAttributes;
    CircuitAttributes _circuitAttributes;

    bool _dirty{false};
};
#endif
