#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
varying vec4 vColor;
varying vec2 vtextureCoords_Stage0;
void main()
{
  vec4 output_Stage0;
  {
    // Stage 0: Texture
    output_Stage0 = texture2D(uSampler0_Stage0, vtextureCoords_Stage0).aaaa;
  }
  gl_FragColor = (vColor * output_Stage0);
}
