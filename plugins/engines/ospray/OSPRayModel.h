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

#ifndef OSPRAYGEOMETRYGROUP_H
#define OSPRAYGEOMETRYGROUP_H

#include "OSPRayMaterialManager.h"

#include <brayns/common/geometry/Model.h>
#include <brayns/parameters/ParametersManager.h>

#include <ospray_cpp/Data.h>

namespace brayns
{
class OSPRayModel : public Model
{
public:
    OSPRayModel(const std::string& name, MaterialManager& materialManager);
    ~OSPRayModel() final;

    void setMemoryFlags(const size_t memoryManagementFlags);

    void unload() final;
    void commitMaterials(const uint32_t flags);
    void commit();
    OSPModel getModel() { return _model; }
    size_t getNbInstances() const { return _instances.size(); };
    OSPGeometry getInstance(const size_t index,
                            ModelTransformation& transformation);
    OSPGeometry getBoundingBoxModelInstance(
        ModelTransformation& transformation);
    OSPGeometry getSimulationModelInstance(ModelTransformation& transformation);

private:
    osp::affine3f _groupTransformationToAffine3f(
        ModelTransformation& transformation);

    void _commitSpheres(const size_t materialId);
    void _commitCylinders(const size_t materialId);
    void _commitCones(const size_t materialId);
    void _commitMeshes(const size_t materialId);
    OSPModel _model{nullptr};
    std::vector<OSPGeometry> _instances;

    // Bounding box
    void _buildBoundingBox();
    size_t _boudingBoxMaterialId{0};
    OSPModel _boundingBoxModel{nullptr};
    OSPGeometry _boundingBoxModelInstance{nullptr};

    // Simulation model
    OSPModel _simulationModel{nullptr};
    OSPGeometry _simulationModelInstance{nullptr};

    // OSPRay data
    std::map<size_t, OSPGeometry> _ospExtendedSpheres;
    std::map<size_t, OSPData> _ospExtendedSpheresData;
    std::map<size_t, OSPGeometry> _ospExtendedCylinders;
    std::map<size_t, OSPData> _ospExtendedCylindersData;
    std::map<size_t, OSPGeometry> _ospExtendedCones;
    std::map<size_t, OSPData> _ospExtendedConesData;
    std::map<size_t, OSPGeometry> _ospMeshes;

    size_t _memoryManagementFlags{OSP_DATA_SHARED_BUFFER};
};
}
#endif // OSPRAYGEOMETRYGROUP_H