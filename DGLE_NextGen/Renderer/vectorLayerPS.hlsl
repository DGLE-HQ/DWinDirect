/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

cbuffer Constants : register(b0, space1)
{
	float3 color;
}

float4 main() : SV_TARGET
{
	return float4(color, 1.f);
}