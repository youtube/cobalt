#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform highp vec4 uinnerRect_Stage1;
uniform mediump vec2 uscale_Stage1;
uniform highp vec2 uinvRadiiXY_Stage1;
uniform sampler2D uTextureSampler_0_Stage0;
varying highp vec2 vlocalCoord_Stage0;
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vec4(1.0);
        highp vec2 texCoord;
        texCoord = vlocalCoord_Stage0;
        outputColor_Stage0 = texture2D(uTextureSampler_0_Stage0, texCoord);
    }
    mediump vec4 output_Stage1;
    {
        highp vec2 dxy0 = uinnerRect_Stage1.xy - sk_FragCoord.xy;
        highp vec2 dxy1 = sk_FragCoord.xy - uinnerRect_Stage1.zw;
        highp vec2 dxy = max(max(dxy0, dxy1), 0.0);
        dxy *= uscale_Stage1.y;
        highp vec2 Z = dxy * uinvRadiiXY_Stage1;
        mediump float implicit = dot(Z, dxy) - 1.0;
        mediump float grad_dot = 4.0 * dot(Z, Z);
        grad_dot = max(grad_dot, 9.9999997473787516e-05);
        mediump float approx_dist = implicit * inversesqrt(grad_dot);
        approx_dist *= uscale_Stage1.x;
        mediump float alpha = clamp(0.5 - approx_dist, 0.0, 1.0);
        output_Stage1 = vec4(alpha);
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
