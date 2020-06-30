#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform highp vec4 urectUniform_Stage1;
uniform sampler2D uTextureSampler_0_Stage0;
varying highp vec2 vTextureCoords_Stage0;
varying highp float vTexIndex_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputColor_Stage0;
    mediump vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vinColor_Stage0;
        mediump vec4 texColor;
        {
            texColor = texture2D(uTextureSampler_0_Stage0, vTextureCoords_Stage0).wwww;
        }
        outputCoverage_Stage0 = texColor;
    }
    mediump vec4 output_Stage1;
    {
        mediump float alpha;
        {
            mediump float xSub, ySub;
            xSub = min(sk_FragCoord.x - urectUniform_Stage1.x, 0.0);
            xSub += min(urectUniform_Stage1.z - sk_FragCoord.x, 0.0);
            ySub = min(sk_FragCoord.y - urectUniform_Stage1.y, 0.0);
            ySub += min(urectUniform_Stage1.w - sk_FragCoord.y, 0.0);
            alpha = (1.0 + max(xSub, -1.0)) * (1.0 + max(ySub, -1.0));
        }
        output_Stage1 = outputCoverage_Stage0 * alpha;
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
