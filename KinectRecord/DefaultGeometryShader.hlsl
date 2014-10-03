struct GSOutput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
	float3 normal : NORMAL0;
	float4 lightSpacePos : POSITION1;
	float3 lRay : NORMAL1;
	float3 rpos : POSITION2;
};

struct GSInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
	float3 normal : NORMAL0;
	float4 lightSpacePos : POSITION1;
	float3 lRay : NORMAL1;
	float3 rpos : POSITION2;
};

[maxvertexcount(6)]
void main(
	triangleadj GSInput input[6],
	inout TriangleStream< GSOutput > output
	)
{
	if (length(input[0].pos - input[2].pos) < 0.3 && length(input[4].pos - input[0].pos) < 0.3) {
		GSOutput element;
		element.pos = input[0].pos;
		float3 n1 = normalize(cross(input[0].pos.xyz - input[1].pos.xyz, input[0].pos.xyz - input[2].pos.xyz));
			float3 n2 = normalize(cross(input[0].pos.xyz - input[4].pos.xyz, input[0].pos.xyz - input[5].pos.xyz));
			element.normal = (n1 + n2) / 2.0;
		element.rpos = input[0].pos.xyz;
		element.color = input[0].color;
		element.lightSpacePos = input[0].lightSpacePos;
		element.lRay = input[0].lRay;

		output.Append(element);

		element.pos = input[2].pos;// +(input[2].pos - input[0].pos)*0.5;
		n1 = normalize(cross(input[1].pos.xyz - input[2].pos.xyz, input[2].pos.xyz - input[0].pos.xyz));
		n2 = normalize(cross(input[2].pos.xyz - input[3].pos.xyz, input[2].pos.xyz - input[4].pos.xyz));
		element.normal = (n1 + n2) / 2.0;
		element.color = input[2].color;
		element.rpos = input[2].pos.xyz;
		element.lightSpacePos = input[2].lightSpacePos;
		element.lRay = input[2].lRay;
		output.Append(element);

		element.pos = element.pos = input[4].pos;// +(input[4].pos - input[0].pos)*0.5;
		n1 = normalize(cross(input[4].pos.xyz - input[5].pos.xyz, input[4].pos.xyz - input[0].pos.xyz));
		n2 = normalize(cross(input[4].pos.xyz - input[2].pos.xyz, input[4].pos.xyz - input[3].pos.xyz));
		element.normal = (n1 + n2) / 2.0;
		element.color = input[4].color;
		element.rpos = input[4].pos.xyz;
		element.lightSpacePos = input[2].lightSpacePos;
		element.lRay = input[2].lRay;
		output.Append(element);
	}
}