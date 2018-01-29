#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform highp vec4 uellipse_Stage2;
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
        output_Stage1 = texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0).xyzw;
    }
    vec4 output_Stage2;
    {
        vec2 d = sk_FragCoord.xy - uellipse_Stage2.xy;
        vec2 Z = d * uellipse_Stage2.zw;
        float implicit = dot(Z, d) - 1.0;
        float grad_dot = 4.0 * dot(Z, Z);
        grad_dot = max(grad_dot, 0.0001);
        float approx_dist = implicit * inversesqrt(grad_dot);
        float alpha;
        {
            alpha = clamp(0.5 + approx_dist, 0.0, 1.0);
        }
        output_Stage2 = output_Stage1 * alpha;
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage2;
    }
}
