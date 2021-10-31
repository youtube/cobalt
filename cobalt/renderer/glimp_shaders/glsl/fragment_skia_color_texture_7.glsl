#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump mat4 um_Stage2;
uniform mediump vec4 uv_Stage2;
uniform sampler2D uTextureSampler_0_Stage1;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
mediump vec4 stage_Stage1_c0_c0(mediump vec4 _input) {
    mediump vec4 child;
    child = _input * texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0);
    return child;
}
void main() {
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    mediump vec4 output_Stage1;
    {
        mediump vec4 child;
        child = stage_Stage1_c0_c0(vec4(1.0));
        output_Stage1 = child * outputColor_Stage0.w;
    }
    mediump vec4 output_Stage2;
    {
        mediump vec4 inputColor = output_Stage1;
        {
            mediump float nonZeroAlpha = max(inputColor.w, 9.9999997473787516e-05);
            inputColor = vec4(inputColor.xyz / nonZeroAlpha, nonZeroAlpha);
        }
        output_Stage2 = um_Stage2 * inputColor + uv_Stage2;
        {
            output_Stage2 = clamp(output_Stage2, 0.0, 1.0);
        }
        {
            output_Stage2.xyz *= output_Stage2.w;
        }
    }
    {
        gl_FragColor = output_Stage2;
    }
}
