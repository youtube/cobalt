#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec4 uscaleAndTranslate_Stage2;
uniform vec4 uTexDom_Stage2;
uniform highp sampler2D uTextureSampler_0_Stage1;
uniform highp sampler2D uTextureSampler_0_Stage2;
varying mediump vec4 vcolor_Stage0;
varying highp vec2 vTransformedCoords_0_Stage0;
void main() {
highp     vec2 _sktmpCoord = gl_FragCoord.xy;
highp     vec4 sk_FragCoord = vec4(_sktmpCoord.x, u_skRTHeight - _sktmpCoord.y, 1.0, 1.0);
    vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    vec4 output_Stage1;
    {
        output_Stage1 = texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0).wwww;
    }
    vec4 output_Stage2;
    {
        vec2 coords = sk_FragCoord.xy * uscaleAndTranslate_Stage2.xy + uscaleAndTranslate_Stage2.zw;
        {
            bvec4 outside;
            outside.xy = lessThan(coords, uTexDom_Stage2.xy);
            outside.zw = greaterThan(coords, uTexDom_Stage2.zw);
            output_Stage2 = any(outside) ? vec4(0.0, 0.0, 0.0, 0.0) : output_Stage1 * texture2D(uTextureSampler_0_Stage2, coords).wwww;
        }
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage2;
    }
}

