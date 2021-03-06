/**
\author		Alexey Shaydurov aka ASH
\date		28.8.2015 (c)Andrey Korotkov

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "FixedFunctionPipelineDX9.h"
#include "CoreRendererDX9.h"

namespace
{
	inline D3DCOLORVALUE Color_DGLE_2_D3D(const TColor4 &dgleColor)
	{
		const D3DCOLORVALUE d3dColor = { dgleColor.r, dgleColor.g, dgleColor.b, dgleColor.a };
		return d3dColor;
	}

	inline TColor4 Color_D3D_2_DGLE(const D3DCOLORVALUE &d3dColor)
	{
		TColor4 dgleColor;
		dgleColor.SetColorF(d3dColor.r, d3dColor.g, d3dColor.b, d3dColor.a);
		return dgleColor;
	}

	inline D3DVECTOR Vector_DGLE_2_D3D(const TVector3 &dgleVector)
	{
		const D3DVECTOR d3dVector = { dgleVector.x, dgleVector.y, dgleVector.z };
		return d3dVector;
	}

	inline TVector3 Vector_D3D_2_DGLE(const D3DVECTOR &d3dVector)
	{
		return TVector3(d3dVector.x, d3dVector.y, d3dVector.z);
	}

	inline TPoint3 operator -(TPoint3 point)
	{
		return{ -point.x, -point.y, -point.z };
	}
}

CFixedFunctionPipelineDX9::CFixedFunctionPipelineDX9(const CCoreRendererDX9 &parent, const IDirect3DDevice9Ptr &device) :
_parent(parent), _device(device),
_maxLights([this]()
{
	D3DCAPS9 caps;
	AssertHR(_device->GetDeviceCaps(&caps));
	return caps.MaxActiveLights;
} ()),
_viewXforms(new decltype(_viewXforms)::element_type [_maxLights])
{
	std::fill_n(_viewXforms.get(), _maxLights, MatrixIdentity());

	AssertHR(_device->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR));
	AssertHR(_device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_RGBA(50, 50, 50, 255)));
	AssertHR(_device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE));
	AssertHR(_device->SetRenderState(D3DRS_SPECULARENABLE, TRUE));

	for (DWORD i = 0; i < _maxLights; ++i)
	{
		AssertHR(_device->LightEnable(i, FALSE));
		const D3DLIGHT9 lightDesc =
		{
			D3DLIGHT_DIRECTIONAL,
			{ 1, 1, 1, 1 },				// Diffuse
			{ 1, 1, 1, 1 },				// Specular
			{ 0, 0, 0, 1 },				// Ambient
			{ 0, 0, 0 },				// Position (ignored for directional light)
			{ 0, 0, 1 },				// Direction
			sqrt(FLT_MAX),				// Range (ignored for directional light)
			0,							// Falloff
			1,							// Attenuation0
			_attenuationFactor / 100.f,	// Attenuation1
			0,							// Attenuation2
			0,							// Theta (ignored for directional light)
			float(M_PI)					// Phi (ignored for directional light)
		};
		AssertHR(_device->SetLight(i, &lightDesc));
	}
}

CFixedFunctionPipelineDX9::~CFixedFunctionPipelineDX9() = default;

void CFixedFunctionPipelineDX9::PushLights()
{
	TLightStateStack::value_type cur_state(new TLightState[_maxLights]);
	for (DWORD i = 0; i < _maxLights; i++)
	{
		D3DLIGHT9 light;
		if (SUCCEEDED(_device->GetLight(i, &light)))
			cur_state[i].light.reset(new decltype(cur_state[i].light)::element_type(light, _viewXforms[i]));

		if (FAILED(_device->GetLightEnable(i, &cur_state[i].enabled)))
			cur_state[i].enabled = -1;
	}
	_lightStateStack.push(move(cur_state));
}

void CFixedFunctionPipelineDX9::PopLights()
{
	for (DWORD i = 0; i < _maxLights; i++)
	{
		const auto &saved_state = _lightStateStack.top()[i];
		if (saved_state.light)
		{
			AssertHR(_device->SetLight(i, &saved_state.light->first));
			_viewXforms[i] = saved_state.light->second;
		}
		if (saved_state.enabled != -1)
			AssertHR(_device->LightEnable(i, saved_state.enabled));
	}
	_lightStateStack.pop();
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::PushStates()
{
	CHECK_DEVICE(_parent);

	// straightforward unoptimized implementation
	TStateStack::value_type curStates;
	AssertHR(_device->GetRenderState(D3DRS_FOGENABLE, &curStates.fogEnable));
	AssertHR(_device->GetRenderState(D3DRS_FOGCOLOR, &curStates.fogColor));
	AssertHR(_device->GetRenderState(D3DRS_FOGSTART, &curStates.fogStart));
	AssertHR(_device->GetRenderState(D3DRS_FOGEND, &curStates.fogEnd));
	AssertHR(_device->GetRenderState(D3DRS_FOGDENSITY, &curStates.fogDensity));
	AssertHR(_device->GetRenderState(D3DRS_LIGHTING, &curStates.lightingEnable));
	AssertHR(_device->GetRenderState(D3DRS_AMBIENT, &curStates.globalAmbient));
	AssertHR(_device->GetMaterial(&curStates.material));
	_stateStack.push(curStates);
	PushLights();
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::PopStates()
{
	CHECK_DEVICE(_parent);

	AssertHR(_device->SetRenderState(D3DRS_FOGENABLE, _stateStack.top().fogEnable));
	AssertHR(_device->SetRenderState(D3DRS_FOGCOLOR, _stateStack.top().fogColor));
	AssertHR(_device->SetRenderState(D3DRS_FOGSTART, _stateStack.top().fogStart));
	AssertHR(_device->SetRenderState(D3DRS_FOGEND, _stateStack.top().fogEnd));
	AssertHR(_device->SetRenderState(D3DRS_FOGDENSITY, _stateStack.top().fogDensity));
	AssertHR(_device->SetRenderState(D3DRS_LIGHTING, _stateStack.top().lightingEnable));
	AssertHR(_device->SetRenderState(D3DRS_AMBIENT, _stateStack.top().globalAmbient));
	AssertHR(_device->SetMaterial(&_stateStack.top().material));
	_stateStack.pop();
	_lightStateStack.pop();
	return S_OK;
}

/*
	material Set* methods uses inefficient get/set pattern (possibly leading to CPU/GPU sync)
	it used to ensure coherency with external state modfications
*/

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetMaterialDiffuseColor(const TColor4 &stColor)
{
	CHECK_DEVICE(_parent);

	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	material.Ambient = material.Diffuse = Color_DGLE_2_D3D(stColor);
	AssertHR(_device->SetMaterial(&material));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetMaterialSpecularColor(const TColor4 &stColor)
{
	CHECK_DEVICE(_parent);

	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	material.Specular = Color_DGLE_2_D3D(stColor);
	AssertHR(_device->SetMaterial(&material));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetMaterialShininess(float fShininess)
{
	CHECK_DEVICE(_parent);

	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	material.Power = (128.f / 100.f) * (100.f - fShininess);
	AssertHR(_device->SetMaterial(&material));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetMaterialDiffuseColor(TColor4 &stColor)
{
	CHECK_DEVICE(_parent);

	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	stColor = Color_D3D_2_DGLE(material.Diffuse);
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetMaterialSpecularColor(TColor4 &stColor)
{
	CHECK_DEVICE(_parent);

	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	stColor = Color_D3D_2_DGLE(material.Specular);
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetMaterialShininess(float &fShininess)
{
	CHECK_DEVICE(_parent);

	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	fShininess = 100.f - material.Power * (100.f / 128.f);
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::ToggleGlobalLighting(bool bEnabled)
{
	CHECK_DEVICE(_parent);

	AssertHR(_device->SetRenderState(D3DRS_LIGHTING, bEnabled));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetGloablAmbientLight(const TColor4 &stColor)
{
	CHECK_DEVICE(_parent);

	AssertHR(_device->SetRenderState(D3DRS_AMBIENT, SwapRB(stColor)));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetMaxLightsPerPassCount(uint &uiCount)
{
	uiCount = _maxLights;
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::IsGlobalLightingEnabled(bool &bEnabled)
{
	CHECK_DEVICE(_parent);

	bEnabled = IsGlobalLightingEnabled();
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetGloablAmbientLight(TColor4 &stColor)
{
	CHECK_DEVICE(_parent);

	DWORD ambient;
	AssertHR(_device->GetRenderState(D3DRS_AMBIENT, &ambient));
	stColor = SwapRB(ambient);
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetLightEnabled(uint uiIdx, bool bEnabled)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	AssertHR(_device->LightEnable(uiIdx, bEnabled));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetLightColor(uint uiIdx, const TColor4 &stColor)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	light.Diffuse = light.Specular = Color_DGLE_2_D3D(stColor);
	AssertHR(_device->SetLight(uiIdx, &light));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetLightIntensity(uint uiIdx, float fIntensity)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= (uint)_maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	light.Attenuation0 = 1.f / fIntensity;
	AssertHR(_device->SetLight(uiIdx, &light));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::ConfigureDirectionalLight(uint uiIdx, const TVector3 &stDirection)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	AssertHR(_device->GetTransform(D3DTS_WORLD, (D3DMATRIX *)&_viewXforms[uiIdx]));

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	light.Type = D3DLIGHT_DIRECTIONAL;
	light.Direction = Vector_DGLE_2_D3D(_viewXforms[uiIdx].ApplyToVector(-stDirection));
	AssertHR(_device->SetLight(uiIdx, &light));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::ConfigurePointLight(uint uiIdx, const TPoint3 &stPosition, float fRange)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	AssertHR(_device->GetTransform(D3DTS_WORLD, (D3DMATRIX *)&_viewXforms[uiIdx]));

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	light.Type = D3DLIGHT_POINT;
	light.Position = Vector_DGLE_2_D3D(_viewXforms[uiIdx].ApplyToPoint(stPosition));
	light.Attenuation1 = _attenuationFactor / fRange;
	light.Range = fRange;
	AssertHR(_device->SetLight(uiIdx, &light));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::ConfigureSpotLight(uint uiIdx, const TPoint3 &stPosition, const TVector3 &stDirection, float fRange, float fSpotAngle)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	AssertHR(_device->GetTransform(D3DTS_WORLD, (D3DMATRIX *)&_viewXforms[uiIdx]));

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	light.Type = D3DLIGHT_SPOT;
	light.Position = Vector_DGLE_2_D3D(_viewXforms[uiIdx].ApplyToPoint(stPosition));
	light.Direction = Vector_DGLE_2_D3D(_viewXforms[uiIdx].ApplyToVector(-stDirection));
	light.Phi = fSpotAngle * (float(M_PI) / 180);
	light.Attenuation1 = _attenuationFactor / fRange;
	light.Range = fRange;
	AssertHR(_device->SetLight(uiIdx, &light));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetLightEnabled(uint uiIdx, bool &bEnabled)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	BOOL enabled;
	AssertHR(_device->GetLightEnable(uiIdx, &enabled));
	bEnabled = enabled;

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetLightColor(uint uiIdx, TColor4 &stColor)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	stColor = Color_D3D_2_DGLE(light.Diffuse);

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetLightIntensity(uint uiIdx, float &fIntensity)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	fIntensity = 1.f / light.Attenuation0;

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetLightType(uint uiIdx, E_LIGHT_TYPE &eType)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	switch (light.Type)
	{
	case D3DLIGHT_DIRECTIONAL:
		return LT_DIRECTIONAL;
	case  D3DLIGHT_POINT:
		return LT_POINT;
	case  D3DLIGHT_SPOT:
		return LT_SPOT;
	default:
		assert(false);
		__assume(false);
	}

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetDirectionalLightConfiguration(uint uiIdx, TVector3 &stDirection)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));

	if (light.Type != D3DLIGHT_DIRECTIONAL)
		return E_INVALIDARG;

	stDirection = -MatrixInverse(_viewXforms[uiIdx]).ApplyToVector(Vector_D3D_2_DGLE(light.Direction));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetPointLightConfiguration(uint uiIdx, TPoint3 &stPosition, float &fRange)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));

	if (light.Type != D3DLIGHT_POINT)
		return E_INVALIDARG;

	stPosition = MatrixInverse(_viewXforms[uiIdx]).ApplyToPoint(Vector_D3D_2_DGLE(light.Position));

	fRange = light.Range;

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetSpotLightConfiguration(uint uiIdx, TPoint3 &stPosition, TVector3 &stDirection, float &fRange, float &fSpotAngle)
{
	CHECK_DEVICE(_parent);

	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));

	if (light.Type != D3DLIGHT_SPOT)
		return E_INVALIDARG;

	const auto xform = MatrixInverse(_viewXforms[uiIdx]);
	stPosition = xform.ApplyToPoint(Vector_D3D_2_DGLE(light.Position));
	stDirection = -xform.ApplyToVector(Vector_D3D_2_DGLE(light.Direction));

	fSpotAngle = light.Phi * (180 / float(M_PI));
	fRange = light.Range;

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetFogEnabled(bool bEnabled)
{
	CHECK_DEVICE(_parent);

	AssertHR(_device->SetRenderState(D3DRS_FOGENABLE, bEnabled));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetFogColor(const TColor4 &stColor)
{
	CHECK_DEVICE(_parent);

	AssertHR(_device->SetRenderState(D3DRS_FOGCOLOR, SwapRB(stColor)));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::ConfigureFog(float fStart, float fEnd)
{
	CHECK_DEVICE(_parent);

	AssertHR(_device->SetRenderState(D3DRS_FOGSTART, reinterpret_cast<const DWORD &>(fStart)));
	AssertHR(_device->SetRenderState(D3DRS_FOGEND, reinterpret_cast<const DWORD &>(fEnd)));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetFogEnabled(bool &bEnabled)
{
	CHECK_DEVICE(_parent);

	DWORD enabled;
	AssertHR(_device->GetRenderState(D3DRS_FOGENABLE, &enabled));
	bEnabled = enabled;
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetFogColor(TColor4 &stColor)
{
	CHECK_DEVICE(_parent);

	DWORD color;
	AssertHR(_device->GetRenderState(D3DRS_FOGCOLOR, &color));
	stColor = SwapRB(color);
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetFogConfiguration(float &fStart, float &fEnd)
{
	CHECK_DEVICE(_parent);

	AssertHR(_device->GetRenderState(D3DRS_FOGSTART, reinterpret_cast<DWORD *>(&fStart)));
	AssertHR(_device->GetRenderState(D3DRS_FOGEND, reinterpret_cast<DWORD *>(&fEnd)));
	return S_OK;
}