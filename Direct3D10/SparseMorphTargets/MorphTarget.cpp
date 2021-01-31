//--------------------------------------------------------------------------------------
// File: MorphTarget.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "MorphTarget.h"


//--------------------------------------------------------------------------------------
void CMorphTarget::RenderToTexture( ID3D10Device* pd3dDevice, ID3D10RenderTargetView* pRTV,
                                    ID3D10DepthStencilView* pDSV, ID3D10EffectTechnique* pTechnique )
{
    // Store the old viewport
    D3D10_VIEWPORT OldVP;
    UINT cRT = 1;
    pd3dDevice->RSGetViewports( &cRT, &OldVP );

    if( pRTV )
    {
        // Set a new viewport that exactly matches the size of our 2d textures
        D3D10_VIEWPORT PVP;
        PVP.Width = m_XRes;
        PVP.Height = m_YRes;
        PVP.MinDepth = 0;
        PVP.MaxDepth = 1;
        PVP.TopLeftX = 0;
        PVP.TopLeftY = 0;
        pd3dDevice->RSSetViewports( 1, &PVP );
    }

    // Set input params
    UINT offsets = 0;
    UINT uStrides[] = { sizeof( QUAD_VERTEX ) };
    pd3dDevice->IASetVertexBuffers( 0, 1, &m_pVB, uStrides, &offsets );
    pd3dDevice->IASetIndexBuffer( m_pIB, DXGI_FORMAT_R32_UINT, 0 );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    // Set the render target and a NULL depth/stencil surface
    if( pRTV )
    {
        ID3D10RenderTargetView* aRTViews[] = { pRTV };
        pd3dDevice->OMSetRenderTargets( 1, aRTViews, pDSV );
    }

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pTechnique->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->DrawIndexed( 4, 0, 0 );
    }

    // Restore the original viewport
    pd3dDevice->RSSetViewports( 1, &OldVP );
}


