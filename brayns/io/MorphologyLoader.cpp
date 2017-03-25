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

#include "MorphologyLoader.h"

#include <brayns/common/geometry/Cone.h>
#include <brayns/common/geometry/Cylinder.h>
#include <brayns/common/geometry/Sphere.h>
#include <brayns/common/log.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/common/simulation/CircuitSimulationHandler.h>
#include <brayns/io/algorithms/MetaballsGenerator.h>

#include <algorithm>

namespace brayns
{
MorphologyLoader::MorphologyLoader(const GeometryParameters &geometryParameters)
    : _geometryParameters(geometryParameters)
{
}

#ifdef BRAYNS_USE_BRION

#ifdef EXPORT_TO_FILE
void MorphologyLoader::_writeToFile(const Vector3f &position,
                                    const float radius)
{
    float f = position.x();
    _outputFile.write((char *)&f, sizeof(float));
    f = position.y();
    _outputFile.write((char *)&f, sizeof(float));
    f = position.z();
    _outputFile.write((char *)&f, sizeof(float));
    f = radius;
    _outputFile.write((char *)&f, sizeof(float));
    f = 1.f;
    _outputFile.write((char *)&f, sizeof(float));
}
#endif

brain::neuron::SectionTypes _getSectionTypes(
    const size_t morphologySectionTypes)
{
    brain::neuron::SectionTypes sectionTypes;
    if (morphologySectionTypes & MST_SOMA)
        sectionTypes.push_back(brain::neuron::SectionType::soma);
    if (morphologySectionTypes & MST_AXON)
        sectionTypes.push_back(brain::neuron::SectionType::axon);
    if (morphologySectionTypes & MST_DENDRITE)
        sectionTypes.push_back(brain::neuron::SectionType::dendrite);
    if (morphologySectionTypes & MST_APICAL_DENDRITE)
        sectionTypes.push_back(brain::neuron::SectionType::apicalDendrite);
    return sectionTypes;
}

bool MorphologyLoader::_importMorphologyAsMesh(const servus::URI &source,
                                               const size_t morphologyIndex,
                                               const MaterialsMap &materials,
                                               const Matrix4f &transformation,
                                               TrianglesMeshMap &meshes,
                                               Boxf &bounds)
{
    try
    {
        const size_t morphologySectionTypes =
            _geometryParameters.getMorphologySectionTypes();

        brain::neuron::Morphology morphology(source, transformation);
        brain::neuron::SectionTypes sectionTypes =
            _getSectionTypes(morphologySectionTypes);

        const brain::neuron::Sections &sections =
            morphology.getSections(sectionTypes);

        Spheres metaballs;

        if (morphologySectionTypes & MST_SOMA)
        {
            // Soma
            const brain::neuron::Soma &soma = morphology.getSoma();
            const size_t material = _getMaterialFromSectionType(
                morphologyIndex, size_t(brain::neuron::SectionType::soma));
            const Vector3f center = soma.getCentroid();

            const float radius =
                (_geometryParameters.getRadiusCorrection() != 0.f
                     ? _geometryParameters.getRadiusCorrection()
                     : soma.getMeanRadius() *
                           _geometryParameters.getRadiusMultiplier());

            metaballs.push_back(
                SpherePtr(new Sphere(material, center, radius, 0.f, 0.f)));
            bounds.merge(center);
        }

        // Dendrites and axon
        for (size_t s = 0; s < sections.size(); ++s)
        {
            const auto &section = sections[s];
            const bool hasParent = section.hasParent();
            if (hasParent)
            {
                const auto parentSectionType = section.getParent().getType();
                if (parentSectionType != brain::neuron::SectionType::soma)
                    continue;
            }

            const auto material =
                _getMaterialFromSectionType(morphologyIndex,
                                            size_t(section.getType()));
            const auto &samples = section.getSamples();
            if (samples.empty())
                continue;

            const size_t samplesFromSoma =
                _geometryParameters.getMetaballsSamplesFromSoma();
            const size_t samplesToProcess =
                std::min(samplesFromSoma, samples.size());
            for (size_t i = 0; i < samplesToProcess; ++i)
            {
                const auto &sample = samples[i];
                const Vector3f position(sample.x(), sample.y(), sample.z());
                const auto radius =
                    (_geometryParameters.getRadiusCorrection() != 0.f
                         ? _geometryParameters.getRadiusCorrection()
                         : sample.w() * 0.5f *
                               _geometryParameters.getRadiusMultiplier());

                if (radius > 0.f)
                    metaballs.push_back(SpherePtr(
                        new Sphere(material, position, radius, 0.f, 0.f)));

                bounds.merge(position);
            }
        }

        // Generate mesh
        const size_t gridSize = _geometryParameters.getMetaballsGridSize();
        const float threshold = _geometryParameters.getMetaballsThreshold();
        MetaballsGenerator metaballsGenerator;
        const size_t material = _getMaterialFromSectionType(
            morphologyIndex, size_t(brain::neuron::SectionType::soma));
        metaballsGenerator.generateMesh(metaballs, gridSize, threshold,
                                        materials, material, meshes);
    }
    catch (const std::runtime_error &e)
    {
        BRAYNS_ERROR << e.what() << std::endl;
        return false;
    }
    return true;
}

void MorphologyLoader::_createSpines(const brain::Circuit &circuit,
                                     const brain::GIDSet &gids,
                                     const size_t gid, const float radius,
                                     SpheresMap &spheres,
#ifdef EXPORT_TO_FILE
                                     CylindersMap &,
#else
                                     CylindersMap &cylinders,
#endif
                                     Boxf &bounds)
{
    return;
    BRAYNS_INFO << "Create spines" << std::endl;

    // Afferent synapses
    brain::SynapsesStream afferentSynapses = circuit.getAfferentSynapses(gids);
    std::future<brain::Synapses> future = afferentSynapses.read();

    size_t i = 0;
    while (!afferentSynapses.eos())
    {
        const brain::Synapses synapses = future.get();
        future = afferentSynapses.read(); // fetch next
        for (const auto &synapse : synapses)
        {
            if (synapse.getPostsynapticSectionID() != 0 &&
                synapse.getPostsynapticGID() == gid)
            {
                const Vector3f from = synapse.getPresynapticSurfacePosition();
                const Vector3f to = synapse.getPostsynapticSurfacePosition();
                bounds.merge(from);
                bounds.merge(to);
#ifdef EXPORT_TO_FILE
                Vector3f dir = (to - from) / 10.f;
                float r = (2.f * radius) / 10.f;
                for (float b = 0.f; b < 10.f; b += 1.f)
                {
                    _writeToFile(from + dir * b, 4.f * radius - r * b);
                    spheres[MATERIAL_AFFERENT_SYNAPSE].push_back(SpherePtr(
                        new Sphere(MATERIAL_AFFERENT_SYNAPSE, from + dir * b,
                                   4.f * radius - r * b, 0.f, 0.f)));
                }
#else
                spheres[MATERIAL_AFFERENT_SYNAPSE].push_back(
                    SpherePtr(new Sphere(MATERIAL_AFFERENT_SYNAPSE, from,
                                         4.f * radius, 0.f, 0.f)));
                cylinders[MATERIAL_AFFERENT_SYNAPSE].push_back(
                    CylinderPtr(new Cylinder(MATERIAL_AFFERENT_SYNAPSE, from,
                                             to, 2.f * radius, 0.f, 0.f)));
#endif
            }
            ++i;
        }
    }
    BRAYNS_INFO << "Added " << i << " afferent synapses" << std::endl;

    // Afferent synapses
    brain::SynapsesStream efferentSynapses = circuit.getEfferentSynapses(gids);
    future = efferentSynapses.read();

    i = 0;
    while (!efferentSynapses.eos())
    {
        const brain::Synapses synapses = future.get();
        future = efferentSynapses.read(); // fetch next
        for (const auto &synapse : synapses)
        {
            if (synapse.getPostsynapticSectionID() != 0 &&
                synapse.getPostsynapticGID() == gid)
            {
                const Vector3f from = synapse.getPostsynapticSurfacePosition();
                const Vector3f to = synapse.getPresynapticSurfacePosition();
                bounds.merge(from);
                bounds.merge(to);
#ifdef EXPORT_TO_FILE
                Vector3f dir = (to - from) / 10.f;
                float r = (2.f * radius) / 10.f;
                for (float b = 0; b < 10.f; b += 1.f)
                {
                    _writeToFile(from + dir * b, 2.f * radius + r * b);
                    spheres[MATERIAL_EFFERENT_SYNAPSE].push_back(SpherePtr(
                        new Sphere(MATERIAL_EFFERENT_SYNAPSE, from + dir * b,
                                   2.f * radius + r * b, 0.f, 0.f)));
                }
#else
                spheres[MATERIAL_EFFERENT_SYNAPSE].push_back(
                    SpherePtr(new Sphere(MATERIAL_EFFERENT_SYNAPSE, from,
                                         4.f * radius, 0.f, 0.f)));
                cylinders[MATERIAL_EFFERENT_SYNAPSE].push_back(
                    CylinderPtr(new Cylinder(MATERIAL_EFFERENT_SYNAPSE, from,
                                             to, 1.5f * radius, 0.f, 0.f)));
#endif
            }
            ++i;
        }
    }
    BRAYNS_INFO << "Added " << i << " efferent synapses" << std::endl;
}

bool MorphologyLoader::importMorphology(const servus::URI &uri,
                                        const int morphologyIndex, Scene &scene)
{
#ifdef EXPORT_TO_FILE
    _outputFile.open("morphology.bin", std::ios::out | std::ios::binary);
#endif

    bool returnValue = true;
    if (_geometryParameters.useMetaballs())
    {
        returnValue =
            _importMorphologyAsMesh(uri, morphologyIndex, scene.getMaterials(),
                                    Matrix4f(), scene.getTriangleMeshes(),
                                    scene.getWorldBounds());
    }
    float maxDistanceToSoma;
    float minRadius;
    returnValue = returnValue &&
                  _importMorphology(uri, morphologyIndex, Matrix4f(), nullptr,
                                    scene.getSpheres(), scene.getCylinders(),
                                    scene.getCones(), scene.getWorldBounds(), 0,
                                    maxDistanceToSoma, minRadius);

#ifdef EXPORT_TO_FILE
    _outputFile.close();
#endif
    return returnValue;
}

bool MorphologyLoader::_importMorphology(
    const servus::URI &source, const size_t morphologyIndex,
    const Matrix4f &transformation,
    const SimulationInformation *simulationInformation, SpheresMap &spheres,
    CylindersMap &cylinders, ConesMap &cones, Boxf &bounds,
    const size_t simulationOffset, float &maxDistanceToSoma, float &minRadius)
{
    Vector3f somaPosition;
    maxDistanceToSoma = 0.f;
    try
    {
        Vector3f translation = {0.f, 0.f, 0.f};

        brain::neuron::Morphology morphology(source, transformation);
        brain::neuron::SectionTypes sectionTypes;

        const MorphologyLayout &layout =
            _geometryParameters.getMorphologyLayout();

        if (layout.nbColumns != 0)
        {
            Boxf morphologyAABB;
            const brain::Vector4fs &points = morphology.getPoints();
            for (Vector4f point : points)
            {
                const Vector3f p = {point.x(), point.y(), point.z()};
                morphologyAABB.merge(p);
            }

            const Vector3f positionInGrid = {
                -layout.horizontalSpacing *
                    static_cast<float>(morphologyIndex % layout.nbColumns),
                -layout.verticalSpacing *
                    static_cast<float>(morphologyIndex / layout.nbColumns),
                0.f};
            translation = positionInGrid - morphologyAABB.getCenter();
        }

        const size_t morphologySectionTypes =
            _geometryParameters.getMorphologySectionTypes();

        sectionTypes = _getSectionTypes(morphologySectionTypes);

        const brain::neuron::Sections &sections =
            morphology.getSections(sectionTypes);

        size_t sectionId = 0;

        float offset = 0.f;
        if (simulationInformation)
            offset = (*simulationInformation->compartmentOffsets)[sectionId];
        else if (simulationOffset != 0)
            offset = simulationOffset;

#ifdef EXPORT_TO_FILE
        {
            const size_t material = _getMaterialFromSectionType(
                morphologyIndex, size_t(brain::neuron::SectionType::soma));
            const brain::neuron::Soma &soma = morphology.getSoma();
            somaPosition = soma.getCentroid();
            const float somaRadius = soma.getMeanRadius();

            // Profile
            const auto &profilePoints = soma.getProfilePoints();
            const float innerSphereRadius =
                2.f * somaRadius / profilePoints.size();

            BRAYNS_INFO << "Adding " << profilePoints.size()
                        << " profile points" << std::endl;

            if (false)
            {
                // Big balls
                Vector3f v0 = profilePoints[0] - somaPosition;
                Vector3f v1 = profilePoints[1] - somaPosition;
                Vector3f v2 = v0.cross(v1);
                Vector3f center = somaPosition + v2 * 0.25f;
                const float bigBallsRadius = innerSphereRadius * 6.f;
                _writeToFile(center, bigBallsRadius);
                spheres[material].push_back(SpherePtr(
                    new Sphere(material, center, bigBallsRadius, 0.f, offset)));
                center = somaPosition - v2 * 0.25f;
                _writeToFile(center, bigBallsRadius);
                spheres[material].push_back(SpherePtr(
                    new Sphere(material, center, bigBallsRadius, 0.f, offset)));
            }

            if (false)
            {
                // Filled profile
                for (const auto &profilePoint : profilePoints)
                {
                    const Vector3f point = {profilePoint.x(), profilePoint.y(),
                                            profilePoint.z()};
                    /*
                    _writeToFile(point, innerSphereRadius);
                    spheres[material].push_back(SpherePtr(
                        new Sphere(material, point, innerSphereRadius, 0.f,
                    offset)));
                    bounds.merge(point);
                    */

                    const size_t nbBalls = 8;
                    const Vector3f dir = point - somaPosition;
                    const Vector3f step = dir / float(nbBalls);
                    for (size_t j = 1; j < nbBalls; ++j)
                    {
                        const Vector3f center = somaPosition + step * j;
                        _writeToFile(center, innerSphereRadius);
                        spheres[material].push_back(SpherePtr(
                            new Sphere(material, center, innerSphereRadius, 0.f,
                                       offset)));
                        bounds.merge(center);
                    }
                }
            }

            // Children
            const auto &children = soma.getChildren();
            for (const auto &child : children)
            {
                const auto &samples = child.getSamples();
                const size_t index = 0; // std::min( 3, int(samples.size()-1));
                const Vector3f position = {samples[index].x(),
                                           samples[index].y(),
                                           samples[index].z()};
                const Vector3f dir = position - somaPosition;
                const size_t nbBalls =
                    dir.length() / float(0.5f * samples[0].w());
                if (nbBalls > 1)
                {
                    const Vector3f step = dir / float(nbBalls);
                    const float stepRadius = 0.5f *
                                             (samples[index].w() - somaRadius) /
                                             float(nbBalls);
                    for (size_t j = 1; j < nbBalls; ++j)
                    {
                        const Vector3f point = somaPosition + float(j) * step;
                        const float rnd = 1.f + float(rand() % 200) / 1000.f;
                        const float radius =
                            (0.5f * somaRadius + j * stepRadius) * rnd;
                        // const float radius = innerSphereRadius;
                        _writeToFile(point, radius);
                        spheres[material].push_back(SpherePtr(
                            new Sphere(material, point, radius, 0.f, offset)));
                        bounds.merge(point);
                    }
                }
            }
        }
#else
        if (!_geometryParameters.useMetaballs() &&
            morphologySectionTypes & MST_SOMA)
        {
            // Soma
            const brain::neuron::Soma &soma = morphology.getSoma();
            const size_t material = _getMaterialFromSectionType(
                morphologyIndex, size_t(brain::neuron::SectionType::soma));
            const Vector3f &center = soma.getCentroid() + translation;

            const float radius =
                (_geometryParameters.getRadiusCorrection() != 0.f
                     ? _geometryParameters.getRadiusCorrection()
                     : soma.getMeanRadius() *
                           _geometryParameters.getRadiusMultiplier());

            spheres[material].push_back(
                SpherePtr(new Sphere(material, center, radius, 0.f, offset)));
            bounds.merge(center);
        }
#endif

        // Dendrites and axon
        for (const auto &section : sections)
        {
            const size_t material =
                _getMaterialFromSectionType(morphologyIndex,
                                            size_t(section.getType()));
            const Vector4fs &samples = section.getSamples();
            if (samples.size() < 1)
                continue;

#ifdef EXPORT_TO_FILE
            if (samples.size() < 2)
                continue;

            // BRANCHES
            for (size_t s = 1; s < samples.size(); ++s)
            {
                const Vector3f origin = {samples[s - 1].x(), samples[s - 1].y(),
                                         samples[s - 1].z()};
                const Vector3f target = {samples[s].x(), samples[s].y(),
                                         samples[s].z()};

                maxDistanceToSoma =
                    std::max(maxDistanceToSoma, section.getDistanceToSoma());

                const float originRadius = samples[s - 1].w() / 2.f;
                const float targetRadius = samples[s].w() / 2.f;

                const Vector3f dir = target - origin;
                const size_t nbBalls =
                    dir.length() /
                    (0.5f * std::min(originRadius, targetRadius));

                if (nbBalls != 0)
                {
                    const Vector3f step = dir / float(nbBalls);
                    const float stepRadius =
                        (targetRadius - originRadius) / float(nbBalls);

                    for (size_t j = 0; j < nbBalls; ++j)
                    {
                        const Vector3f point = origin + float(j) * step;
                        const float rnd = 1.f + float(rand() % 200) / 1000.f;
                        const float radius =
                            (originRadius + j * stepRadius) * rnd;
                        _writeToFile(point, radius);
                        minRadius = std::min(minRadius, radius);

                        spheres[material].push_back(SpherePtr(
                            new Sphere(material, point, radius, 0.f, offset)));
                        bounds.merge(point);
                    }
                }
            }
            continue;
#endif

            Vector4f previousSample = samples[0];
            size_t step = 1;
            switch (_geometryParameters.getGeometryQuality())
            {
            case GeometryQuality::low:
                step = samples.size() - 1;
                break;
            case GeometryQuality::medium:
                step = samples.size() / 2;
                step = (step == 0) ? 1 : step;
                break;
            default:
                step = 1;
            }

            const float distanceToSoma = section.getDistanceToSoma();
            const floats &distancesToSoma = section.getSampleDistancesToSoma();

            float segmentStep = 0.f;
            if (simulationInformation)
            {
                const auto &counts = *simulationInformation->compartmentCounts;
                // Number of compartments usually differs from number of samples
                if (samples.empty() && counts[sectionId] > 1)
                    segmentStep = counts[sectionId] / float(samples.size());
            }

            bool done = false;
            for (size_t i = step; !done && i < samples.size() + step; i += step)
            {
                if (i >= samples.size())
                {
                    i = samples.size() - 1;
                    done = true;
                }

                const float distance = distanceToSoma + distancesToSoma[i];

                maxDistanceToSoma = std::max(maxDistanceToSoma, distance);

                if (simulationInformation)
                    offset = (*simulationInformation
                                   ->compartmentOffsets)[sectionId] +
                             float(i) * segmentStep;
                else if (simulationOffset != 0)
                    offset = simulationOffset + distance;

                Vector4f sample = samples[i];
                const float previousRadius =
                    (_geometryParameters.getRadiusCorrection() != 0.f
                         ? _geometryParameters.getRadiusCorrection()
                         : samples[i - step].w() * 0.5f *
                               _geometryParameters.getRadiusMultiplier());

                Vector3f position(sample.x(), sample.y(), sample.z());
                position += translation;
                Vector3f target(previousSample.x(), previousSample.y(),
                                previousSample.z());
                target += translation;
                const float radius =
                    (_geometryParameters.getRadiusCorrection() != 0.f
                         ? _geometryParameters.getRadiusCorrection()
                         : samples[i].w() * 0.5f *
                               _geometryParameters.getRadiusMultiplier());
                minRadius = std::min(minRadius, radius);

                if (radius > 0.f)
                    spheres[material].push_back(
                        SpherePtr(new Sphere(material, position, radius,
                                             distance, offset)));

                bounds.merge(position);
                if (position != target && radius > 0.f && previousRadius > 0.f)
                {
                    if (radius == previousRadius)
                        cylinders[material].push_back(CylinderPtr(
                            new Cylinder(material, position, target, radius,
                                         distance, offset)));
                    else
                        cones[material].push_back(ConePtr(
                            new Cone(material, position, target, radius,
                                     previousRadius, distance, offset)));
                    bounds.merge(target);
                }
                previousSample = sample;
            }
            ++sectionId;
        }
        BRAYNS_DEBUG << "Soma position       : " << somaPosition << std::endl;
        BRAYNS_DEBUG << "Normalized position : "
                     << (somaPosition - bounds.getMin()) / bounds.getSize()
                     << std::endl;
        BRAYNS_DEBUG << "Distance to soma    : " << maxDistanceToSoma
                     << std::endl;
        BRAYNS_ERROR << minRadius << std::endl;
    }
    catch (const std::runtime_error &e)
    {
        BRAYNS_ERROR << e.what() << std::endl;
        return false;
    }
    return true;
}

bool MorphologyLoader::importCircuit(const servus::URI &circuitConfig,
                                     const std::string &target, Scene &scene)
{
    const std::string &filename = circuitConfig.getPath();
    const brion::BlueConfig bc(filename);
    const brain::Circuit circuit(bc);

    brain::GIDSet gids =
        (target.empty() ? circuit.getGIDs() : circuit.getGIDs(target));

    for (const auto &gid : gids)
        BRAYNS_INFO << gid << std::endl;

    const auto gid = _geometryParameters.getNeuronGID();
    if (gid != std::numeric_limits<uint64_t>::max())
    {
        gids.clear();
        gids.insert(gid);
    }

    if (gids.empty())
    {
        BRAYNS_ERROR << "Circuit does not contain any cells" << std::endl;
        return false;
    }
    const Matrix4fs &transforms = circuit.getTransforms(gids);

    const brain::URIs &uris = circuit.getMorphologyURIs(gids);

    BRAYNS_INFO << "Loading " << uris.size() << " cells" << std::endl;

    std::map<size_t, float> morphologyOffsets;

    size_t simulationOffset = 1;
    size_t simulatedCells = 0;
    size_t progress = 0;
    float minRadius = std::numeric_limits<float>::max();

#ifdef EXPORT_TO_FILE
    std::stringstream morphologyFilebname;
    morphologyFilebname << gid << ".bin";
    _outputFile.open(morphologyFilebname.str(),
                     std::ios::out | std::ios::binary);
#endif

#pragma omp parallel
    {
        SpheresMap private_spheres;
        CylindersMap private_cylinders;
        ConesMap private_cones;
        Boxf private_bounds;
#pragma omp for nowait
        for (size_t i = 0; i < uris.size(); ++i)
        {
            const auto &uri = uris[i];
            float maxDistanceToSoma = 0.f;

            if (_geometryParameters.useMetaballs())
            {
                _importMorphologyAsMesh(uri, i, scene.getMaterials(),
                                        transforms[i],
                                        scene.getTriangleMeshes(),
                                        scene.getWorldBounds());
            }

            if (_importMorphology(uri, i, transforms[i], 0, private_spheres,
                                  private_cylinders, private_cones,
                                  private_bounds, simulationOffset,
                                  maxDistanceToSoma, minRadius))
            {
                morphologyOffsets[simulatedCells] = maxDistanceToSoma;
                simulationOffset += maxDistanceToSoma;
            }

            BRAYNS_PROGRESS(progress, uris.size());
#pragma omp atomic
            ++progress;
        }

#pragma omp critical
        for (const auto &p : private_spheres)
        {
            const size_t material = p.first;
            scene.getSpheres()[material].insert(
                scene.getSpheres()[material].end(),
                private_spheres[material].begin(),
                private_spheres[material].end());
        }

#pragma omp critical
        for (const auto &p : private_cylinders)
        {
            const size_t material = p.first;
            scene.getCylinders()[material].insert(
                scene.getCylinders()[material].end(),
                private_cylinders[material].begin(),
                private_cylinders[material].end());
        }

#pragma omp critical
        for (const auto &p : private_cones)
        {
            const size_t material = p.first;
            scene.getCones()[material].insert(scene.getCones()[material].end(),
                                              private_cones[material].begin(),
                                              private_cones[material].end());
        }

        scene.getWorldBounds().merge(private_bounds);
    }

    // Spines
    gids = (target.empty() ? circuit.getGIDs() : circuit.getGIDs(target));
    _createSpines(circuit, gids, gid, minRadius, scene.getSpheres(),
                  scene.getCylinders(), scene.getWorldBounds());
#ifdef EXPORT_TO_FILE
    _outputFile.close();
#endif
    return true;
}

bool MorphologyLoader::importCircuit(const servus::URI &circuitConfig,
                                     const std::string &target,
                                     const std::string &report, Scene &scene)
{
#ifdef EXPORT_TO_FILE
    std::ofstream outputFile("morphology.bin",
                             std::ios::out | std::ios::binary);
#endif

    const std::string &filename = circuitConfig.getPath();
    const brion::BlueConfig bc(filename);
    const brain::Circuit circuit(bc);
    const brain::GIDSet &gids =
        (target.empty() ? circuit.getGIDs() : circuit.getGIDs(target));
    if (gids.empty())
    {
        BRAYNS_ERROR << "Circuit does not contain any cells" << std::endl;
        return false;
    }

    const Matrix4fs &transforms = circuit.getTransforms(gids);

    const brain::URIs &uris = circuit.getMorphologyURIs(gids);

    // Load simulation information from compartment reports
    const brion::CompartmentReport compartmentReport(
        brion::URI(bc.getReportSource(report).getPath()), brion::MODE_READ,
        gids);

    const brion::CompartmentCounts &compartmentCounts =
        compartmentReport.getCompartmentCounts();

    const brion::SectionOffsets &compartmentOffsets =
        compartmentReport.getOffsets();

    brain::URIs cr_uris;
    const brain::GIDSet &cr_gids = compartmentReport.getGIDs();

    BRAYNS_INFO << "Loading " << cr_gids.size() << " simulated cells"
                << std::endl;
    for (const auto cr_gid : cr_gids)
    {
        auto it = std::find(gids.begin(), gids.end(), cr_gid);
        auto index = std::distance(gids.begin(), it);
        cr_uris.push_back(uris[index]);
    }

    size_t progress = 0;
    float minRadius = std::numeric_limits<float>::max();

#pragma omp parallel
    {
        SpheresMap private_spheres;
        CylindersMap private_cylinders;
        ConesMap private_cones;
        Boxf private_bounds;
#pragma omp for nowait
        for (size_t i = 0; i < cr_uris.size(); ++i)
        {
            const auto &uri = cr_uris[i];
            const SimulationInformation simulationInformation = {
                &compartmentCounts[i], &compartmentOffsets[i]};

            if (_geometryParameters.useMetaballs())
            {
                _importMorphologyAsMesh(uri, i, scene.getMaterials(),
                                        transforms[i],
                                        scene.getTriangleMeshes(),
                                        scene.getWorldBounds());
            }

            float maxDistanceToSoma;
            _importMorphology(uri, i, transforms[i], &simulationInformation,
                              private_spheres, private_cylinders, private_cones,
                              private_bounds, 0, maxDistanceToSoma, minRadius);

            BRAYNS_PROGRESS(progress, cr_uris.size());
#pragma omp atomic
            ++progress;
        }

#pragma omp critical
        for (const auto &p : private_spheres)
        {
            const size_t material = p.first;
            scene.getSpheres()[material].insert(
                scene.getSpheres()[material].end(),
                private_spheres[material].begin(),
                private_spheres[material].end());
        }

#pragma omp critical
        for (const auto &p : private_cylinders)
        {
            const size_t material = p.first;
            scene.getCylinders()[material].insert(
                scene.getCylinders()[material].end(),
                private_cylinders[material].begin(),
                private_cylinders[material].end());
        }

#pragma omp critical
        for (const auto &p : private_cones)
        {
            const size_t material = p.first;
            scene.getCones()[material].insert(scene.getCones()[material].end(),
                                              private_cones[material].begin(),
                                              private_cones[material].end());
        }

        scene.getWorldBounds().merge(private_bounds);
    }

    size_t nonSimulatedCells = _geometryParameters.getNonSimulatedCells();
    if (nonSimulatedCells != 0)
    {
        // Non simulated cells
        const brain::GIDSet &allGids = circuit.getGIDs();
        const brain::URIs &allUris = circuit.getMorphologyURIs(allGids);
        const Matrix4fs &allTransforms = circuit.getTransforms(allGids);

        cr_uris.clear();
        size_t index = 0;
        for (const auto gid : allGids)
        {
            auto it = std::find(cr_gids.begin(), cr_gids.end(), gid);
            if (it == cr_gids.end())
                cr_uris.push_back(allUris[index]);
            ++index;
        }

        if (cr_uris.size() < nonSimulatedCells)
            nonSimulatedCells = cr_uris.size();

        BRAYNS_INFO << "Loading " << nonSimulatedCells << " non-simulated cells"
                    << std::endl;

        progress = 0;
#pragma omp parallel
        {
            SpheresMap private_spheres;
            CylindersMap private_cylinders;
            ConesMap private_cones;
            Boxf private_bounds;
#pragma omp for nowait
            for (size_t i = 0; i < nonSimulatedCells; ++i)
            {
                float maxDistanceToSoma;
                const auto &uri = allUris[i];

                _importMorphology(uri, i, allTransforms[i], 0, private_spheres,
                                  private_cylinders, private_cones,
                                  private_bounds, 0, maxDistanceToSoma,
                                  minRadius);

                BRAYNS_PROGRESS(progress, allUris.size());
#pragma omp atomic
                ++progress;
            }

#pragma omp critical
            for (const auto &p : private_spheres)
            {
                const size_t material = p.first;
                scene.getSpheres()[material].insert(
                    scene.getSpheres()[material].end(),
                    private_spheres[material].begin(),
                    private_spheres[material].end());
            }

#pragma omp critical
            for (const auto &p : private_cylinders)
            {
                const size_t material = p.first;
                scene.getCylinders()[material].insert(
                    scene.getCylinders()[material].end(),
                    private_cylinders[material].begin(),
                    private_cylinders[material].end());
            }

#pragma omp critical
            for (const auto &p : private_cones)
            {
                const size_t material = p.first;
                scene.getCones()[material].insert(
                    scene.getCones()[material].end(),
                    private_cones[material].begin(),
                    private_cones[material].end());
            }

            scene.getWorldBounds().merge(private_bounds);
        }
    }

    // Spines
    _createSpines(circuit, gids, 0, minRadius, scene.getSpheres(),
                  scene.getCylinders(), scene.getWorldBounds());

#ifdef EXPORT_TO_FILE
    outputFile.close();
#endif
    return true;
}

bool MorphologyLoader::importSimulationData(const servus::URI &circuitConfig,
                                            const std::string &target,
                                            const std::string &report,
                                            Scene &scene)
{
    const std::string &filename = circuitConfig.getPath();
    const brion::BlueConfig bc(filename);
    const brain::Circuit circuit(bc);
    const brain::GIDSet &gids =
        (target.empty() ? circuit.getGIDs() : circuit.getGIDs(target));
    if (gids.empty())
    {
        BRAYNS_ERROR << "Circuit does not contain any cells" << std::endl;
        return false;
    }

    // Load simulation information from compartment reports
    brion::CompartmentReport compartmentReport(
        brion::URI(bc.getReportSource(report).getPath()), brion::MODE_READ,
        gids);

    CircuitSimulationHandlerPtr simulationHandler(
        new CircuitSimulationHandler(_geometryParameters));
    scene.setSimulationHandler(simulationHandler);
    const std::string &cacheFile = _geometryParameters.getSimulationCacheFile();
    if (simulationHandler->attachSimulationToCacheFile(cacheFile))
        // Cache already exists, no need to create it.
        return true;

    BRAYNS_INFO << "Cache file does not exist, creating it" << std::endl;
    std::ofstream file(cacheFile, std::ios::out | std::ios::binary);

    if (!file.is_open())
    {
        BRAYNS_ERROR << "Failed to create cache file" << std::endl;
        return false;
    }

    // Load simulation information from compartment reports
    const float start = compartmentReport.getStartTime();
    const float end = compartmentReport.getEndTime();
    const float step = compartmentReport.getTimestep();

    const float firstFrame =
        std::max(start, _geometryParameters.getStartSimulationTime());
    const float lastFrame =
        std::min(end, _geometryParameters.getEndSimulationTime());
    const uint64_t frameSize = compartmentReport.getFrameSize();

    const uint64_t nbFrames = (lastFrame - firstFrame) / step;

    BRAYNS_INFO
        << "Loading values from compartment report and saving them to cache"
        << std::endl;

    // Write header
    simulationHandler->setNbFrames(nbFrames);
    simulationHandler->setFrameSize(frameSize);
    simulationHandler->writeHeader(file);

    // Write body
    for (uint64_t frame = 0; frame < nbFrames; ++frame)
    {
        BRAYNS_PROGRESS(frame, nbFrames);
        const float frameTime = firstFrame + step * frame;
        const brion::floatsPtr &valuesPtr =
            compartmentReport.loadFrame(frameTime);
        const floats &values = *valuesPtr;
        simulationHandler->writeFrame(file, values);
    }
    file.close();

    BRAYNS_INFO << "----------------------------------------" << std::endl;
    BRAYNS_INFO << "Cache file successfully created" << std::endl;
    BRAYNS_INFO << "Number of frames: " << nbFrames << std::endl;
    BRAYNS_INFO << "Frame size      : " << frameSize << std::endl;
    BRAYNS_INFO << "----------------------------------------" << std::endl;
    return true;
}

#else

bool MorphologyLoader::importMorphology(const servus::URI &, const int, Scene &)
{
    BRAYNS_ERROR << "Brion is required to load morphologies" << std::endl;
    return false;
}

bool MorphologyLoader::importCircuit(const servus::URI &, const std::string &,
                                     Scene &)
{
    BRAYNS_ERROR << "Brion is required to load circuits" << std::endl;
    return false;
}

bool MorphologyLoader::importCircuit(const servus::URI &, const std::string &,
                                     const std::string &, Scene &)
{
    BRAYNS_ERROR << "Brion is required to load circuits" << std::endl;
    return false;
}

bool MorphologyLoader::importSimulationData(const servus::URI &,
                                            const std::string &,
                                            const std::string &, Scene &)
{
    BRAYNS_ERROR << "Brion is required to load circuits" << std::endl;
    return false;
}

#endif

size_t MorphologyLoader::_getMaterialFromSectionType(
    const size_t morphologyIndex, const size_t sectionType)
{
    size_t material;
    switch (_geometryParameters.getColorScheme())
    {
    case ColorScheme::neuron_by_id:
        material = morphologyIndex % (NB_MAX_MATERIALS - NB_SYSTEM_MATERIALS);
        break;
    case ColorScheme::neuron_by_segment_type:
        material = sectionType % (NB_MAX_MATERIALS - NB_SYSTEM_MATERIALS);
        break;
    default:
        material = 0;
    }
    return material;
}
}
