#version 100

precision mediump float;
precision mediump sampler2D;
uniform sampler2D uTextureSampler_0_Stage0;
varying mediump vec4 vcolor_Stage0;
varying highp vec2 vlocalCoord_Stage0;
void main() {
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
        highp vec2 texCoord;
        texCoord = vlocalCoord_Stage0;
        outputColor_Stage0 = texture2D(uTextureSampler_0_Stage0, texCoord) * outputColor_Stage0;
    }
    {
        gl_FragColor = outputColor_Stage0;
    }
}
