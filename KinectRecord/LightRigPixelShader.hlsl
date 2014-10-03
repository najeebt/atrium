struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
	float3 normal : NORMAL;
	float3 rpos : POSITION;
};

cbuffer SceneConstants : register(b1)
{
	float time;
};

float lightCompute(float3 p, float3 pn, float3 lp, float intensity)
{
	float d = length(lp - p);
	return intensity*dot(pn, lp - p) / (d * d);
}

// -----------------------------------------------
float4 main(PixelShaderInput input) : SV_TARGET
{
	float3 key = float3(-5, 2.0, -3.0);
	float i = lightCompute(input.rpos, input.normal, key, 6.0);

	float3 fill = float3(3.0, 0.0, -2.0);
	i += lightCompute(input.rpos, input.normal, fill, 2.0);

	float3 bounce = float3(0.0, -5.0, -2.0);
	i += lightCompute(input.rpos, input.normal, bounce, 2.0);

	return float4(i, i, i, 1.0);
}