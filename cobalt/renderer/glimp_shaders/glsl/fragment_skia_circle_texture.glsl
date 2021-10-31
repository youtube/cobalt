#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform highp vec4 ucircle_Stage1;
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
        mediump float d;
        {
            d = (1.0 - length((ucircle_Stage1.xy - sk_FragCoord.xy) * ucircle_Stage1.w)) * ucircle_Stage1.z;
        }
        {
            output_Stage1 = vec4(clamp(d, 0.0, 1.0));
        }
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
