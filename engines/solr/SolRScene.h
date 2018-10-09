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

#ifndef SolRSCENE_H
#define SolRSCENE_H

#include <solr/solr.h>

#include <brayns/common/scene/Scene.h>
#include <brayns/common/types.h>

namespace brayns
{
/**

   SolR specific scene

   This object is the SolR specific implementation of a scene

*/
class SolRScene : public Scene
{
public:
    SolRScene(solr::GPUKernel* kernel, ParametersManager& parametersManager);
    ~SolRScene();

    /** @copydoc Scene::commit */
    void commit() final;

    /** @copydoc Scene::commitLights */
    bool commitLights() final;

    /** @copydoc Scene::commitTransferFunctionData */
    bool commitTransferFunctionData() final;

    SharedDataVolumePtr createSharedDataVolume(const Vector3ui& dimensions,
                                               const Vector3f& spacing,
                                               const DataType type) const final;
    BrickedVolumePtr createBrickedVolume(const Vector3ui& dimensions,
                                         const Vector3f& spacing,
                                         const DataType type) const final;
    ModelPtr createModel() const final;

private:
    solr::GPUKernel* _kernel;
};
} // namespace brayns
#endif // SolRSCENE_H
