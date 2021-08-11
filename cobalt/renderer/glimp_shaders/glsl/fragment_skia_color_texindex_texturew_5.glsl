#version 100

#extension GL_OES_standard_derivatives : require
precision mediump float;
precision mediump sampler2D;
uniform sampler2D uTextureSampler_0_Stage0;
varying highp vec2 vTextureCoords_Stage0;
varying highp float vTexIndex_Stage0;
varying highp vec2 vIntTextureCoords_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    mediump vec4 outputColor_Stage0;
    mediump vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vinColor_Stage0;
        highp vec2 uv = vTextureCoords_Stage0;
        mediump vec4 texColor;
        {
            texColor = texture2D(uTextureSampler_0_Stage0, uv).wwww;
        }
        mediump float distance = 7.96875 * (texColor.x - 0.501960813999176);
        mediump float afwidth;
        mediump float st_grad_len = length(-dFdy(vIntTextureCoords_Stage0));
        afwidth = abs(0.6499999761581421 * st_grad_len);
        mediump float val = smoothstep(-afwidth, afwidth, distance);
        outputCoverage_Stage0 = vec4(val);
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
