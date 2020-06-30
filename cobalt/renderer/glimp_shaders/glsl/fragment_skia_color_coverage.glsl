#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uColor_Stage0;
varying mediump float vinCoverage_Stage0;
void main() {
    mediump vec4 outputColor_Stage0;
    mediump vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = uColor_Stage0;
        mediump float alpha = 1.0;
        alpha = vinCoverage_Stage0;
        outputCoverage_Stage0 = vec4(alpha);
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
