#version 100

precision mediump float;
uniform vec4 uColors_Stage1_c0[2];
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
            vec4 colorTemp = mix(uColors_Stage1_c0[0], uColors_Stage1_c0[1], clamp(length(vTransformedCoords_0_Stage0), 0.0, 1.0));
            child = colorTemp;
        }
        output_Stage1 = child * outputColor_Stage0.w;
    }
    {
        gl_FragColor = output_Stage1;
    }
}
