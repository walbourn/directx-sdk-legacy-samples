//--------------------------------------------------------------------------------------
// File: PRTMesh.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "prtmesh.h"
#include <stdio.h>

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 

//--------------------------------------------------------------------------------------
CPRTMesh::CPRTMesh( void )
{
    m_pMesh = NULL;
    m_pPRTBuffer = NULL;
    m_pPRTCompBuffer = NULL;
    m_pPRTEffect = NULL;
    m_pSHIrradEnvMapEffect = NULL;
    m_pNDotLEffect = NULL;
    m_fObjectRadius = 0.0f;
    m_vObjectCenter = D3DXVECTOR3( 0, 0, 0 );
    m_aPRTClusterBases = NULL;
    m_dwPRTOrder = 0;
    m_aPRTConstants = NULL;
    m_pMaterials = NULL;
    m_dwNumMaterials = 0;

    m_pLDPRTShadingNormals = NULL;
    m_pLDPRTBuffer = NULL;
    m_pLDPRTMesh = NULL;
    m_pLDPRTEffect = NULL;
    m_hValidLDPRTTechnique = NULL;
    m_hValidLDPRTTechniqueWithTex = NULL;

    for( int i = 0; i < 9; i++ )
        m_pSHBasisTextures[i] = NULL;

    ZeroMemory( &m_ReloadState, sizeof( RELOAD_STATE ) );
}


