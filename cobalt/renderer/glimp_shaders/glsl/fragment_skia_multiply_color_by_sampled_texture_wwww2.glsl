#version 100

precision mediump float;
uniform highp sampler2D uTextureSampler_0_Stage0;
varying highp vec2 vTextureCoords_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    vec4 outputColor_Stage0;
    vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vinColor_Stage0;
        outputCoverage_Stage0 = texture2D(uTextureSampler_0_Stage0, vTextureCoords_Stage0).wwww;
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
