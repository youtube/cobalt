#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uscaleAndTranslate_Stage2;
uniform mediump vec4 uTexDom_Stage2;
uniform mediump vec3 uDecalParams_Stage2;
uniform mediump vec3 uedges_Stage3[4];
uniform sampler2D uTextureSampler_0_Stage1;
uniform sampler2D uTextureSampler_0_Stage2;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    mediump vec4 output_Stage1;
    {
        output_Stage1 = texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0).wwww;
    }
    mediump vec4 output_Stage2;
    {
        mediump vec2 coords = sk_FragCoord.xy * uscaleAndTranslate_Stage2.xy + uscaleAndTranslate_Stage2.zw;
        {
            highp vec2 origCoord = coords;
            highp vec2 clampedCoord = clamp(origCoord, uTexDom_Stage2.xy, uTexDom_Stage2.zw);
            mediump vec4 inside = texture2D(uTextureSampler_0_Stage2, clampedCoord).wwww * output_Stage1;
            mediump float err = max(abs(clampedCoord.x - origCoord.x) * uDecalParams_Stage2.x, abs(clampedCoord.y - origCoord.y) * uDecalParams_Stage2.y);
            if (err > uDecalParams_Stage2.z) {
                err = 1.0;
            } else if (uDecalParams_Stage2.z < 1.0) {
                err = 0.0;
            }
            output_Stage2 = mix(inside, vec4(0.0, 0.0, 0.0, 0.0), err);
        }
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
        output_Stage3 = output_Stage2 * alpha;
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage3;
    }
}
