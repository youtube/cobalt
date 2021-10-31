#version 100

precision mediump float;
precision mediump sampler2D;
uniform sampler2D uTextureSampler_0_Stage1;
varying highp vec2 vTransformedCoords_0_Stage0;
void main() {
    mediump vec4 output_Stage1;
    {
        output_Stage1 = texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0);
    }
    {
        gl_FragColor = output_Stage1;
    }
}
