#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uleftBorderColor_Stage1_c0_c0;
uniform mediump vec4 urightBorderColor_Stage1_c0_c0;
uniform sampler2D uTextureSampler_0_Stage1;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
varying highp float vcoverage_Stage0;
mediump vec4 stage_Stage1_c0_c0_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1099_c0_c0;
    mediump float t = vTransformedCoords_0_Stage0.x + 9.999999747378752e-06;
    _sample1099_c0_c0 = vec4(t, 1.0, 0.0, 0.0);
    return _sample1099_c0_c0;
}
mediump vec4 stage_Stage1_c0_c0_c1_c0(mediump vec4 _input) {
    mediump vec4 _sample1767_c0_c0;
    mediump vec2 coord = vec2(_input.x, 0.5);
    _sample1767_c0_c0 = texture2D(uTextureSampler_0_Stage1, coord);
    return _sample1767_c0_c0;
}
mediump vec4 stage_Stage1_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1992;
    mediump vec4 _sample1099_c0_c0;
    _sample1099_c0_c0 = stage_Stage1_c0_c0_c0_c0(vec4(1.0));
    mediump vec4 t = _sample1099_c0_c0;
    if (t.x < 0.0) {
        _sample1992 = uleftBorderColor_Stage1_c0_c0;
    } else if (t.x > 1.0) {
        _sample1992 = urightBorderColor_Stage1_c0_c0;
    } else {
        mediump vec4 _sample1767_c0_c0;
        _sample1767_c0_c0 = stage_Stage1_c0_c0_c1_c0(t);
        _sample1992 = _sample1767_c0_c0;
    }
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
