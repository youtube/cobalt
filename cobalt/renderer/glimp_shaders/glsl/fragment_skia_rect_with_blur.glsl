#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec4 uColor_Stage0;
uniform vec4 uproxyRect_Stage1;
uniform float ucornerRadius_Stage1;
uniform float ublurRadius_Stage1;
uniform vec4 uinnerRect_Stage2;
uniform vec2 uradiusPlusHalf_Stage2;
uniform highp sampler2D uTextureSampler_0_Stage1;
void main() {
highp     vec2 _sktmpCoord = gl_FragCoord.xy;
highp     vec4 sk_FragCoord = vec4(_sktmpCoord.x, u_skRTHeight - _sktmpCoord.y, 1.0, 1.0);
    vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = uColor_Stage0;
    }
    vec4 output_Stage1;
    {
        vec2 translatedFragPos = sk_FragCoord.xy - uproxyRect_Stage1.xy;
        float threshold = ucornerRadius_Stage1 + 2.0 * ublurRadius_Stage1;
        vec2 middle = (uproxyRect_Stage1.zw - uproxyRect_Stage1.xy) - 2.0 * threshold;
        if (translatedFragPos.x >= threshold && translatedFragPos.x < middle.x + threshold) {
            translatedFragPos.x = threshold;
        } else if (translatedFragPos.x >= middle.x + threshold) {
            translatedFragPos.x -= middle.x - 1.0;
        }
        if (translatedFragPos.y > threshold && translatedFragPos.y < middle.y + threshold) {
            translatedFragPos.y = threshold;
        } else if (translatedFragPos.y >= middle.y + threshold) {
            translatedFragPos.y -= middle.y - 1.0;
        }
        vec2 proxyDims = vec2(2.0 * threshold + 1.0);
        vec2 texCoord = translatedFragPos / proxyDims;
        output_Stage1 = texture2D(uTextureSampler_0_Stage1, texCoord);
    }
    vec4 output_Stage2;
    {
        vec2 dxy0 = uinnerRect_Stage2.xy - sk_FragCoord.xy;
        vec2 dxy1 = sk_FragCoord.xy - uinnerRect_Stage2.zw;
        vec2 dxy = max(max(dxy0, dxy1), 0.0);
        float alpha = clamp(uradiusPlusHalf_Stage2.x - length(dxy), 0.0, 1.0);
        alpha = 1.0 - alpha;
        output_Stage2 = output_Stage1 * alpha;
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage2;
    }
}