//--------------------------------------------------------------------------------------
CPRTMesh::~CPRTMesh( void )
{
    SAFE_RELEASE( m_pMesh );
    SAFE_RELEASE( m_pPRTBuffer );
    SAFE_RELEASE( m_pPRTCompBuffer );
    SAFE_RELEASE( m_pLDPRTMesh );
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, D3DFORMAT fmt )
{
    int i;

    m_pd3dDevice = pd3dDevice;

    if( m_ReloadState.bUseReloadState )
    {
        LoadMesh( pd3dDevice, m_ReloadState.strMeshFileName );
        if( m_ReloadState.bLoadCompressed )
        {
            LoadCompPRTBufferFromFile( m_ReloadState.strPRTBufferFileName );
        }
        else
        {
            LoadPRTBufferFromFile( m_ReloadState.strPRTBufferFileName );
            CompressPRTBuffer( m_ReloadState.quality, m_ReloadState.dwNumClusters, m_ReloadState.dwNumPCA );
        }
        LoadLDPRTFromFiles( m_ReloadState.strLDPRTFile, m_ReloadState.strShadingNormalsFile );
        ExtractCompressedDataForPRTShader();
        LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );
    }

    // Fill the cube textures with SH basis functions
    for( i = 0; i < 9; i++ )
    {
        HRESULT hr;
        V( D3DXCreateCubeTexture( pd3dDevice, 32, 1, 0, fmt, D3DPOOL_MANAGED, &m_pSHBasisTextures[i] ) );

        D3DXFillCubeTexture( m_pSHBasisTextures[i], StaticFillCubeTextureWithSHCallback, ( LPVOID )( LONG_PTR )
                             ( i * 4 ) );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Callback to fill cube texture with SH basis functions 
//--------------------------------------------------------------------------------------
VOID WINAPI CPRTMesh::StaticFillCubeTextureWithSHCallback( D3DXVECTOR4* pOut,
                          CONST D3DXVECTOR3* pTexCoord,
                          CONST D3DXVECTOR3* pTexelSize,
                          LPVOID pData )
 {
    unsigned int uStart = ( unsigned int ) ( LONG_PTR ) pData;

    D3DXVECTOR3 nrm;
    D3DXVec3Normalize( &nrm,pTexCoord );
    float fBF[36];

    D3DXSHEvalDirection( fBF,6,&nrm );

    D3DXVECTOR4 vUse;
    for( int i=0;i<4;i++ ) vUse[i] = fBF[uStart+i];

    *pOut = vUse;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::OnResetDevice()
{
    HRESULT hr;
    if( m_pPRTEffect )
        V( m_pPRTEffect->OnResetDevice() );
    if( m_pSHIrradEnvMapEffect )
        V( m_pSHIrradEnvMapEffect->OnResetDevice() );
    if( m_pNDotLEffect )
        V( m_pNDotLEffect->OnResetDevice() );
    if( m_pLDPRTEffect )
        V( m_pLDPRTEffect->OnResetDevice() );

    // Cache this information for picking
    m_pd3dDevice->GetViewport( &m_ViewPort );

    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT AttribSortMesh( ID3DXMesh** ppInOutMesh )
{
    HRESULT hr;
    ID3DXMesh* pInputMesh = *ppInOutMesh;

    // Attribute sort the faces & vertices.  This call will sort the verts independent of hardware
    ID3DXMesh* pTempMesh = NULL;
    DWORD* rgdwAdjacency = new DWORD[pInputMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
        return E_OUTOFMEMORY;
    DWORD* rgdwAdjacencyOut = new DWORD[pInputMesh->GetNumFaces() * 3];
    if( rgdwAdjacencyOut == NULL )
    {
        delete []rgdwAdjacency;
        return E_OUTOFMEMORY;
    }
    V( pInputMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    V( pInputMesh->Optimize( D3DXMESHOPT_ATTRSORT,
                             rgdwAdjacency, rgdwAdjacencyOut, NULL, NULL, &pTempMesh ) );
    SAFE_RELEASE( pInputMesh );
    pInputMesh = pTempMesh;

    // Sort just the faces for optimal vertex cache perf on the current HW device.
    // But note that the vertices are not sorted for optimal vertex cache because
    // this would reorder the vertices depending on the HW which would make it difficult 
    // to precompute the PRT results for all HW.  Alternatively you could do the vcache 
    // optimize with vertex reordering after putting the PRT data into the VB for a 
    // more optimal result
    if( rgdwAdjacency == NULL )
        return E_OUTOFMEMORY;
    V( pInputMesh->Optimize( D3DXMESHOPT_VERTEXCACHE | D3DXMESHOPT_IGNOREVERTS,
                             rgdwAdjacencyOut, NULL, NULL, NULL, &pTempMesh ) );
    delete []rgdwAdjacency;
    delete []rgdwAdjacencyOut;
    SAFE_RELEASE( pInputMesh );
    *ppInOutMesh = pTempMesh;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This function loads the mesh and ensures the mesh has normals; it also optimizes the 
// mesh for the graphics card's vertex cache, which improves performance by organizing 
// the internal triangle list for less cache misses.
//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strMeshFileName )
{
    WCHAR str[MAX_PATH];
    HRESULT hr;

    // Release any previous mesh object
    SAFE_RELEASE( m_pMesh );
    SAFE_RELEASE( m_pMaterialBuffer );
    for( int i = 0; i < m_pAlbedoTextures.GetSize(); i++ )
    {
        SAFE_RELEASE( m_pAlbedoTextures[i] );
    }
    m_pAlbedoTextures.RemoveAll();

    // Load the mesh object
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strMeshFileName ) );
    wcscpy_s( m_ReloadState.strMeshFileName, MAX_PATH, str );
    V_RETURN( D3DXLoadMeshFromX( str, D3DXMESH_MANAGED, pd3dDevice, NULL,
                                 &m_pMaterialBuffer, NULL, &m_dwNumMaterials, &m_pMesh ) );
    m_pMaterials = ( D3DXMATERIAL* )m_pMaterialBuffer->GetBufferPointer();

    // Change the current directory to the mesh's directory so we can
    // find the textures.
    WCHAR* pLastSlash = wcsrchr( str, L'\\' );
    if( pLastSlash )
        *( pLastSlash + 1 ) = 0;
    WCHAR strCWD[MAX_PATH];
    GetCurrentDirectory( MAX_PATH, strCWD );
    SetCurrentDirectory( str );

    // Lock the vertex buffer to get the object's radius & center
    // simply to help position the camera a good distance away from the mesh.
    IDirect3DVertexBuffer9* pVB = NULL;
    void* pVertices;
    V_RETURN( m_pMesh->GetVertexBuffer( &pVB ) );
    V_RETURN( pVB->Lock( 0, 0, &pVertices, 0 ) );

    D3DVERTEXELEMENT9 Declaration[MAXD3DDECLLENGTH + 1];
    m_pMesh->GetDeclaration( Declaration );
    DWORD dwStride = D3DXGetDeclVertexSize( Declaration, 0 );
    V_RETURN( D3DXComputeBoundingSphere( ( D3DXVECTOR3* )pVertices, m_pMesh->GetNumVertices(),
                                         dwStride, &m_vObjectCenter,
                                         &m_fObjectRadius ) );

    pVB->Unlock();
    SAFE_RELEASE( pVB );

    // Make the mesh have a known decl in order to pass per vertex CPCA 
    // data to the shader
    V_RETURN( AdjustMeshDecl( pd3dDevice, &m_pMesh ) );

    AttribSortMesh( &m_pMesh );
    AttribSortMesh( &m_pLDPRTMesh );

    for( UINT i = 0; i < m_dwNumMaterials; i++ )
    {
        if ( m_pMaterials[i].pTextureFilename ) 
        {
            // First attempt to look for texture in the same folder as the input folder.
            WCHAR strTextureTemp[MAX_PATH];
            MultiByteToWideChar( CP_ACP, 0, m_pMaterials[i].pTextureFilename, -1, strTextureTemp, MAX_PATH );
            strTextureTemp[MAX_PATH - 1] = 0;

            // Create the mesh texture from a file
            if( SUCCEEDED( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strTextureTemp ) ) )
            {
                IDirect3DTexture9* pAlbedoTexture;
                V( D3DXCreateTextureFromFileEx( pd3dDevice, str, D3DX_DEFAULT, D3DX_DEFAULT,
                                            D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                                            D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                            NULL, NULL, &pAlbedoTexture ) );
                m_pAlbedoTextures.Add( pAlbedoTexture );
                continue;
            }
        }

        m_pAlbedoTextures.Add( NULL );
    }

    SetCurrentDirectory( strCWD );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::SetMesh( IDirect3DDevice9* pd3dDevice, ID3DXMesh* pMesh )
{
    HRESULT hr;

    // Release any previous mesh object
    SAFE_RELEASE( m_pMesh );

    m_pMesh = pMesh;

    V( AdjustMeshDecl( pd3dDevice, &m_pMesh ) );

    // Sort the attributes
    DWORD* rgdwAdjacency = NULL;
    rgdwAdjacency = new DWORD[m_pMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
        return E_OUTOFMEMORY;
    V( m_pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    V( m_pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE | D3DXMESHOPT_ATTRSORT | D3DXMESHOPT_IGNOREVERTS,
                                 rgdwAdjacency, NULL, NULL, NULL ) );
    delete []rgdwAdjacency;

    // Sort the attributes
    rgdwAdjacency = new DWORD[m_pLDPRTMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
        return E_OUTOFMEMORY;
    V( m_pLDPRTMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    V( m_pLDPRTMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE | D3DXMESHOPT_ATTRSORT | D3DXMESHOPT_IGNOREVERTS,
                                      rgdwAdjacency, NULL, NULL, NULL ) );
    delete []rgdwAdjacency;

    return S_OK;
}


//-----------------------------------------------------------------------------
bool DoesMeshHaveUsage( ID3DXMesh* pMesh, BYTE Usage )
{
    D3DVERTEXELEMENT9 decl[MAX_FVF_DECL_SIZE];
    pMesh->GetDeclaration( decl );

    for( int di = 0; di < MAX_FVF_DECL_SIZE; di++ )
    {
        if( decl[di].Usage == Usage )
            return true;
        if( decl[di].Stream == 255 )
            return false;
    }

    return false;
}


//-----------------------------------------------------------------------------
// Make the mesh have a known decl in order to pass per vertex CPCA 
// data to the shader
//-----------------------------------------------------------------------------
HRESULT CPRTMesh::AdjustMeshDecl( IDirect3DDevice9* pd3dDevice, ID3DXMesh** ppMesh )
{
    HRESULT hr;
    LPD3DXMESH pInMesh = *ppMesh;
    LPD3DXMESH pOutMesh = NULL;

    D3DVERTEXELEMENT9 decl[MAX_FVF_DECL_SIZE] =
    {
        {0,  0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0,  12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
        {0,  24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},

        {0,  32, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0},
        {0,  36, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 1},
        {0,  52, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 2},
        {0,  68, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 3},
        {0,  84, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 4},
        {0, 100, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 5},
        {0, 116, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 6},
        D3DDECL_END()
    };

    // To do CPCA, we need to store (g_dwNumPCAVectors + 1) scalers per vertex, so 
    // make the mesh have a known decl to store this data.  Since we can't do 
    // skinning and PRT at once, we use D3DDECLUSAGE_BLENDWEIGHT[0] 
    // to D3DDECLUSAGE_BLENDWEIGHT[6] to store our per vertex data needed for PRT.
    // Notice that D3DDECLUSAGE_BLENDWEIGHT[0] is a float1, and
    // D3DDECLUSAGE_BLENDWEIGHT[1]-D3DDECLUSAGE_BLENDWEIGHT[6] are float4.  This allows 
    // up to 24 PCA weights and 1 float that gives the vertex shader 
    // an index into the vertex's cluster's data
    V( pInMesh->CloneMesh( pInMesh->GetOptions(), decl, pd3dDevice, &pOutMesh ) );

    D3DVERTEXELEMENT9 declLDPRT[MAX_FVF_DECL_SIZE] =
    {
        {0,  0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0,  12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
        {0,  24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},

        {0,  32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
        {0,  48, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
        {0,  64, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3},
        {0,  80, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 4},
        {0,  96, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 5},
        D3DDECL_END()
    };

    // Create a mesh that to store LDPRT (zonal harmonic) coeffs to render LDPRT
    SAFE_RELEASE( m_pLDPRTMesh );
    V( pInMesh->CloneMesh( pInMesh->GetOptions(), declLDPRT, pd3dDevice, &m_pLDPRTMesh ) );

    // Make sure there are normals which are required for lighting
    if( !DoesMeshHaveUsage( pInMesh, D3DDECLUSAGE_NORMAL ) )
        V( D3DXComputeNormals( pOutMesh, NULL ) );

    SAFE_RELEASE( pInMesh );

    *ppMesh = pOutMesh;

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::LoadPRTBufferFromFile( WCHAR* strFile )
{
    HRESULT hr;
    SAFE_RELEASE( m_pPRTBuffer );
    SAFE_RELEASE( m_pPRTCompBuffer );

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFile ) );
    wcscpy_s( m_ReloadState.strPRTBufferFileName, MAX_PATH, str );

    V_RETURN( D3DXLoadPRTBufferFromFile( str, &m_pPRTBuffer ) );
    m_dwPRTOrder = GetOrderFromNumCoeffs( m_pPRTBuffer->GetNumCoeffs() );
    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::CreateLDPRTData()
{
    HRESULT hr;
    ID3DXPRTEngine* pPRTEngine;
    ID3DXPRTBuffer* pBufferC;
    D3DXVECTOR3* pShadingNormals = NULL;

    DWORD* pdwAdj = new DWORD[m_pMesh->GetNumFaces() * 3];
    m_pMesh->GenerateAdjacency( 1e-6f, pdwAdj );
    V( D3DXCreatePRTEngine( m_pMesh, pdwAdj, false, NULL, &pPRTEngine ) );
    delete[] pdwAdj;

    // Create LDPRT coeffs from PRT simulator data and store the data in the mesh
    DWORD dwNumSamples = m_pPRTBuffer->GetNumSamples();
    assert( dwNumSamples > 0 );
    DWORD dwNumChannels = m_pPRTCompBuffer->GetNumChannels();
    V_RETURN( D3DXCreatePRTBuffer( dwNumSamples, m_dwPRTOrder, dwNumChannels, &pBufferC ) );
    pShadingNormals = new D3DXVECTOR3[dwNumSamples];
    V_RETURN( pPRTEngine->ComputeLDPRTCoeffs( m_pPRTBuffer, m_dwPRTOrder, pShadingNormals, pBufferC ) );
    SetLDPRTData( pBufferC, pShadingNormals ); // m_pPRTMesh will free pShadingNormals when done

    SAFE_RELEASE( pBufferC );
    SAFE_RELEASE( pPRTEngine );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::LoadLDPRTFromFiles( WCHAR* strLDPRTFile, WCHAR* strShadingNormalsFile )
{
    HRESULT hr;
    ID3DXPRTBuffer* pLDPRTBuffer;

    wcscpy_s( m_ReloadState.strLDPRTFile, MAX_PATH, strLDPRTFile );
    wcscpy_s( m_ReloadState.strShadingNormalsFile, MAX_PATH, strShadingNormalsFile );

    if( GetFileAttributes( strShadingNormalsFile ) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributes( strLDPRTFile ) == INVALID_FILE_ATTRIBUTES )
        return E_FAIL;

    V_RETURN( D3DXLoadPRTBufferFromFile( strLDPRTFile, &pLDPRTBuffer ) );

    DWORD dwNumSamples = pLDPRTBuffer->GetNumSamples();
    D3DXVECTOR3* pShadingNormals = new D3DXVECTOR3[dwNumSamples];
    if( pShadingNormals == NULL )
        return E_OUTOFMEMORY;

    HANDLE hFile = CreateFile( strShadingNormalsFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL );
    if( hFile )
    {
        DWORD dwRead;
        ReadFile( hFile, ( BYTE* )pShadingNormals, sizeof( D3DXVECTOR3 ) * dwNumSamples, &dwRead, NULL );
        CloseHandle( hFile );
        if( dwRead != sizeof( D3DXVECTOR3 ) * dwNumSamples )
        {
            SAFE_DELETE_ARRAY( pShadingNormals );
            return E_FAIL;
        }
    }

    SetLDPRTData( pLDPRTBuffer, pShadingNormals );
    SAFE_RELEASE( pLDPRTBuffer );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::SavePRTBufferToFile( WCHAR* strFile )
{
    HRESULT hr;
    V( D3DXSavePRTBufferToFile( strFile, m_pPRTBuffer ) );
    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::SaveCompPRTBufferToFile( WCHAR* strFile )
{
    HRESULT hr;
    V( D3DXSavePRTCompBufferToFile( strFile, m_pPRTCompBuffer ) );
    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::SaveLDPRTToFiles( WCHAR* strLDPRTFile, WCHAR* strShadingNormalsFile )
{
    HRESULT hr;

    wcscpy_s( m_ReloadState.strLDPRTFile, MAX_PATH, strLDPRTFile );
    wcscpy_s( m_ReloadState.strShadingNormalsFile, MAX_PATH, strShadingNormalsFile );
    V( D3DXSavePRTBufferToFile( strLDPRTFile, m_pLDPRTBuffer ) );

    HANDLE hFile = CreateFile( strShadingNormalsFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                               NULL );
    if( hFile )
    {
        DWORD dwWritten;
        unsigned int dwNumSamples = m_pLDPRTBuffer->GetNumSamples();
        WriteFile( hFile, m_pLDPRTShadingNormals, sizeof( D3DXVECTOR3 ) * dwNumSamples, &dwWritten, NULL );
        CloseHandle( hFile );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::LoadCompPRTBufferFromFile( WCHAR* strFile )
{
    HRESULT hr;
    SAFE_RELEASE( m_pPRTCompBuffer );

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFile ) );
    wcscpy_s( m_ReloadState.strPRTBufferFileName, MAX_PATH, str );

    V( D3DXLoadPRTCompBufferFromFile( str, &m_pPRTCompBuffer ) );
    m_ReloadState.bUseReloadState = true;
    m_ReloadState.bLoadCompressed = true;
    m_dwPRTOrder = GetOrderFromNumCoeffs( m_pPRTCompBuffer->GetNumCoeffs() );
    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTMesh::CompressPRTBuffer( D3DXSHCOMPRESSQUALITYTYPE Quality, UINT NumClusters, UINT NumPCA,
                                     LPD3DXSHPRTSIMCB pCB, LPVOID lpUserContext )
{
    HRESULT hr;
    assert( m_pPRTBuffer != NULL );

    ID3DXPRTCompBuffer* pNewPRTCompBuffer;

    hr = D3DXCreatePRTCompBuffer( Quality, NumClusters, NumPCA, pCB, lpUserContext, m_pPRTBuffer, &pNewPRTCompBuffer );
    if( FAILED( hr ) )
        return hr;// handle user aborting compression via callback 

    SAFE_RELEASE( m_pPRTCompBuffer );
    m_pPRTCompBuffer = pNewPRTCompBuffer;

    m_ReloadState.quality = Quality;
    m_ReloadState.dwNumClusters = NumClusters;
    m_ReloadState.dwNumPCA = NumPCA;
    m_ReloadState.bUseReloadState = true;
    m_ReloadState.bLoadCompressed = false;
    m_dwPRTOrder = GetOrderFromNumCoeffs( m_pPRTBuffer->GetNumCoeffs() );
    return S_OK;
}


//--------------------------------------------------------------------------------------
void CPRTMesh::SetPRTBuffer( ID3DXPRTBuffer* pPRTBuffer, WCHAR* strFile )
{
    SAFE_RELEASE( m_pPRTBuffer );
    SAFE_RELEASE( m_pPRTCompBuffer );
    m_pPRTBuffer = pPRTBuffer;
    m_pPRTBuffer->AddRef();
    m_dwPRTOrder = GetOrderFromNumCoeffs( m_pPRTBuffer->GetNumCoeffs() );
    wcscpy_s( m_ReloadState.strPRTBufferFileName, MAX_PATH, strFile );
}


//-----------------------------------------------------------------------------
HRESULT CPRTMesh::LoadEffects( IDirect3DDevice9* pd3dDevice, const D3DCAPS9* pDeviceCaps )
{
    HRESULT hr;

    UINT dwNumChannels = m_pPRTCompBuffer->GetNumChannels();
    UINT dwNumClusters = m_pPRTCompBuffer->GetNumClusters();
    UINT dwNumPCA = m_pPRTCompBuffer->GetNumPCA();

    // The number of vertex consts need by the shader can't exceed the 
    // amount the HW can support
    DWORD dwNumVConsts = dwNumClusters * ( 1 + dwNumChannels * dwNumPCA / 4 ) + 4;
    if( dwNumVConsts > pDeviceCaps->MaxVertexShaderConst )
        return E_FAIL;

    SAFE_RELEASE( m_pPRTEffect );
    SAFE_RELEASE( m_pSHIrradEnvMapEffect );
    SAFE_RELEASE( m_pNDotLEffect );
    SAFE_RELEASE( m_pLDPRTEffect );

    D3DXMACRO aDefines[3];
    CHAR szMaxNumClusters[64];
    sprintf_s( szMaxNumClusters, 64, "%d", dwNumClusters );
    szMaxNumClusters[63] = 0;
    CHAR szMaxNumPCA[64];
    sprintf_s( szMaxNumPCA, 64, "%d", dwNumPCA );
    szMaxNumPCA[63] = 0;
    aDefines[0].Name = "NUM_CLUSTERS";
    aDefines[0].Definition = szMaxNumClusters;
    aDefines[1].Name = "NUM_PCA";
    aDefines[1].Definition = szMaxNumPCA;
    aDefines[2].Name = NULL;
    aDefines[2].Definition = NULL;

    // Define DEBUG_VS and/or DEBUG_PS to debug vertex and/or pixel shaders with the shader debugger.  
    // Debugging vertex shaders requires either REF or software vertex processing, and debugging 
    // pixel shaders requires REF.  The D3DXSHADER_FORCE_*_SOFTWARE_NOOPT flag improves the debug 
    // experience in the shader debugger.  It enables source level debugging, prevents instruction 
    // reordering, prevents dead code elimination, and forces the compiler to compile against the next 
    // higher available software target, which ensures that the unoptimized shaders do not exceed 
    // the shader model limitations.  Setting these flags will cause slower rendering since the shaders 
    // will be unoptimized and forced into software.  See the DirectX documentation for more information 
    // about using the shader debugger.
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;

#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DXSHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DXSHADER_DEBUG;
    #endif

#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "PRT.fx" ) ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V( D3DXCreateEffectFromFile( pd3dDevice, str, aDefines, NULL,
                                 dwShaderFlags, NULL, &m_pPRTEffect, NULL ) );

    // Make sure the technique works on this card
    hr = m_pPRTEffect->ValidateTechnique( "RenderWithPRTColorLights" );
    if( FAILED( hr ) )
        return DXTRACE_ERR( TEXT( "ValidateTechnique" ), hr );

    V( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "NdotL.fx" ) ) );
    V( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL,
                                 dwShaderFlags, NULL, &m_pNDotLEffect, NULL ) );

    V( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "SHIrradianceEnvMap.fx" ) ) );
    V( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL,
                                 dwShaderFlags, NULL, &m_pSHIrradEnvMapEffect, NULL ) );


    V( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "LDPRT.fx" ) ) );

    V( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL,
                                 dwShaderFlags, NULL, &m_pLDPRTEffect, NULL ) );

    // Find an LDPRT technique that works
    m_pLDPRTEffect->FindNextValidTechnique( NULL, &m_hValidLDPRTTechnique );
    m_pLDPRTEffect->FindNextValidTechnique( "RenderFullB", &m_hValidLDPRTTechniqueWithTex );

    return S_OK;
}


//--------------------------------------------------------------------------------------
void CPRTMesh::ExtractCompressedDataForPRTShader()
{
    HRESULT hr;

    // First call ID3DXPRTCompBuffer::NormalizeData.  This isn't nessacary, 
    // but it makes it easier to use data formats that have little presision.
    // It normalizes the PCA weights so that they are between [-1,1]
    // and modifies the basis vectors accordingly.  
    V( m_pPRTCompBuffer->NormalizeData() );

    UINT dwNumSamples = m_pPRTCompBuffer->GetNumSamples();
    UINT dwNumCoeffs = m_pPRTCompBuffer->GetNumCoeffs();
    UINT dwNumChannels = m_pPRTCompBuffer->GetNumChannels();
    UINT dwNumClusters = m_pPRTCompBuffer->GetNumClusters();
    UINT dwNumPCA = m_pPRTCompBuffer->GetNumPCA();

    // With clustered PCA, each vertex is assigned to a cluster.  To figure out 
    // which vertex goes with which cluster, call ID3DXPRTCompBuffer::ExtractClusterIDs.
    // This will return a cluster ID for every vertex.  Simply pass in an array of UINTs
    // that is the size of the number of vertices (which also equals the number of samples), and 
    // the cluster ID for vertex N will be at puClusterIDs[N].
    UINT* pClusterIDs = new UINT[ dwNumSamples ];
    assert( pClusterIDs );
    if( pClusterIDs == NULL )
        return;
    V( m_pPRTCompBuffer->ExtractClusterIDs( pClusterIDs ) );

    D3DVERTEXELEMENT9 declCur[MAX_FVF_DECL_SIZE];
    m_pMesh->GetDeclaration( declCur );

    // Now use this cluster ID info to store a value in the mesh in the 
    // D3DDECLUSAGE_BLENDWEIGHT[0] which is declared in the vertex decl to be a float1
    // This value will be passed into the vertex shader to allow the shader 
    // use this number as an offset into an array of shader constants.  
    // The value we set per vertex is based on the cluster ID and the stride 
    // of the shader constants array.  
    BYTE* pV = NULL;
    V( m_pMesh->LockVertexBuffer( 0, ( void** )&pV ) );
    UINT uStride = m_pMesh->GetNumBytesPerVertex();
    BYTE* pClusterID = pV + 32; // 32 == D3DDECLUSAGE_BLENDWEIGHT[0] offset
    for( UINT uVert = 0; uVert < dwNumSamples; uVert++ )
    {
        float fArrayOffset = ( float )( pClusterIDs[uVert] * ( 1 + 3 * ( dwNumPCA / 4 ) ) );
        memcpy( pClusterID, &fArrayOffset, sizeof( float ) );
        pClusterID += uStride;
    }
    m_pMesh->UnlockVertexBuffer();
    SAFE_DELETE_ARRAY( pClusterIDs );

    // Now we also need to store the per vertex PCA weights.  Earilier when
    // the mesh was loaded, we changed the vertex decl to make room to store these
    // PCA weights.  In this sample, we will use D3DDECLUSAGE_BLENDWEIGHT[1] to 
    // D3DDECLUSAGE_BLENDWEIGHT[6].  Using D3DDECLUSAGE_BLENDWEIGHT intead of some other 
    // usage was an arbritatey decision.  Since D3DDECLUSAGE_BLENDWEIGHT[1-6] were 
    // declared as float4 then we can store up to 6*4 PCA weights per vertex.  They don't
    // have to be declared as float4, but its a reasonable choice.  So for example, 
    // if dwNumPCAVectors=16 the function will write data to D3DDECLUSAGE_BLENDWEIGHT[1-4]
    V( m_pPRTCompBuffer->ExtractToMesh( dwNumPCA, D3DDECLUSAGE_BLENDWEIGHT, 1, m_pMesh ) );

    // Extract the cluster bases into a large array of floats.  
    // ID3DXPRTCompBuffer::ExtractBasis will extract the basis 
    // for a single cluster.  
    //
    // A single cluster basis is an array of
    // (NumPCA+1)*NumCoeffs*NumChannels floats
    // The "1+" is for the cluster mean.
    int nClusterBasisSize = ( dwNumPCA + 1 ) * dwNumCoeffs * dwNumChannels;
    int nBufferSize = nClusterBasisSize * dwNumClusters;

    SAFE_DELETE_ARRAY( m_aPRTClusterBases );
    m_aPRTClusterBases = new float[nBufferSize];
    assert( m_aPRTClusterBases );

    for( DWORD iCluster = 0; iCluster < dwNumClusters; iCluster++ )
    {
        // ID3DXPRTCompBuffer::ExtractBasis() extracts the basis for a single cluster at a time.
        V( m_pPRTCompBuffer->ExtractBasis( iCluster, &m_aPRTClusterBases[iCluster * nClusterBasisSize] ) );
    }

    SAFE_DELETE_ARRAY( m_aPRTConstants );
    m_aPRTConstants = new float[dwNumClusters * ( 4 + dwNumChannels * dwNumPCA )];
    assert( m_aPRTConstants );
}


//--------------------------------------------------------------------------------------
void CPRTMesh::ComputeSHIrradEnvMapConstants( float* pSHCoeffsRed, float* pSHCoeffsGreen, float* pSHCoeffsBlue )
{
    HRESULT hr;

    float* fLight[3] = { pSHCoeffsRed, pSHCoeffsGreen, pSHCoeffsBlue };

    // Lighting environment coefficients
    D3DXVECTOR4 vCoefficients[3];

    // These constants are described in the article by Peter-Pike Sloan titled 
    // "Efficient Evaluation of Irradiance Environment Maps" in the book 
    // "ShaderX 2 - Shader Programming Tips and Tricks" by Wolfgang F. Engel.
    static const float s_fSqrtPI = ( ( float )sqrtf( D3DX_PI ) );
    const float fC0 = 1.0f / ( 2.0f * s_fSqrtPI );
    const float fC1 = ( float )sqrt( 3.0f ) / ( 3.0f * s_fSqrtPI );
    const float fC2 = ( float )sqrt( 15.0f ) / ( 8.0f * s_fSqrtPI );
    const float fC3 = ( float )sqrt( 5.0f ) / ( 16.0f * s_fSqrtPI );
    const float fC4 = 0.5f * fC2;

    int iChannel;
    for( iChannel = 0; iChannel < 3; iChannel++ )
    {
        vCoefficients[iChannel].x = -fC1 * fLight[iChannel][3];
        vCoefficients[iChannel].y = -fC1 * fLight[iChannel][1];
        vCoefficients[iChannel].z = fC1 * fLight[iChannel][2];
        vCoefficients[iChannel].w = fC0 * fLight[iChannel][0] - fC3 * fLight[iChannel][6];
    }

    V( m_pSHIrradEnvMapEffect->SetVector( "cAr", &vCoefficients[0] ) );
    V( m_pSHIrradEnvMapEffect->SetVector( "cAg", &vCoefficients[1] ) );
    V( m_pSHIrradEnvMapEffect->SetVector( "cAb", &vCoefficients[2] ) );

    for( iChannel = 0; iChannel < 3; iChannel++ )
    {
        vCoefficients[iChannel].x = fC2 * fLight[iChannel][4];
        vCoefficients[iChannel].y = -fC2 * fLight[iChannel][5];
        vCoefficients[iChannel].z = 3.0f * fC3 * fLight[iChannel][6];
        vCoefficients[iChannel].w = -fC2 * fLight[iChannel][7];
    }

    V( m_pSHIrradEnvMapEffect->SetVector( "cBr", &vCoefficients[0] ) );
    V( m_pSHIrradEnvMapEffect->SetVector( "cBg", &vCoefficients[1] ) );
    V( m_pSHIrradEnvMapEffect->SetVector( "cBb", &vCoefficients[2] ) );

    vCoefficients[0].x = fC4 * fLight[0][8];
    vCoefficients[0].y = fC4 * fLight[1][8];
    vCoefficients[0].z = fC4 * fLight[2][8];
    vCoefficients[0].w = 1.0f;

    V( m_pSHIrradEnvMapEffect->SetVector( "cC", &vCoefficients[0] ) );
}


//--------------------------------------------------------------------------------------
void CPRTMesh::ComputeShaderConstants( float* pSHCoeffsRed, float* pSHCoeffsGreen, float* pSHCoeffsBlue,
                                       DWORD dwNumCoeffsPerChannel )
{
    HRESULT hr;
    assert( dwNumCoeffsPerChannel == m_pPRTCompBuffer->GetNumCoeffs() );

    UINT dwNumCoeffs = m_pPRTCompBuffer->GetNumCoeffs();
    UINT dwOrder = m_dwPRTOrder;
    UINT dwNumChannels = m_pPRTCompBuffer->GetNumChannels();
    UINT dwNumClusters = m_pPRTCompBuffer->GetNumClusters();
    UINT dwNumPCA = m_pPRTCompBuffer->GetNumPCA();

    //
    // With compressed PRT, a single diffuse channel is caluated by:
    //       R[p] = (M[k] dot L') + sum( w[p][j] * (B[k][j] dot L');
    // where the sum runs j between 0 and # of PCA vectors
    //       R[p] = exit radiance at point p
    //       M[k] = mean of cluster k 
    //       L' = source radiance approximated with SH coefficients
    //       w[p][j] = the j'th PCA weight for point p
    //       B[k][j] = the j'th PCA basis vector for cluster k
    //
    // Note: since both (M[k] dot L') and (B[k][j] dot L') can be computed on the CPU, 
    // these values are passed as constants using the array m_aPRTConstants.   
    // 
    // So we compute an array of floats, m_aPRTConstants, here.
    // This array is the L' dot M[k] and L' dot B[k][j].
    // The source radiance is the lighting environment in terms of spherical
    // harmonic coefficients which can be computed with D3DXSHEval* or D3DXSHProjectCubeMap.  
    // M[k] and B[k][j] are also in terms of spherical harmonic basis coefficients 
    // and come from ID3DXPRTCompBuffer::ExtractBasis().
    //
    DWORD dwClusterStride = dwNumChannels * dwNumPCA + 4;
    DWORD dwBasisStride = dwNumCoeffs * dwNumChannels * ( dwNumPCA + 1 );

    for( DWORD iCluster = 0; iCluster < dwNumClusters; iCluster++ )
    {
        // For each cluster, store L' dot M[k] per channel, where M[k] is the mean of cluster k
        m_aPRTConstants[iCluster * dwClusterStride + 0] = D3DXSHDot( dwOrder, &m_aPRTClusterBases[iCluster *
                                                                     dwBasisStride + 0 * dwNumCoeffs], pSHCoeffsRed );
        m_aPRTConstants[iCluster * dwClusterStride + 1] = D3DXSHDot( dwOrder, &m_aPRTClusterBases[iCluster *
                                                                     dwBasisStride + 1 * dwNumCoeffs],
                                                                     pSHCoeffsGreen );
        m_aPRTConstants[iCluster * dwClusterStride + 2] = D3DXSHDot( dwOrder, &m_aPRTClusterBases[iCluster *
                                                                     dwBasisStride + 2 * dwNumCoeffs], pSHCoeffsBlue );
        m_aPRTConstants[iCluster * dwClusterStride + 3] = 0.0f;

        // Then per channel we compute L' dot B[k][j], where B[k][j] is the jth PCA basis vector for cluster k
        float* pPCAStart = &m_aPRTConstants[iCluster * dwClusterStride + 4];
        for( DWORD iPCA = 0; iPCA < dwNumPCA; iPCA++ )
        {
            int nOffset = iCluster * dwBasisStride + ( iPCA + 1 ) * dwNumCoeffs * dwNumChannels;

            pPCAStart[0 * dwNumPCA + iPCA] = D3DXSHDot( dwOrder, &m_aPRTClusterBases[nOffset + 0 * dwNumCoeffs],
                                                        pSHCoeffsRed );
            pPCAStart[1 * dwNumPCA + iPCA] = D3DXSHDot( dwOrder, &m_aPRTClusterBases[nOffset + 1 * dwNumCoeffs],
                                                        pSHCoeffsGreen );
            pPCAStart[2 * dwNumPCA + iPCA] = D3DXSHDot( dwOrder, &m_aPRTClusterBases[nOffset + 2 * dwNumCoeffs],
                                                        pSHCoeffsBlue );
        }
    }

    V( m_pPRTEffect->SetFloatArray( "aPRTConstants", ( float* )m_aPRTConstants, dwNumClusters *
                                    ( 4 + dwNumChannels * dwNumPCA ) ) );
}


//--------------------------------------------------------------------------------------
void CPRTMesh::RenderWithPRT( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj, bool bRenderWithAlbedo )
{
    HRESULT hr;
    UINT iPass, cPasses;

    m_pPRTEffect->SetMatrix( "g_mWorldViewProjection", pmWorldViewProj );

    bool bHasAlbedoTexture = false;
    for( int i = 0; i < m_pAlbedoTextures.GetSize(); i++ )
    {
        if( m_pAlbedoTextures.GetAt( i ) != NULL )
            bHasAlbedoTexture = true;
    }
    if( !bHasAlbedoTexture )
        bRenderWithAlbedo = false;

    if( bRenderWithAlbedo )
    {
        V( m_pPRTEffect->SetTechnique( "RenderWithPRTColorLights" ) );
    }
    else
    {
        V( m_pPRTEffect->SetTechnique( "RenderWithPRTColorLightsNoAlbedo" ) );
    }

    if( !bRenderWithAlbedo )
    {
        D3DXCOLOR clrWhite = D3DXCOLOR( 1, 1, 1, 1 );
        V( m_pPRTEffect->SetValue( "MaterialDiffuseColor", &clrWhite, sizeof( D3DCOLORVALUE ) ) );
    }

    V( m_pPRTEffect->Begin( &cPasses, 0 ) );

    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( m_pPRTEffect->BeginPass( iPass ) );

        DWORD dwAttribs = 0;
        V( m_pMesh->GetAttributeTable( NULL, &dwAttribs ) );
        for( DWORD i = 0; i < dwAttribs; i++ )
        {
            if( bRenderWithAlbedo )
            {
                if( m_pAlbedoTextures.GetSize() > ( int )i )
                {
                    V( m_pPRTEffect->SetTexture( "AlbedoTexture", m_pAlbedoTextures.GetAt( i ) ) );
                    D3DXCOLOR clrWhite = D3DXCOLOR( 1, 1, 1, 1 );
                    V( m_pPRTEffect->SetValue( "MaterialDiffuseColor", &clrWhite, sizeof( D3DCOLORVALUE ) ) );
                }
                else
                {
                    V( m_pPRTEffect->SetValue( "MaterialDiffuseColor", &m_pMaterials[i].MatD3D.Diffuse, sizeof
                                               ( D3DCOLORVALUE ) ) );
                }
                V( m_pPRTEffect->CommitChanges() );
            }
            V( m_pMesh->DrawSubset( i ) );
        }

        V( m_pPRTEffect->EndPass() );
    }

    V( m_pPRTEffect->End() );
}


//--------------------------------------------------------------------------------------
void CPRTMesh::RenderWithSHIrradEnvMap( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj,
                                        bool bRenderWithAlbedo )
{
    HRESULT hr;
    UINT iPass, cPasses;

    m_pSHIrradEnvMapEffect->SetMatrix( "g_mWorldViewProjection", pmWorldViewProj );

    bool bHasAlbedoTexture = false;
    for( int i = 0; i < m_pAlbedoTextures.GetSize(); i++ )
    {
        if( m_pAlbedoTextures.GetAt( i ) != NULL )
            bHasAlbedoTexture = true;
    }
    if( !bHasAlbedoTexture )
        bRenderWithAlbedo = false;

    if( bRenderWithAlbedo )
    {
        V( m_pSHIrradEnvMapEffect->SetTechnique( "RenderWithSHIrradEnvMap" ) );
    }
    else
    {
        V( m_pSHIrradEnvMapEffect->SetTechnique( "RenderWithSHIrradEnvMapNoAlbedo" ) );
    }

    if( !bRenderWithAlbedo )
    {
        D3DXCOLOR clrWhite = D3DXCOLOR( 1, 1, 1, 1 );
        V( m_pSHIrradEnvMapEffect->SetValue( "MaterialDiffuseColor", &clrWhite, sizeof( D3DCOLORVALUE ) ) );
    }

    V( m_pSHIrradEnvMapEffect->Begin( &cPasses, 0 ) );

    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( m_pSHIrradEnvMapEffect->BeginPass( iPass ) );

        DWORD dwAttribs = 0;
        V( m_pMesh->GetAttributeTable( NULL, &dwAttribs ) );
        for( DWORD i = 0; i < dwAttribs; i++ )
        {
            if( bRenderWithAlbedo )
            {
                if( m_pAlbedoTextures.GetSize() > ( int )i )
                {
                    V( m_pSHIrradEnvMapEffect->SetTexture( "AlbedoTexture", m_pAlbedoTextures.GetAt( i ) ) );
                    D3DXCOLOR clrWhite = D3DXCOLOR( 1, 1, 1, 1 );
                    V( m_pSHIrradEnvMapEffect->SetValue( "MaterialDiffuseColor", &clrWhite, sizeof
                                                         ( D3DCOLORVALUE ) ) );
                }
                else
                {
                    V( m_pSHIrradEnvMapEffect->SetValue( "MaterialDiffuseColor", &m_pMaterials[i].MatD3D.Diffuse,
                                                         sizeof( D3DCOLORVALUE ) ) );
                }

                V( m_pSHIrradEnvMapEffect->CommitChanges() );
            }
            V( m_pMesh->DrawSubset( i ) );
        }

        V( m_pSHIrradEnvMapEffect->EndPass() );
    }

    V( m_pSHIrradEnvMapEffect->End() );
}


//--------------------------------------------------------------------------------------
void CPRTMesh::RenderWithNdotL( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj, D3DXMATRIX* pmWorldInv,
                                bool bRenderWithAlbedo, CDXUTDirectionWidget* aLightControl, int nNumLights,
                                float fLightScale )
{
    HRESULT hr;
    UINT iPass, cPasses;

    m_pNDotLEffect->SetMatrix( "g_mWorldViewProjection", pmWorldViewProj );
    m_pNDotLEffect->SetMatrix( "g_mWorldInv", pmWorldInv );

    D3DXVECTOR4 vLightDir[10];
    D3DXVECTOR4 vLightsDiffuse[10];
    D3DXVECTOR4 lightOn( 1,1,1,1 );
    D3DXVECTOR4 lightOff( 0,0,0,0 );
    lightOn *= fLightScale;

    for( int i = 0; i < nNumLights; i++ )
        vLightDir[i] = D3DXVECTOR4( aLightControl[i].GetLightDirection(), 0 );
    for( int i = 0; i < 10; i++ )
        vLightsDiffuse[i] = ( nNumLights > i ) ? lightOn : lightOff;

    bool bHasAlbedoTexture = false;
    for( int i = 0; i < m_pAlbedoTextures.GetSize(); i++ )
    {
        if( m_pAlbedoTextures.GetAt( i ) != NULL )
            bHasAlbedoTexture = true;
    }
    if( !bHasAlbedoTexture )
        bRenderWithAlbedo = false;

    if( bRenderWithAlbedo )
    {
        V( m_pNDotLEffect->SetTechnique( "RenderWithNdotL" ) );
    }
    else
    {
        V( m_pNDotLEffect->SetTechnique( "RenderWithNdotLNoAlbedo" ) );
    }

    if( !bRenderWithAlbedo )
    {
        D3DXCOLOR clrWhite = D3DXCOLOR( 1, 1, 1, 1 );
        V( m_pNDotLEffect->SetValue( "MaterialDiffuseColor", &clrWhite, sizeof( D3DCOLORVALUE ) ) );
    }

    V( m_pNDotLEffect->Begin( &cPasses, 0 ) );

    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( m_pNDotLEffect->BeginPass( iPass ) );

        // 10 and 20 are the register constants
        V( pd3dDevice->SetVertexShaderConstantF( 10, ( float* )vLightDir, nNumLights ) );
        V( pd3dDevice->SetVertexShaderConstantF( 20, ( float* )vLightsDiffuse, 10 ) );

        DWORD dwAttribs = 0;
        V( m_pMesh->GetAttributeTable( NULL, &dwAttribs ) );
        for( DWORD i = 0; i < dwAttribs; i++ )
        {
            if( bRenderWithAlbedo )
            {
                if( m_pAlbedoTextures.GetSize() > ( int )i )
                {
                    V( m_pNDotLEffect->SetTexture( "AlbedoTexture", m_pAlbedoTextures.GetAt( i ) ) );
                    D3DXCOLOR clrWhite = D3DXCOLOR( 1, 1, 1, 1 );
                    V( m_pNDotLEffect->SetValue( "MaterialDiffuseColor", &clrWhite, sizeof( D3DCOLORVALUE ) ) );
                }
                else
                {
                    // if no texture for this attrib, then just use the material's diffuse color
                    V( m_pNDotLEffect->SetValue( "MaterialDiffuseColor", &m_pMaterials[i].MatD3D.Diffuse, sizeof
                                                 ( D3DCOLORVALUE ) ) );
                }

                V( m_pNDotLEffect->CommitChanges() );
            }
            V( m_pMesh->DrawSubset( i ) );
        }

        V( m_pNDotLEffect->EndPass() );
    }

    V( m_pNDotLEffect->End() );
}


//--------------------------------------------------------------------------------------
void CPRTMesh::ComputeLDPRTConstants( float* pSHCoeffsR, float* pSHCoeffsG, float* pSHCoeffsB,
                                      DWORD dwNumCoeffsPerChannel )
{
    HRESULT hr;

    // Set coefficients 
    V( m_pLDPRTEffect->SetFloatArray( "RLight", pSHCoeffsR, 36 ) );
    V( m_pLDPRTEffect->SetFloatArray( "GLight", pSHCoeffsG, 36 ) );
    V( m_pLDPRTEffect->SetFloatArray( "BLight", pSHCoeffsB, 36 ) );
}


//--------------------------------------------------------------------------------------
void CPRTMesh::SetLDPRTData( ID3DXPRTBuffer* pLDPRTBuff, D3DXVECTOR3* pShadingNormal )
{
    SAFE_RELEASE( m_pLDPRTBuffer );
    if( m_pLDPRTShadingNormals )
        delete[] m_pLDPRTShadingNormals;

    m_pLDPRTBuffer = pLDPRTBuff;
    m_pLDPRTBuffer->AddRef();
    m_pLDPRTShadingNormals = pShadingNormal;

    unsigned int dwNumSamples = pLDPRTBuff->GetNumSamples();
    unsigned int dwNumCoeffs = pLDPRTBuff->GetNumCoeffs();
    unsigned int uSampSize = dwNumCoeffs * pLDPRTBuff->GetNumChannels();
    float* pRawData;
    HRESULT hr;
    float* pConvData;

    const unsigned int uChanMul = ( pLDPRTBuff->GetNumChannels() == 1 )?0:1;

    // Put everything into the buffer
    V( m_pLDPRTMesh->LockVertexBuffer( 0, ( void** )&pRawData ) );
    UINT uStride = m_pLDPRTMesh->GetNumBytesPerVertex() / 4; // all floats - change if data types change...

    pLDPRTBuff->LockBuffer( 0, dwNumSamples, &pConvData );

    // Change this to false if you don't want to use the shading normal
    bool bUpdateNormal = true;

    if( !m_pLDPRTShadingNormals )
    {
        m_pLDPRTShadingNormals = new D3DXVECTOR3[dwNumSamples];
        bUpdateNormal = false;
    }


    for( UINT uVert = 0; uVert < dwNumSamples; uVert++ )
    {
        if( m_pLDPRTShadingNormals )
        {
            if( bUpdateNormal )
                memcpy( pRawData + uVert * uStride + 3, &m_pLDPRTShadingNormals[uVert], sizeof( float ) * 3 ); // copy from m_pLDPRTShadingNormals to m_pLDPRTMesh
            else
                memcpy( &m_pLDPRTShadingNormals[uVert], pRawData + uVert * uStride + 3, sizeof( float ) * 3 ); // copy from m_pLDPRTMesh to m_pLDPRTShadingNormals
        }

        float fRCoeffs[6], fGCoeffs[6], fBCoeffs[6];

        for( UINT i = 0; i < dwNumCoeffs; i++ )
        {
            fRCoeffs[i] = pConvData[uVert * uSampSize + i];
            fGCoeffs[i] = pConvData[uVert * uSampSize + i + uChanMul * dwNumCoeffs];
            fBCoeffs[i] = pConvData[uVert * uSampSize + i + 2 * uChanMul * dwNumCoeffs];
        }

        // Through the cubics...
        memcpy( pRawData + uVert * uStride + 8, fRCoeffs, sizeof( float ) * 4 );
        memcpy( pRawData + uVert * uStride + 8 + 4, fGCoeffs, sizeof( float ) * 4 );
        memcpy( pRawData + uVert * uStride + 8 + 8, fBCoeffs, sizeof( float ) * 4 );

        memcpy( pRawData + uVert * uStride + 8 + 12, &fRCoeffs[4], sizeof( float ) * 2 );
        memcpy( pRawData + uVert * uStride + 8 + 14, &fGCoeffs[4], sizeof( float ) * 2 );
        memcpy( pRawData + uVert * uStride + 8 + 16, &fBCoeffs[4], sizeof( float ) * 2 );
    }

    pLDPRTBuff->UnlockBuffer();
    m_pLDPRTMesh->UnlockVertexBuffer();
}


//--------------------------------------------------------------------------------------
void CPRTMesh::RenderWithLDPRT( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj, D3DXMATRIX* pmNormalXForm,
                                bool bUniform, bool bRenderWithAlbedo )
{
    if( m_hValidLDPRTTechnique == NULL )
        return;

    HRESULT hr;
    UINT iPass, cPasses;

    m_pLDPRTEffect->SetMatrix( "g_mWorldViewProjection", pmWorldViewProj );
    D3DXMATRIX mIdentity;

    D3DXMatrixIdentity( &mIdentity );
    m_pLDPRTEffect->SetMatrix( "g_mNormalXform", &mIdentity );

    m_pLDPRTEffect->SetTexture( "CLinBF", m_pSHBasisTextures[0] );
    m_pLDPRTEffect->SetTexture( "QuadBF", m_pSHBasisTextures[1] );

    m_pLDPRTEffect->SetTexture( "CubeBFA", m_pSHBasisTextures[2] );
    m_pLDPRTEffect->SetTexture( "CubeBFB", m_pSHBasisTextures[3] );

    m_pLDPRTEffect->SetTexture( "QuarBFA", m_pSHBasisTextures[4] );
    m_pLDPRTEffect->SetTexture( "QuarBFB", m_pSHBasisTextures[5] );
    m_pLDPRTEffect->SetTexture( "QuarBFC", m_pSHBasisTextures[6] );

    m_pLDPRTEffect->SetTexture( "QuinBFA", m_pSHBasisTextures[7] );
    m_pLDPRTEffect->SetTexture( "QuinBFB", m_pSHBasisTextures[8] );

    bool bHasAlbedoTexture = false;
    for( int i = 0; i < m_pAlbedoTextures.GetSize(); i++ )
    {
        if( m_pAlbedoTextures.GetAt( i ) != NULL )
            bHasAlbedoTexture = true;
    }
    if( !bHasAlbedoTexture )
        bRenderWithAlbedo = false;

    if( bRenderWithAlbedo )
        m_pLDPRTEffect->SetTechnique( m_hValidLDPRTTechniqueWithTex );
    else
        m_pLDPRTEffect->SetTechnique( m_hValidLDPRTTechnique );


    if( !bRenderWithAlbedo )
    {
        D3DXCOLOR clrWhite = D3DXCOLOR( 1, 1, 1, 1 );
        V( m_pLDPRTEffect->SetValue( "MaterialDiffuseColor", &clrWhite, sizeof( D3DCOLORVALUE ) ) );
    }

    V( m_pLDPRTEffect->Begin( &cPasses, 0 ) );
    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( m_pLDPRTEffect->BeginPass( iPass ) );

        DWORD dwAttribs = 0;
        V( m_pLDPRTMesh->GetAttributeTable( NULL, &dwAttribs ) );
        for( DWORD i = 0; i < dwAttribs; i++ )
        {
            if( bRenderWithAlbedo )
            {
                if( m_pAlbedoTextures.GetSize() > ( int )i )
                {
                    V( m_pLDPRTEffect->SetTexture( "AlbedoTexture", m_pAlbedoTextures.GetAt( i ) ) );
                    D3DXCOLOR clrWhite = D3DXCOLOR( 1, 1, 1, 1 );
                    V( m_pLDPRTEffect->SetValue( "MaterialDiffuseColor", &clrWhite, sizeof( D3DCOLORVALUE ) ) );
                }
                else
                {
                    // if no texture for this attrib, then just use the material's diffuse color
                    V( m_pLDPRTEffect->SetValue( "MaterialDiffuseColor", &m_pMaterials[i].MatD3D.Diffuse, sizeof
                                                 ( D3DCOLORVALUE ) ) );
                }

                V( m_pLDPRTEffect->CommitChanges() );
            }

            V( m_pLDPRTMesh->DrawSubset( i ) );
        }

        V( m_pLDPRTEffect->EndPass() );
    }
    V( m_pLDPRTEffect->End() );
}


