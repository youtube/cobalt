#version 100

#extension GL_OES_standard_derivatives : require
precision mediump float;
uniform highp sampler2D uTextureSampler_0_Stage0;
varying mediump vec4 vinColor_Stage0;
varying highp vec2 vTextureCoords_Stage0;
varying highp vec2 vIntTextureCoords_Stage0;
void main() {
    vec4 outputColor_Stage0;
    vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vinColor_Stage0;
        highp vec2 uv = vTextureCoords_Stage0;
        float texColor = texture2D(uTextureSampler_0_Stage0, uv).wwww.x;
        float distance = 7.96875 * (texColor - 0.50196078431000002);
        float afwidth;
        vec2 dist_grad = vec2(dFdx(distance), dFdy(distance));
        float dg_len2 = dot(dist_grad, dist_grad);
        if (dg_len2 < 0.0001) {
            dist_grad = vec2(0.70709999999999995, 0.70709999999999995);
        } else {
            dist_grad = dist_grad * inversesqrt(dg_len2);
        }
        vec2 Jdx = dFdx(vIntTextureCoords_Stage0);
        vec2 Jdy = dFdy(vIntTextureCoords_Stage0);
        vec2 grad = vec2(dist_grad.x * Jdx.x + dist_grad.y * Jdy.x, dist_grad.x * Jdx.y + dist_grad.y * Jdy.y);
        afwidth = 0.65000000000000002 * length(grad);
        float val = smoothstep(-afwidth, afwidth, distance);
        outputCoverage_Stage0 = vec4(val);
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
