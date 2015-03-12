struct GSOutput
{
	float4 pos : SV_POSITION;
};

struct GSInput
{
	float4 pos : SV_POSITION;
};

[maxvertexcount(6)]
void main(
	triangleadj GSInput input[6],
	inout TriangleStream< GSOutput > output
	)
{
	if (length(cross(input[0].pos - input[2].pos, input[4].pos - input[0].pos)) < 0.0001) {
		GSOutput element;
		element.pos = input[0].pos;
		output.Append(element);

		element.pos = input[2].pos;// +(input[2].pos - input[0].pos)*0.5;
		output.Append(element);

		element.pos = element.pos = input[4].pos;// +(input[4].pos - input[0].pos)*0.5;
		output.Append(element);
	}
}