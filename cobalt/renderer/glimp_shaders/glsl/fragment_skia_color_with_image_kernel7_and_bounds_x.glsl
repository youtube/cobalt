#version 100

precision mediump float;
uniform vec2 uImageIncrement_Stage1;
uniform vec2 uBounds_Stage1;
uniform vec4 uKernel_Stage1[7];
uniform highp sampler2D uTextureSampler_0_Stage1;
varying mediump vec4 vcolor_Stage0;
varying highp vec2 vTransformedCoords_0_Stage0;
void main() {
    vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    vec4 output_Stage1;
    {
        output_Stage1 = vec4(0.0, 0.0, 0.0, 0.0);
        vec2 coord = vTransformedCoords_0_Stage0 - 12.0 * uImageIncrement_Stage1;
        vec2 coordSampled = vec2(0.0, 0.0);
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[0].x;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[0].y;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[0].z;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[0].w;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[1].x;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[1].y;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[1].z;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[1].w;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[2].x;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[2].y;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[2].z;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[2].w;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[3].x;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[3].y;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[3].z;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[3].w;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[4].x;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[4].y;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[4].z;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[4].w;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[5].x;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[5].y;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[5].z;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[5].w;
        }
        coord += uImageIncrement_Stage1;
        coordSampled = coord;
        if (coord.x >= uBounds_Stage1.x && coord.x <= uBounds_Stage1.y) {
            output_Stage1 += texture2D(uTextureSampler_0_Stage1, coordSampled) * uKernel_Stage1[6].x;
        }
        coord += uImageIncrement_Stage1;
        output_Stage1 *= outputColor_Stage0;
    }
    {
        gl_FragColor = output_Stage1;
    }
}