//--------------------------------------------------------------------------------------
void CPRTMesh::GetSHTransferFunctionAtVertex( unsigned int uVert, int uTechnique, unsigned int uChan, float* pfVals )
{
    float* pfRawData;

    if( !m_pPRTBuffer || m_pPRTBuffer->GetNumSamples() == 0 ) return;

    switch( uTechnique )
    {
        case 3: // APP_TECH_PRT
        {
            // uncompressed PRT transfer function
            m_pPRTBuffer->LockBuffer( uVert, 1, &pfRawData );
            if( uChan >= m_pPRTBuffer->GetNumChannels() )
                uChan = m_pPRTBuffer->GetNumChannels() - 1; // just get last channel

            for( DWORD i = 0; i < m_pPRTBuffer->GetNumCoeffs(); i++ )
                pfVals[i] = pfRawData[uChan * m_pPRTBuffer->GetNumCoeffs() + i];

            m_pPRTBuffer->UnlockBuffer();
            break;
        }

        case 4: // APP_TECH_NEAR_LOSSLESS_PRT
        {
            // compressed PRT transfer function
            if( uChan >= m_pPRTBuffer->GetNumChannels() )
                uChan = m_pPRTBuffer->GetNumChannels() - 1;
            UINT uClusterID = 0;

            const unsigned int uNumPCA = m_pPRTCompBuffer->GetNumPCA();
            const unsigned int uNumCoeffs = m_pPRTCompBuffer->GetNumCoeffs();
            const unsigned int uNumChannels = m_pPRTBuffer->GetNumChannels();
            unsigned int i, j;

            // clusters/PCAWts should just be stored in the mesh instead...
            // TODO: cache this
            UINT* pdwClusterIDs = new UINT[m_pPRTCompBuffer->GetNumSamples()];
            if( pdwClusterIDs )
            {
                m_pPRTCompBuffer->ExtractClusterIDs( pdwClusterIDs );
                uClusterID = pdwClusterIDs[uVert];
                delete[] pdwClusterIDs;
            }

            float* pPCAWts = new float[m_pPRTCompBuffer->GetNumSamples() * uNumPCA];
            if( pPCAWts )
            {
                m_pPRTCompBuffer->ExtractPCA( 0, m_pPRTCompBuffer->GetNumPCA(), pPCAWts );

                float* pStartPCA = pPCAWts + uVert * uNumPCA;

                for( i = 0; i < 36; i++ ) pfVals[i] = 0.0f; // initialize to zero

                for( i = 0; i < uNumCoeffs; i++ )
                {
                    pfVals[i] = m_aPRTClusterBases[uClusterID * uNumCoeffs * uNumChannels * ( uNumPCA + 1 ) + uChan *
                        uNumCoeffs + i];
                }

                for( j = 0; j < uNumPCA; j++ )
                {
                    float* pStartBasis = m_aPRTClusterBases + uClusterID * uNumCoeffs * uNumChannels *
                        ( uNumPCA + 1 ) + ( j + 1 ) * uNumCoeffs * uNumChannels + uChan * uNumCoeffs;
                    for( i = 0; i < uNumCoeffs; i++ )
                    {
                        pfVals[i] += pStartBasis[i] * pStartPCA[j];
                    }
                }

                delete[] pPCAWts;
            }
            break;
        }

        case 2: // APP_TECH_LDPRT
        {
            // LDPRT transfer function (aka Zonal Harmonic coefficients)
            float fVals[36];

            D3DXSHEvalDirection( fVals, 6, &m_pLDPRTShadingNormals[uVert] );
            int l, m, idx = 0;
            if( uChan >= m_pPRTBuffer->GetNumChannels() ) uChan = m_pPRTBuffer->GetNumChannels() - 1; // just get last channel

            float fZHCoeffs[6];

            m_pLDPRTBuffer->LockBuffer( uVert, 1, &pfRawData );


            for( l = 0; l < ( int )m_pLDPRTBuffer->GetNumCoeffs(); l++ )
            {
                fZHCoeffs[l] = pfRawData[uChan * m_pLDPRTBuffer->GetNumCoeffs() + l];
            }
            for(; l < 6; l++ )
            {
                fZHCoeffs[l] = 0.0f;
            }

            for( l = 0; l < 6; l++ )
            {
                for( m = -l; m <= l; m++ )
                {
                    pfVals[idx] = fVals[idx] * fZHCoeffs[l];
                    idx++;
                }
            }
            m_pLDPRTBuffer->UnlockBuffer();
            break;
        }

        default:
        case 1: // APP_TECH_SHIRRADENVMAP
            {
                // SH irradiance environment map transfer function 
                float fVals[36];
                float fcCoefs[6] = {1.0f,2.0f / 3.0f,0.25f,0.0f,0.0f,0.0f};

                D3DXSHEvalDirection( fVals, 6, &m_pLDPRTShadingNormals[uVert] );
                int l, m, idx = 0;

                for( l = 0; l < 6; l++ )
                {
                    for( m = -l; m <= l; m++ )
                    {
                        pfVals[idx] = fVals[idx] * fcCoefs[l];
                        idx++;
                    }
                }
                break;
            }
    }
}


