#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec4 ucircleData_Stage1;
uniform vec4 ucircle_Stage2;
uniform highp sampler2D uTextureSampler_0_Stage1;
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
        vec2 vec = vec2((sk_FragCoord.x - ucircleData_Stage1.x) * ucircleData_Stage1.w, (sk_FragCoord.y - ucircleData_Stage1.y) * ucircleData_Stage1.w);
        float dist = length(vec) + (0.5 - ucircleData_Stage1.z) * ucircleData_Stage1.w;
        output_Stage1 = vec4(texture2D(uTextureSampler_0_Stage1, vec2(dist, 0.5)).wwww.w);
    }
    vec4 output_Stage2;
    {
        float d;
        {
            d = (length((ucircle_Stage2.xy - sk_FragCoord.xy) * ucircle_Stage2.w) - 1.0) * ucircle_Stage2.z;
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
