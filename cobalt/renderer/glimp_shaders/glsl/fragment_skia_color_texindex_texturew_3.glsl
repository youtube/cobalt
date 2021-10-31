#version 100

#extension GL_OES_standard_derivatives : require
precision mediump float;
precision mediump sampler2D;
uniform sampler2D uTextureSampler_0_Stage0;
varying mediump vec4 vinColor_Stage0;
varying highp vec2 vTextureCoords_Stage0;
varying highp float vTexIndex_Stage0;
varying highp vec2 vIntTextureCoords_Stage0;
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
        mediump vec2 dist_grad = vec2(dFdx(distance), -dFdy(distance));
        mediump float dg_len2 = dot(dist_grad, dist_grad);
        if (dg_len2 < 9.999999747378752e-05) {
            dist_grad = vec2(0.707099974155426, 0.707099974155426);
        } else {
            dist_grad = dist_grad * inversesqrt(dg_len2);
        }
        mediump vec2 Jdx = dFdx(vIntTextureCoords_Stage0);
        mediump vec2 Jdy = -dFdy(vIntTextureCoords_Stage0);
        mediump vec2 grad = vec2(dist_grad.x * Jdx.x + dist_grad.y * Jdy.x, dist_grad.x * Jdx.y + dist_grad.y * Jdy.y);
        afwidth = 0.6499999761581421 * length(grad);
        mediump float val = smoothstep(-afwidth, afwidth, distance);
        outputCoverage_Stage0 = vec4(val);
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