//--------------------------------------------------------------------------------------
void CPRTMesh::GetVertexPosition( unsigned int uVert, D3DXVECTOR3* pvPos )
{
    while( uVert > m_pPRTBuffer->GetNumSamples() )
    {
        uVert -= m_pPRTBuffer->GetNumSamples();
    }

    unsigned int uSize = m_pLDPRTMesh->GetNumBytesPerVertex();
    unsigned char* pbData;
    m_pLDPRTMesh->LockVertexBuffer( 0, ( LPVOID* )&pbData );

    *pvPos = *( ( D3DXVECTOR3* )( pbData + ( uSize * uVert ) ) );
    m_pLDPRTMesh->UnlockVertexBuffer();
}


//--------------------------------------------------------------------------------------
void CPRTMesh::GetVertexUnderMouse( const D3DXMATRIX* pmProj, const D3DXMATRIX* pmView, const D3DXMATRIX* pmWorld,
                                    unsigned int* puVert )
{
    POINT pt;
    GetCursorPos( &pt );
    ScreenToClient( DXUTGetHWND(), &pt );
    D3DXVECTOR3 vRayDirNDC( ( float )pt.x,( float )pt.y,0.0f );
    D3DXVECTOR3 vRayDirB( ( float )pt.x,( float )pt.y,1.0f );
    D3DXVECTOR3 vNPt;
    D3DXVECTOR3 vNPtB;

    D3DXVec3Unproject( &vNPt, &vRayDirNDC, &m_ViewPort, pmProj, pmView, pmWorld );
    D3DXVec3Unproject( &vNPtB, &vRayDirB, &m_ViewPort, pmProj, pmView, pmWorld );

    vNPtB = vNPtB - vNPt;

    const D3DXVECTOR3* pEye = &vNPt;
    const D3DXVECTOR3* pRayDir = &vNPtB;

    BOOL bHit;
    DWORD uFaceIndex = 0;
    float fU, fV, fG;
    float fDist;
    unsigned int uVert = 0;
    unsigned short* psVerts = NULL;
    DWORD* pwVerts = NULL;
    LPDIRECT3DINDEXBUFFER9 pIB;
    D3DINDEXBUFFER_DESC descIB;
    bool bUseShorts = false;
    LPVOID pVoid;

    D3DXIntersect( m_pMesh, pEye, pRayDir, &bHit, &uFaceIndex, &fV, &fG, &fDist, NULL, NULL );
    if( !bHit )
        return;

    fU = 1.0f - fG - fV;
    uVert = ( fU > fV )?( fU > fG )?0:2:( fV > fG )?1:2;

    m_pMesh->GetIndexBuffer( &pIB );
    pIB->GetDesc( &descIB );

    if( descIB.Format == D3DFMT_INDEX16 )
        bUseShorts = true;

    pIB->Release();

    m_pMesh->LockIndexBuffer( D3DLOCK_READONLY, &pVoid );

    if( bUseShorts )
        psVerts = ( unsigned short* )pVoid;
    else
        pwVerts = ( DWORD* )pVoid;

    if( psVerts )
        *puVert = psVerts[uFaceIndex * 3 + uVert];
    else
        *puVert = pwVerts[uFaceIndex * 3 + uVert];

    m_pMesh->UnlockIndexBuffer();
}


