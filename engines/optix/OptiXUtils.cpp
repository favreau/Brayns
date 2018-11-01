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

#include "OptiXUtils.h"

#include <brayns/common/log.h>

namespace brayns
{
void setOptiXProperties(::optix::Context context, const PropertyMap& properties)
{
    try
    {
        for (const auto& property : properties.getProperties())
        {
            switch (property->type)
            {
            case PropertyMap::Property::Type::Float:
            {
                context[property->name]->setFloat(
                    properties.getProperty<float>(property->name, 0.f));
                break;
            }
            case PropertyMap::Property::Type::Int:
                context[property->name]->setInt(
                    properties.getProperty<int>(property->name, 0));
                break;
            case PropertyMap::Property::Type::Bool:
                context[property->name]->setUint(
                    properties.getProperty<unsigned int>(property->name, 0));
                break;
            case PropertyMap::Property::Type::String:
                BRAYNS_THROW(std::runtime_error(
                    "String is not a supported type for OptiX variables"));
                break;
            case PropertyMap::Property::Type::Vec2f:
            {
                const auto& value =
                    properties.getProperty<Vector2f>(property->name, {0, 0});
                context[property->name]->setFloat(value.x(), value.y());
                break;
            }
            case PropertyMap::Property::Type::Vec2i:
            {
                const auto& value =
                    properties.getProperty<Vector2i>(property->name, {0, 0});
                context[property->name]->setInt(value.x(), value.y());
                break;
            }
            case PropertyMap::Property::Type::Vec3f:
            {
                const auto& value =
                    properties.getProperty<Vector3f>(property->name,
                                                     {0.f, 0.f, 0.f});
                context[property->name]->setFloat(value.x(), value.y(),
                                                  value.z());
                break;
            }
            case PropertyMap::Property::Type::Vec3i:
            {
                const auto& value =
                    properties.getProperty<Vector3i>(property->name, {0, 0, 0});
                context[property->name]->setInt(value.x(), value.y(),
                                                value.z());
                break;
            }
            case PropertyMap::Property::Type::Vec4f:
            {
                const auto& value =
                    properties.getProperty<Vector4f>(property->name,
                                                     {0.f, 0.f, 0.f, 0.f});
                context[property->name]->setFloat(value.x(), value.y(),
                                                  value.z(), value.w());
                break;
            }
            }
        }
    }
    catch (const std::exception& e)
    {
        BRAYNS_ERROR << "Failed to apply properties for OptiX " << e.what()
                     << std::endl;
    }
}
} // namespace brayns