//--------------------------------------------------------------------------------------
HRESULT CMorphTarget::CreateTexturesFLOAT( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Create Position and Velocity Textures
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = m_Header.XRes;
    dstex.Height = m_Header.YRes;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 3;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &m_pTexture ) );

    // Create Resource Views
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
    SRVDesc.Texture2DArray.MipLevels = 1;
    SRVDesc.Texture2DArray.FirstArraySlice = 0;
    SRVDesc.Texture2DArray.ArraySize = 3;
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pTexture, &SRVDesc, &m_pTexRV ) );

    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT CMorphTarget::CreateTexturesBIASED( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Create Position and Velocity Textures
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = m_Header.XRes;
    dstex.Height = m_Header.YRes;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 3;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &m_pTexture ) );

    // Create Resource Views
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
    SRVDesc.Texture2DArray.MipLevels = 1;
    SRVDesc.Texture2DArray.FirstArraySlice = 0;
    SRVDesc.Texture2DArray.ArraySize = 3;
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pTexture, &SRVDesc, &m_pTexRV ) );

    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT CMorphTarget::LoadTextureDataFLOAT( ID3D10Device* pd3dDevice, ID3D10Texture2D* pTex, HANDLE hFile )
{
    if( m_Header.XRes * m_Header.YRes * 3 < 1 )
        return E_FAIL;

    // Create enough room to load the data from the file
    D3DXVECTOR4* pvFileData = new D3DXVECTOR4[ m_Header.XRes * m_Header.YRes * 3 ];
    if( !pvFileData )
        return E_OUTOFMEMORY;

    // Load the data
    DWORD dwBytesRead = 0;
    if( !ReadFile( hFile, pvFileData, sizeof( D3DXVECTOR4 ) * m_Header.XRes * m_Header.YRes * 3, &dwBytesRead, NULL ) )
        return E_FAIL;

    // Update the texture with this information

    // Position
    pd3dDevice->UpdateSubresource( pTex,
                                   D3D10CalcSubresource( 0, 0, 1 ),
                                   NULL,
                                   pvFileData,
                                   m_Header.XRes * sizeof( D3DXVECTOR4 ),
                                   0 );

    // Normal
    pd3dDevice->UpdateSubresource( pTex,
                                   D3D10CalcSubresource( 0, 1, 1 ),
                                   NULL,
                                   &pvFileData[m_Header.XRes * m_Header.YRes],
                                   m_Header.XRes * sizeof( D3DXVECTOR4 ),
                                   0 );

    // Tangent
    pd3dDevice->UpdateSubresource( pTex,
                                   D3D10CalcSubresource( 0, 2, 1 ),
                                   NULL,
                                   &pvFileData[2 * m_Header.XRes * m_Header.YRes],
                                   m_Header.XRes * sizeof( D3DXVECTOR4 ),
                                   0 );

    SAFE_DELETE_ARRAY( pvFileData );

    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT CMorphTarget::LoadTextureDataBIASED( ID3D10Device* pd3dDevice, ID3D10Texture2D* pTex, HANDLE hFile )
{
    if( m_Header.XRes * m_Header.YRes * 3 < 1 )
        return E_FAIL;

    // Create enough room to load the data from the file
    BYTE* pvFileData = new BYTE[ m_Header.XRes * m_Header.YRes * 3 * 4 ];
    if( !pvFileData )
        return E_OUTOFMEMORY;

    // Load the data
    DWORD dwBytesRead = 0;
    if( !ReadFile( hFile, pvFileData, m_Header.XRes * m_Header.YRes * 3 * 4, &dwBytesRead, NULL ) )
        return E_FAIL;

    // Update the texture with this information

    // Position
    pd3dDevice->UpdateSubresource( pTex,
                                   D3D10CalcSubresource( 0, 0, 1 ),
                                   NULL,
                                   pvFileData,
                                   m_Header.XRes * 4,
                                   0 );

    // Normal
    pd3dDevice->UpdateSubresource( pTex,
                                   D3D10CalcSubresource( 0, 1, 1 ),
                                   NULL,
                                   &pvFileData[m_Header.XRes * m_Header.YRes * 4],
                                   m_Header.XRes * 4,
                                   0 );

    // Tangent
    pd3dDevice->UpdateSubresource( pTex,
                                   D3D10CalcSubresource( 0, 2, 1 ),
                                   NULL,
                                   &pvFileData[2 * m_Header.XRes * m_Header.YRes * 4],
                                   m_Header.XRes * 4,
                                   0 );

    SAFE_DELETE_ARRAY( pvFileData );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CMorphTarget::CreateRenderQuad( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // First create space for the vertices
    UINT uiVertBufSize = 4 * sizeof( QUAD_VERTEX );
    QUAD_VERTEX* pVerts = new QUAD_VERTEX[ uiVertBufSize ];
    if( !pVerts )
        return E_OUTOFMEMORY;

    float fLeft = ( m_Header.XStart / ( float )m_XRes ) * 2.0f - 1.0f;
    float fRight = ( ( m_Header.XStart + m_Header.XRes ) / ( float )m_XRes ) * 2.0f - 1.0f;
    float fBottom = ( m_Header.YStart / ( float )m_YRes ) * 2.0f - 1.0f;
    float fTop = ( ( m_Header.YStart + m_Header.YRes ) / ( float )m_YRes ) * 2.0f - 1.0f;

    pVerts[0].pos = D3DXVECTOR3( fLeft, fBottom, 0 );
    pVerts[0].tex = D3DXVECTOR2( 0, 0 );
    pVerts[1].pos = D3DXVECTOR3( fLeft, fTop, 0 );
    pVerts[1].tex = D3DXVECTOR2( 0, 1 );
    pVerts[2].pos = D3DXVECTOR3( fRight, fBottom, 0 );
    pVerts[2].tex = D3DXVECTOR2( 1, 0 );
    pVerts[3].pos = D3DXVECTOR3( fRight, fTop, 0 );
    pVerts[3].tex = D3DXVECTOR2( 1, 1 );

    D3D10_BUFFER_DESC vbdesc =
    {
        uiVertBufSize,
        D3D10_USAGE_IMMUTABLE,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVerts;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &InitData, &m_pVB ) );
    SAFE_DELETE_ARRAY( pVerts );

    //
    // Now create space for the indices
    //
    UINT uiIndexBufSize = 4 * sizeof( DWORD );
    DWORD* pIndices = new DWORD[ uiIndexBufSize ];
    if( !pIndices )
        return E_OUTOFMEMORY;

    pIndices[0] = 0;
    pIndices[1] = 1;
    pIndices[2] = 2;
    pIndices[3] = 3;

    D3D10_BUFFER_DESC ibdesc =
    {
        uiIndexBufSize,
        D3D10_USAGE_IMMUTABLE,
        D3D10_BIND_INDEX_BUFFER,
        0,
        0
    };

    InitData.pSysMem = pIndices;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &ibdesc, &InitData, &m_pIB ) );
    SAFE_DELETE_ARRAY( pIndices );

    return hr;
}


