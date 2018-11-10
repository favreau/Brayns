#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (c) 2016-2018, Blue Brain Project
#                          Cyrille Favreau <cyrille.favreau@epfl.ch>
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>
#
# This library is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License version 3.0 as published
# by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
# All rights reserved. Do not distribute without further notice.

class CircuitExplorer():

    """ Color schemes """
    CIRCUIT_COLOR_SCHEME_NONE = 0
    CIRCUIT_COLOR_SCHEME_NEURON_BY_ID = 1
    CIRCUIT_COLOR_SCHEME_NEURON_BY_TYPE = 2
    CIRCUIT_COLOR_SCHEME_NEURON_BY_LAYER = 3
    CIRCUIT_COLOR_SCHEME_NEURON_BY_MTYPE = 4
    CIRCUIT_COLOR_SCHEME_NEURON_BY_ETYPE = 5
    CIRCUIT_COLOR_SCHEME_NEURON_BY_TARGET = 6

    """ Morphology types """
    MORPHOLOGY_SECTION_TYPE_ALL = 255
    MORPHOLOGY_SECTION_TYPE_SOMA = 1
    MORPHOLOGY_SECTION_TYPE_AXON = 2
    MORPHOLOGY_SECTION_TYPE_DENTRITE = 4
    MORPHOLOGY_SECTION_TYPE_APICAL_DENDRITE = 8

    """ Geometry quality """
    GEOMETRY_QUALITY_LOW = 0
    GEOMETRY_QUALITY_MEDIUM = 1
    GEOMETRY_QUALITY_HIGH = 2

    """ Defaults """
    DEFAULT_RESPONSE_TIMEOUT = 360

    """ Shading modes """
    SHADING_MODE_NONE = 'none'
    SHADING_MODE_DIFFUSE = 'diffuse'
    SHADING_MODE_ELECTRON = 'electron'
    SHADING_MODE_CARTOON = 'cartoon'
    SHADING_MODE_ELECTRON_TRANSPARENCY = 'electron-transparency'

    """ Circuit Explorer """
    def __init__(self, client):
        """
        Create a new Circuit Explorer instance
        """
        self._client = client

    def __str__(self):
        """Return a pretty-print of the class"""
        # pylint: disable=E1101
        return "Circuit Explorer"

    def set_material(self,
            model_id, material_id, diffuse = (1,1,1), specular = (1,1,1), specular_exponent = 20, opacity = 1,
            reflection_index = 0.0, refraction_index = 1.0, cast_simulation = True, glossiness = 1, intensity = 1,
            shading_mode = SHADING_MODE_NONE, emission = 0):
        params = dict()
        params['modelId'] = model_id
        params['materialId'] = material_id
        params['diffuseColor'] = (diffuse[0] * intensity, diffuse[1] * intensity, diffuse[2] * intensity)
        params['specularColor'] = specular
        params['specularExponent'] = specular_exponent
        params['reflectionIndex'] = reflection_index
        params['opacity'] = opacity
        params['refractionIndex'] = refraction_index
        params['emission'] = emission
        params['glossiness'] = glossiness
        params['castSimulationData'] = cast_simulation
        params['shadingMode'] = shading_mode

        return  self._client.request("setMaterial", params=params, response_timeout=self.DEFAULT_RESPONSE_TIMEOUT)

    def set_circuit_attributes(self,
            aabb=(0,0,0,0,0,0), density=100, targets='', report='', color_scheme=CIRCUIT_COLOR_SCHEME_NONE,
            mesh_file_pattern = '', mesh_folder = '', mesh_transformation = False, use_simulation_model = False):
        params = dict()
        params['aabb'] = aabb
        params['density'] = density
        params['meshFilenamePattern'] = mesh_file_pattern
        params['meshFolder'] = mesh_folder
        params['meshTransformation'] = mesh_transformation
        params['targets'] = targets
        params['report'] = report
        params['randomSeed'] = 0
        params['colorScheme'] = color_scheme
        params['useSimulationModel'] = use_simulation_model

        """ unused """
        params['startSimulationTime'] = 0
        params['endSimulationTime'] = 0
        params['simulationStep'] = 0
        params['simulationValueRange'] = (-80, -10)
        params['simulationHistogramSize'] = 256

        return self._client.request('setCircuitAttributes', params=params, response_timeout=self.DEFAULT_RESPONSE_TIMEOUT)

    def set_morphology_attributes(self,
            section_types=[MORPHOLOGY_SECTION_TYPE_ALL], radius_multiplier=1, radius_correction=0,
            dampenBranchThicknessChangerate = True, useSDFGeometries=True, geometry_quality = GEOMETRY_QUALITY_HIGH):
        params = dict()
        params['radiusMultiplier'] = radius_multiplier
        params['radiusCorrection'] = radius_correction
        params['dampenBranchThicknessChangerate'] = dampenBranchThicknessChangerate
        params['useSDFGeometries'] = useSDFGeometries
        params['geometryQuality'] = geometry_quality
        st = 0
        for section_type in section_types:
            st = st + section_type
        params['sectionTypes'] = st

        """ Unused """
        params['colorScheme'] = 0
        params['realisticSoma'] = False
        params['metaballsSamplesFromSoma'] = 0
        params['metaballsGridSize'] = 0
        params['metaballsThreshold'] = 0
        params['useSimulationModel'] = False

        return self._client.request('setMorphologyAttributes', params=params, response_timeout=self.DEFAULT_RESPONSE_TIMEOUT)

    def set_transfer_function(self, palette, data_range=(0,255), contribution=1, intensity = 1, emission=(0,0,0)):
        ml  = self._client.transfer_function
        ml.diffuse = []
        ml.contribution = []
        ml.emission = []
        for color in palette:
            alpha = 1.0
            if len(color) == 4:
                alpha = color[3]
            ml.diffuse.append((color[0]*intensity,color[1]*intensity,color[2]*intensity,alpha))
            ml.contribution.append(contribution)
            ml.emission.append(emission)
        ml.range = data_range
        ml.commit()

    def load_from_cache(self, name, path):
        """
        Loads a model from a cache file
        :param name: Name of the loaded model
        :param path: Path of the cache file
        :return: Result of the request submission
        """
        params = dict()
        params['name'] = name
        params['path'] = path
        return self._client.request('loadModelFromCache', params=params, response_timeout=self.DEFAULT_RESPONSE_TIMEOUT)

    def save_to_cache(self, model_id, path):
        """
        Saves a model to the specified cache file
        :param model_id: Id of the model to save
        :param path: Path of the cache file
        :return: Result of the request submission
        """
        params = dict()
        params['modelId'] = model_id
        params['path'] = path
        return self._client.request('saveModelToCache', params=params, response_timeout=self.DEFAULT_RESPONSE_TIMEOUT)
