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

#include "Scene.h"

#include <brayns/common/Transformation.h>
#include <brayns/common/log.h>
#include <brayns/common/material/Material.h>
#include <brayns/common/scene/ClipPlane.h>
#include <brayns/common/scene/Model.h>
#include <brayns/common/utils/Utils.h>
#include <brayns/parameters/ParametersManager.h>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <fstream>

namespace
{
const size_t CACHE_VERSION = 10;

template <typename T, typename U = T> // U seems to be needed when getID is a
                                      // member function of a base of T.
std::shared_ptr<T> _find(const std::vector<std::shared_ptr<T>>& list,
                         const size_t id,
                         size_t (U::*getID)() const = &T::getID)
{
    auto i = std::find_if(list.begin(), list.end(), [id, getID](auto x) {
        return id == ((*x).*getID)();
    });
    return i == list.end() ? std::shared_ptr<T>{} : *i;
}

template <typename T, typename U = T>
std::shared_ptr<T> _remove(std::vector<std::shared_ptr<T>>& list,
                           const size_t id,
                           size_t (U::*getID)() const = &T::getID)
{
    auto i = std::find_if(list.begin(), list.end(), [id, getID](auto x) {
        return id == ((*x).*getID)();
    });
    if (i == list.end())
        return std::shared_ptr<T>{};
    auto result = *i;
    list.erase(i);
    return result;
}
} // namespace

