#version 100

precision mediump float;
precision mediump sampler2D;
varying highp vec4 vinCircleEdge_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    mediump vec4 outputColor_Stage0;
    mediump vec4 outputCoverage_Stage0;
    {
        highp vec4 circleEdge;
        circleEdge = vinCircleEdge_Stage0;
        outputColor_Stage0 = vinColor_Stage0;
        highp float d = length(circleEdge.xy);
        mediump float distanceToOuterEdge = circleEdge.z * (1.0 - d);
        mediump float edgeAlpha = clamp(distanceToOuterEdge, 0.0, 1.0);
        mediump float distanceToInnerEdge = circleEdge.z * (d - circleEdge.w);
        mediump float innerAlpha = clamp(distanceToInnerEdge, 0.0, 1.0);
        edgeAlpha *= innerAlpha;
        outputCoverage_Stage0 = vec4(edgeAlpha);
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