//--------------------------------------------------------------------------------------
CMorphTarget::CMorphTarget() : m_pVB( NULL ),
                               m_pIB( NULL ),
                               m_pTexture( NULL ),
                               m_pTexRV( NULL ),
                               m_XRes( 0 ),
                               m_YRes( 0 )
{
    ZeroMemory( &m_Header, sizeof( MORPH_TARGET_BLOCK_HEADER ) );
}


//--------------------------------------------------------------------------------------
CMorphTarget::~CMorphTarget()
{
    SAFE_RELEASE( m_pVB );
    SAFE_RELEASE( m_pIB );
    SAFE_RELEASE( m_pTexture );
    SAFE_RELEASE( m_pTexRV );
}


//--------------------------------------------------------------------------------------
WCHAR* CMorphTarget::GetName()
{
    return m_Header.szName;
}


//--------------------------------------------------------------------------------------
HRESULT CMorphTarget::LoadFLOAT( ID3D10Device* pd3dDevice, HANDLE hFile, UINT XRes, UINT YRes )
{
    HRESULT hr = S_OK;

    m_XRes = XRes;
    m_YRes = YRes;

    // Read in the header
    DWORD dwBytesRead = 0;
    if( !ReadFile( hFile, &m_Header, sizeof( MORPH_TARGET_BLOCK_HEADER ), &dwBytesRead, NULL ) )
    {
        return E_FAIL;
    }

    // Create the textures
    V_RETURN( CreateTexturesFLOAT( pd3dDevice ) );

    // Load the texture data
    V_RETURN( LoadTextureDataFLOAT( pd3dDevice, m_pTexture, hFile ) );

    // Create a vb
    V_RETURN( CreateRenderQuad( pd3dDevice ) );

    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT CMorphTarget::LoadBIASED( ID3D10Device* pd3dDevice, HANDLE hFile, UINT XRes, UINT YRes )
{
    HRESULT hr = S_OK;

    m_XRes = XRes;
    m_YRes = YRes;

    // Read in the header
    DWORD dwBytesRead = 0;
    if( !ReadFile( hFile, &m_Header, sizeof( MORPH_TARGET_BLOCK_HEADER ), &dwBytesRead, NULL ) )
    {
        return E_FAIL;
    }

    // Create the textures
    V_RETURN( CreateTexturesBIASED( pd3dDevice ) );

    // Load the texture data
    V_RETURN( LoadTextureDataBIASED( pd3dDevice, m_pTexture, hFile ) );

    // Create a vb
    V_RETURN( CreateRenderQuad( pd3dDevice ) );

    return hr;
}


//--------------------------------------------------------------------------------------
void CMorphTarget::Apply( ID3D10Device* pd3dDevice,
                          ID3D10RenderTargetView* pRTV,
                          ID3D10DepthStencilView* pDSV,
                          ID3D10EffectTechnique* pTechnique,
                          ID3D10EffectShaderResourceVariable* pVertData )
{
    pVertData->SetResource( m_pTexRV );
    RenderToTexture( pd3dDevice, pRTV, pDSV, pTechnique );
}


//--------------------------------------------------------------------------------------
HRESULT CMorphTargetManager::SetLDPRTData( MESH_VERTEX* pVertices, ID3DXPRTBuffer* pLDPRTBuff )
{
    HRESULT hr = S_OK;

    unsigned int dwNumSamples = pLDPRTBuff->GetNumSamples();
    unsigned int dwNumCoeffs = pLDPRTBuff->GetNumCoeffs();
    unsigned int uSampSize = dwNumCoeffs * pLDPRTBuff->GetNumChannels();
    float* pConvData;

    const unsigned int uChanMul = ( pLDPRTBuff->GetNumChannels() == 1 )?0:1;

    V_RETURN( pLDPRTBuff->LockBuffer( 0, dwNumSamples, &pConvData ) );

    for( UINT uVert = 0; uVert < dwNumSamples; uVert++ )
    {
        float fRCoeffs[6], fGCoeffs[6], fBCoeffs[6];

        for( UINT i = 0; i < dwNumCoeffs; i++ )
        {
            fRCoeffs[i] = pConvData[uVert * uSampSize + i];
            fGCoeffs[i] = pConvData[uVert * uSampSize + i + uChanMul * dwNumCoeffs];
            fBCoeffs[i] = pConvData[uVert * uSampSize + i + 2 * uChanMul * dwNumCoeffs];
        }

        // Through the cubics...
        pVertices[uVert].coeff0.x = fRCoeffs[0];
        pVertices[uVert].coeff0.y = fRCoeffs[1];
        pVertices[uVert].coeff0.z = fRCoeffs[2];
        pVertices[uVert].coeff0.w = fRCoeffs[3];

        pVertices[uVert].coeff1.x = fGCoeffs[0];
        pVertices[uVert].coeff1.y = fGCoeffs[1];
        pVertices[uVert].coeff1.z = fGCoeffs[2];
        pVertices[uVert].coeff1.w = fGCoeffs[3];

        pVertices[uVert].coeff2.x = fBCoeffs[0];
        pVertices[uVert].coeff2.y = fBCoeffs[1];
        pVertices[uVert].coeff2.z = fBCoeffs[2];
        pVertices[uVert].coeff2.w = fBCoeffs[3];

        pVertices[uVert].coeff3.x = fRCoeffs[4];
        pVertices[uVert].coeff3.y = fRCoeffs[5];

        pVertices[uVert].coeff3.z = fGCoeffs[4];
        pVertices[uVert].coeff3.w = fGCoeffs[5];

        pVertices[uVert].coeff4.x = fBCoeffs[4];
        pVertices[uVert].coeff4.y = fBCoeffs[5];
    }

    pLDPRTBuff->UnlockBuffer();

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CMorphTargetManager::CreateVB( ID3D10Device* pd3dDevice, HANDLE hFile, ID3DXPRTBuffer* pLDPRTBuff )
{
    HRESULT hr = S_OK;

    if( m_fileHeader.NumBaseVerts < 1 )
        return E_FAIL;

    // Create space for the VB data
    MESH_VERTEX* pVerts = new MESH_VERTEX[ m_fileHeader.NumBaseVerts ];
    if( !pVerts )
        return E_OUTOFMEMORY;

    // Read in the file verts
    FILE_VERTEX* pFileVerts = new FILE_VERTEX[ m_fileHeader.NumBaseVerts ];
    if( !pFileVerts )
        return E_OUTOFMEMORY;

    DWORD dwBytesRead = 0;
    if( !ReadFile( hFile, pFileVerts, m_fileHeader.NumBaseVerts * sizeof( FILE_VERTEX ), &dwBytesRead, NULL ) )
        return E_FAIL;

    // Fill in the mesh vertices with the file vertices
    for( UINT i = 0; i < m_fileHeader.NumBaseVerts; i++ )
    {
        pVerts[i].DataIndex = pFileVerts[i].DataIndex;
        pVerts[i].tex = pFileVerts[i].tex;
    }
    SAFE_DELETE_ARRAY( pFileVerts );

    // Set the LDPRT data
    if( FAILED( SetLDPRTData( pVerts, pLDPRTBuff ) ) )
    {
        SAFE_DELETE_ARRAY( pVerts );
        return E_FAIL;
    }

    // Create a VB
    D3D10_BUFFER_DESC vbdesc =
    {
        m_fileHeader.NumBaseVerts * sizeof( MESH_VERTEX ),
        D3D10_USAGE_IMMUTABLE,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVerts;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    if( FAILED( pd3dDevice->CreateBuffer( &vbdesc, &InitData, &m_pMeshVB ) ) )
    {
        SAFE_DELETE_ARRAY( pVerts );
        return E_FAIL;
    }

    SAFE_DELETE_ARRAY( pVerts );
    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CMorphTargetManager::CreateIB( ID3D10Device* pd3dDevice, HANDLE hFile )
{
    HRESULT hr = S_OK;

    // Create space for the VB data
    DWORD* pIndices = new DWORD[ m_fileHeader.NumBaseIndices ];
    if( !pIndices )
        return E_OUTOFMEMORY;

    // Read in the indices
    DWORD dwBytesRead = 0;
    if( !ReadFile( hFile, pIndices, m_fileHeader.NumBaseIndices * sizeof( DWORD ), &dwBytesRead, NULL ) )
    {
        SAFE_DELETE_ARRAY( pIndices );
        return E_FAIL;
    }

    // Create a VB
    D3D10_BUFFER_DESC ibdesc =
    {
        m_fileHeader.NumBaseIndices * sizeof( DWORD ),
        D3D10_USAGE_IMMUTABLE,
        D3D10_BIND_INDEX_BUFFER,
        0,
        0
    };

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pIndices;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &ibdesc, &InitData, &m_pMeshIB ) );
    SAFE_DELETE_ARRAY( pIndices );

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CMorphTargetManager::CreateTextures( ID3D10Device* pd3dDevice, UINT uiXRes, UINT uiYRes )
{
    HRESULT hr = S_OK;

    // Create Position and Velocity Textures
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = uiXRes;
    dstex.Height = uiYRes;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 3;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &m_pTexture ) );
    dstex.Format = DXGI_FORMAT_D32_FLOAT;
    dstex.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &m_pDepth ) );

    // Create Resource Views
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
    SRVDesc.Texture2DArray.MipLevels = 1;
    SRVDesc.Texture2DArray.FirstArraySlice = 0;
    SRVDesc.Texture2DArray.ArraySize = 3;
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pTexture, &SRVDesc, &m_pTexRV ) );

    // Create Render Target Views
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
    DescRT.Texture2DArray.FirstArraySlice = 0;
    DescRT.Texture2DArray.ArraySize = 3;
    DescRT.Texture2DArray.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateRenderTargetView( m_pTexture, &DescRT, &m_pRTV ) );

    // Create Depth stencil view
    D3D10_DEPTH_STENCIL_VIEW_DESC DescDS;
    DescDS.Format = DXGI_FORMAT_D32_FLOAT;
    DescDS.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
    DescDS.Texture2DArray.FirstArraySlice = 0;
    DescDS.Texture2DArray.ArraySize = 3;
    DescDS.Texture2DArray.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateDepthStencilView( m_pDepth, &DescDS, &m_pDSV ) );

    return hr;
}


