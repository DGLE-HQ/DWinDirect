/**
\author		Alexey Shaydurov aka ASH
\date		12.2.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#include "stdafx.h"
#include "DisplayModes.h"

using namespace std;
using namespace DirectX::ComPtrs;
using DisplayModesImpl::CDisplayModes;
using DisplayModesImpl::Interface::IDisplayModes;
using DisplayModesImpl::displayModes;
using DGLE2::uint;

namespace
{
	static inline const char *Format2String(DXGI_FORMAT format) noexcept
	{
		switch (format)
		{
		case DXGI_FORMAT_UNKNOWN:						return "DXGI_FORMAT_UNKNOWN";
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:			return "DXGI_FORMAT_R32G32B32A32_TYPELESS";
		case DXGI_FORMAT_R32G32B32A32_FLOAT:			return "DXGI_FORMAT_R32G32B32A32_FLOAT";
		case DXGI_FORMAT_R32G32B32A32_UINT:				return "DXGI_FORMAT_R32G32B32A32_UINT";
		case DXGI_FORMAT_R32G32B32A32_SINT:				return "DXGI_FORMAT_R32G32B32A32_SINT";
		case DXGI_FORMAT_R32G32B32_TYPELESS:			return "DXGI_FORMAT_R32G32B32_TYPELESS";
		case DXGI_FORMAT_R32G32B32_FLOAT:				return "DXGI_FORMAT_R32G32B32_FLOAT";
		case DXGI_FORMAT_R32G32B32_UINT:				return "DXGI_FORMAT_R32G32B32_UINT";
		case DXGI_FORMAT_R32G32B32_SINT:				return "DXGI_FORMAT_R32G32B32_SINT";
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:			return "DXGI_FORMAT_R16G16B16A16_TYPELESS";
		case DXGI_FORMAT_R16G16B16A16_FLOAT:			return "DXGI_FORMAT_R16G16B16A16_FLOAT";
		case DXGI_FORMAT_R16G16B16A16_UNORM:			return "DXGI_FORMAT_R16G16B16A16_UNORM";
		case DXGI_FORMAT_R16G16B16A16_UINT:				return "DXGI_FORMAT_R16G16B16A16_UINT";
		case DXGI_FORMAT_R16G16B16A16_SNORM:			return "DXGI_FORMAT_R16G16B16A16_SNORM";
		case DXGI_FORMAT_R16G16B16A16_SINT:				return "DXGI_FORMAT_R16G16B16A16_SINT";
		case DXGI_FORMAT_R32G32_TYPELESS:				return "DXGI_FORMAT_R32G32_TYPELESS";
		case DXGI_FORMAT_R32G32_FLOAT:					return "DXGI_FORMAT_R32G32_FLOAT";
		case DXGI_FORMAT_R32G32_UINT:					return "DXGI_FORMAT_R32G32_UINT";
		case DXGI_FORMAT_R32G32_SINT:					return "DXGI_FORMAT_R32G32_SINT";
		case DXGI_FORMAT_R32G8X24_TYPELESS:				return "DXGI_FORMAT_R32G8X24_TYPELESS";
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:			return "DXGI_FORMAT_D32_FLOAT_S8X24_UINT";
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:		return "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS";
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:		return "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT";
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:			return "DXGI_FORMAT_R10G10B10A2_TYPELESS";
		case DXGI_FORMAT_R10G10B10A2_UNORM:				return "DXGI_FORMAT_R10G10B10A2_UNORM";
		case DXGI_FORMAT_R10G10B10A2_UINT:				return "DXGI_FORMAT_R10G10B10A2_UINT";
		case DXGI_FORMAT_R11G11B10_FLOAT:				return "DXGI_FORMAT_R11G11B10_FLOAT";
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:				return "DXGI_FORMAT_R8G8B8A8_TYPELESS";
		case DXGI_FORMAT_R8G8B8A8_UNORM:				return "DXGI_FORMAT_R8G8B8A8_UNORM";
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:			return "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB";
		case DXGI_FORMAT_R8G8B8A8_UINT:					return "DXGI_FORMAT_R8G8B8A8_UINT";
		case DXGI_FORMAT_R8G8B8A8_SNORM:				return "DXGI_FORMAT_R8G8B8A8_SNORM";
		case DXGI_FORMAT_R8G8B8A8_SINT:					return "DXGI_FORMAT_R8G8B8A8_SINT";
		case DXGI_FORMAT_R16G16_TYPELESS:				return "DXGI_FORMAT_R16G16_TYPELESS";
		case DXGI_FORMAT_R16G16_FLOAT:					return "DXGI_FORMAT_R16G16_FLOAT";
		case DXGI_FORMAT_R16G16_UNORM:					return "DXGI_FORMAT_R16G16_UNORM";
		case DXGI_FORMAT_R16G16_UINT:					return "DXGI_FORMAT_R16G16_UINT";
		case DXGI_FORMAT_R16G16_SNORM:					return "DXGI_FORMAT_R16G16_SNORM";
		case DXGI_FORMAT_R16G16_SINT:					return "DXGI_FORMAT_R16G16_SINT";
		case DXGI_FORMAT_R32_TYPELESS:					return "DXGI_FORMAT_R32_TYPELESS";
		case DXGI_FORMAT_D32_FLOAT:						return "DXGI_FORMAT_D32_FLOAT";
		case DXGI_FORMAT_R32_FLOAT:						return "DXGI_FORMAT_R32_FLOAT";
		case DXGI_FORMAT_R32_UINT:						return "DXGI_FORMAT_R32_UINT";
		case DXGI_FORMAT_R32_SINT:						return "DXGI_FORMAT_R32_SINT";
		case DXGI_FORMAT_R24G8_TYPELESS:				return "DXGI_FORMAT_R24G8_TYPELESS";
		case DXGI_FORMAT_D24_UNORM_S8_UINT:				return "DXGI_FORMAT_D24_UNORM_S8_UINT";
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:			return "DXGI_FORMAT_R24_UNORM_X8_TYPELESS";
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:			return "DXGI_FORMAT_X24_TYPELESS_G8_UINT";
		case DXGI_FORMAT_R8G8_TYPELESS:					return "DXGI_FORMAT_R8G8_TYPELESS";
		case DXGI_FORMAT_R8G8_UNORM:					return "DXGI_FORMAT_R8G8_UNORM";
		case DXGI_FORMAT_R8G8_UINT:						return "DXGI_FORMAT_R8G8_UINT";
		case DXGI_FORMAT_R8G8_SNORM:					return "DXGI_FORMAT_R8G8_SNORM";
		case DXGI_FORMAT_R8G8_SINT:						return "DXGI_FORMAT_R8G8_SINT";
		case DXGI_FORMAT_R16_TYPELESS:					return "DXGI_FORMAT_R16_TYPELESS";
		case DXGI_FORMAT_R16_FLOAT:						return "DXGI_FORMAT_R16_FLOAT";
		case DXGI_FORMAT_D16_UNORM:						return "DXGI_FORMAT_D16_UNORM";
		case DXGI_FORMAT_R16_UNORM:						return "DXGI_FORMAT_R16_UNORM";
		case DXGI_FORMAT_R16_UINT:						return "DXGI_FORMAT_R16_UINT";
		case DXGI_FORMAT_R16_SNORM:						return "DXGI_FORMAT_R16_SNORM";
		case DXGI_FORMAT_R16_SINT:						return "DXGI_FORMAT_R16_SINT";
		case DXGI_FORMAT_R8_TYPELESS:					return "DXGI_FORMAT_R8_TYPELESS";
		case DXGI_FORMAT_R8_UNORM:						return "DXGI_FORMAT_R8_UNORM";
		case DXGI_FORMAT_R8_UINT:						return "DXGI_FORMAT_R8_UINT";
		case DXGI_FORMAT_R8_SNORM:						return "DXGI_FORMAT_R8_SNORM";
		case DXGI_FORMAT_R8_SINT:						return "DXGI_FORMAT_R8_SINT";
		case DXGI_FORMAT_A8_UNORM:						return "DXGI_FORMAT_A8_UNORM";
		case DXGI_FORMAT_R1_UNORM:						return "DXGI_FORMAT_R1_UNORM";
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:			return "DXGI_FORMAT_R9G9B9E5_SHAREDEXP";
		case DXGI_FORMAT_R8G8_B8G8_UNORM:				return "DXGI_FORMAT_R8G8_B8G8_UNORM";
		case DXGI_FORMAT_G8R8_G8B8_UNORM:				return "DXGI_FORMAT_G8R8_G8B8_UNORM";
		case DXGI_FORMAT_BC1_TYPELESS:					return "DXGI_FORMAT_BC1_TYPELESS";
		case DXGI_FORMAT_BC1_UNORM:						return "DXGI_FORMAT_BC1_UNORM";
		case DXGI_FORMAT_BC1_UNORM_SRGB:				return "DXGI_FORMAT_BC1_UNORM_SRGB";
		case DXGI_FORMAT_BC2_TYPELESS:					return "DXGI_FORMAT_BC2_TYPELESS";
		case DXGI_FORMAT_BC2_UNORM:						return "DXGI_FORMAT_BC2_UNORM";
		case DXGI_FORMAT_BC2_UNORM_SRGB:				return "DXGI_FORMAT_BC2_UNORM_SRGB";
		case DXGI_FORMAT_BC3_TYPELESS:					return "DXGI_FORMAT_BC3_TYPELESS";
		case DXGI_FORMAT_BC3_UNORM:						return "DXGI_FORMAT_BC3_UNORM";
		case DXGI_FORMAT_BC3_UNORM_SRGB:				return "DXGI_FORMAT_BC3_UNORM_SRGB";
		case DXGI_FORMAT_BC4_TYPELESS:					return "DXGI_FORMAT_BC4_TYPELESS";
		case DXGI_FORMAT_BC4_UNORM:						return "DXGI_FORMAT_BC4_UNORM";
		case DXGI_FORMAT_BC4_SNORM:						return "DXGI_FORMAT_BC4_SNORM";
		case DXGI_FORMAT_BC5_TYPELESS:					return "DXGI_FORMAT_BC5_TYPELESS";
		case DXGI_FORMAT_BC5_UNORM:						return "DXGI_FORMAT_BC5_UNORM";
		case DXGI_FORMAT_BC5_SNORM:						return "DXGI_FORMAT_BC5_SNORM";
		case DXGI_FORMAT_B5G6R5_UNORM:					return "DXGI_FORMAT_B5G6R5_UNORM";
		case DXGI_FORMAT_B5G5R5A1_UNORM:				return "DXGI_FORMAT_B5G5R5A1_UNORM";
		case DXGI_FORMAT_B8G8R8A8_UNORM:				return "DXGI_FORMAT_B8G8R8A8_UNORM";
		case DXGI_FORMAT_B8G8R8X8_UNORM:				return "DXGI_FORMAT_B8G8R8X8_UNORM";
		case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:	return "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM";
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:				return "DXGI_FORMAT_B8G8R8A8_TYPELESS";
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:			return "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB";
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:				return "DXGI_FORMAT_B8G8R8X8_TYPELESS";
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:			return "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB";
		case DXGI_FORMAT_BC6H_TYPELESS:					return "DXGI_FORMAT_BC6H_TYPELESS";
		case DXGI_FORMAT_BC6H_UF16:						return "DXGI_FORMAT_BC6H_UF16";
		case DXGI_FORMAT_BC6H_SF16:						return "DXGI_FORMAT_BC6H_SF16";
		case DXGI_FORMAT_BC7_TYPELESS:					return "DXGI_FORMAT_BC7_TYPELESS";
		case DXGI_FORMAT_BC7_UNORM:						return "DXGI_FORMAT_BC7_UNORM";
		case DXGI_FORMAT_BC7_UNORM_SRGB:				return "DXGI_FORMAT_BC7_UNORM_SRGB";
		default:
			_ASSERT(false);
			__assume(false);
		}
	}

	static inline const char *ScanlineOrdering2string(DXGI_MODE_SCANLINE_ORDER scanlineOrder) noexcept
	{
		switch (scanlineOrder)
		{
		case DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED:			return "DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED";
		case DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE:			return "DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE";
		case DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST:	return "DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST";
		case DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST:	return "DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST";
		default:
			_ASSERT(false);
			__assume(false);
		}
	}

	static inline const char *Scaling2String(DXGI_MODE_SCALING scaling) noexcept
	{
		switch (scaling)
		{
		case DXGI_MODE_SCALING_UNSPECIFIED:	return "DXGI_MODE_SCALING_UNSPECIFIED";
		case DXGI_MODE_SCALING_CENTERED:	return "DXGI_MODE_SCALING_CENTERED";
		case DXGI_MODE_SCALING_STRETCHED:	return "DXGI_MODE_SCALING_STRETCHED";
		default:
			_ASSERT(false);
			__assume(false);
		}
	}

	static string ModeDesc2String(const DXGI_MODE_DESC &desc)
	{
		return string("Format:\t") + Format2String(desc.Format) +
			"\nScanlineOrdering:\t" + ScanlineOrdering2string(desc.ScanlineOrdering) +
			"\nScaling:\t" + Scaling2String(desc.Scaling);
	}
}

CDisplayModes::CDisplayModes()
{
	/*
	WARNING: DXGI_ERROR_MORE_DATA for GetDisplayModeList() currently does not handled.
	More info then available in MSDN required for proper handling.
	*/
	IDXGIFactory1Ptr factory;
	ASSERT_HR(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory)))
	IDXGIAdapter1Ptr adapter;
	ASSERT_HR(factory->EnumAdapters1(0, &adapter))
	UINT modes_count;
	IDXGIOutputPtr output;
	ASSERT_HR(adapter->EnumOutputs(0, &output))
	ASSERT_HR(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_SCALING, &modes_count, NULL))
	_modes.resize(modes_count);
	ASSERT_HR(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_SCALING, &modes_count, _modes.data()))
}

auto CDisplayModes::Get(uint idx) const -> TDispModeDesc
{
	Interface::TDispMode mode =
	{
		_modes[idx].Width,
		_modes[idx].Height,
		float(_modes[idx].RefreshRate.Numerator) / _modes[idx].RefreshRate.Denominator
	};
	return TDispModeDesc(mode, new CDesc(ModeDesc2String(_modes[idx])));
}

extern "C" const IDisplayModes &DisplayModesImpl::Interface::GetDisplayModes()
{
	return displayModes;
}

const CDisplayModes displayModes;