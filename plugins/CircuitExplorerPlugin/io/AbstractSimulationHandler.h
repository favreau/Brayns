/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Jafet Villafranca Diaz <jafet.villafrancadiaz@epfl.ch>
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

#ifndef ABSTRACTSIMULATIONHANDLER_H
#define ABSTRACTSIMULATIONHANDLER_H

#include "../api/CircuitExplorerParams.h"

namespace brayns
{
/**
 * @brief The AbstractSimulationHandler class handles simulation frames for the
 * current circuit
 */
class AbstractSimulationHandler
{
public:
    /**
     * @brief Default constructor
     * @param geometryParameters Geometry parameters
     */
    AbstractSimulationHandler(const CircuitAttributes& circuitAttributes);

    /**
     * @brief Default desctuctor
     */
    virtual ~AbstractSimulationHandler();

    AbstractSimulationHandler& operator=(const AbstractSimulationHandler& rhs);

    /**
    * @brief Attaches a memory mapped file to the scene so that renderers can
    * access the data
    *        as if it was in memory. The OS is in charge of dealing with the map
    * file in system
    *        memory.
    * @param cacheFile File containing the simulation values
    * @return True if the file was successfully attached, false otherwise
    */
    bool attachSimulationToCacheFile(const std::string& cacheFile);

    /**
    * @brief Writes the header to a stream. The header contains the number of
    * frames and the frame
    *        size.
    * @param stream Stream where the header should be written
    */
    void writeHeader(std::ofstream& stream);

    /**
    * @brief Writes a frame to a stream. A frame is a set of float values.
    * @param stream Stream where the header should be written
    * @param values Frame values
    */
    void writeFrame(std::ofstream& stream, const std::vector<float>& values);

    /** @return the current loaded frame for the simulation. */
    uint32_t getCurrentFrame() const { return _currentFrame; }
    /**
     * @brief returns a void pointer to the simulation data for the given frame
     * or nullptr if the frame is not loaded yet.
     */
    virtual void* getFrameData(uint32_t) { return _frameData.data(); }
    /**
     * @brief getFrameSize return the size of the current simulation frame
     */
    uint64_t getFrameSize() const { return _frameSize; }
    /**
     * @brief setFrameSize Sets the size of the current simulation frame
     */
    void setFrameSize(const uint64_t frameSize) { _frameSize = frameSize; }
    /**
     * @brief getNbFrames returns the number of frame for the current simulation
     */
    uint32_t getNbFrames() const { return _nbFrames; }
    /**
     * @brief setNbFrames sets the number of frame for the current simulation
     */
    void setNbFrames(const uint32_t nbFrames) { _nbFrames = nbFrames; }
    /**
     * @return the dt of the simulation in getUnit() time unit; 0 if not
     *         reported
     */
    double getDt() const { return _dt; }
    /** @return the time unit of the simulation; empty if not reported. */
    const std::string& getUnit() const { return _unit; }
    /** @return true if the requested frame from getFrameData() is ready to
     * consume and if it is allowed to advance to the next frame. */
    virtual bool isReady() const { return true; }
protected:
    uint32_t _getBoundedFrame(const uint32_t frame) const;

    const CircuitAttributes& _circuitAttributes;
    uint32_t _currentFrame{std::numeric_limits<uint32_t>::max()};
    uint32_t _nbFrames{0};
    uint64_t _frameSize{0};
    double _dt{0};
    std::string _unit;

    uint64_t _headerSize{0};
    void* _memoryMapPtr{nullptr};
    int _cacheFileDescriptor{-1};
    std::vector<float> _frameData;
};
}
#endif // ABSTRACTSIMULATIONHANDLER_H