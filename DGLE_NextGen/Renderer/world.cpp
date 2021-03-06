/**
\author		Alexey Shaydurov aka ASH
\date		01.11.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "world.hh"
#include "viewport.hh"
#include "terrain.hh"
#include "world hierarchy.inl"
#include "frustum culling.h"
#include "occlusion query shceduling.h"
#include "frame versioning.h"

#include "vectorLayerVS.csh"
#include "vectorLayerPS.csh"

using namespace std;
using namespace Renderer;
using namespace Math::VectorMath::HLSL;
using WRL::ComPtr;

template<typename Int>
static constexpr inline unsigned BitCount(Int x)
{
	static_assert(is_integral_v<Int>, "x should be integral");
	typedef make_unsigned_t<Int> UInt;
	UInt y = x;
	unsigned count = 0;
	for (unsigned i = 0; i < numeric_limits<UInt>::digits; i++, y >>= 1)
		count += y & 1u;
	return count;
}

template<unsigned alignment, typename UInt>
static constexpr inline auto AlignSize(UInt x)
{
	static_assert(is_unsigned_v<UInt>, "alignment should be unsigned integral");
	static_assert(BitCount(alignment) == 1, "alignemt should be power of 2");
	return x + alignment - 1 & ~(alignment - 1);
}

static constexpr unsigned int CB_overlap = 3, terrainCB_dataSize = sizeof(float[4][4]) * 3, terrainCB_storeSize = AlignSize<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(terrainCB_dataSize);

Impl::World::World(const float(&terrainXform)[4][3])// : bvh(Hierarchy::SplitTechnique::MEAN, .5f)
{
	extern ComPtr<ID3D12Device2> device;

	// create terrain root signature
	{
		CD3DX12_ROOT_PARAMETER1 CBV_params[2];
		CBV_params[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		CBV_params[1].InitAsConstants(3, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(2, CBV_params, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		ComPtr<ID3DBlob> sig, error;
		const HRESULT hr = D3D12SerializeVersionedRootSignature(&sigDesc, &sig, &error);
		if (error)
		{
			cerr.write((const char *)error->GetBufferPointer(), error->GetBufferSize()) << endl;
		}
		CheckHR(hr);
		CheckHR(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&terrainVectorLayerRootSig)));
	}

	// create terrain PSO
	{
		const CD3DX12_RASTERIZER_DESC rasterDesc
		(
			D3D12_FILL_MODE_SOLID,
			D3D12_CULL_MODE_NONE,
			FALSE,										// front CCW
			D3D12_DEFAULT_DEPTH_BIAS,
			D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
			D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
			TRUE,										// depth clip
			FALSE,										// MSAA
			FALSE,										// AA line
			0,											// force sample count
			D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
		);

		const CD3DX12_DEPTH_STENCIL_DESC dsDesc
		(
			FALSE,																								// depth
			D3D12_DEPTH_WRITE_MASK_ZERO,
			D3D12_COMPARISON_FUNC_ALWAYS,
			FALSE,																								// stencil
			D3D12_DEFAULT_STENCIL_READ_MASK,
			D3D12_DEFAULT_STENCIL_WRITE_MASK,
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,	// front
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS	// back
		);

		const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
		{
			terrainVectorLayerRootSig.Get(),								// root signature
			CD3DX12_SHADER_BYTECODE(vectorLayerVS, sizeof vectorLayerVS),	// VS
			CD3DX12_SHADER_BYTECODE(vectorLayerPS, sizeof vectorLayerPS),	// PS
			{},																// DS
			{},																// HS
			{},																// GS
			{},																// SO
			CD3DX12_BLEND_DESC(D3D12_DEFAULT),								// blend
			UINT_MAX,														// sample mask
			rasterDesc,														// rasterizer
			dsDesc,															// depth stencil
			{ VB_decl, size(VB_decl) },										// IA
			D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,					// restart primtive
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,							// primitive topology
			1,																// render targets
			{ DXGI_FORMAT_R8G8B8A8_UNORM },									// RT formats
			DXGI_FORMAT_UNKNOWN,											// depth stencil format
			{1}																// MSAA
		};

		CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(&terrainVectorLayerPSO)));
	}

	// create terrain CB
	{
		// create buffer
		CheckHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(terrainCB_storeSize * CB_overlap/*, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE*/),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			NULL,	// clear value
			IID_PPV_ARGS(&terrainCB)));

#if PERSISTENT_MAPS
		// map buffer
		const CD3DX12_RANGE readRange(0, 0);
		CheckHR(terrainCB->Map(0, &readRange, const_cast<void **>(&terrainCB_CPU_ptr)));
#endif
	}

	memcpy(this->terrainXform, terrainXform, sizeof terrainXform);
}

Impl::World::~World() = default;

