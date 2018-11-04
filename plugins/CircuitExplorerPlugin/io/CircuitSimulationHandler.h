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

#ifndef CIRCUITSIMULATIONHANDLER_H
#define CIRCUITSIMULATIONHANDLER_H

#include "AbstractSimulationHandler.h"

#include <brayns/api.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/common/types.h>
#include <brion/brion.h>

typedef std::shared_ptr<brion::CompartmentReport> CompartmentReportPtr;

/**
 * @brief The CircuitSimulationHandler class handles simulation frames for the
 * current circuit. Frames are stored in a memory mapped file that is accessed
 * according to a specified timestamp. The CircuitSimulationHandler class is in
 * charge of keeping the handle to the memory mapped file.
 */
class CircuitSimulationHandler : public brayns::AbstractSimulationHandler
{
public:
    /**
     * @brief Default constructor
     * @param geometryParameters Geometry parameters
     * @param reportSource path to report source
     * @param gids GIDS to load
     */
    CircuitSimulationHandler(const CircuitAttributes& circuitAttributes,
                             const brion::URI& reportSource,
                             const brion::GIDSet& gids,
                             const bool synchronousMode = false);
    ~CircuitSimulationHandler();

    void* getFrameData(uint32_t frame) final;

    CompartmentReportPtr getCompartmentReport() { return _compartmentReport; }
    bool isReady() const final;

private:
    void _triggerLoading(uint32_t frame);
    bool _isFrameLoaded() const;
    bool _makeFrameReady(uint32_t frame);
    bool _synchronousMode{false};

    CompartmentReportPtr _compartmentReport;
    double _startTime;
    double _endTime;
    std::future<brion::floatsPtr> _currentFrameFuture;
    bool _ready{false};
};

#endif // CIRCUITSIMULATIONHANDLER