#version 100

precision mediump float;
varying mediump vec4 vcolor_Stage0;
void main() {
    vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    {
        gl_FragColor = outputColor_Stage0;
    }
}
