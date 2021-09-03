#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump float uweight_Stage2;
uniform mediump mat4 um_Stage2_c1_c0_c0_c0;
uniform mediump vec4 uv_Stage2_c1_c0_c0_c0;
uniform sampler2D uTextureSampler_0_Stage1;
uniform sampler2D uTextureSampler_0_Stage2;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
mediump vec4 stage_Stage1_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1992;
    _sample1992 = _input * texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0);
    return _sample1992;
}
mediump vec4 stage_Stage2_c1_c0_c0_c0(mediump vec4 _input) {
    mediump vec4 out0_c1_c0;
    mediump vec4 inputColor = _input;
    {
        mediump float nonZeroAlpha = max(inputColor.w, 9.999999747378752e-05);
        inputColor = vec4(inputColor.xyz / nonZeroAlpha, nonZeroAlpha);
    }
    out0_c1_c0 = um_Stage2_c1_c0_c0_c0 * inputColor + uv_Stage2_c1_c0_c0_c0;
    {
        out0_c1_c0 = clamp(out0_c1_c0, 0.0, 1.0);
    }
    {
        out0_c1_c0.xyz *= out0_c1_c0.w;
    }
    return out0_c1_c0;
}
mediump vec4 stage_Stage2_c1_c0_c1_c0(mediump vec4 _input) {
    mediump vec4 _sample1278;
    mediump float nonZeroAlpha = max(_input.w, 9.999999747378752e-05);
    mediump vec4 coord = vec4(_input.xyz / nonZeroAlpha, nonZeroAlpha);
    coord = coord * 0.9960939884185791 + vec4(0.0019529999699443579, 0.0019529999699443579, 0.0019529999699443579, 0.0019529999699443579);
    _sample1278.w = texture2D(uTextureSampler_0_Stage2, vec2(coord.w, 0.125)).w;
    _sample1278.x = texture2D(uTextureSampler_0_Stage2, vec2(coord.x, 0.375)).w;
    _sample1278.y = texture2D(uTextureSampler_0_Stage2, vec2(coord.y, 0.625)).w;
    _sample1278.z = texture2D(uTextureSampler_0_Stage2, vec2(coord.z, 0.875)).w;
    _sample1278.xyz *= _sample1278.w;
    return _sample1278;
}
mediump vec4 stage_Stage2_c1_c0(mediump vec4 _input) {
    mediump vec4 _sample1278;
    mediump vec4 out0_c1_c0;
    out0_c1_c0 = stage_Stage2_c1_c0_c0_c0(_input);
    _sample1278 = stage_Stage2_c1_c0_c1_c0(out0_c1_c0);
    return _sample1278;
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
        mediump vec4 _sample1278;
        _sample1278 = stage_Stage2_c1_c0(output_Stage1);
        mediump vec4 in0 = _sample1278;
        mediump vec4 in1 = output_Stage1;
        output_Stage2 = mix(in0, in1, uweight_Stage2);
    }
    {
        gl_FragColor = output_Stage2;
    }
}
