#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uTexDom_Stage1_c0_c0;
uniform mediump vec4 ucolor_Stage2_c1_c0;
uniform sampler2D uTextureSampler_0_Stage1;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
varying highp float vcoverage_Stage0;
mediump vec4 stage_Stage1_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1992;
    {
        highp vec2 origCoord = vTransformedCoords_0_Stage0;
        highp vec2 clampedCoord = clamp(origCoord, uTexDom_Stage1_c0_c0.xy, uTexDom_Stage1_c0_c0.zw);
        mediump vec4 inside = texture2D(uTextureSampler_0_Stage1, clampedCoord) * _input;
        _sample1992 = inside;
    }
    return _sample1992;
}
mediump vec4 stage_Stage2_c1_c0(mediump vec4 _input) {
    mediump vec4 child;
    {
        child = ucolor_Stage2_c1_c0;
    }
    return child;
}
void main() {
    mediump vec4 outputCoverage_Stage0;
    {
        highp float coverage = vcoverage_Stage0;
        outputCoverage_Stage0 = vec4(coverage);
    }
    mediump vec4 output_Stage1;
    {
        mediump vec4 _sample1992;
        _sample1992 = stage_Stage1_c0_c0(vec4(1.0, 1.0, 1.0, 1.0));
        output_Stage1 = _sample1992;
    }
    mediump vec4 output_Stage2;
    {
        mediump vec4 child;
        child = stage_Stage2_c1_c0(vec4(1.0));
        output_Stage2 = child * output_Stage1.w;
    }
    {
        gl_FragColor = output_Stage2 * outputCoverage_Stage0;
    }
}
