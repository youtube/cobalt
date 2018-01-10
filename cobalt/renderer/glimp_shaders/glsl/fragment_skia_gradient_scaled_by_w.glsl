#version 100

precision mediump float;
uniform float uGradientYCoordFS_Stage1_c0;
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
        vec4 child;
        {
            vec2 coord = vec2(vTransformedCoords_0_Stage0.x, uGradientYCoordFS_Stage1_c0);
            child = texture2D(uTextureSampler_0_Stage1, coord);
        }
        output_Stage1 = child * outputColor_Stage0.w;
    }
    {
        gl_FragColor = output_Stage1;
    }
}
