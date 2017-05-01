#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform vec4 uproxyRect_Stage0;
uniform float ucornerRadius_Stage0;
uniform float ublurRadius_Stage0;
uniform float uRTHeight;
uniform vec4 uinnerRect_Stage1;
uniform float uradiusPlusHalf_Stage1;
varying vec4 vColor;
void main() 
{
	vec4 fragCoordYDown = vec4(gl_FragCoord.x, uRTHeight - gl_FragCoord.y, 1.0, 1.0);
	vec4 output_Stage0;
	{
		// Stage 0: GrRRectBlur
		vec2 rectCenter = (uproxyRect_Stage0.xy + uproxyRect_Stage0.zw)/2.0;
		vec2 translatedFragPos = fragCoordYDown.xy - uproxyRect_Stage0.xy;
		float threshold = ucornerRadius_Stage0 + 2.0*ublurRadius_Stage0;
		vec2 middle = uproxyRect_Stage0.zw - uproxyRect_Stage0.xy - 2.0*threshold;
		if (translatedFragPos.x >= threshold && translatedFragPos.x < (middle.x+threshold)) 
		{
			translatedFragPos.x = threshold;
		}
		else if (translatedFragPos.x >= (middle.x + threshold)) 
		{
			translatedFragPos.x -= middle.x - 1.0;
		}
		if (translatedFragPos.y > threshold && translatedFragPos.y < (middle.y+threshold)) 
		{
			translatedFragPos.y = threshold;
		}
		else if (translatedFragPos.y >= (middle.y + threshold)) 
		{
			translatedFragPos.y -= middle.y - 1.0;
		}
		vec2 proxyDims = vec2(2.0*threshold+1.0);
		vec2 texCoord = translatedFragPos / proxyDims;
		output_Stage0 = (vColor * texture2D(uSampler0_Stage0, texCoord).aaaa);
	}
	vec4 output_Stage1;
	{
		// Stage 1: CircularRRect
		vec2 dxy0 = uinnerRect_Stage1.xy - fragCoordYDown.xy;
		vec2 dxy1 = fragCoordYDown.xy - uinnerRect_Stage1.zw;
		vec2 dxy = max(max(dxy0, dxy1), 0.0);
		float alpha = clamp(uradiusPlusHalf_Stage1 - length(dxy), 0.0, 1.0);
		alpha = 1.0 - alpha;
		output_Stage1 = (output_Stage0 * alpha);
	}
	gl_FragColor = output_Stage1;
}
