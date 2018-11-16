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

#ifndef MORPHOLOGY_LOADER_H
#define MORPHOLOGY_LOADER_H

#include <api/CircuitExplorerParams.h>

#include <brayns/common/geometry/SDFGeometry.h>
#include <brayns/common/loader/Loader.h>
#include <brayns/common/types.h>
#include <brayns/parameters/GeometryParameters.h>

#include <brain/neuron/types.h>
#include <brion/types.h>

#include <unordered_map>
#include <vector>

namespace servus
{
class URI;
}

class CircuitLoader;
struct ParallelModelContainer;
using GIDOffsets = std::vector<uint64_t>;
using CompartmentReportPtr = std::shared_ptr<brion::CompartmentReport>;

// SDF structures
struct SDFMorphologyData
{
    std::vector<brayns::SDFGeometry> geometries;
    std::vector<std::set<size_t>> neighbours;
    std::vector<size_t> materials;
    std::vector<size_t> localToGlobalIdx;
    std::vector<size_t> bifurcationIndices;
    std::unordered_map<size_t, int> geometrySection;
    std::unordered_map<int, std::vector<size_t>> sectionGeometries;
};

struct MorphologyTreeStructure
{
    std::vector<int> sectionParent;
    std::vector<std::vector<size_t>> sectionChildren;
    std::vector<size_t> sectionTraverseOrder;
};

/** Loads morphologies from SWC and H5, and Circuit Config files */
class MorphologyLoader : public brayns::Loader
{
public:
    MorphologyLoader(brayns::Scene& scene,
                     const MorphologyAttributes& morphologyAttributes);
    ~MorphologyLoader();

    /** @copydoc Loader::getSupportedDataTypes */
    static std::set<std::string> getSupportedDataTypes();

    /** @copydoc Loader::importFromBlob */
    brayns::ModelDescriptorPtr importFromBlob(brayns::Blob&& blob,
                                              const size_t index,
                                              const size_t materialID) final;

    /** @copydoc Loader::importFromFile */
    brayns::ModelDescriptorPtr importFromFile(const std::string& filename,
                                              const size_t index,
                                              const size_t materialID) final;

    /**
     * @brief importMorphology imports a single morphology from a specified URI
     * @param uri URI of the morphology
     * @param index Index of the morphology
     * @param defaultMaterialId Material to use
     * @param transformation Transformation to apply to the morphology
     * @param compartmentReport Compartment report to map to the morphology
     * @return Position of the soma
     */
    brayns::Vector3f importMorphology(
        const servus::URI& source, brayns::Model& model, const uint64_t index,
        const brayns::Matrix4f& transformation,
        CompartmentReportPtr compartmentReport = nullptr) const;

    /**
     * @brief setDefaultMaterialId Set the default material for the morphology
     * @param materialId Id of the default material for the morphology
     */
    void setDefaultMaterialId(const size_t materialId)
    {
        _defaultMaterialId = materialId;
    }

private:
    /**
     * @brief _getCorrectedRadius Modifies the radius of the geometry according
     * to --radius-multiplier and --radius-correction geometry parameters
     * @param radius Radius to be corrected
     * @return Corrected value of a radius according to geometry parameters
     */
    float _getCorrectedRadius(const float radius) const;

    /**
     * @brief _getSectionTypes converts Brayns section types into brain::neuron
     * section types
     * @param morphologySectionTypes Brayns section types
     * @return brain::neuron section types
     */
    brain::neuron::SectionTypes _getSectionTypes(
        const size_t morphologySectionTypes) const;

    /**
     * @brief _getIndexAsTextureCoordinates converts a uint64_t index into 2
     * floats so that it can be stored in the texture coordinates of the the
     * geometry to which it is attached
     * @param index Index to be stored in texture coordinates
     * @return Texture coordinates for the given index
     */
    brayns::Vector2f _getIndexAsTextureCoordinates(const uint64_t index) const;

    brayns::Vector3f _importMorphology(const servus::URI& source,
                                       const uint64_t index,
                                       const brayns::Matrix4f& transformation,
                                       CompartmentReportPtr compartmentReport,
                                       ParallelModelContainer& model) const;

    /**
     * @brief _importMorphologyAsPoint places sphere at the specified morphology
     * position
     * @param index Index of the current morphology
     * @param transformation Transformation to apply to the morphology
     * @param material Material that is forced in case geometry parameters do
     * not apply
     * @param compartmentReport Compartment report to map to the morphology
     * @param scene Scene to which the morphology should be loaded into
     * @return Position of the soma
     */
    brayns::Vector3f _importMorphologyAsPoint(
        const uint64_t index, const brayns::Matrix4f& transformation,
        CompartmentReportPtr compartmentReport,
        ParallelModelContainer& model) const;

