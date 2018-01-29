#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec4 urect_Stage1;
uniform vec4 urect_Stage2;
uniform vec4 uinnerRect_Stage3;
uniform vec2 uinvRadiiXY_Stage3;
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
        float xSub, ySub;
        xSub = min(sk_FragCoord.x - urect_Stage1.x, 0.0);
        xSub += min(urect_Stage1.z - sk_FragCoord.x, 0.0);
        ySub = min(sk_FragCoord.y - urect_Stage1.y, 0.0);
        ySub += min(urect_Stage1.w - sk_FragCoord.y, 0.0);
        float alpha = (1.0 + max(xSub, -1.0)) * (1.0 + max(ySub, -1.0));
        alpha = 1.0 - alpha;
        output_Stage1 = vec4(alpha);
    }
    vec4 output_Stage2;
    {
        float xSub, ySub;
        xSub = min(sk_FragCoord.x - urect_Stage2.x, 0.0);
        xSub += min(urect_Stage2.z - sk_FragCoord.x, 0.0);
        ySub = min(sk_FragCoord.y - urect_Stage2.y, 0.0);
        ySub += min(urect_Stage2.w - sk_FragCoord.y, 0.0);
        float alpha = (1.0 + max(xSub, -1.0)) * (1.0 + max(ySub, -1.0));
        output_Stage2 = output_Stage1 * alpha;
    }
    vec4 output_Stage3;
    {
        vec2 dxy0 = uinnerRect_Stage3.xy - sk_FragCoord.xy;
        vec2 dxy1 = sk_FragCoord.xy - uinnerRect_Stage3.zw;
        vec2 dxy = max(max(dxy0, dxy1), 0.0);
        vec2 Z = dxy * uinvRadiiXY_Stage3.xy;
        float implicit = dot(Z, dxy) - 1.0;
        float grad_dot = 4.0 * dot(Z, Z);
        grad_dot = max(grad_dot, 0.0001);
        float approx_dist = implicit * inversesqrt(grad_dot);
        float alpha = clamp(0.5 - approx_dist, 0.0, 1.0);
        output_Stage3 = output_Stage2 * alpha;
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage3;
    }
}
