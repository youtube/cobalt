#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uleftBorderColor_Stage1_c0_c0_c0_c0;
uniform mediump vec4 urightBorderColor_Stage1_c0_c0_c0_c0;
uniform highp vec4 uscale0_1_Stage1_c0_c0_c0_c0_c1_c0;
uniform highp vec4 uscale2_3_Stage1_c0_c0_c0_c0_c1_c0;
uniform highp vec4 uscale4_5_Stage1_c0_c0_c0_c0_c1_c0;
uniform highp vec4 ubias0_1_Stage1_c0_c0_c0_c0_c1_c0;
uniform highp vec4 ubias2_3_Stage1_c0_c0_c0_c0_c1_c0;
uniform highp vec4 ubias4_5_Stage1_c0_c0_c0_c0_c1_c0;
uniform mediump vec4 uthresholds1_7_Stage1_c0_c0_c0_c0_c1_c0;
uniform mediump vec4 uthresholds9_13_Stage1_c0_c0_c0_c0_c1_c0;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
mediump vec4 stage_Stage1_c0_c0_c0_c0_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1099_c0_c0_c0_c0;
    mediump float t = vTransformedCoords_0_Stage0.x + 9.999999747378752e-06;
    _sample1099_c0_c0_c0_c0 = vec4(t, 1.0, 0.0, 0.0);
    return _sample1099_c0_c0_c0_c0;
}
mediump vec4 stage_Stage1_c0_c0_c0_c0_c1_c0(mediump vec4 _input) {
    mediump vec4 _sample1767_c0_c0_c0_c0;
    mediump float t = _input.x;
    highp vec4 scale, bias;
    {
        if (t < uthresholds1_7_Stage1_c0_c0_c0_c0_c1_c0.y) {
            if (t < uthresholds1_7_Stage1_c0_c0_c0_c0_c1_c0.x) {
                scale = uscale0_1_Stage1_c0_c0_c0_c0_c1_c0;
                bias = ubias0_1_Stage1_c0_c0_c0_c0_c1_c0;
            } else {
                scale = uscale2_3_Stage1_c0_c0_c0_c0_c1_c0;
                bias = ubias2_3_Stage1_c0_c0_c0_c0_c1_c0;
            }
        } else {
            {
                scale = uscale4_5_Stage1_c0_c0_c0_c0_c1_c0;
                bias = ubias4_5_Stage1_c0_c0_c0_c0_c1_c0;
            }
        }
    }
    _sample1767_c0_c0_c0_c0 = t * scale + bias;
    return _sample1767_c0_c0_c0_c0;
}
mediump vec4 stage_Stage1_c0_c0_c0_c0(mediump vec4 _input) {
    mediump vec4 child_c0_c0;
    mediump vec4 _sample1099_c0_c0_c0_c0;
    _sample1099_c0_c0_c0_c0 = stage_Stage1_c0_c0_c0_c0_c0_c0(vec4(1.0));
    mediump vec4 t = _sample1099_c0_c0_c0_c0;
    if (t.x < 0.0) {
        child_c0_c0 = uleftBorderColor_Stage1_c0_c0_c0_c0;
    } else if (t.x > 1.0) {
        child_c0_c0 = urightBorderColor_Stage1_c0_c0_c0_c0;
    } else {
        mediump vec4 _sample1767_c0_c0_c0_c0;
        _sample1767_c0_c0_c0_c0 = stage_Stage1_c0_c0_c0_c0_c1_c0(t);
        child_c0_c0 = _sample1767_c0_c0_c0_c0;
    }
    {
        child_c0_c0.xyz *= child_c0_c0.w;
    }
    return child_c0_c0;
}
mediump vec4 stage_Stage1_c0_c0(mediump vec4 _input) {
    mediump vec4 child;
    mediump vec4 child_c0_c0;
    child_c0_c0 = stage_Stage1_c0_c0_c0_c0(vec4(1.0));
    child = child_c0_c0 * _input.w;
    return child;
}
void main() {
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    mediump vec4 output_Stage1;
    {
        mediump vec4 child;
        child = stage_Stage1_c0_c0(vec4(1.0));
        output_Stage1 = vec4(child.w);
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
