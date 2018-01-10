#version 100

precision mediump float;
varying mediump vec4 vcolor_Stage0;
varying mediump float vinCoverage_Stage0;
void main() {
    vec4 outputCoverage_Stage0;
    {
        float alpha = 1.0;
        alpha = vinCoverage_Stage0;
        outputCoverage_Stage0 = vec4(alpha);
    }
    {
        gl_FragColor = outputCoverage_Stage0;
    }
}
