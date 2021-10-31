#version 100

precision mediump float;
precision mediump sampler2D;
uniform sampler2D uTextureSampler_0_Stage1;
varying highp vec3 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
void main() {
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    mediump vec4 output_Stage1;
    {
        highp vec2 vTransformedCoords_0_Stage0_ensure2D = vTransformedCoords_0_Stage0.xy / vTransformedCoords_0_Stage0.z;
        output_Stage1 = texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0_ensure2D).wwww;
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
