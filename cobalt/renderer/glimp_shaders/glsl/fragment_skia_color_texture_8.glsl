#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform mediump vec2 uDstTextureUpperLeft_Stage2;
uniform mediump vec2 uDstTextureCoordScale_Stage2;
uniform sampler2D uTextureSampler_0_Stage1;
uniform sampler2D uDstTextureSampler_Stage2;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
mediump vec4 stage_Stage1_c0_c0(mediump vec4 _input) {
    mediump vec4 child;
    child = _input * texture2D(uTextureSampler_0_Stage1, vTransformedCoords_0_Stage0);
    return child;
}
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputColor_Stage0;
    {
        outputColor_Stage0 = vcolor_Stage0;
    }
    mediump vec4 output_Stage1;
    {
        mediump vec4 child;
        child = stage_Stage1_c0_c0(vec4(1.0));
        output_Stage1 = child * outputColor_Stage0.w;
    }
    {
        if (all(lessThanEqual(vec4(1.0).xyz, vec3(0.0)))) {
            discard;
        }
        mediump vec2 _dstTexCoord = (sk_FragCoord.xy - uDstTextureUpperLeft_Stage2) * uDstTextureCoordScale_Stage2;
        _dstTexCoord.y = 1.0 - _dstTexCoord.y;
        mediump vec4 _dstColor = texture2D(uDstTextureSampler_Stage2, _dstTexCoord);
        gl_FragColor.w = output_Stage1.w + (1.0 - output_Stage1.w) * _dstColor.w;
        gl_FragColor.xyz = ((1.0 - output_Stage1.w) * _dstColor.xyz + (1.0 - _dstColor.w) * output_Stage1.xyz) + output_Stage1.xyz * _dstColor.xyz;
        gl_FragColor = gl_FragColor;
    }
}
