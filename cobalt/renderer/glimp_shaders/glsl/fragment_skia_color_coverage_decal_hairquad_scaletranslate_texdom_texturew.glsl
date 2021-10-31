#version 100

#extension GL_OES_standard_derivatives : require
uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uColor_Stage0;
uniform mediump float uCoverage_Stage0;
uniform mediump vec4 uscaleAndTranslate_Stage1;
uniform mediump vec4 uTexDom_Stage1;
uniform mediump vec3 uDecalParams_Stage1;
uniform sampler2D uTextureSampler_0_Stage1;
varying mediump vec4 vHairQuadEdge_Stage0;
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputColor_Stage0;
    mediump vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = uColor_Stage0;
        mediump float edgeAlpha;
        mediump vec2 duvdx = dFdx(vHairQuadEdge_Stage0.xy);
        mediump vec2 duvdy = -dFdy(vHairQuadEdge_Stage0.xy);
        mediump vec2 gF = vec2((2.0 * vHairQuadEdge_Stage0.x) * duvdx.x - duvdx.y, (2.0 * vHairQuadEdge_Stage0.x) * duvdy.x - duvdy.y);
        edgeAlpha = vHairQuadEdge_Stage0.x * vHairQuadEdge_Stage0.x - vHairQuadEdge_Stage0.y;
        edgeAlpha = sqrt((edgeAlpha * edgeAlpha) / dot(gF, gF));
        edgeAlpha = max(1.0 - edgeAlpha, 0.0);
        outputCoverage_Stage0 = vec4(uCoverage_Stage0 * edgeAlpha);
    }
    mediump vec4 output_Stage1;
    {
        mediump vec2 coords = sk_FragCoord.xy * uscaleAndTranslate_Stage1.xy + uscaleAndTranslate_Stage1.zw;
        {
            highp vec2 origCoord = coords;
            highp vec2 clampedCoord = clamp(origCoord, uTexDom_Stage1.xy, uTexDom_Stage1.zw);
            mediump vec4 inside = texture2D(uTextureSampler_0_Stage1, clampedCoord).wwww * outputCoverage_Stage0;
            mediump float err = max(abs(clampedCoord.x - origCoord.x) * uDecalParams_Stage1.x, abs(clampedCoord.y - origCoord.y) * uDecalParams_Stage1.y);
            if (err > uDecalParams_Stage1.z) {
                err = 1.0;
            } else if (uDecalParams_Stage1.z < 1.0) {
                err = 0.0;
            }
            output_Stage1 = mix(inside, vec4(0.0, 0.0, 0.0, 0.0), err);
        }
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