    /**
     * @brief _createRealisticSoma Creates a realistic soma using the metaballs
     * algorithm.
     * @param uri URI of the morphology for which the soma is created
     * @param index Index of the current morphology
     * @param transformation Transformation to apply to the morphology
     * @param material Material that is forced in case geometry parameters
     * do not apply
     * @param scene Scene to which the morphology should be loaded into
     * @return Position of the soma
     */
    brayns::Vector3f _createRealisticSoma(
        const servus::URI& uri, const brayns::Matrix4f& transformation,
        ParallelModelContainer& model) const;

    size_t _addSDFGeometry(SDFMorphologyData& sdfMorphologyData,
                           const brayns::SDFGeometry& geometry,
                           const std::set<size_t>& neighbours,
                           const size_t materialId, const int section) const;

    /**
     * Creates an SDF soma by adding and connecting the soma children using cone
     * pills
     */
    void _connectSDFSomaChildren(const brayns::Vector3f& somaPosition,
                                 const float somaRadius,
                                 const size_t materialId, const float distance,
                                 const brayns::Vector2f& textureCoordinates,
                                 const brain::neuron::Sections& somaChildren,
                                 SDFMorphologyData& sdfMorphologyData) const;

    /**
     * Goes through all bifurcations and connects to all connected SDF
     * geometries it is overlapping. Every section that has a bifurcation will
     * traverse its children and blend the geometries inside the bifurcation.
     */
    void _connectSDFBifurcations(SDFMorphologyData& sdfMorphologyData,
                                 const MorphologyTreeStructure& mts) const;

    /**
     * Calculates all neighbours and adds the geometries to the model container.
     */
    void _finalizeSDFGeometries(ParallelModelContainer& modelContainer,
                                SDFMorphologyData& sdfMorphologyData) const;

    /**
     * Calculates the structure of the morphology tree by finding overlapping
     * beginnings and endings of the sections.
     */
    MorphologyTreeStructure _calculateMorphologyTreeStructure(
        const brain::neuron::Sections& sections,
        const bool dampenThickness) const;

    /**
     * Adds a Soma geometry to the model
     */
    void _addSomaGeometry(const brain::neuron::Soma& soma,
                          const brayns::Vector3f& translation, uint64_t offset,
                          bool useSDFGeometries, ParallelModelContainer& model,
                          SDFMorphologyData& sdfMorphologyData) const;

    /**
     * Adds the sphere between the steps in the sections
     */
    void _addStepSphereGeometry(const bool useSDFGeometries, const bool isDone,
                                const brayns::Vector3f& position,
                                const float radius, const size_t materialId,
                                const float distance,
                                const brayns::Vector2f& textureCoordinates,
                                ParallelModelContainer& model,
                                const size_t section,
                                SDFMorphologyData& sdfMorphologyData) const;

    /**
     * Adds the cone between the steps in the sections
     */
    void _addStepConeGeometry(
        const bool useSDFGeometries, const brayns::Vector3f& position,
        const float radius, const brayns::Vector3f& target,
        const float previousRadius, const size_t materialId,
        const float distance, const brayns::Vector2f& textureCoordinates,
        ParallelModelContainer& model, const size_t section,
        SDFMorphologyData& sdfMorphologyData) const;

    /**
     * @brief _importMorphologyFromURI imports a morphology from the specified
     * URI
     * @param uri URI of the morphology
     * @param index Index of the current morphology
     * @param materialFunc A function mapping brain::neuron::SectionType to a
     * material id
     * @param transformation Transformation to apply to the morphology
     * @param compartmentReport Compartment report to map to the morphology
     * @param model Model container to whichh the morphology should be loaded
     * into
     * @return Position of the soma
     */
    brayns::Vector3f _importMorphologyFromURI(
        const servus::URI& uri, const uint64_t index,
        const brayns::Matrix4f& transformation,
        CompartmentReportPtr compartmentReport,
        ParallelModelContainer& model) const;

    /**
     * @brief _getMaterialIdFromColorScheme returns the material id
     * corresponding to the morphology color scheme and the section type
     * @param sectionType Section type of the morphology
     * @return Material Id
     */
    size_t _getMaterialIdFromColorScheme(
        const brain::neuron::SectionType& sectionType) const;

    friend class CircuitLoader;

    const MorphologyAttributes& _morphologyAttributes;
    size_t _defaultMaterialId{brayns::NO_MATERIAL};
};

#endif // MORPHOLOGY_LOADER_H
