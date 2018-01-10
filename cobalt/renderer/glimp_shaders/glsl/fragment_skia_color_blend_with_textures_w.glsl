#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec4 uTexDom_Stage1_c0;
uniform vec4 ucircle_Stage2;
uniform highp sampler2D uTextureSampler_0_Stage1;
varying mediump vec4 vcolor_Stage0;
varying highp vec2 vTransformedCoords_0_Stage0;
void main() {
highp     vec2 _sktmpCoord = gl_FragCoord.xy;
highp     vec4 sk_FragCoord = vec4(_sktmpCoord.x, u_skRTHeight - _sktmpCoord.y, 1.0, 1.0);
    vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    vec4 output_Stage1;
    {
        vec4 child;
        {
            child = texture2D(uTextureSampler_0_Stage1, clamp(vTransformedCoords_0_Stage0, uTexDom_Stage1_c0.xy, uTexDom_Stage1_c0.zw));
        }
        output_Stage1 = child * outputColor_Stage0.w;
    }
    vec4 output_Stage2;
    {
        float d;
        {
            d = (1.0 - length((ucircle_Stage2.xy - sk_FragCoord.xy) * ucircle_Stage2.w)) * ucircle_Stage2.z;
        }
        {
            d = clamp(d, 0.0, 1.0);
        }
        output_Stage2 = vec4(d);
    }
    {
        gl_FragColor = output_Stage1 * output_Stage2;
    }
}
