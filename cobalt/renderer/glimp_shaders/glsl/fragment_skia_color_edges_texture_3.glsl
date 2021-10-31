#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 ucolor_Stage2_c1_c0;
uniform mediump vec3 uedges_Stage3[4];
uniform sampler2D uTextureSampler_0_Stage1;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
mediump vec4 stage_Stage1_c0_c0(mediump vec4 _input) {
    mediump vec4 child;
    child = _input * texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0);
    return child;
}
mediump vec4 stage_Stage2_c1_c0(mediump vec4 _input) {
    mediump vec4 child;
    {
        child = ucolor_Stage2_c1_c0;
    }
    return child;
}
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    mediump vec4 output_Stage1;
    {
        mediump vec4 child;
        child = stage_Stage1_c0_c0(vec4(1.0));
        output_Stage1 = child * outputColor_Stage0.w;
    }
    mediump vec4 output_Stage2;
    {
        mediump vec4 child;
        child = stage_Stage2_c1_c0(vec4(1.0));
        output_Stage2 = child * output_Stage1.w;
    }
    mediump vec4 output_Stage3;
    {
        mediump float alpha = 1.0;
        mediump float edge;
        edge = dot(uedges_Stage3[0], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = clamp(edge, 0.0, 1.0);
        alpha *= edge;
        edge = dot(uedges_Stage3[1], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = clamp(edge, 0.0, 1.0);
        alpha *= edge;
        edge = dot(uedges_Stage3[2], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = clamp(edge, 0.0, 1.0);
        alpha *= edge;
        edge = dot(uedges_Stage3[3], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = clamp(edge, 0.0, 1.0);
        alpha *= edge;
        output_Stage3 = vec4(alpha);
    }
    {
        gl_FragColor = output_Stage2 * output_Stage3;
    }
}
