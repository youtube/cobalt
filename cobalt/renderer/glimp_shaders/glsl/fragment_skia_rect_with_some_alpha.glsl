#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec4 urect_Stage1;
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
        float xSub, ySub;
        xSub = min(sk_FragCoord.x - urect_Stage1.x, 0.0);
        xSub += min(urect_Stage1.z - sk_FragCoord.x, 0.0);
        ySub = min(sk_FragCoord.y - urect_Stage1.y, 0.0);
        ySub += min(urect_Stage1.w - sk_FragCoord.y, 0.0);
        float alpha = (1.0 + max(xSub, -1.0)) * (1.0 + max(ySub, -1.0));
        alpha = 1.0 - alpha;
        output_Stage1 = vec4(alpha);
    }
    {
        gl_FragColor = outputColor_Stage0 * output_Stage1;
    }
}