//--------------------------------------------------------------------------------------
CMorphTargetManager::CMorphTargetManager() : m_pMeshVB( NULL ),
                                             m_pMeshIB( NULL ),
                                             m_pMeshLayout( NULL ),
                                             m_pQuadLayout( NULL ),
                                             m_pTexture( NULL ),
                                             m_pTexRV( NULL ),
                                             m_pRTV( NULL ),
                                             m_pDepth( NULL ),
                                             m_pDSV( NULL ),
                                             m_XRes( 0 ),
                                             m_YRes( 0 ),
                                             m_pMorphTargets( NULL )
{
    ZeroMemory( &m_fileHeader, sizeof( MORPH_TARGET_FILE_HEADER ) );
}


//--------------------------------------------------------------------------------------
CMorphTargetManager::~CMorphTargetManager()
{
    Destroy();
}


//--------------------------------------------------------------------------------------
HRESULT CMorphTargetManager::Create( ID3D10Device* pd3dDevice, WCHAR* szMeshFile, WCHAR* szLDPRTFile )
{
    HRESULT hr = S_OK;

    // Error checking
    if( INVALID_FILE_ATTRIBUTES == GetFileAttributes( szMeshFile ) ||
        INVALID_FILE_ATTRIBUTES == GetFileAttributes( szLDPRTFile ) )
        return E_FAIL;

    // Load the LDPRT buffer
    ID3DXPRTBuffer* pLDPRTBuffer = NULL;
    V_RETURN( D3DXLoadPRTBufferFromFile( szLDPRTFile, &pLDPRTBuffer ) );

    // Open the file
    HANDLE hFile = CreateFile( szMeshFile, FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        return DXUTERR_MEDIANOTFOUND;

    DWORD dwBytesRead = 0;
    if( !ReadFile( hFile, &m_fileHeader, sizeof( MORPH_TARGET_FILE_HEADER ), &dwBytesRead, NULL ) )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Read in the VB and IB
    if( FAILED( CreateVB( pd3dDevice, hFile, pLDPRTBuffer ) ) )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }
    if( FAILED( CreateIB( pd3dDevice, hFile ) ) )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    // We're done with the LDPRT buffer
    SAFE_RELEASE( pLDPRTBuffer );

    // Allocate the morph targets
    m_pMorphTargets = new CMorphTarget[ m_fileHeader.NumTargets ];
    if( !m_pMorphTargets )
    {
        CloseHandle( hFile );
        return E_OUTOFMEMORY;
    }

    // Get the X and Y base res for the data
    float fRes = sqrt( ( float )m_fileHeader.NumBaseVerts );
    m_XRes = ( UINT )fRes + 1;
    m_YRes = m_XRes;

    // Create the textures
    if( FAILED( CreateTextures( pd3dDevice, m_XRes, m_YRes ) ) )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Read in the base target
    if( FAILED( ( m_pMorphTargets[0].LoadFLOAT( pd3dDevice, hFile, m_XRes, m_YRes ) ) ) )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Read in all morph targets
    for( UINT i = 1; i < m_fileHeader.NumTargets; i++ )
    {
        if( FAILED( ( m_pMorphTargets[i].LoadBIASED( pd3dDevice, hFile, m_XRes, m_YRes ) ) ) )
        {
            CloseHandle( hFile );
            return E_FAIL;
        }
    }

    CloseHandle( hFile );
    return hr;
}


