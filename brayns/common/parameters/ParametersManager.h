/* Copyright (c) 2011-2016, EPFL/Blue Brain Project
 *                     Cyrille Favreau <cyrille.favreau@epfl.ch>
 *
 * This file is part of BRayns
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

#ifndef PARAMETERSMANAGER_H
#define PARAMETERSMANAGER_H

#include <brayns/api.h>
#include <brayns/common/types.h>
#include <boost/program_options.hpp>
#include <brayns/common/parameters/ApplicationParameters.h>
#include <brayns/common/parameters/RenderingParameters.h>
#include <brayns/common/parameters/GeometryParameters.h>
#include <brayns/common/parameters/SceneParameters.h>

namespace brayns
{

/**
   Class managing all parameters registered by the application. By default
   this class create an instance of Application, Rendering, and Geometry
   parameters are registered. Other parameters can also be added using the
   registerParameters method for as long as they are inherited from
   AbstractParameters.
 */
class ParametersManager
{
public:
    ParametersManager();

    /**
       Registers specific parameters to the manager
       @param parameters to be registered
     */
    BRAYNS_API void registerParameters( AbstractParameters* parameters );

    /**
       Displays help screen for registered parameters
     */
    BRAYNS_API void printHelp( );

    /**
       Displays values registered parameters
     */
    BRAYNS_API void print( );

    /**
       Parses registered parameters
       @param argc number of command line parameters
       @param argv actual command line parameters
     */
    BRAYNS_API void parse( int argc, const char **argv );

    /**
       Gets rendering parameters
       @return Rendering parameters for the current scene
    */
    BRAYNS_API RenderingParameters& getRenderingParameters();

    /**
       Gets geometry parameters
       @return Geometry parameters for the current scene
    */
    BRAYNS_API GeometryParameters& getGeometryParameters();

    /**
       Gets application parameters
       @return Application parameters for the current scene
    */
    BRAYNS_API ApplicationParameters& getApplicationParameters();

    /**
       Gets scene parameters
       @return Parameters for the current scene
    */
    BRAYNS_API SceneParameters& getSceneParameters();

private:
    std::vector< AbstractParameters* > _parameterSets;
    boost::program_options::options_description _parameters;
    ApplicationParameters _applicationParameters;
    RenderingParameters _renderingParameters;
    GeometryParameters _geometryParameters;
    SceneParameters _sceneParameters;
};

}
#endif // PARAMETERSMANAGER_H