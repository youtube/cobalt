#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 uscale01_Stage1_c0_c0_c0_c0_c1_c0;
uniform highp vec4 ubias01_Stage1_c0_c0_c0_c0_c1_c0;
uniform highp vec4 uscale23_Stage1_c0_c0_c0_c0_c1_c0;
uniform highp vec4 ubias23_Stage1_c0_c0_c0_c0_c1_c0;
uniform mediump float uthreshold_Stage1_c0_c0_c0_c0_c1_c0;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
mediump vec4 stage_Stage1_c0_c0_c0_c0_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample453_c0_c0_c0_c0;
    mediump float t = vTransformedCoords_0_Stage0.x + 9.999999747378752e-06;
    _sample453_c0_c0_c0_c0 = vec4(t, 1.0, 0.0, 0.0);
    return _sample453_c0_c0_c0_c0;
}
mediump vec4 stage_Stage1_c0_c0_c0_c0_c1_c0(mediump vec4 _input) {
    mediump vec4 _sample1464_c0_c0_c0_c0;
    mediump float t = _input.x;
    highp vec4 scale, bias;
    if (t < uthreshold_Stage1_c0_c0_c0_c0_c1_c0) {
        scale = uscale01_Stage1_c0_c0_c0_c0_c1_c0;
        bias = ubias01_Stage1_c0_c0_c0_c0_c1_c0;
    } else {
        scale = uscale23_Stage1_c0_c0_c0_c0_c1_c0;
        bias = ubias23_Stage1_c0_c0_c0_c0_c1_c0;
    }
    _sample1464_c0_c0_c0_c0 = t * scale + bias;
    return _sample1464_c0_c0_c0_c0;
}
mediump vec4 stage_Stage1_c0_c0_c0_c0(mediump vec4 _input) {
    mediump vec4 child_c0_c0;
    mediump vec4 _sample453_c0_c0_c0_c0;
    _sample453_c0_c0_c0_c0 = stage_Stage1_c0_c0_c0_c0_c0_c0(vec4(1.0));
    mediump vec4 t = _sample453_c0_c0_c0_c0;
    {
        {
            t.x = fract(t.x);
        }
        mediump vec4 _sample1464_c0_c0_c0_c0;
        _sample1464_c0_c0_c0_c0 = stage_Stage1_c0_c0_c0_c0_c1_c0(t);
        child_c0_c0 = _sample1464_c0_c0_c0_c0;
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
