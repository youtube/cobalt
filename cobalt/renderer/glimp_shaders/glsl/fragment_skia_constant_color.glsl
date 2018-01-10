#version 100

precision mediump float;
uniform vec4 uColor_Stage0;
void main() {
    vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = uColor_Stage0;
    }
    {
        gl_FragColor = outputColor_Stage0;
    }
}
