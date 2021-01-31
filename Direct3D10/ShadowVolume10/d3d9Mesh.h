//-----------------------------------------------------------------------------
// File: D3D9Mesh.h
//
// Desc: Support code for loading DirectX .X files.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Class for loading and rendering file-based meshes using D3D9
//-----------------------------------------------------------------------------
class CD3D9Mesh
{
public:
    WCHAR       m_strName[512];
    LPD3DXMESH m_pMesh;   // Managed mesh

    // Cache of data in m_pMesh for easy access
    IDirect3DVertexBuffer9* m_pVB;
    IDirect3DIndexBuffer9* m_pIB;
    IDirect3DVertexDeclaration9* m_pDecl;
    DWORD m_dwNumVertices;
    DWORD m_dwNumFaces;
    DWORD m_dwBytesPerVertex;

    DWORD m_dwNumMaterials; // Materials for the mesh
    D3DMATERIAL9* m_pMaterials;
    CHAR        (*m_strMaterials )[MAX_PATH];
    IDirect3DBaseTexture9** m_pTextures;
    bool m_bUseMaterials;

public:
    // Rendering
    HRESULT     Render( LPDIRECT3DDEVICE9 pd3dDevice,
                        bool bDrawOpaqueSubsets = true,
                        bool bDrawAlphaSubsets = true );
    HRESULT     Render( ID3DXEffect* pEffect,
                        D3DXHANDLE hTexture = NULL,
                        D3DXHANDLE hDiffuse = NULL,
                        D3DXHANDLE hAmbient = NULL,
                        D3DXHANDLE hSpecular = NULL,
                        D3DXHANDLE hEmissive = NULL,
                        D3DXHANDLE hPower = NULL,
                        bool bDrawOpaqueSubsets = true,
                        bool bDrawAlphaSubsets = true );

    // Mesh access
    LPD3DXMESH  GetMesh()
    {
        return m_pMesh;
    }

    // Rendering options
    void        UseMeshMaterials( bool bFlag )
    {
        m_bUseMaterials = bFlag;
    }
    HRESULT     SetFVF( LPDIRECT3DDEVICE9 pd3dDevice, DWORD dwFVF );
    HRESULT     SetVertexDecl( LPDIRECT3DDEVICE9 pd3dDevice, const D3DVERTEXELEMENT9* pDecl,
                               bool bAutoComputeNormals = true, bool bAutoComputeTangents = true,
                               bool bSplitVertexForOptimalTangents = false );

    // Initializing
    HRESULT     RestoreDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice );
    HRESULT     InvalidateDeviceObjects();

    // Creation/destruction
    HRESULT     Create( LPDIRECT3DDEVICE9 pd3dDevice, LPCWSTR strFilename );
    HRESULT     Create( LPDIRECT3DDEVICE9 pd3dDevice, LPD3DXFILEDATA pFileData );
    HRESULT     Create( LPDIRECT3DDEVICE9 pd3dDevice, ID3DXMesh* pInMesh, D3DXMATERIAL* pd3dxMaterials,
                        DWORD dwMaterials );
    HRESULT     CreateMaterials( LPCWSTR strPath, IDirect3DDevice9* pd3dDevice, D3DXMATERIAL* d3dxMtrls,
                                 DWORD dwNumMaterials );
    HRESULT     Destroy();

                CD3D9Mesh( LPCWSTR strName = L"CD3D9MeshFile_Mesh" );
    virtual     ~CD3D9Mesh();
};
