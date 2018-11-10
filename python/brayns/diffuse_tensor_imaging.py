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

class DiffuseTensorImaging():

    """ Diffuse Tensor Imaging """
    def __init__(self, client):
        """
        Create a new Diffuse Tensor Imaging instance
        """
        self._client = client

    def __str__(self):
        """Return a pretty-print of the class"""
        # pylint: disable=E1101
        return "Diffuse Tensor Imaging"

    def add_streamlines(self, name, streams, radius=1, opacity=1):
        """
        Adds streamlines to the scene. All streamlines are added into a single model
        :param name: Name of the model
        :param streams: Streamline points
        :param radius: Radius of the streamlines
        :param opacity: Opacity of the streamlines
        :return: Result of the request submission
        """
        count = 0
        indices = []
        points = []
        for stream in streams:
            for x in stream:
                if x is not None:
                    indices.append(count)
                    for y in x:
                        for z in y:
                            points.append(float(z))
                    count = count + len(x)

        ''' RPC call to Brayns '''
        params = dict()
        params['name'] = name
        params['indices'] = indices
        params['points'] = points
        params['radius'] = radius
        params['opacity'] = opacity
        return self._client.request("streamline", params=params)
