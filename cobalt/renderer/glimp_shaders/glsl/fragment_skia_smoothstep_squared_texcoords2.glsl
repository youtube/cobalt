#version 100

#extension GL_OES_standard_derivatives : require
precision mediump float;
uniform vec2 uTextureSize_Stage0;
uniform highp sampler2D uTextureSampler_0_Stage0;
varying highp vec2 vTextureCoords_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    vec4 outputColor_Stage0;
    vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vinColor_Stage0;
        highp vec2 uv = vTextureCoords_Stage0;
        float texColor = texture2D(uTextureSampler_0_Stage0, uv).wwww.x;
        float distance = 7.96875 * (texColor - 0.50196078431000002);
        highp vec2 st = uv * uTextureSize_Stage0;
        float afwidth;
        afwidth = abs(0.65000000000000002 * dFdy(st.y));
        float val = smoothstep(-afwidth, afwidth, distance);
        outputCoverage_Stage0 = vec4(val);
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
