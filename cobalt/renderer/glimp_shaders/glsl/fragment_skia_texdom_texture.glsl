#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uTexDom_Stage1;
uniform sampler2D uTextureSampler_0_Stage1;
varying highp vec2 vTransformedCoords_0_Stage0;
void main() {
    mediump vec4 output_Stage1;
    {
        {
            highp vec2 origCoord = vTransformedCoords_0_Stage0;
            highp vec2 clampedCoord = clamp(origCoord, uTexDom_Stage1.xy, uTexDom_Stage1.zw);
            mediump vec4 inside = texture2D(uTextureSampler_0_Stage1, clampedCoord);
            output_Stage1 = inside;
        }
    }
    {
        gl_FragColor = output_Stage1;
    }
}
