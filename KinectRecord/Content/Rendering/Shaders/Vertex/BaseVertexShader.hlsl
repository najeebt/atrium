// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	matrix model;
	matrix view;
	matrix projection;
};

cbuffer SceneConstants : register(b1)
{
	float time;
	float3 lPos;
};

//// A constant buffer that stores matrices for composing geometry for the shadow buffer.
cbuffer LightViewProjectionConstantBuffer : register(b2)
{
	matrix lModel;
	matrix lView;
	matrix lProjection;
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
	float3 color : COLOR0;
	float3 normal : NORMAL0;
	float4 lightSpacePos : POSITION1;
	float3 lRay : NORMAL1;
	float3 rpos : POSITION2;
};

GeometryShaderInput main(VertexShaderInput input)
{
	GeometryShaderInput output;
	float4 pos = input.pos;
	//pos = (input.pos + input.pos2 + input.pos3) / 3.0;

	// Transform the vertex position into projected space.
	float4 modelPos = mul(pos, model);
	pos = mul(modelPos, view);
	pos = mul(pos, projection);
	output.pos = pos;

	output.color = input.color;

	// Transform the vertex position into projected space from the POV of the light.
	float4 lightSpacePos = mul(modelPos, lView);
	lightSpacePos = mul(lightSpacePos, lProjection);
	output.lightSpacePos = lightSpacePos;

	// Light ray
	float3 lRay = lPos - modelPos.xyz;
	output.lRay = lRay;

	return output;
}