//--------------------------------------------------------------------------------------
void CMorphTargetManager::Destroy()
{
    SAFE_RELEASE( m_pMeshVB );
    SAFE_RELEASE( m_pMeshIB );
    SAFE_RELEASE( m_pMeshLayout );
    SAFE_RELEASE( m_pQuadLayout );
    SAFE_RELEASE( m_pTexture );
    SAFE_RELEASE( m_pTexRV );
    SAFE_RELEASE( m_pRTV );
    SAFE_RELEASE( m_pDepth );
    SAFE_RELEASE( m_pDSV );
    SAFE_DELETE_ARRAY( m_pMorphTargets );
}


//--------------------------------------------------------------------------------------
void CMorphTargetManager::ResetToBase( ID3D10Device* pd3dDevice,
                                       ID3D10EffectTechnique* pTechnique,
                                       ID3D10EffectShaderResourceVariable* pVertData,
                                       ID3D10EffectScalarVariable* pBlendAmt,
                                       ID3D10EffectVectorVariable* pMaxDeltas )
{
    ApplyMorph( pd3dDevice, ( UINT )0, 1.0f, pTechnique, pVertData, pBlendAmt, pMaxDeltas );
}


//--------------------------------------------------------------------------------------
void CMorphTargetManager::ApplyMorph( ID3D10Device* pd3dDevice,
                                      WCHAR* szMorph,
                                      float fAmount,
                                      ID3D10EffectTechnique* pTechnique,
                                      ID3D10EffectShaderResourceVariable* pVertData,
                                      ID3D10EffectScalarVariable* pBlendAmt,
                                      ID3D10EffectVectorVariable* pMaxDeltas )
{
    for( UINT i = 0; i < m_fileHeader.NumTargets; i++ )
    {
        if( 0 == wcscmp( szMorph, m_pMorphTargets[i].GetName() ) )
            ApplyMorph( pd3dDevice, i, fAmount, pTechnique, pVertData, pBlendAmt, pMaxDeltas );
    }
}


