struct GSOutput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
	float3 normal : NORMAL;
	float3 rpos : POSITION;
};

struct GSInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};

[maxvertexcount(3)]
void main(
	triangle GSInput input[3],
	inout PointStream< GSOutput > output
	)
{
	GSOutput element;
	element.pos = input[0].pos;
	element.normal = normalize(cross(input[0].pos.xyz - input[1].pos.xyz, input[0].pos.xyz - input[2].pos.xyz));
	element.color = input[0].color;
	element.rpos = input[0].pos.xyz;
	output.Append(element);
}