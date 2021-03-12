#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
varying mediump vec4 vcolor_Stage0;
varying highp float vcoverage_Stage0;
varying highp vec4 vgeomDomain_Stage0;
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputColor_Stage0;
    mediump vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
        highp float coverage = vcoverage_Stage0 * sk_FragCoord.w;
        highp vec4 geoDomain;
        geoDomain = vgeomDomain_Stage0;
        if (coverage < 0.5) {
            highp vec4 dists4 = clamp(vec4(1.0, 1.0, -1.0, -1.0) * (sk_FragCoord.xyxy - geoDomain), 0.0, 1.0);
            highp vec2 dists2 = dists4.xy * dists4.zw;
            coverage = min(coverage, dists2.x * dists2.y);
        }
        outputCoverage_Stage0 = vec4(coverage);
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