//--------------------------------------------------------------------------------------
void CMorphTargetManager::ApplyMorph( ID3D10Device* pd3dDevice,
                                      UINT iMorph,
                                      float fAmount,
                                      ID3D10EffectTechnique* pTechnique,
                                      ID3D10EffectShaderResourceVariable* pVertData,
                                      ID3D10EffectScalarVariable* pBlendAmt,
                                      ID3D10EffectVectorVariable* pMaxDeltas )
{
    pBlendAmt->SetFloat( fAmount );

    D3DXVECTOR4 delta[3];
    delta[0] = m_pMorphTargets[ iMorph ].GetMaxPositionDelta();
    delta[1] = m_pMorphTargets[ iMorph ].GetMaxNormalDelta();
    delta[2] = m_pMorphTargets[ iMorph ].GetMaxTangentDelta();

    pMaxDeltas->SetFloatVectorArray( ( float* )delta, 0, 3 );
    pd3dDevice->IASetInputLayout( m_pQuadLayout );
    m_pMorphTargets[ iMorph ].Apply( pd3dDevice, m_pRTV, m_pDSV, pTechnique, pVertData );
}


//--------------------------------------------------------------------------------------
void CMorphTargetManager::Render( ID3D10Device* pd3dDevice,
                                  ID3D10EffectTechnique* pTechnique,
                                  ID3D10EffectShaderResourceVariable* pVertData,
                                  ID3D10EffectShaderResourceVariable* pVertDataOrig,
                                  ID3D10EffectScalarVariable* pDataTexSize )
{
    pVertData->SetResource( m_pTexRV );
    pDataTexSize->SetInt( m_XRes );

    // Set the original data for the wrinkle map
    ID3D10ShaderResourceView* pOrigRV = m_pMorphTargets[0].GetTexRV();
    pVertDataOrig->SetResource( pOrigRV );

    pd3dDevice->IASetInputLayout( m_pMeshLayout );
    ID3D10Buffer* pBuffers[1] = { m_pMeshVB };
    UINT uStrides = sizeof( MESH_VERTEX );
    UINT offsets = 0;
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, &uStrides, &offsets );
    pd3dDevice->IASetIndexBuffer( m_pMeshIB, DXGI_FORMAT_R32_UINT, 0 );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pTechnique->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->DrawIndexed( m_fileHeader.NumBaseIndices, 0, 0 );
    }
}


