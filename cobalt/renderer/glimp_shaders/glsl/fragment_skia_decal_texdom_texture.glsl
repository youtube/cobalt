#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uTexDom_Stage1;
uniform mediump vec3 uDecalParams_Stage1;
uniform sampler2D uTextureSampler_0_Stage1;
varying highp vec2 vTransformedCoords_0_Stage0;
void main() {
    mediump vec4 output_Stage1;
    {
        {
            highp vec2 origCoord = vTransformedCoords_0_Stage0;
            highp vec2 clampedCoord = clamp(origCoord, uTexDom_Stage1.xy, uTexDom_Stage1.zw);
            mediump vec4 inside = texture2D(uTextureSampler_0_Stage1, clampedCoord);
            mediump float err = max(abs(clampedCoord.x - origCoord.x) * uDecalParams_Stage1.x, abs(clampedCoord.y - origCoord.y) * uDecalParams_Stage1.y);
            if (err > uDecalParams_Stage1.z) {
                err = 1.0;
            } else if (uDecalParams_Stage1.z < 1.0) {
                err = 0.0;
            }
            output_Stage1 = mix(inside, vec4(0.0, 0.0, 0.0, 0.0), err);
        }
    }
    {
        gl_FragColor = output_Stage1;
    }
}
