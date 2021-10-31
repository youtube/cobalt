#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uColor_Stage0;
uniform sampler2D uTextureSampler_0_Stage0;
varying highp vec2 vTextureCoords_Stage0;
varying highp float vTexIndex_Stage0;
void main() {
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = uColor_Stage0;
        mediump vec4 texColor;
        {
            texColor = texture2D(uTextureSampler_0_Stage0, vTextureCoords_Stage0);
        }
        outputColor_Stage0 = outputColor_Stage0 * texColor;
    }
    {
        gl_FragColor = outputColor_Stage0;
    }
}
