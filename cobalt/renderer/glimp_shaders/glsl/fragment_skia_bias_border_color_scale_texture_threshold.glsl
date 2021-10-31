#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uleftBorderColor_Stage2_c1_c0_c0_c0;
uniform mediump vec4 urightBorderColor_Stage2_c1_c0_c0_c0;
uniform mediump float ubias_Stage2_c1_c0_c0_c0_c0_c0;
uniform mediump float uscale_Stage2_c1_c0_c0_c0_c0_c0;
uniform highp vec4 uscale01_Stage2_c1_c0_c0_c0_c1_c0;
uniform highp vec4 ubias01_Stage2_c1_c0_c0_c0_c1_c0;
uniform highp vec4 uscale23_Stage2_c1_c0_c0_c0_c1_c0;
uniform highp vec4 ubias23_Stage2_c1_c0_c0_c0_c1_c0;
uniform mediump float uthreshold_Stage2_c1_c0_c0_c0_c1_c0;
uniform sampler2D uTextureSampler_0_Stage1;
varying highp vec2 vTransformedCoords_0_Stage0;
varying highp vec2 vTransformedCoords_1_Stage0;
varying mediump vec4 vcolor_Stage0;
mediump vec4 stage_Stage1_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1992;
    _sample1992 = _input * texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0);
    return _sample1992;
}
mediump vec4 stage_Stage2_c1_c0_c0_c0_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1099_c1_c0_c0_c0;
    mediump float angle;
    {
        angle = atan(-vTransformedCoords_1_Stage0.y, -vTransformedCoords_1_Stage0.x);
    }
    mediump float t = ((angle * 0.15915493667125702 + 0.5) + ubias_Stage2_c1_c0_c0_c0_c0_c0) * uscale_Stage2_c1_c0_c0_c0_c0_c0;
    _sample1099_c1_c0_c0_c0 = vec4(t, 1.0, 0.0, 0.0);
    return _sample1099_c1_c0_c0_c0;
}
mediump vec4 stage_Stage2_c1_c0_c0_c0_c1_c0(mediump vec4 _input) {
    mediump vec4 _sample1767_c1_c0_c0_c0;
    mediump float t = _input.x;
    highp vec4 scale, bias;
    if (t < uthreshold_Stage2_c1_c0_c0_c0_c1_c0) {
        scale = uscale01_Stage2_c1_c0_c0_c0_c1_c0;
        bias = ubias01_Stage2_c1_c0_c0_c0_c1_c0;
    } else {
        scale = uscale23_Stage2_c1_c0_c0_c0_c1_c0;
        bias = ubias23_Stage2_c1_c0_c0_c0_c1_c0;
    }
    _sample1767_c1_c0_c0_c0 = t * scale + bias;
    return _sample1767_c1_c0_c0_c0;
}
mediump vec4 stage_Stage2_c1_c0_c0_c0(mediump vec4 _input) {
    mediump vec4 child_c1_c0;
    mediump vec4 _sample1099_c1_c0_c0_c0;
    _sample1099_c1_c0_c0_c0 = stage_Stage2_c1_c0_c0_c0_c0_c0(vec4(1.0));
    mediump vec4 t = _sample1099_c1_c0_c0_c0;
    if (t.x < 0.0) {
        child_c1_c0 = uleftBorderColor_Stage2_c1_c0_c0_c0;
    } else if (t.x > 1.0) {
        child_c1_c0 = urightBorderColor_Stage2_c1_c0_c0_c0;
    } else {
        mediump vec4 _sample1767_c1_c0_c0_c0;
        _sample1767_c1_c0_c0_c0 = stage_Stage2_c1_c0_c0_c0_c1_c0(t);
        child_c1_c0 = _sample1767_c1_c0_c0_c0;
    }
    {
        child_c1_c0.xyz *= child_c1_c0.w;
    }
    return child_c1_c0;
}
mediump vec4 stage_Stage2_c1_c0(mediump vec4 _input) {
    mediump vec4 child;
    mediump vec4 child_c1_c0;
    child_c1_c0 = stage_Stage2_c1_c0_c0_c0(vec4(1.0));
    child = child_c1_c0 * _input.w;
    return child;
}
void main() {
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
        output_Stage2 = vec4(child.w);
    }
    {
        gl_FragColor = output_Stage1 * output_Stage2;
    }
}
