#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec4 ucircle_Stage1;
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
        float d;
        {
            d = (length((ucircle_Stage1.xy - sk_FragCoord.xy) * ucircle_Stage1.w) - 1.0) * ucircle_Stage1.z;
        }
        {
            d = clamp(d, 0.0, 1.0);
        }
        output_Stage1 = outputCoverage_Stage0 * d;
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
