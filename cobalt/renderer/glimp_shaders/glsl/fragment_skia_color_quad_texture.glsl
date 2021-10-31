#version 100

#extension GL_OES_standard_derivatives : require
uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform mediump vec2 uDstTextureUpperLeft_Stage1;
uniform mediump vec2 uDstTextureCoordScale_Stage1;
uniform sampler2D uDstTextureSampler_Stage1;
varying mediump vec4 vQuadEdge_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputColor_Stage0;
    mediump vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vinColor_Stage0;
        mediump float edgeAlpha;
        mediump vec2 duvdx = dFdx(vQuadEdge_Stage0.xy);
        mediump vec2 duvdy = -dFdy(vQuadEdge_Stage0.xy);
        if (vQuadEdge_Stage0.z > 0.0 && vQuadEdge_Stage0.w > 0.0) {
            edgeAlpha = min(min(vQuadEdge_Stage0.z, vQuadEdge_Stage0.w) + 0.5, 1.0);
        } else {
            mediump vec2 gF = vec2((2.0 * vQuadEdge_Stage0.x) * duvdx.x - duvdx.y, (2.0 * vQuadEdge_Stage0.x) * duvdy.x - duvdy.y);
            edgeAlpha = vQuadEdge_Stage0.x * vQuadEdge_Stage0.x - vQuadEdge_Stage0.y;
            edgeAlpha = clamp(0.5 - edgeAlpha / length(gF), 0.0, 1.0);
        }
        outputCoverage_Stage0 = vec4(edgeAlpha);
    }
    {
        if (all(lessThanEqual(outputCoverage_Stage0.xyz, vec3(0.0)))) {
            discard;
        }
        mediump vec2 _dstTexCoord = (sk_FragCoord.xy - uDstTextureUpperLeft_Stage1) * uDstTextureCoordScale_Stage1;
        _dstTexCoord.y = 1.0 - _dstTexCoord.y;
        mediump vec4 _dstColor = texture2D(uDstTextureSampler_Stage1, _dstTexCoord);
        gl_FragColor.w = outputColor_Stage0.w + (1.0 - outputColor_Stage0.w) * _dstColor.w;
        gl_FragColor.xyz = ((1.0 - outputColor_Stage0.w) * _dstColor.xyz + (1.0 - _dstColor.w) * outputColor_Stage0.xyz) + outputColor_Stage0.xyz * _dstColor.xyz;
        gl_FragColor = outputCoverage_Stage0 * gl_FragColor + (vec4(1.0) - outputCoverage_Stage0) * _dstColor;
    }
}
