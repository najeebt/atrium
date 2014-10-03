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
	inout TriangleStream< GSOutput > output
)
{	
	if (length(input[1].pos - input[0].pos) < 0.3 && length(input[2].pos - input[0].pos) < 0.3) {
		GSOutput element;
		element.pos = input[0].pos;
		element.normal = normalize(cross(input[0].pos.xyz - input[1].pos.xyz, input[0].pos.xyz - input[2].pos.xyz));
		element.color = input[0].color;
		element.rpos = input[0].pos.xyz;
		output.Append(element);

		element.pos = input[1].pos;// +(input[1].pos - input[0].pos)*0.5;
		element.normal = normalize(cross(input[0].pos.xyz - input[1].pos.xyz, input[1].pos.xyz - input[2].pos.xyz));
		element.color = input[1].color;
		element.rpos = input[1].pos.xyz;
		output.Append(element);
		
		element.pos = element.pos = input[2].pos;// +(input[2].pos - input[0].pos)*0.5;
		element.normal = normalize(cross(input[2].pos.xyz - input[0].pos.xyz, input[2].pos.xyz - input[1].pos.xyz));
		element.color = input[2].color;
		element.rpos = input[2].pos.xyz;
		output.Append(element);
	}
}