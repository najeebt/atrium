// Per-pixel color data passed through the pixel shader.
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

// -----------------------------------------------
float4 main(PixelShaderInput input) : SV_TARGET
{
	float3 lightpos = float3(-5.0, 5.0, 0.0);
	float d = length(lightpos - input.rpos);
	float i = 10.0*dot(input.normal, lightpos - input.rpos) / (d*d);
	return float4(i, i, i, 1.0);
}