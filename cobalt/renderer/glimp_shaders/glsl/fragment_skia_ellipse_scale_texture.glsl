#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform highp vec4 uellipse_Stage1;
uniform highp vec2 uscale_Stage1;
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
        highp vec2 d = sk_FragCoord.xy - uellipse_Stage1.xy;
        {
            d *= uscale_Stage1.y;
        }
        highp vec2 Z = d * uellipse_Stage1.zw;
        highp float implicit = dot(Z, d) - 1.0;
        highp float grad_dot = 4.0 * dot(Z, Z);
        {
            grad_dot = max(grad_dot, 6.1036000261083245e-05);
        }
        highp float approx_dist = implicit * inversesqrt(grad_dot);
        {
            approx_dist *= uscale_Stage1.x;
        }
        mediump float alpha;
        {
            alpha = clamp(0.5 - approx_dist, 0.0, 1.0);
        }
        output_Stage1 = vec4(alpha);
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
