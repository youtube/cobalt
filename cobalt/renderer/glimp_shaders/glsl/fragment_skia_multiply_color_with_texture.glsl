#version 100

precision mediump float;
uniform vec4 uTexDom_Stage1;
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
        {
            bvec4 outside;
            outside.xy = lessThan(vTransformedCoords_0_Stage0, uTexDom_Stage1.xy);
            outside.zw = greaterThan(vTransformedCoords_0_Stage0, uTexDom_Stage1.zw);
            output_Stage1 = any(outside) ? vec4(0.0, 0.0, 0.0, 0.0) : outputColor_Stage0 * texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0);
        }
    }
    {
        gl_FragColor = output_Stage1;
    }
}
