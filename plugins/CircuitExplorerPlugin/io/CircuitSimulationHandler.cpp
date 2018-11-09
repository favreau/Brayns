/* Copyright (c) 2015-2016, EPFL/Blue Brain Project
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

#include "CircuitSimulationHandler.h"

#include <brayns/common/log.h>

#include <servus/types.h>

CircuitSimulationHandler::CircuitSimulationHandler(
    const brion::URI& reportSource, const brion::GIDSet& gids,
    const bool synchronousMode)
    : brayns::AbstractSimulationHandler()
    , _synchronousMode(synchronousMode)
    , _compartmentReport(
          new brion::CompartmentReport(reportSource, brion::MODE_READ, gids))
{
    // Load simulation information from compartment reports
    _startTime = _compartmentReport->getStartTime();
    _endTime = _compartmentReport->getEndTime();
    _dt = _compartmentReport->getTimestep();
    _unit = _compartmentReport->getTimeUnit();
    _frameSize = _compartmentReport->getFrameSize();
    _nbFrames = (_endTime - _startTime) / _dt;

    BRAYNS_INFO << "-----------------------------------------------------------"
                << std::endl;
    BRAYNS_INFO << "Simulation information" << std::endl;
    BRAYNS_INFO << "----------------------" << std::endl;
    BRAYNS_INFO << "Start frame          : " << _startTime << std::endl;
    BRAYNS_INFO << "End frame            : " << _endTime << std::endl;
    BRAYNS_INFO << "Steps between frames : " << _dt << std::endl;
    BRAYNS_INFO << "Number of frames     : " << _nbFrames << std::endl;
    BRAYNS_INFO << "-----------------------------------------------------------"
                << std::endl;
}

CircuitSimulationHandler::~CircuitSimulationHandler()
{
}

bool CircuitSimulationHandler::isReady() const
{
    return _ready;
}

void* CircuitSimulationHandler::getFrameData(uint32_t frame)
{
    frame = _getBoundedFrame(frame);

    if (!_currentFrameFuture.valid() && _currentFrame != frame)
        _triggerLoading(frame);

    if (!_makeFrameReady(frame))
        return nullptr;

    return _frameData.data();
}

void CircuitSimulationHandler::_triggerLoading(const uint32_t frame)
{
    auto timestamp = _startTime + frame * _dt;
    timestamp = std::max(_startTime, timestamp);
    timestamp = std::min(_endTime, timestamp);

    if (_currentFrameFuture.valid())
        _currentFrameFuture.wait();

    _ready = false;
    _currentFrameFuture = _compartmentReport->loadFrame(timestamp);
}

bool CircuitSimulationHandler::_isFrameLoaded() const
{
    if (!_currentFrameFuture.valid())
        return false;

    if (_synchronousMode)
    {
        _currentFrameFuture.wait();
        return true;
    }

    return _currentFrameFuture.wait_for(std::chrono::milliseconds(0)) ==
           std::future_status::ready;
}

bool CircuitSimulationHandler::_makeFrameReady(const uint32_t frame)
{
    if (_isFrameLoaded())
    {
        try
        {
            _frameData = std::move(*_currentFrameFuture.get());
        }
        catch (const std::exception& e)
        {
            BRAYNS_ERROR << "Error loading simulation frame " << frame << ": "
                         << e.what() << std::endl;
            return false;
        }
        _currentFrame = frame;
        _ready = true;
    }
    return true;
}