namespace brayns
{
Scene::Scene(ParametersManager& parametersManager)
    : _parametersManager(parametersManager)
{
}

Scene& Scene::operator=(const Scene& rhs)
{
    if (this == &rhs)
        return *this;

    {
        std::unique_lock<std::shared_timed_mutex> lock(_modelMutex);
        std::shared_lock<std::shared_timed_mutex> rhsLock(rhs._modelMutex);
        _modelDescriptors = rhs._modelDescriptors;
    }

    *_backgroundMaterial = *rhs._backgroundMaterial;
    _backgroundMaterial->markModified();

    _lights = rhs._lights;
    _clipPlanes = rhs._clipPlanes;

    _transferFunction = rhs._transferFunction;
    _transferFunction.markModified();

    markModified();
    return *this;
}

size_t Scene::getSizeInBytes() const
{
    auto lock = acquireReadAccess();
    size_t sizeInBytes = 0;
    for (auto modelDescriptor : _modelDescriptors)
        sizeInBytes += modelDescriptor->getModel().getSizeInBytes();
    return sizeInBytes;
}

size_t Scene::getNumModels() const
{
    auto lock = acquireReadAccess();
    return _modelDescriptors.size();
}

void Scene::addLight(LightPtr light)
{
    removeLight(light);
    _lights.push_back(light);
}

void Scene::removeLight(LightPtr light)
{
    Lights::iterator it = std::find(_lights.begin(), _lights.end(), light);
    if (it != _lights.end())
        _lights.erase(it);
}

LightPtr Scene::getLight(const size_t index)
{
    if (index < _lights.size())
        return _lights[index];
    return 0;
}

void Scene::clearLights()
{
    _lights.clear();
}

size_t Scene::addModel(ModelDescriptorPtr model)
{
    if (model->getModel().empty())
        throw std::runtime_error("Empty models not supported.");

    model->getModel().buildBoundingBox();
    model->getModel().commit();

    {
        std::unique_lock<std::shared_timed_mutex> lock(_modelMutex);
        model->setModelID(_modelID++);
        _modelDescriptors.push_back(model);

        // add default instance of this model to render something
        if (model->getInstances().empty())
            model->addInstance({true, true, model->getTransformation()});
    }

    return model->getModelID();
}

bool Scene::removeModel(const size_t id)
{
    std::unique_lock<std::shared_timed_mutex> lock(_modelMutex);
    auto model = _remove(_modelDescriptors, id, &ModelDescriptor::getModelID);
    if (model)
    {
        model->callOnRemoved();
        return true;
    }
    return false;
}

ModelDescriptorPtr Scene::getModel(const size_t id) const
{
    auto lock = acquireReadAccess();
    return _find(_modelDescriptors, id, &ModelDescriptor::getModelID);
}

bool Scene::empty() const
{
    auto lock = acquireReadAccess();
    for (auto modelDescriptor : _modelDescriptors)
        if (!modelDescriptor->getModel().empty())
            return false;
    return true;
}

size_t Scene::addClipPlane(const Plane& plane)
{
    auto clipPlane = std::make_shared<ClipPlane>(plane);
    clipPlane->onModified([&](const BaseObject&) { markModified(); });
    _clipPlanes.emplace_back(std::move(clipPlane));
    markModified();
    return _clipPlanes.back()->getID();
}

ClipPlanePtr Scene::getClipPlane(const size_t id) const
{
    return _find(_clipPlanes, id);
}

void Scene::removeClipPlane(const size_t id)
{
    if (_remove(_clipPlanes, id))
        markModified();
}

ModelDescriptorPtr Scene::loadModel(Blob&& blob, const size_t materialID,
                                    const ModelParams& params,
                                    Loader::UpdateCallback cb)
{
    auto loader = _loaderRegistry.createLoader(blob.type);
    loader->setProgressCallback(cb);
    auto modelDescriptor =
        loader->importFromBlob(std::move(blob), 0, materialID);
    if (!modelDescriptor)
        throw std::runtime_error("No model returned by loader");
    *modelDescriptor = params;
    addModel(modelDescriptor);
    markModified();
    return modelDescriptor;
}

ModelDescriptorPtr Scene::loadModel(const std::string& path,
                                    const size_t materialID,
                                    const ModelParams& params,
                                    Loader::UpdateCallback cb)
{
    ModelDescriptorPtr modelDescriptor;
    if (fs::is_directory(path))
    {
        fs::directory_iterator begin(path), end;
        const int numFiles =
            std::count_if(begin, end,
                          [& registry = _loaderRegistry](const auto& d) {
                              return !fs::is_directory(d.path()) &&
                                     registry.isSupported(d.path().string());
                          });

        if (numFiles == 0)
            throw std::runtime_error("No supported file found to load");

        float totalProgress = 0.f;

        size_t index = 0;
        for (const auto& i :
             boost::make_iterator_range(fs::directory_iterator(path), {}))
        {
            const auto& currentPath = i.path().string();
            if (fs::is_directory(i.path()) ||
                !_loaderRegistry.isSupported(currentPath))
            {
                continue;
            }
            auto loader = _loaderRegistry.createLoader(currentPath);

            auto progressCb = [cb, numFiles, totalProgress](auto msg,
                                                            auto amount) {
                cb(msg, totalProgress + (amount / numFiles));
            };

            loader->setProgressCallback(progressCb);
            modelDescriptor =
                loader->importFromFile(currentPath, index++, materialID);
            if (!modelDescriptor)
                throw std::runtime_error("No model returned by loader");
            *modelDescriptor = params;
            addModel(modelDescriptor);

            totalProgress += 1.f / numFiles;
        }
    }
    else
    {
        auto loader = _loaderRegistry.createLoader(path);
        loader->setProgressCallback(cb);
        modelDescriptor = loader->importFromFile(path, 0, materialID);
        if (!modelDescriptor)
            throw std::runtime_error("No model returned by loader");
        *modelDescriptor = params;
        addModel(modelDescriptor);
    }
    buildEnvironmentMap();
    markModified();
    return modelDescriptor;
}

void Scene::buildDefault()
{
    BRAYNS_INFO << "Building default Cornell Box scene" << std::endl;

    auto model = createModel();

    const Vector3f WHITE = {1.f, 1.f, 1.f};

    const Vector3f positions[8] = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, //    6--------7
        {0.f, 1.f, 0.f},                  //   /|       /|
        {1.f, 1.f, 0.f},                  //  2--------3 |
        {0.f, 0.f, 1.f},                  //  | |      | |
        {1.f, 0.f, 1.f},                  //  | 4------|-5
        {0.f, 1.f, 1.f},                  //  |/       |/
        {1.f, 1.f, 1.f}                   //  0--------1
    };

    const uint16_t indices[6][6] = {
        {5, 4, 6, 6, 7, 5}, // Front
        {7, 5, 1, 1, 3, 7}, // Right
        {3, 1, 0, 0, 2, 3}, // Back
        {2, 0, 4, 4, 6, 2}, // Left
        {0, 1, 5, 5, 4, 0}, // Bottom
        {7, 3, 2, 2, 6, 7}  // Top
    };

    const Vector3f colors[6] = {{0.8f, 0.8f, 0.8f}, {1.f, 0.f, 0.f},
                                {0.8f, 0.8f, 0.8f}, {0.f, 1.f, 0.f},
                                {0.8f, 0.8f, 0.8f}, {0.8f, 0.8f, 0.8f}};

