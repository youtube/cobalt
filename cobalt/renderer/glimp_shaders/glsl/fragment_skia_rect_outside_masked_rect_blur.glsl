#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform vec4 uproxyRect_Stage0;
uniform float uprofileSize_Stage0;
uniform float uRTHeight;
uniform vec4 urect_Stage1;
varying vec4 vColor;

void main() {
  vec4 fragCoordYDown = vec4(gl_FragCoord.x, uRTHeight - gl_FragCoord.y, 1.0, 1.0);
  
  vec4 output_Stage0;
  {
    // Stage 0: RectBlur
    vec4 src=vColor;
    vec2 translatedPos = fragCoordYDown.xy - uproxyRect_Stage0.xy;
    float width = uproxyRect_Stage0.z - uproxyRect_Stage0.x;
    float height = uproxyRect_Stage0.w - uproxyRect_Stage0.y;
    vec2 smallDims = vec2(width - uprofileSize_Stage0, height-uprofileSize_Stage0);
    float center = 2.0 * floor(uprofileSize_Stage0/2.0 + .25) - 1.0;
    vec2 wh = smallDims - vec2(center,center);

    float horiz_lookup;
    {
      float coord = (0.5 * (abs(2.0*translatedPos.x - width) - wh.x))/uprofileSize_Stage0;
      horiz_lookup = texture2D(uSampler0_Stage0, vec2(coord,0.5)).aaaa.a;
    }

    float vert_lookup;
    {
      float coord = (0.5 * (abs(2.0*translatedPos.y - height) - wh.y))/uprofileSize_Stage0;
      vert_lookup = texture2D(uSampler0_Stage0, vec2(coord,0.5)).aaaa.a;
    }

    float final = horiz_lookup * vert_lookup;
    output_Stage0 = src * vec4(final);
  }

  vec4 output_Stage1;
  {
    // Stage 1: AARect
    float xSub, ySub;
    xSub = min(fragCoordYDown.x - urect_Stage1.x, 0.0);
    xSub += min(urect_Stage1.z - fragCoordYDown.x, 0.0);
    ySub = min(fragCoordYDown.y - urect_Stage1.y, 0.0);
    ySub += min(urect_Stage1.w - fragCoordYDown.y, 0.0);
    float alpha = (1.0 + max(xSub, -1.0)) * (1.0 + max(ySub, -1.0));
    alpha = 1.0 - alpha;
    output_Stage1 = (output_Stage0 * alpha);
  }

  gl_FragColor = output_Stage1;
}

