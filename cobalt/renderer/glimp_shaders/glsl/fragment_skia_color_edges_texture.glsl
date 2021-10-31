#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform mediump vec3 uedges_Stage1[4];
uniform sampler2D uTextureSampler_0_Stage0;
varying mediump vec4 vcolor_Stage0;
varying highp vec2 vlocalCoord_Stage0;
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
        highp vec2 texCoord;
        texCoord = vlocalCoord_Stage0;
        outputColor_Stage0 = texture2D(uTextureSampler_0_Stage0, texCoord) * outputColor_Stage0;
    }
    mediump vec4 output_Stage1;
    {
        mediump float alpha = 1.0;
        mediump float edge;
        edge = dot(uedges_Stage1[0], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = edge >= 0.5 ? 1.0 : 0.0;
        alpha *= edge;
        edge = dot(uedges_Stage1[1], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = edge >= 0.5 ? 1.0 : 0.0;
        alpha *= edge;
        edge = dot(uedges_Stage1[2], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = edge >= 0.5 ? 1.0 : 0.0;
        alpha *= edge;
        edge = dot(uedges_Stage1[3], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
        edge = edge >= 0.5 ? 1.0 : 0.0;
        alpha *= edge;
        output_Stage1 = vec4(alpha);
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
