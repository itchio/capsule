
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#pragma once

static const char* kVertexSource = R"glsl(
    #version 120
    attribute vec2 position;
    attribute vec2 texcoord;

    varying vec2 texcoordOut;

    void main() {
        texcoordOut = texcoord;
        gl_Position = vec4(position, 0.0, 1.0);
    }
)glsl";

static const char* kFragmentSource = R"glsl(
    #version 120
    varying vec2 texcoordOut;

    uniform sampler2D diffuse;
    void main() {
        gl_FragColor = texture2D(diffuse, texcoordOut);
        gl_FragColor.a = 0.4;
    }
)glsl";