void Impl::World::Render(const float (&viewXform)[4][3], const float (&projXform)[4][4], const function<void (ID3D12GraphicsCommandList1 *target)> &setupRenderOutputCallback) const
{
	using namespace placeholders;

	const float4x3 viewTransform(viewXform);
	const float4x4 frustumTransform = mul(float4x4(viewTransform[0], 0.f, viewTransform[1], 0.f, viewTransform[2], 0.f, viewTransform[3], 1.f), float4x4(projXform));
	//bvh.Shcedule(/*bind(&World::ScheduleNode, this, _1),*/ frustumTransform, &viewTransform);

	const auto CB_offset = terrainCB_storeSize * ringBufferIdx++;
	ringBufferIdx %= maxFrameLatency;

	// update CB
	{
#if !PERSISTENT_MAPS
		volatile void *terrainCB_CPU_ptr;
		CD3DX12_RANGE range(0, 0);
		CheckHR(terrainCB->Map(0, &range, const_cast<void **>(&terrainCB_CPU_ptr)));
#endif
		const auto curCB_region = reinterpret_cast<volatile unsigned char *>(terrainCB_CPU_ptr) + CB_offset;
		auto CB_writePtr = reinterpret_cast<volatile float *>(curCB_region);
		memcpy(const_cast<float *>(CB_writePtr), projXform, sizeof projXform), CB_writePtr += 16;
		for (unsigned i = 0; i < extent_v<remove_reference_t<decltype(viewXform)>, 0>; i++, CB_writePtr += 4)
			memcpy(const_cast<float *>(CB_writePtr), viewXform[i], sizeof viewXform[i]);
		for (unsigned i = 0; i < extent_v<remove_reference_t<decltype(terrainXform)>, 0>; i++, CB_writePtr += 4)
			memcpy(const_cast<float *>(CB_writePtr), terrainXform[i], sizeof terrainXform[i]);
#if !PERSISTENT_MAPS
		range.Begin = CB_offset;
		range.End = range.Begin + terrainCB_dataSize;
		terrainCB->Unmap(0, &range);
#endif
	}

	const float4x3 terrainTransform(terrainXform);
	const float4x4 terrainFrustumXform = mul(float4x4(terrainTransform[0], 0.f, terrainTransform[1], 0.f, terrainTransform[2], 0.f, terrainTransform[3], 1.f), frustumTransform);
	const FrustumCuller<2> terrainFrustumCuller(terrainFrustumXform);
	const function<void (ID3D12GraphicsCommandList1 *target)> terrainMainPassSetupCallback =
		[
			&setupRenderOutputCallback,
			rootSig = terrainVectorLayerRootSig,
			CB_location = terrainCB->GetGPUVirtualAddress() + CB_offset
		](ID3D12GraphicsCommandList1 *cmdList)
	{
		setupRenderOutputCallback(cmdList);
		cmdList->SetGraphicsRootSignature(rootSig.Get());
		cmdList->SetGraphicsRootConstantBufferView(0, CB_location);
	};
	for (const auto &layer : terrainVectorLayers)
		layer.ShceduleRenderStage(terrainFrustumCuller, terrainFrustumXform, terrainMainPassSetupCallback);
}

//void Impl::World::ScheduleNode(decltype(bvh)::Node &node) const
//{
//}

shared_ptr<Renderer::Viewport> Impl::World::CreateViewport() const
{
	return make_shared<Renderer::Viewport>(shared_from_this());
}

auto Impl::World::AddTerrainVectorLayer(unsigned int layerIdx, const float (&color)[3]) -> shared_ptr<TerrainVectorLayer>
{
	// keep layers list sorted by idx
	class Idx
	{
		unsigned int idx;

	public:
		Idx(unsigned int idx) noexcept : idx(idx) {}
		Idx(const TerrainVectorLayer &layer) noexcept : idx(layer.layerIdx) {}

	public:
		operator unsigned int () const noexcept { return idx; }
	};
	const auto insertLocation = upper_bound(terrainVectorLayers.cbegin(), terrainVectorLayers.cend(), layerIdx, less<Idx>());
	const auto inserted = terrainVectorLayers.emplace(insertLocation, shared_from_this(), layerIdx, color, terrainVectorLayerPSO);
	// consider using custom allocator for shared_ptr's internal data in order to improve memory management
	return { &*inserted, [inserted](TerrainVectorLayer *layerToRemove) { layerToRemove->world->terrainVectorLayers.erase(inserted); } };
}

/*
	VS 2017 STL uses allocator's construct() to construct combined object (main object + shared ptr data)
	but constructs containing object directly via placement new which does not have access to private members.
	GCC meanwhile compiles it fine.
*/
#if defined _MSC_VER && _MSC_VER <= 1911
shared_ptr<World> __cdecl Renderer::MakeWorld(const float (&terrainXform)[4][3])
{
	return make_shared<World>(World::tag(), terrainXform);
}
#else
shared_ptr<World> __cdecl Renderer::MakeWorld(const float (&terrainXform)[4][3])
{
	return allocate_shared<World>(World::Allocator<World>(), terrainXform);
}
#endif