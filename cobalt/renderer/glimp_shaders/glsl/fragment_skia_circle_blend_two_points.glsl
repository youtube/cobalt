#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec4 ucircle_Stage1;
uniform vec4 ucircle_Stage2;
varying mediump vec4 vcolor_Stage0;
void main() {
highp     vec2 _sktmpCoord = gl_FragCoord.xy;
highp     vec4 sk_FragCoord = vec4(_sktmpCoord.x, u_skRTHeight - _sktmpCoord.y, 1.0, 1.0);
    vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    vec4 output_Stage1;
    {
        float d;
        {
            d = (length((ucircle_Stage1.xy - sk_FragCoord.xy) * ucircle_Stage1.w) - 1.0) * ucircle_Stage1.z;
        }
        {
            d = clamp(d, 0.0, 1.0);
        }
        output_Stage1 = vec4(d);
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
        output_Stage2 = output_Stage1 * d;
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage2;
    }
}
