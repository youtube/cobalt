#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uTexDom_Stage1;
uniform mediump vec2 uDstTextureUpperLeft_Stage2;
uniform mediump vec2 uDstTextureCoordScale_Stage2;
uniform sampler2D uTextureSampler_0_Stage1;
uniform sampler2D uDstTextureSampler_Stage2;
varying highp vec2 vTransformedCoords_0_Stage0;
varying highp float vcoverage_Stage0;
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputCoverage_Stage0;
    {
        highp float coverage = vcoverage_Stage0;
        outputCoverage_Stage0 = vec4(coverage);
    }
    mediump vec4 output_Stage1;
    {
        {
            highp vec2 origCoord = vTransformedCoords_0_Stage0;
            highp vec2 clampedCoord = clamp(origCoord, uTexDom_Stage1.xy, uTexDom_Stage1.zw);
            mediump vec4 inside = texture2D(uTextureSampler_0_Stage1, clampedCoord);
            output_Stage1 = inside;
        }
    }
    {
        if (all(lessThanEqual(outputCoverage_Stage0.xyz, vec3(0.0)))) {
            discard;
        }
        mediump vec2 _dstTexCoord = (sk_FragCoord.xy - uDstTextureUpperLeft_Stage2) * uDstTextureCoordScale_Stage2;
        _dstTexCoord.y = 1.0 - _dstTexCoord.y;
        mediump vec4 _dstColor = texture2D(uDstTextureSampler_Stage2, _dstTexCoord);
        gl_FragColor = output_Stage1;
        gl_FragColor = outputCoverage_Stage0 * gl_FragColor + (vec4(1.0) - outputCoverage_Stage0) * _dstColor;
    }
}
