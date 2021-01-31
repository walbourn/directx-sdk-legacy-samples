//--------------------------------------------------------------------------------------
// File: ResourceReuseCache.h
//
// Illustrates streaming content using Direct3D 9/10
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef RESOURCE_REUSE_CACHE_H
#define RESOURCE_REUSE_CACHE_H

#define USE_D3D10_STAGING_RESOURCES 1

#include "DXUTmisc.h"
#include "dds.h"

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------
#define E_TRYAGAIN MAKE_HRESULT(1,FACILITY_WIN32,123456)

enum LOADER_DEVICE_TYPE
{
    LDT_D3D10 = 0x0,
    LDT_D3D9,
};

//--------------------------------------------------------------------------------------
// structures
//--------------------------------------------------------------------------------------
struct LOADER_DEVICE
{
LOADER_DEVICE_TYPE Type;
union
{
LPDIRECT3DDEVICE9 pDev9;
ID3D10Device*     pDev10;
};

LOADER_DEVICE() {}
LOADER_DEVICE( ID3D10Device* pDevice ) { pDev10 = pDevice; Type = LDT_D3D10; }
LOADER_DEVICE( IDirect3DDevice9* pDevice ) { pDev9 = pDevice; Type = LDT_D3D9; }
};

struct DEVICE_TEXTURE
{
UINT Width;
UINT Height;
UINT MipLevels;
UINT Format;
union
{
ID3D10ShaderResourceView* pRV10;
IDirect3DTexture9*        pTexture9;
UINT64                    Align64bit;

};
#if defined(USE_D3D10_STAGING_RESOURCES)
ID3D10Texture2D*			  pStaging10;
#endif

UINT64 EstimatedSize;
BOOL bInUse;
UINT RecentUseCounter;
};

struct DEVICE_VERTEX_BUFFER
{
UINT iSizeBytes;

union
{
ID3D10Buffer*            pVB10;
IDirect3DVertexBuffer9*  pVB9;
UINT64                   Align64bit;
};

BOOL bInUse;
UINT RecentUseCounter;
};

struct DEVICE_INDEX_BUFFER
{
UINT iSizeBytes;
UINT ibFormat;

union
{
ID3D10Buffer*            pIB10;
IDirect3DIndexBuffer9*   pIB9;
UINT64                   Align64bit;
};

BOOL bInUse;
UINT RecentUseCounter;
};

//--------------------------------------------------------------------------------------
// CResourceReuseCache class
//--------------------------------------------------------------------------------------
class CResourceReuseCache
{
private:
LOADER_DEVICE							m_Device;
CGrowableArray<DEVICE_TEXTURE*>			m_TextureList;
CGrowableArray<DEVICE_VERTEX_BUFFER*>	m_VBList;
CGrowableArray<DEVICE_INDEX_BUFFER*>	m_IBList;
UINT64									m_MaxManagedMemory;
UINT64									m_UsedManagedMemory;
BOOL									m_bSilent;
BOOL									m_bDontCreateResources;

int FindTexture( ID3D10ShaderResourceView* pRV10 );
int FindTexture( IDirect3DTexture9* pTex9 );
int EnsureFreeTexture( UINT Width, UINT Height, UINT MipLevels, UINT Format );
UINT64 GetEstimatedSize( UINT Width, UINT Height, UINT MipLevels, UINT Format );

int FindVB( ID3D10Buffer* pVB );
int FindVB( IDirect3DVertexBuffer9* pVB );
int EnsureFreeVB( UINT iSizeBytes );

int FindIB( ID3D10Buffer* pIB );
int FindIB( IDirect3DIndexBuffer9* pIB );
int EnsureFreeIB( UINT iSizeBytes, UINT ibFormat );

void DestroyTexture9( DEVICE_TEXTURE* pTex );
void DestroyTexture10( DEVICE_TEXTURE* pTex );
void DestroyVB9( DEVICE_VERTEX_BUFFER* pVB );
void DestroyVB10( DEVICE_VERTEX_BUFFER* pVB );
void DestroyIB9( DEVICE_INDEX_BUFFER* pVB );
void DestroyIB10( DEVICE_INDEX_BUFFER* pVB );

public:
CResourceReuseCache( ID3D10Device* pDev );
CResourceReuseCache( LPDIRECT3DDEVICE9 pDev );
~CResourceReuseCache();

// memory handling
void SetMaxManagedMemory( UINT64 MaxMemory );
UINT64 GetMaxManagedMemory();
UINT64 GetUsedManagedMemory();
void SetDontCreateResources( BOOL bDontCreateResources );
UINT64 DestroyLRUTexture();
UINT64 DestroyLRUVB();
UINT64 DestroyLRUIB();
void DestroyLRUResources( UINT64 SizeGainNeeded );

// texture functions
ID3D10ShaderResourceView* GetFreeTexture10( UINT Width, UINT Height, UINT MipLevels, UINT Format,
                                            ID3D10Texture2D** ppStaging10 );
IDirect3DTexture9* GetFreeTexture9( UINT Width, UINT Height, UINT MipLevels, UINT Format );
void UnuseDeviceTexture10( ID3D10ShaderResourceView* pRV );
void UnuseDeviceTexture9( IDirect3DTexture9* pTexture );
int GetNumTextures();
DEVICE_TEXTURE* GetTexture( int i );

// vertex buffer functions
ID3D10Buffer* GetFreeVB10( UINT sizeBytes );
IDirect3DVertexBuffer9* GetFreeVB9( UINT sizeBytes );
void UnuseDeviceVB10( ID3D10Buffer* pVB );
void UnuseDeviceVB9( IDirect3DVertexBuffer9* pVB );
int GetNumVBs();
DEVICE_VERTEX_BUFFER* GetVB( int i );

// index buffer functions
ID3D10Buffer* GetFreeIB10( UINT sizeBytes, UINT ibFormat );
IDirect3DIndexBuffer9* GetFreeIB9( UINT sizeBytes, UINT ibFormat );
void UnuseDeviceIB10( ID3D10Buffer* pVB );
void UnuseDeviceIB9( IDirect3DIndexBuffer9* pVB );
int GetNumIBs();
DEVICE_INDEX_BUFFER* GetIB( int i );

void OnDestroy();

};

D3DFORMAT GetD3D9Format( DDS_PIXELFORMAT ddpf );
void GetSurfaceInfo( UINT width, UINT height, D3DFORMAT fmt, UINT* pNumBytes, UINT* pRowBytes, UINT* pNumRows );

#endif
