#version 100

precision mediump float;
precision mediump sampler2D;
uniform sampler2D uTextureSampler_0_Stage1;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
varying highp float vcoverage_Stage0;
mediump vec4 stage_Stage1_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1992;
    _sample1992 = _input * texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0);
    return _sample1992;
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
    {
        gl_FragColor = output_Stage1 * outputCoverage_Stage0;
    }
}