    size_t materialId = 0;
    for (size_t i = 1; i < 6; ++i)
    {
        // Cornell box
        auto material =
            model->createMaterial(materialId,
                                  "wall_" + std::to_string(materialId));
        material->setDiffuseColor(colors[i]);
        material->setSpecularColor(WHITE);
        material->setSpecularExponent(10.f);
        material->setReflectionIndex(i == 4 ? 0.2f : 0.f);
        material->setGlossiness(i == 4 ? 0.9f : 1.f);
        material->setOpacity(1.f);

        auto& trianglesMesh = model->getTrianglesMeshes()[materialId];
        for (size_t j = 0; j < 6; ++j)
        {
            const auto position = positions[indices[i][j]];
            trianglesMesh.vertices.push_back(position);
        }
        trianglesMesh.indices.push_back(Vector3ui(0, 1, 2));
        trianglesMesh.indices.push_back(Vector3ui(3, 4, 5));
        ++materialId;
    }

    {
        // Sphere
        auto material = model->createMaterial(materialId, "sphere");
        material->setOpacity(0.2f);
        material->setRefractionIndex(1.5f);
        material->setReflectionIndex(0.1f);
        material->setDiffuseColor(WHITE);
        material->setSpecularColor(WHITE);
        material->setSpecularExponent(100.f);
        model->addSphere(materialId, {{0.25f, 0.26f, 0.30f}, 0.25f});
        ++materialId;
    }

    {
        // Cylinder
        auto material = model->createMaterial(materialId, "cylinder");
        material->setDiffuseColor({0.1f, 0.1f, 0.8f});
        material->setSpecularColor(WHITE);
        material->setSpecularExponent(10.f);
        model->addCylinder(materialId, {{0.25f, 0.126f, 0.75f},
                                        {0.75f, 0.126f, 0.75f},
                                        0.125f});
        ++materialId;
    }

    {
        // Cone
        auto material = model->createMaterial(materialId, "cone");
        material->setReflectionIndex(0.8f);
        material->setSpecularColor(WHITE);
        material->setSpecularExponent(10.f);
        model->addCone(materialId, {{0.75f, 0.01f, 0.25f},
                                    {0.75f, 0.5f, 0.25f},
                                    0.15f,
                                    0.f});
        ++materialId;
    }

    {
        // Lamp
        auto material = model->createMaterial(materialId, "lamp");
        material->setDiffuseColor(WHITE);
        material->setEmission(5.f);
        const Vector3f lampInfo = {0.15f, 0.99f, 0.15f};
        const Vector3f lampPositions[4] = {
            {0.5f - lampInfo.x(), lampInfo.y(), 0.5f - lampInfo.z()},
            {0.5f + lampInfo.x(), lampInfo.y(), 0.5f - lampInfo.z()},
            {0.5f + lampInfo.x(), lampInfo.y(), 0.5f + lampInfo.z()},
            {0.5f - lampInfo.x(), lampInfo.y(), 0.5f + lampInfo.z()}};
        auto& trianglesMesh = model->getTrianglesMeshes()[materialId];
        for (size_t i = 0; i < 4; ++i)
            trianglesMesh.vertices.push_back(lampPositions[i]);
        trianglesMesh.indices.push_back(Vector3i(2, 1, 0));
        trianglesMesh.indices.push_back(Vector3i(0, 3, 2));
    }

    addModel(
        std::make_shared<ModelDescriptor>(std::move(model), "DefaultScene"));
}

void Scene::setMaterialsColorMap(MaterialsColorMap colorMap)
{
    {
        auto lock = acquireReadAccess();
        for (auto modelDescriptors : _modelDescriptors)
            modelDescriptors->getModel().setMaterialsColorMap(colorMap);
    }
    markModified();
}

void Scene::_computeBounds()
{
    std::unique_lock<std::shared_timed_mutex> lock(_modelMutex);
    _bounds.reset();
    for (auto modelDescriptor : _modelDescriptors)
    {
        modelDescriptor->computeBounds();
        _bounds.merge(modelDescriptor->getBounds());
    }

    if (_bounds.isEmpty())
        // If no model is enabled. return empty bounding box
        _bounds.merge({0, 0, 0});
}

void Scene::buildEnvironmentMap()
{
    const auto& environmentMap =
        _parametersManager.getSceneParameters().getEnvironmentMap();
    if (!environmentMap.empty())
        _backgroundMaterial->setTexture(environmentMap, TextureType::diffuse);
}
} // namespace brayns
