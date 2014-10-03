// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
	float3 normal : NORMAL;
	float3 rpos : POSITION;
};

// -----------------------------------------------
float4 main(PixelShaderInput input) : SV_TARGET
{
	const float lerpfac = 0.2;
	const float lerpdist = 5.0;
	float r = lerp(1.0, 0.0, input.rpos.z*lerpfac);
	float g = input.rpos.z < lerpdist ? lerp(0.0, 1.0, input.rpos.z *lerpfac) : lerp(1.0, 0.0, input.rpos.z *lerpfac);
	float b = lerp(0.0, 1.0, input.rpos.z*lerpfac - 1.0);
	return float4(r, g, b, 1.0f);
}