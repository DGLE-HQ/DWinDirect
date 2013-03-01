/**
\author		Alexey Shaydurov aka ASH
\date		7.2.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "Interface\Renderer.h"
#include "RendererBase.h"
#include "Dtor.h"
#include "Win32Heap.h"
#include "DynamicVB.h"

class RendererImpl::C2D: virtual public CRendererBase
{
#pragma pack(push, 4)
	struct TQuad
	{
		TQuad() noexcept {}
		TQuad(float x, float y, float width, float height, DGLE2::uint16 layer, float angle, DGLE2::uint32 color):
			pos(x, y), extents(width, height), layer(layer), angle(angle), color(color)
		{
		}
		bool operator <(const TQuad &quad) const noexcept
		{
			return layer < quad.layer;
		}
#ifdef _2D_FLOAT32_TO_FLOAT16
		D3DXVECTOR2_16F pos, extents;
		DGLE2::uint16 layer;
		D3DXFLOAT16 angle;
		DGLE2::uint32 color;
#else
		D3DXVECTOR2 pos, extents;
		DGLE2::uint16 layer;
		float angle;
		DGLE2::uint32 color;
#endif
	};
#pragma pack(pop)

	class CQuadFiller;

protected:
	C2D(const DXGI_MODE_DESC &modeDesc, bool multithreaded);
#ifdef MSVC_LIMITATIONS
	~C2D() {}
#else
	~C2D() = default;
#endif

public:
	virtual void DrawPoint(float x, float y, DGLE2::uint32 color, float size) const override;
	virtual void DrawLine(DGLE2::uint vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const DGLE2::uint32 colors[], bool closed, float width) const override;
	virtual void DrawRect(float x, float y, float width, float height, DGLE2::uint32 color, Interface::Textures::ITexture2D *texture, float angle) const override;
	virtual void DrawRect(float x, float y, float width, float height, DGLE2::uint32 (&colors)[4], Interface::Textures::ITexture2D *texture, float angle) const override;
	virtual void DrawPolygon(DGLE2::uint vcount, _In_count_(vcount) const float coords[][2], DGLE2::uint32 color) const override;
	virtual void DrawCircle(float x, float y, float r, DGLE2::uint32 color) const override;
	virtual void DrawEllipse(float x, float y, float rx, float ry, DGLE2::uint32 color, bool AA, float angle) const override;

	virtual Interface::Instances::_2D::IRect *AddRect(bool dynamic, DGLE2::uint16 layer, float x, float y, float width, float height, DGLE2::uint32 color, float angle) override;
	virtual Interface::Instances::_2D::IEllipse *AddEllipse(bool dynamic, DGLE2::uint16 layer, float x, float y, float rx, float ry, DGLE2::uint32 color, bool AA, float angle) override;

protected:
	void _NextFrame() const;

private:
	void _DrawScene() const;

private:
	Win32Heap::CWin32Heap _dedicatedHeap;
	Win32Heap::CWin32HeapAllocator<TQuad> _staticRectsAllocator, _dynamicRectsAllocator;
	Win32Heap::CWin32HeapAllocator<TQuad> _staticEllipsesAllocator, _dynamicEllipsesAllocator;
	Win32Heap::CWin32HeapAllocator<TQuad> _staticEllipsesAAAllocator, _dynamicEllipsesAAAllocator;
	//class CRect;
	//class CEllipse;
	//typedef list<CRect> TRects;
	//typedef list<CEllipse> TEllipses;

	// 2D state objects
	// temp for test
protected:
	DirectX::ComPtrs::ID3DX11EffectPtr _effect2D;
private:
	DirectX::ComPtrs::ID3D11InputLayoutPtr _quadLayout;
	DirectX::ComPtrs::ID3D11QueryPtr query;
	ID3DX11EffectPass *_rectPass, *_ellipsePass, *_ellipseAAPass;

	// immediate 2D
	mutable CDynamicVB _2DVB;
	mutable DirectX::ComPtrs::ID3D11BufferPtr _VB;
	mutable TQuad *_mappedVB;
	mutable UINT _VBSize, _VBStart, _VCount;
	mutable unsigned int _count;
	mutable unsigned short _curLayer;

	// 2D scene
	typedef std::list<TQuad, Win32Heap::CWin32HeapAllocator<TQuad>> TQuads;
	class CQuadHandle;
	class CRectHandle;
	class CEllipseHandle;
	mutable TQuads _staticRects;TQuads _dynamicRects;
	mutable TQuads _staticEllipses;TQuads _dynamicEllipses;
	mutable TQuads _staticEllipsesAA;TQuads _dynamicEllipsesAA;
	mutable DirectX::ComPtrs::ID3D11BufferPtr _static2DVB, _dynamic2DVB;
	mutable UINT _dynamic2DVBSize;
	mutable bool _static2DDirty;
};

class RendererImpl::C2D::CQuadFiller
{
public:
	CQuadFiller(float x, float y, float width, float height, DGLE2::uint16 layer, float angle, DGLE2::uint32 color):
		_quad(x, y, width, height, layer, angle, color)
	{
	}
	void operator ()(void *dst)
	{
		*reinterpret_cast<TQuad *>(dst) = _quad;
	}
private:
	TQuad _quad;
};

class RendererImpl::C2D::CQuadHandle: DtorImpl::CDtor
{
protected:
	CQuadHandle(std::shared_ptr<C2D> &&parent, TQuads C2D::*const container, TQuad &&quad, bool dynamic);
	virtual ~CQuadHandle();
protected:
	const std::shared_ptr<C2D> _parent;
private:
	TQuads C2D::*const _container;
protected:
	const TQuads::iterator _quad;
};

class RendererImpl::C2D::CRectHandle: private CQuadHandle, public Interface::Instances::_2D::IRect
{
public:
	CRectHandle(
		std::shared_ptr<C2D> &&parent, bool dynamic, DGLE2::uint16 layer,
		float x, float y, float width, float height, DGLE2::uint32 color, float angle);
private:
#ifdef MSVC_LIMITATIONS
	virtual ~CRectHandle() {}
#else
	virtual ~CRectHandle() = default;
#endif

public:
	virtual void SetPos(float x, float y) override
	{
		_quad->pos.x = x;
		_quad->pos.y = y;
	}

	virtual void SetExtents(float x, float y) override
	{
		_quad->extents.x = x;
		_quad->extents.y = y;
	}

	virtual void SetColor(DGLE2::uint32 color) override
	{
		_quad->color = color;
	}

	virtual void SetAngle(float angle) override
	{
		_quad->angle = angle;
	}
};

class RendererImpl::C2D::CEllipseHandle: private CQuadHandle, public Interface::Instances::_2D::IEllipse
{
public:
	CEllipseHandle(
		std::shared_ptr<C2D> &&parent, bool dynamic, DGLE2::uint16 layer,
		float x, float y, float rx, float ry, DGLE2::uint32 color, bool AA, float angle);
private:
#ifdef MSVC_LIMITATIONS
	virtual ~CEllipseHandle() {}
#else
	virtual ~CEllipseHandle() = default;
#endif

public:
	virtual void SetPos(float x, float y) override
	{
		_quad->pos.x = x;
		_quad->pos.y = y;
	}
	virtual void SetRadii(float rx, float ry) override
	{
		_quad->extents.x = rx;
		_quad->extents.y = ry;
	}
	virtual void SetColor(DGLE2::uint32 color) override
	{
		_quad->color = color;
	}
	virtual void SetAngle(float angle) override
	{
		_quad->angle = angle;
	}
};