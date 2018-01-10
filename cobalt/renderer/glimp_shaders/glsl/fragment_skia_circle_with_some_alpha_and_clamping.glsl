#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec4 uinnerRect_Stage1;
uniform vec2 uradiusPlusHalf_Stage1;
varying highp vec4 vinCircleEdge_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
highp     vec2 _sktmpCoord = gl_FragCoord.xy;
highp     vec4 sk_FragCoord = vec4(_sktmpCoord.x, u_skRTHeight - _sktmpCoord.y, 1.0, 1.0);
    vec4 outputColor_Stage0;
    vec4 outputCoverage_Stage0;
    {
        highp vec4 circleEdge;
        circleEdge = vinCircleEdge_Stage0;
        outputColor_Stage0 = vinColor_Stage0;
        highp float d = length(circleEdge.xy);
        float distanceToOuterEdge = circleEdge.z * (1.0 - d);
        float edgeAlpha = clamp(distanceToOuterEdge, 0.0, 1.0);
        outputCoverage_Stage0 = vec4(edgeAlpha);
    }
    vec4 output_Stage1;
    {
        vec2 dxy0 = uinnerRect_Stage1.xy - sk_FragCoord.xy;
        vec2 dxy1 = sk_FragCoord.xy - uinnerRect_Stage1.zw;
        vec2 dxy = max(max(dxy0, dxy1), 0.0);
        float alpha = clamp(uradiusPlusHalf_Stage1.x - length(dxy), 0.0, 1.0);
        alpha = 1.0 - alpha;
        output_Stage1 = outputCoverage_Stage0 * alpha;
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
