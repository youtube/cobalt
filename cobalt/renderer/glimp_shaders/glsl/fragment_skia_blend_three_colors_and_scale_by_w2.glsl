#version 100

precision mediump float;
uniform vec4 uColors_Stage1_c0[3];
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
            float oneMinus2t = 1.0 - 2.0 * length(vTransformedCoords_0_Stage0);
            vec4 colorTemp = clamp(oneMinus2t, 0.0, 1.0) * uColors_Stage1_c0[0];
            colorTemp += (1.0 - min(abs(oneMinus2t), 1.0)) * uColors_Stage1_c0[1];
            colorTemp += clamp(-oneMinus2t, 0.0, 1.0) * uColors_Stage1_c0[2];
            child = colorTemp;
        }
        output_Stage1 = child * outputColor_Stage0.w;
    }
    {
        gl_FragColor = output_Stage1;
    }
}
