#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uColor_Stage0;
void main() {
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = uColor_Stage0;
    }
    {
        gl_FragColor = outputColor_Stage0;
    }
}
