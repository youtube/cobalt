#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec4 uproxyRect_Stage1;
uniform float uprofileSize_Stage1;
uniform highp sampler2D uTextureSampler_0_Stage1;
varying mediump vec4 vcolor_Stage0;
void main() {
highp     vec2 _sktmpCoord = gl_FragCoord.xy;
highp     vec4 sk_FragCoord = vec4(_sktmpCoord.x, u_skRTHeight - _sktmpCoord.y, 1.0, 1.0);
    vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    vec4 output_Stage1;
    {
        vec2 translatedPos = sk_FragCoord.xy - uproxyRect_Stage1.xy;
        float width = uproxyRect_Stage1.z - uproxyRect_Stage1.x;
        float height = uproxyRect_Stage1.w - uproxyRect_Stage1.y;
        vec2 smallDims = vec2(width - uprofileSize_Stage1, height - uprofileSize_Stage1);
        float center = 2.0 * floor(uprofileSize_Stage1 / 2.0 + 0.25) - 1.0;
        vec2 wh = smallDims - vec2(center, center);
        float horiz_lookup;
        {
            float coord = (abs(translatedPos.x - 0.5 * width) - 0.5 * wh.x) / uprofileSize_Stage1;
            horiz_lookup = texture2D(uTextureSampler_0_Stage1, vec2(coord, 0.5)).wwww.w;
        }
        float vert_lookup;
        {
            float coord = (abs(translatedPos.y - 0.5 * height) - 0.5 * wh.y) / uprofileSize_Stage1;
            vert_lookup = texture2D(uTextureSampler_0_Stage1, vec2(coord, 0.5)).wwww.w;
        }
        float final = horiz_lookup * vert_lookup;
        output_Stage1 = vec4(final);
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
