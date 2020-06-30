#version 100

#extension GL_OES_standard_derivatives : require
uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uColor_Stage0;
uniform mediump float uCoverage_Stage0;
uniform mediump vec3 uedges_Stage1[4];
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
        mediump float alpha = 1.0;
        mediump float edge;
        edge = dot(uedges_Stage1[0], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = clamp(edge, 0.0, 1.0);
        alpha *= edge;
        edge = dot(uedges_Stage1[1], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = clamp(edge, 0.0, 1.0);
        alpha *= edge;
        edge = dot(uedges_Stage1[2], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = clamp(edge, 0.0, 1.0);
        alpha *= edge;
        edge = dot(uedges_Stage1[3], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = clamp(edge, 0.0, 1.0);
        alpha *= edge;
        output_Stage1 = outputCoverage_Stage0 * alpha;
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
