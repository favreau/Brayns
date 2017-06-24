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

#ifndef TISSUESLICELOADER_H
#define TISSUESLICELOADER_H

#include <brayns/common/types.h>

#include <brain/brain.h>
#include <brion/brion.h>

namespace brayns
{
class TissueSliceLoader
{
public:
    TissueSliceLoader(const GeometryParameters& geometryParameters);

    bool importFromFile(const std::string& filename,
                        const std::string& circuitConfig, Scene& scene,
                        MeshLoader& meshLoader);

private:
    bool _parsePositions(const std::string& filename);
    bool _getGIDs(const std::string& circuitConfig,
                  const size_t neuronCriteria);
    void _filterOutMeshes();
    void _importMeshes(Scene& scene, MeshLoader& meshLoader);

    const GeometryParameters& _geometryParameters;

    Vector3fs _positions;
    brain::GIDSet _availableGids;
    strings _meshesFilenames;
    Matrix4fs _meshesPositions;
    Matrix4fs _transforms;
};
}

#endif // TISSUESLICELOADER_H
