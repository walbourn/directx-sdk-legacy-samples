//--------------------------------------------------------------------------------------
// File: PackedFile.h
//
// Illustrates streaming content using Direct3D 9/10
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef PACKD_FILE_H
#define PACKD_FILE_H

#include "ResourceReuseCache.h"

//--------------------------------------------------------------------------------------
// Packed file structures
//--------------------------------------------------------------------------------------
struct PACKED_FILE_HEADER
{
    UINT64 FileSize;
    UINT64 NumFiles;
    UINT64 NumChunks;
    UINT64 Granularity;
    UINT MaxChunksInVA;

    UINT64 TileBytesSize;
    float TileSideSize;
    float LoadingRadius;
    UINT64 VideoMemoryUsageAtFullMips;
};

struct CHUNK_HEADER
{
    UINT64 ChunkOffset;
    UINT64 ChunkSize;
};

struct FILE_INDEX
{
    WCHAR szFileName[MAX_PATH];
    UINT64 FileSize;
    UINT64 ChunkIndex;
    UINT64 OffsetIntoChunk;
    D3DXVECTOR3 vCenter;
};

struct LEVEL_ITEM
{
    D3DXVECTOR3 vCenter;
    DEVICE_VERTEX_BUFFER VB;
    DEVICE_INDEX_BUFFER IB;
    DEVICE_TEXTURE Diffuse;
    DEVICE_TEXTURE Normal;
    WCHAR   szVBName[MAX_PATH];
    WCHAR   szIBName[MAX_PATH];
    WCHAR   szDiffuseName[MAX_PATH];
    WCHAR   szNormalName[MAX_PATH];
    bool bLoaded;
    bool bLoading;
    bool bInLoadRadius;
    bool bInFrustum;
    int CurrentCountdownDiff;
    int CurrentCountdownNorm;
    bool bHasBeenRenderedDiffuse;
    bool bHasBeenRenderedNormal;
};

struct MAPPED_CHUNK
{
    void* pMappingPointer;
    UINT UseCounter;
    bool bInUse;
};

struct BOX_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR3 norm;
    D3DXVECTOR3 tan;
    D3DXVECTOR2 tex;
};

//--------------------------------------------------------------------------------------
// CPackedFile class
//--------------------------------------------------------------------------------------
class CPackedFile
{
private:
    PACKED_FILE_HEADER m_FileHeader;
    FILE_INDEX* m_pFileIndices;
    CHUNK_HEADER* m_pChunks;
    MAPPED_CHUNK* m_pMappedChunks;

    HANDLE m_hFile;
    HANDLE m_hFileMapping;
    UINT m_ChunksMapped;
    UINT m_MaxChunksMapped;
    UINT m_CurrentUseCounter;

public:
            CPackedFile();
            ~CPackedFile();

    bool    CreatePackedFile( ID3D10Device* pDev10, IDirect3DDevice9* pDev9, WCHAR* szFileName, UINT SqrtNumTiles,
                              UINT SidesPerTile, float fWorldScale, float fHeightScale );
    bool    LoadPackedFile( WCHAR* szFileName, bool b64Bit, CGrowableArray <LEVEL_ITEM*>* pLevelItemArray );
    void    UnloadPackedFile();
    void    EnsureChunkMapped( UINT64 iChunk );
    bool    GetPackedFileInfo( char* szFile, UINT* pDataBytes );
    bool    GetPackedFileInfo( WCHAR* szFile, UINT* pDataBytes );
    bool    GetPackedFile( char* szFile, BYTE** ppData, UINT* pDataBytes );
    bool    GetPackedFile( WCHAR* szFile, BYTE** ppData, UINT* pDataBytes );
    bool    UsingMemoryMappedIO();

    void    SetMaxChunksMapped( UINT maxmapped );
    UINT64  GetTileBytesSize();
    float   GetTileSideSize();
    float   GetLoadingRadius();
    UINT    GetMaxChunksMapped();
    UINT    GetMaxChunksInVA();
    UINT64  GetNumChunks();
    UINT64  GetVideoMemoryUsageAtFullMips();
};

#endif
