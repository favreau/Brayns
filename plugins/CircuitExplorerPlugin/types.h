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

#ifndef CIRCUIT_EXPLORER_TYPES_H
#define CIRCUIT_EXPLORER_TYPES_H

#include <brayns/common/mathTypes.h>
#include <memory>
#include <vector>

/** Simulation handlers */
class AbstractSimulationHandler;
using AbstractSimulationHandlerPtr = std::shared_ptr<AbstractSimulationHandler>;

class CircuitSimulationHandler;
using CircuitSimulationHandlerPtr = std::shared_ptr<CircuitSimulationHandler>;

/** Circuit color scheme */
enum class CircuitColorScheme
{
    none = 0,
    neuron_by_id = 1,
    neuron_by_type = 2,
    neuron_by_layer = 3,
    neuron_by_mtype = 4,
    neuron_by_etype = 5,
    neuron_by_target = 6
};

/** Morphology color scheme */
enum class MorphologyColorScheme
{
    none = 0,
    neuron_by_segment_type = 1
};

/** Morphology geometry quality */
enum class GeometryQuality
{
    low = 0,
    medium = 1,
    high = 2
};

/** Morphology element types */
enum class MorphologySectionType
{
    soma = 0x01,
    axon = 0x02,
    dendrite = 0x04,
    apical_dendrite = 0x08,
    all = 0xff
};
using MorphologySectionTypes = std::vector<MorphologySectionType>;

#endif // CIRCUIT_EXPLORER_TYPES_H
