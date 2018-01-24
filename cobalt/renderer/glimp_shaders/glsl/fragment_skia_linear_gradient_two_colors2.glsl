#version 100

precision mediump float;
uniform vec4 uColors_Stage1_c0[4];
uniform float uHardStopT_Stage1_c0;
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
            float clamp_t = clamp(vTransformedCoords_0_Stage0.x, 0.0, 1.0);
            vec4 start, end;
            float relative_t;
            if (clamp_t < uHardStopT_Stage1_c0) {
                start = uColors_Stage1_c0[0];
                end = uColors_Stage1_c0[1];
                relative_t = clamp_t / uHardStopT_Stage1_c0;
            } else {
                start = uColors_Stage1_c0[2];
                end = uColors_Stage1_c0[3];
                relative_t = (clamp_t - uHardStopT_Stage1_c0) / (1.0 - uHardStopT_Stage1_c0);
            }
            vec4 colorTemp = mix(start, end, relative_t);
            child = colorTemp;
        }
        output_Stage1 = child * outputColor_Stage0.w;
    }
    {
        gl_FragColor = output_Stage1;
    }
}
