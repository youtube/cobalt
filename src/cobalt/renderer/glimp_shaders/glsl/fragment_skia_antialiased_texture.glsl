#version 100

uniform highp float u_skRTHeight;
precision mediump float;
uniform vec3 uedges_Stage2[4];
uniform highp sampler2D uTextureSampler_0_Stage1;
varying mediump vec4 vcolor_Stage0;
varying highp vec2 vTransformedCoords_0_Stage0;

void main() {
  highp vec2 _sktmpCoord = gl_FragCoord.xy;
  highp vec4 sk_FragCoord =
      vec4(_sktmpCoord.x, u_skRTHeight - _sktmpCoord.y, 1.0, 1.0);

  vec4 outputColor_Stage0;
  {
    outputColor_Stage0 = vcolor_Stage0;
  }

  vec4 output_Stage1;
  {
    vec4 child;
    {
      child = texture2D(uTextureSampler_0_Stage1,
                        vTransformedCoords_0_Stage0).xyzw;
    }
    output_Stage1 = child * outputColor_Stage0.w;
  }
  
  vec4 output_Stage2;
  {
    float alpha = 1.0;
    float edge;
    edge = dot(uedges_Stage2[0], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
    edge = edge >= 0.5 ? 1.0 : 0.0;
    alpha *= edge;
    edge = dot(uedges_Stage2[1], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
    edge = edge >= 0.5 ? 1.0 : 0.0;
    alpha *= edge;
    edge = dot(uedges_Stage2[2], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
    edge = edge >= 0.5 ? 1.0 : 0.0;
    alpha *= edge;
    edge = dot(uedges_Stage2[3], vec3(sk_FragCoord.x, sk_FragCoord.y, 1.0));
    edge = edge >= 0.5 ? 1.0 : 0.0;
    alpha *= edge;
    output_Stage2 = vec4(alpha);
  }

  {
    gl_FragColor = output_Stage1 * output_Stage2;
  }
}