//--------------------------------------------------------------------------------------
void CMorphTargetManager::SetVertexLayouts( ID3D10InputLayout* pMeshLayout, ID3D10InputLayout* pQuadLayout )
{
    m_pMeshLayout = pMeshLayout;
    m_pMeshLayout->AddRef();
    m_pQuadLayout = pQuadLayout;
    m_pQuadLayout->AddRef();
}


//--------------------------------------------------------------------------------------
void CMorphTargetManager::ShowMorphTexture( ID3D10Device* pd3dDevice,
                                            ID3D10EffectTechnique* pTechnique,
                                            ID3D10EffectShaderResourceVariable* pVertData,
                                            ID3D10EffectScalarVariable* pRT,
                                            ID3D10EffectScalarVariable* pBlendAmt,
                                            int iRT,
                                            int iTarget )
{
    pVertData->SetResource( m_pMorphTargets[iTarget].GetTexRV() );
    pRT->SetInt( iRT );
    pBlendAmt->SetFloat( 100.0f );

    pd3dDevice->IASetInputLayout( m_pQuadLayout );

    m_pMorphTargets[ iTarget ].RenderToTexture( pd3dDevice, NULL, NULL, pTechnique );
}


//--------------------------------------------------------------------------------------
void CMorphTargetManager::ShowMorphTexture( ID3D10Device* pd3dDevice,
                                            ID3D10EffectTechnique* pTechnique,
                                            ID3D10EffectShaderResourceVariable* pVertData,
                                            ID3D10EffectScalarVariable* pRT,
                                            ID3D10EffectScalarVariable* pBlendAmt,
                                            int iRT,
                                            WCHAR* szMorph )
{
    for( UINT i = 0; i < m_fileHeader.NumTargets; i++ )
    {
        if( 0 == wcscmp( szMorph, m_pMorphTargets[i].GetName() ) )
            ShowMorphTexture( pd3dDevice, pTechnique, pVertData, pRT, pBlendAmt, iRT, i );
    }
}

