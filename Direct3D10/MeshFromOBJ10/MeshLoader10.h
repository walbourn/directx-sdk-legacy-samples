//--------------------------------------------------------------------------------------
// File: MeshLoader10.h
//
// Wrapper class for ID3DX10Mesh interface. Handles loading mesh data from an .obj file
// and resource management for material textures.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#ifndef _MESHLOADER10_H_
#define _MESHLOADER10_H_
#pragma once

#define ERROR_RESOURCE_VALUE 1

template<typename TYPE> BOOL IsErrorResource( TYPE data )
{
    if( ( TYPE )ERROR_RESOURCE_VALUE == data )
        return TRUE;
    return FALSE;
}

// Vertex format
struct VERTEX
{
    D3DXVECTOR3 position;
    D3DXVECTOR3 normal;
    D3DXVECTOR2 texcoord;
};


// Used for a hashtable vertex cache when creating the mesh from a .obj file
struct CacheEntry
{
    UINT index;
    CacheEntry* pNext;
};


// Material properties per mesh subset
struct Material
{
    WCHAR   strName[MAX_PATH];

    D3DXVECTOR3 vAmbient;
    D3DXVECTOR3 vDiffuse;
    D3DXVECTOR3 vSpecular;

    int nShininess;
    float fAlpha;

    bool bSpecular;

    WCHAR   strTexture[MAX_PATH];
    ID3D10ShaderResourceView* pTextureRV10;
    ID3D10EffectTechnique*  pTechnique;
};


class CMeshLoader10
{
public:
            CMeshLoader10();
            ~CMeshLoader10();

    HRESULT Create( ID3D10Device* pd3dDevice, const WCHAR* strFilename );
    void    Destroy();


    UINT    GetNumMaterials() const
    {
        return m_Materials.GetSize();
    }
    Material* GetMaterial( UINT iMaterial )
    {
        return m_Materials.GetAt( iMaterial );
    }

    UINT GetNumSubsets()
    {
        return m_NumAttribTableEntries;
    }
    Material* GetSubsetMaterial( UINT iSubset )
    {
        return m_Materials.GetAt( m_pAttribTable[iSubset].AttribId );
    }

    ID3DX10Mesh* GetMesh()
    {
        return m_pMesh;
    }
    WCHAR* GetMediaDirectory()
    {
        return m_strMediaDir;
    }

private:

    HRESULT LoadGeometryFromOBJ( const WCHAR* strFilename );
    HRESULT LoadMaterialsFromMTL( const WCHAR* strFileName );
    void    InitMaterial( Material* pMaterial );

    DWORD   AddVertex( UINT hash, VERTEX* pVertex );
    void    DeleteCache();

    ID3D10Device* m_pd3dDevice;    // Direct3D Device object associated with this mesh
    ID3DX10Mesh* m_pMesh;         // Encapsulated D3DX Mesh

    CGrowableArray <CacheEntry*> m_VertexCache;   // Hashtable cache for locating duplicate vertices
    CGrowableArray <VERTEX> m_Vertices;      // Filled and copied to the vertex buffer
    CGrowableArray <DWORD> m_Indices;       // Filled and copied to the index buffer
    CGrowableArray <DWORD> m_Attributes;    // Filled and copied to the attribute buffer
    CGrowableArray <Material*> m_Materials;     // Holds material properties per subset

    UINT        m_NumAttribTableEntries;
    D3DX10_ATTRIBUTE_RANGE *m_pAttribTable;

    WCHAR   m_strMediaDir[ MAX_PATH ];               // Directory where the mesh was found
};

#endif // _MESHLOADER_H_

