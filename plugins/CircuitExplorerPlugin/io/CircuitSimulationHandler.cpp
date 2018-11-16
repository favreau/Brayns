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
#include "log.h"

#include <brayns/parameters/AnimationParameters.h>

#include <servus/types.h>

#include <chrono>
#include <thread>

CircuitSimulationHandler::CircuitSimulationHandler(
    brayns::AnimationParameters& animationParameters,
    const brion::URI& reportSource, const brion::GIDSet& gids,
    const bool synchronousMode)
    : brayns::AbstractUserDataHandler()
    , _synchronousMode(synchronousMode)
    , _compartmentReport(
          new brion::CompartmentReport(reportSource, brion::MODE_READ, gids))
    , _animationParameters(animationParameters)
{
    // Load simulation information from compartment reports
    _animationParameters.setStart(_compartmentReport->getStartTime());
    _animationParameters.setEnd(_compartmentReport->getEndTime() /
                                _compartmentReport->getTimestep());
    _animationParameters.setDt(_compartmentReport->getTimestep());
    _animationParameters.setUnit(_compartmentReport->getTimeUnit());

    _frameSize = _compartmentReport->getFrameSize();
    _nbFrames =
        (_animationParameters.getEnd() - _animationParameters.getStart()) /
        _animationParameters.getDt();

    PLUGIN_INFO << "-----------------------------------------------------------"
                << std::endl;
    PLUGIN_INFO << "Simulation information" << std::endl;
    PLUGIN_INFO << "----------------------" << std::endl;
    PLUGIN_INFO << "Start frame          : " << _animationParameters.getStart()
                << std::endl;
    PLUGIN_INFO << "End frame            : " << _animationParameters.getEnd()
                << std::endl;
    PLUGIN_INFO << "Steps between frames : " << _animationParameters.getDt()
                << std::endl;
    PLUGIN_INFO << "Number of frames     : " << _nbFrames << std::endl;
    PLUGIN_INFO << "-----------------------------------------------------------"
                << std::endl;
}

CircuitSimulationHandler::~CircuitSimulationHandler() {}

bool CircuitSimulationHandler::isReady() const
{
    return _ready;
}

void* CircuitSimulationHandler::getFrameData(const uint32_t frame)
{
    const auto boundedFrame = _getBoundedFrame(frame);

    if (!_currentFrameFuture.valid() && _currentFrame != boundedFrame)
        _triggerLoading(boundedFrame);

    if (!_makeFrameReady(boundedFrame))
        return nullptr;

    return _frameData.data();
}

void CircuitSimulationHandler::_triggerLoading(const uint32_t frame)
{
    float timestamp =
        _animationParameters.getStart() + frame * _animationParameters.getDt();
    timestamp = std::max(static_cast<float>(_animationParameters.getStart()),
                         timestamp);
    timestamp =
        std::min(static_cast<float>(_animationParameters.getEnd()), timestamp);

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
            PLUGIN_ERROR << "Error loading simulation frame " << frame << ": "
                         << e.what() << std::endl;
            return false;
        }
        _currentFrame = frame;
        _ready = true;
    }
    return true;
}
