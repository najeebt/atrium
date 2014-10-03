struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
	float3 normal : NORMAL0;
	float4 lightSpacePos : POSITION1;
	float3 lRay : NORMAL1;
	float3 rpos : POSITION2;
};

cbuffer SceneConstants : register(b1)
{
	float time;
	float3 lPos;
};

Texture2D shadowMap : register(t0);
SamplerComparisonState shadowSampler : register(s0);

float lightCompute(float3 p, float3 pn, float3 lp, float intensity)
{
	float d = length(lp - p);
	return intensity*dot(pn, lp - p) / (d * d);
}

// -----------------------------------------------
float4 main(PixelShaderInput input) : SV_TARGET
{

	const float3 ambient = float3(0.1f, 0.1f, 0.1f);

	// NdotL for shadow offset, lighting.
	float3 N = normalize(input.normal);
		float3 L = normalize(input.lRay);
		float NdotL = dot(N, L);

	// Compute texture coordinates for the current point's location on the shadow map.
	float2 shadowTexCoords;
	shadowTexCoords.x = 0.5f + (input.lightSpacePos.x / input.lightSpacePos.w * 0.5f);
	shadowTexCoords.y = 0.5f - (input.lightSpacePos.y / input.lightSpacePos.w * 0.5f);
	float pixelDepth = input.lightSpacePos.z / input.lightSpacePos.w;

	float lighting = 1;

	if ((saturate(shadowTexCoords.x) == shadowTexCoords.x) &&
		(saturate(shadowTexCoords.y) == shadowTexCoords.y) &&
		(pixelDepth > 0))
	{
		// Use an offset value to mitigate shadow artifacts due to imprecise 
		// floating-point values (shadow acne).
		//
		// This is an approximation of epsilon * tan(acos(saturate(NdotL))):
		float margin = acos(saturate(NdotL));
#ifdef LINEAR
		// The offset can be slightly smaller with smoother shadow edges.
		float epsilon = 0.0005 / margin;
#else
		float epsilon = 0.001 / margin;
#endif
		// Clamp epsilon to a fixed range so it doesn't go overboard.
		epsilon = clamp(epsilon, 0, 0.1);

		// Use the SampleCmpLevelZero Texture2D method (or SampleCmp) to sample from 
		// the shadow map, just as you would with Direct3D feature level 10_0 and
		// higher.  Feature level 9_1 only supports LessOrEqual, which returns 0 if
		// the pixel is in the shadow.
		lighting = float(shadowMap.SampleCmpLevelZero(
			shadowSampler,
			shadowTexCoords,
			pixelDepth + epsilon
			)
			);

		if (lighting == 0.f)
		{
			return float4(input.color * ambient, 1.f);
		}
#ifdef LINEAR
		else if (lighting < 1.0f)
		{
			// Blends the shadow area into the lit area.
			float3 light = lighting * (ambient + float3(0, 0, 1));
				float3 shadow = (1.0f - lighting) * ambient;
				return float4(input.color * (light + shadow), 1.f);
		}
#endif
	}

	//float3 key = float3(-5, 2.0, -3.0);
	//	float i = lightCompute(input.rpos, input.normal, key, 6.0);

	//float3 fill = float3(3.0, 0.0, -2.0);
	//	i += lightCompute(input.rpos, input.normal, fill, 2.0);

	//float3 bounce = float3(0.0, -5.0, -2.0);
	//	i += lightCompute(input.rpos, input.normal, bounce, 2.0);

	//i = 0.0;
	//float3 rim = float3(0.0, 2.0, 7.0);
	//i += lightCompute(input.rpos, input.normal, rim, 2.0);

	return float4(1, 1, 1, 1.0);
}