//--------------------------------------------------------------------------------------
void CPRTMesh::OnLostDevice()
{
    HRESULT hr;
    if( m_pPRTEffect )
        V( m_pPRTEffect->OnLostDevice() );
    if( m_pSHIrradEnvMapEffect )
        V( m_pSHIrradEnvMapEffect->OnLostDevice() );
    if( m_pNDotLEffect )
        V( m_pNDotLEffect->OnLostDevice() );
    if( m_pLDPRTEffect )
        V( m_pLDPRTEffect->OnLostDevice() );
}


//--------------------------------------------------------------------------------------
void CPRTMesh::OnDestroyDevice()
{
    if( !m_ReloadState.bUseReloadState )
        ZeroMemory( &m_ReloadState, sizeof( RELOAD_STATE ) );

    SAFE_RELEASE( m_pMesh );
    for( int i = 0; i < m_pAlbedoTextures.GetSize(); i++ )
    {
        SAFE_RELEASE( m_pAlbedoTextures[i] );
    }
    m_pAlbedoTextures.RemoveAll();
    SAFE_RELEASE( m_pMaterialBuffer );
    SAFE_RELEASE( m_pPRTBuffer );
    SAFE_RELEASE( m_pPRTCompBuffer );
    SAFE_RELEASE( m_pPRTEffect );
    SAFE_RELEASE( m_pSHIrradEnvMapEffect );
    SAFE_RELEASE( m_pNDotLEffect );
    SAFE_RELEASE( m_pLDPRTMesh );
    SAFE_RELEASE( m_pLDPRTEffect );

    delete[] m_pLDPRTShadingNormals;
    m_pLDPRTShadingNormals = NULL;
    SAFE_RELEASE( m_pLDPRTBuffer );

    for( int i = 0; i < 9; i++ )
    {
        SAFE_RELEASE( m_pSHBasisTextures[i] );
    }

    SAFE_DELETE_ARRAY( m_aPRTClusterBases );
    SAFE_DELETE_ARRAY( m_aPRTConstants );
}


//--------------------------------------------------------------------------------------
UINT CPRTMesh::GetOrderFromNumCoeffs( UINT dwNumCoeffs )
{
    UINT dwOrder = 1;
    while( dwOrder * dwOrder < dwNumCoeffs )
        dwOrder++;

    return dwOrder;
}

