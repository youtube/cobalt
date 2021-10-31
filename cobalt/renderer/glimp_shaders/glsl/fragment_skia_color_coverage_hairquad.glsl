#version 100

#extension GL_OES_standard_derivatives : require
precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uColor_Stage0;
uniform mediump float uCoverage_Stage0;
varying mediump vec4 vHairQuadEdge_Stage0;
void main() {
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
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
