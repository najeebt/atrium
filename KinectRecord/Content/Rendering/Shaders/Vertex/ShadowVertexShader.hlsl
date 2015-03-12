// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	matrix model;
	matrix view;
	matrix projection;
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	float4 pos : POSITION0;
	float3 color : COLOR0;
};
// Per-pixel color data passed through the pixel shader.
struct GeometryShaderInput
{
	float4 pos : SV_POSITION;
};

GeometryShaderInput main(VertexShaderInput input)
{
	GeometryShaderInput output;
	float4 pos = input.pos;

	// Transform the vertex position into projected space.
	float4 modelPos = mul(pos, model);
	pos = mul(modelPos, view);
	pos = mul(pos, projection);
	output.pos = pos;

	return output;
}