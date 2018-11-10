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

class GraphExplorer():

    """ GraphExplorer """
    def __init__(self, client):
        """
        Create a new Graph instance
        """
        self._client = client

    def __str__(self):
        """Return a pretty-print of the class"""
        # pylint: disable=E1101
        return "Graphs"

    def load_positions_from_file(self, path, radius=1, scale=(1,1,1)):
        """
        Loads node positions from file
        :param path: Path of the file where positions are stored
        :param radius: Radius of the sphere used to represent the node
        :param scale: Scaling to use for the positions
        :return: Result of the request submission
        """
        xs = []
        ys = []
        zs = []
        with open(path) as f:
            for l in f:
                x, y, z = l.split()
                xs.append(float(x))
                ys.append(float(y))
                zs.append(float(z))

        params = dict()
        params['x'] = xs * scale[0]
        params['y'] = ys * scale[1]
        params['z'] = zs * scale[2]
        params['radius'] = radius
        self._client.request('positions', params=params)

    def create_random_connectivity(self, min_distance=0, max_distance=1e6, density=1):
        """
        Creates random connectivity between nodes
        :param min_distance: Minimum distance between nodes to be connected
        :param max_distance: Maximum distance between nodes to be connected
        :param density: Nodes to skip between every new connection
        :return: Result of the request submission
        """
        params = dict()
        params['minLength'] = min_distance
        params['maxLength'] = max_distance
        params['density'] = density
        return self._client.request('random-connectivity', params=params)

    def load_connectivity_from_file(self, path, matrix_id, dimension_range=(1,1e6), scale=(1,1,1)):
        """
        :param path: Path to the h5 file containing the connectivity data
        :param matrix_id: Id of the matrix used to create the connections
        :param dimension_range: Range of dimensions
        :return:
        """
        params = dict()
        params['filename'] = path
        params['matrixId'] = matrix_id
        params['minDimension'] = dimension_range[0]
        params['maxDimension'] = dimension_range[1]
        return self._client.request('connectivity', params=params)
