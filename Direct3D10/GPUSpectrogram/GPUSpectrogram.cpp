//--------------------------------------------------------------------------------------
// File: GPUSpectrogram.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <assert.h>
#include <wchar.h>
#include <limits.h>
#pragma warning( disable : 4996 ) // disable deprecated warning 
#include <strsafe.h>
#pragma warning( default : 4996 )

#include <d3d9.h>
#include <d3d10.h>
#include <d3dx10.h>

#include "resource.h"
#include "AudioData.h"

// CRT's memory leak detection
#if defined(DEBUG) || defined(_DEBUG)
#include <crtdbg.h>
#endif

struct QUAD_VERTEX
{
    D3DXVECTOR3 pos;
    UINT texx,texy;
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
UINT                                g_uiTexX = 512;         // This must always be 512
UINT                                g_uiTexY = 0;

ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10InputLayout*                  g_pQuadVertexLayout = NULL;
ID3D10Buffer*                       g_pQuadVB;
ID3D10Buffer*                       g_pQuadIB;

ID3D10Texture2D*                    g_pSourceTexture;
ID3D10ShaderResourceView*           g_pSourceTexRV;
ID3D10RenderTargetView*             g_pSourceRTV;
ID3D10Texture2D*                    g_pDestTexture;
ID3D10ShaderResourceView*           g_pDestTexRV;
ID3D10RenderTargetView*             g_pDestRTV;

ID3D10EffectTechnique*              g_pReverse;
ID3D10EffectTechnique*              g_pFFTInner;
ID3D10EffectTechnique*              g_pRenderQuad;
ID3D10EffectShaderResourceVariable* g_ptxSource;
ID3D10EffectVectorVariable*         g_pTextureSize;

ID3D10EffectScalarVariable*         g_pWR;
ID3D10EffectScalarVariable*         g_pWI;
ID3D10EffectScalarVariable*         g_pMMAX;
ID3D10EffectScalarVariable*         g_pM;
ID3D10EffectScalarVariable*         g_pISTEP;

WCHAR g_strBitmapName[MAX_PATH] = {0};
WCHAR g_strWaveName[MAX_PATH] = {0};


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
HRESULT InitResources( ID3D10Device* pd3dDevice );
void DestroyResources();
void CreateSpectrogram( ID3D10Device* pd3dDevice );
HRESULT CreateQuad( ID3D10Device* pd3dDevice, UINT uiTexSizeX, UINT uiTexSizeY );
HRESULT CreateBuffers( ID3D10Device* pd3dDevice, UINT uiTexSizeX, UINT uiTexSizeY );
HRESULT InitializeBuffer( ID3D10Device* pd3dDevice, UINT uiTexSizeX, UINT uiTexSizeY );
HRESULT LoadAudioIntoBuffer( ID3D10Device* pd3dDevice, UINT uiTexSizeX, LPCTSTR szFileName );
void RenderToTexture( ID3D10Device* pd3dDevice, ID3D10RenderTargetView* pRTV, ID3D10ShaderResourceView* pSRV,
                      bool bClear, ID3D10EffectTechnique* pTechnique );
HRESULT SaveSpectogramToFile( ID3D10Device* pd3dDevice, LPCTSTR szFileName, ID3D10Texture2D* pTex );
HRESULT FindMediaFileCch( WCHAR* strDestPath, int cchDest, LPCWSTR strFilename );


//--------------------------------------------------------------------------------------
void PrintUsage()
{
    printf( "GPUSpectrogram Usage:\n" );
    printf( "GPUSpectrogram.exe -w <wavefile> -b <bitmapfile>\n" );
    printf( "\t-w <wavefile> - the wave file to load\n" );
    printf( "\t-b <bitmapfile> - the bitmap to export\n" );
    printf( "\nPress any key to exit.\n" );
    getchar();
}

//--------------------------------------------------------------------------------------
void PrintError( const char* errortext )
{
    printf( errortext );
    printf( "\nPress any key to exit.\n" );
    getchar();
}

//--------------------------------------------------------------------------------------
// Parse Command Line
//--------------------------------------------------------------------------------------
bool ParseCommandLine( char** ppCmdLine, int NumArgs )
{
    char* strCmd = NULL;

    if( NumArgs < 5 )
    {
        PrintUsage();
        return false;
    }

    for( int i = 1; i < NumArgs; i++ )
    {
        strCmd = ppCmdLine[i];

        if( 0 == _stricmp( strCmd, "-w" ) )
        {
            i++;
            MultiByteToWideChar( CP_ACP, 0, ppCmdLine[i], -1, g_strWaveName, MAX_PATH );
        }
        else if( 0 == _stricmp( strCmd, "-b" ) )
        {
            i++;
            MultiByteToWideChar( CP_ACP, 0, ppCmdLine[i], -1, g_strBitmapName, MAX_PATH );
        }
        else
        {
            PrintUsage();
            return false;
        }
    }
    return true;
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int _cdecl main( int NumArgs, char** ppCmdLine )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // This may fail if Direct3D 10 isn't installed
    WCHAR wszPath[MAX_PATH+1] = {0};
    if( !::GetSystemDirectory( wszPath, MAX_PATH + 1 ) )
        return false;
    wcscat_s( wszPath, MAX_PATH, L"\\d3d10.dll" );
    HMODULE hMod = LoadLibrary( wszPath );
    if( NULL == hMod )
    {
        PrintError( "DirectX 10 is necessary to run GPUSpectrogram.\n" );
        return 1;
    }
    FreeLibrary( hMod );

    // parse the command line
    if( !ParseCommandLine( ppCmdLine, NumArgs ) )
        return 1;

    // create a device
    HRESULT hr = S_OK;
    ID3D10Device* pDevice = NULL;
    DWORD dwCreateFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    dwCreateFlags |= D3D10_CREATE_DEVICE_DEBUG;
#endif
    hr = D3D10CreateDevice( NULL, D3D10_DRIVER_TYPE_HARDWARE, ( HMODULE )0, dwCreateFlags,
                            D3D10_SDK_VERSION, &pDevice );
    if( FAILED( hr ) )
    {
        hr = D3D10CreateDevice( NULL, D3D10_DRIVER_TYPE_REFERENCE, ( HMODULE )0, dwCreateFlags,
                                D3D10_SDK_VERSION, &pDevice );
        if( FAILED( hr ) )
        {
            PrintError( "A suitable D3D10 device could not be created.\n" );
            return 1;
        }
    }

    if( FAILED( InitResources( pDevice ) ) )
    {
        PrintError( "GPUSpectrogram encountered an error creating resources.\n" );
        return 2;
    }

    CreateSpectrogram( pDevice );

    if( FAILED( SaveSpectogramToFile( pDevice, g_strBitmapName, g_pSourceTexture ) ) )
        PrintError( "GPUSpectrogram encountered an error saving the spectrogram image file.\n" );

    DestroyResources();
    SAFE_RELEASE( pDevice );

    return 0;
}


//--------------------------------------------------------------------------------------
HRESULT InitResources( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    hr = FindMediaFileCch( str, MAX_PATH, L"GPUSpectrogram.fx" );
    if( FAILED( hr ) )
        return hr;
    hr = D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL, NULL, &g_pEffect10,
                                     NULL, NULL );
    if( FAILED( hr ) )
        return hr;

    // Obtain the technique handles
    g_pReverse = g_pEffect10->GetTechniqueByName( "Reverse" );
    g_pFFTInner = g_pEffect10->GetTechniqueByName( "FFTInner" );
    g_pRenderQuad = g_pEffect10->GetTechniqueByName( "RenderQuad" );
    g_ptxSource = g_pEffect10->GetVariableByName( "g_txSource" )->AsShaderResource();
    g_pTextureSize = g_pEffect10->GetVariableByName( "g_TextureSize" )->AsVector();

    g_pWR = g_pEffect10->GetVariableByName( "g_WR" )->AsScalar();
    g_pWI = g_pEffect10->GetVariableByName( "g_WI" )->AsScalar();
    g_pMMAX = g_pEffect10->GetVariableByName( "g_MMAX" )->AsScalar();
    g_pM = g_pEffect10->GetVariableByName( "g_M" )->AsScalar();
    g_pISTEP = g_pEffect10->GetVariableByName( "g_ISTEP" )->AsScalar();

    hr = LoadAudioIntoBuffer( pd3dDevice, g_uiTexX, g_strWaveName );
    if( FAILED( hr ) )
        return hr;

    D3DXVECTOR4 vTextureSize( ( float )g_uiTexX, ( float )g_uiTexY, 0.0f, 0.0f );
    g_pTextureSize->SetFloatVector( ( float* )vTextureSize );

    // Create our quad vertex input layout
    const D3D10_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_UINT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3D10_PASS_DESC PassDesc;
    g_pReverse->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    hr = pd3dDevice->CreateInputLayout( quadlayout, sizeof( quadlayout ) / sizeof( quadlayout[0] ),
                                        PassDesc.pIAInputSignature,
                                        PassDesc.IAInputSignatureSize, &g_pQuadVertexLayout );
    if( FAILED( hr ) )
        return hr;

    hr = CreateQuad( pd3dDevice, g_uiTexX, g_uiTexY );
    if( FAILED( hr ) )
        return hr;

    return hr;
}


//--------------------------------------------------------------------------------------
void RenderToTexture( ID3D10Device* pd3dDevice, ID3D10RenderTargetView* pRTV, ID3D10ShaderResourceView* pSRV,
                      bool bClear, ID3D10EffectTechnique* pTechnique )
{
    // Clear if we need to
    if( bClear )
    {
        float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
        pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    }

    // Store the old viewport
    D3D10_VIEWPORT OldVP;
    UINT cRT = 1;
    pd3dDevice->RSGetViewports( &cRT, &OldVP );

    if( pRTV )
    {
        // Set a new viewport that exactly matches the size of our 2d textures
        D3D10_VIEWPORT PVP;
        PVP.Height = g_uiTexY;
        PVP.Width = g_uiTexX;
        PVP.MinDepth = 0;
        PVP.MaxDepth = 1;
        PVP.TopLeftX = 0;
        PVP.TopLeftY = 0;
        pd3dDevice->RSSetViewports( 1, &PVP );
    }

    // Set input params
    pd3dDevice->IASetInputLayout( g_pQuadVertexLayout );
    UINT offsets = 0;
    UINT uStrides[] = { sizeof( QUAD_VERTEX ) };
    pd3dDevice->IASetVertexBuffers( 0, 1, &g_pQuadVB, uStrides, &offsets );
    pd3dDevice->IASetIndexBuffer( g_pQuadIB, DXGI_FORMAT_R32_UINT, 0 );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Set the render target and a NULL depth/stencil surface
    if( pRTV )
    {
        ID3D10RenderTargetView* aRTViews[] = { pRTV };
        pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );
    }

    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_ptxSource->SetResource( pSRV );
        pTechnique->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->DrawIndexed( 6, 0, 0 );
    }

    // Retore the original viewport
    pd3dDevice->RSSetViewports( 1, &OldVP );
}


//--------------------------------------------------------------------------------------
void CreateSpectrogram( ID3D10Device* pd3dDevice )
{
    //store the original rt and ds buffer views
    ID3D10RenderTargetView* apOldRTVs[1] = { NULL };
    ID3D10DepthStencilView* pOldDS = NULL;
    pd3dDevice->OMGetRenderTargets( 1, apOldRTVs, &pOldDS );

    // Reverse Indices
    RenderToTexture( pd3dDevice, g_pDestRTV, g_pSourceTexRV, false, g_pReverse );
    pd3dDevice->OMSetRenderTargets( 1, apOldRTVs, pOldDS );

    // Danielson-Lanczos routine
    UINT iterations = 0;
    float wtemp, wr, wpr, wpi, wi, theta;
    UINT n = g_uiTexX;
    UINT mmax = 1;
    while( n > mmax )
    {
        UINT istep = mmax << 1;
        theta = 6.28318530717959f / ( ( float )mmax * 2.0f );
        wtemp = sin( 0.5f * theta );
        wpr = -2.0f * wtemp * wtemp;
        wpi = sin( theta );
        wr = 1.0f;
        wi = 0.0f;

        for( UINT m = 0; m < mmax; m++ )
        {
            // Inner loop is handled on the GPU
            {
                g_pWR->SetFloat( wr );
                g_pWI->SetFloat( wi );
                g_pMMAX->SetInt( mmax );
                g_pM->SetInt( m );
                g_pISTEP->SetInt( istep );

                // Make sure we immediately unbind the previous RT from the shader pipeline
                ID3D10ShaderResourceView* const pSRV[1] = {NULL};
                pd3dDevice->PSSetShaderResources( 0, 1, pSRV );

                if( 0 == iterations % 2 )
                {
                    RenderToTexture( pd3dDevice, g_pSourceRTV, g_pDestTexRV, false, g_pFFTInner );
                }
                else
                {
                    RenderToTexture( pd3dDevice, g_pDestRTV, g_pSourceTexRV, false, g_pFFTInner );
                }
                pd3dDevice->OMSetRenderTargets( 1, apOldRTVs, pOldDS );

                iterations++;
            }

            wtemp = wr;
            wr = wtemp * wpr - wi * wpi + wr;
            wi = wi * wpr + wtemp * wpi + wi;
        }
        mmax = istep;
    }

    // Restore the original RT and DS
    pd3dDevice->OMSetRenderTargets( 1, apOldRTVs, pOldDS );
    SAFE_RELEASE( apOldRTVs[0] );
    SAFE_RELEASE( pOldDS );

    return;
}


//--------------------------------------------------------------------------------------
void DestroyResources()
{
    SAFE_RELEASE( g_pQuadVertexLayout );

    SAFE_RELEASE( g_pQuadVB );
    SAFE_RELEASE( g_pQuadIB );

    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pSourceTexture );
    SAFE_RELEASE( g_pSourceTexRV );
    SAFE_RELEASE( g_pSourceRTV );
    SAFE_RELEASE( g_pDestTexture );
    SAFE_RELEASE( g_pDestTexRV );
    SAFE_RELEASE( g_pDestRTV );
}


//--------------------------------------------------------------------------------------
HRESULT CreateQuad( ID3D10Device* pd3dDevice, UINT uiTexX, UINT uiTexY )
{
    HRESULT hr = S_OK;

    // First create space for the vertices
    UINT uiVertBufSize = 4 * sizeof( QUAD_VERTEX );
    QUAD_VERTEX* pVerts = new QUAD_VERTEX[ uiVertBufSize ];
    if( !pVerts )
        return E_OUTOFMEMORY;

    int index = 0;
    pVerts[index].pos = D3DXVECTOR3( -1, -1, 0 );
    pVerts[index].texx = 0;
    pVerts[index].texy = 0;

    index++;
    pVerts[index].pos = D3DXVECTOR3( -1, 1, 0 );
    pVerts[index].texx = 0;
    pVerts[index].texy = uiTexY;

    index++;
    pVerts[index].pos = D3DXVECTOR3( 1, -1, 0 );
    pVerts[index].texx = uiTexX;
    pVerts[index].texy = 0;

    index++;
    pVerts[index].pos = D3DXVECTOR3( 1, 1, 0 );
    pVerts[index].texx = uiTexX;
    pVerts[index].texy = uiTexY;

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
    hr = pd3dDevice->CreateBuffer( &vbdesc, &InitData, &g_pQuadVB );
    SAFE_DELETE_ARRAY( pVerts );
    if( FAILED( hr ) )
        return hr;

    // Now create space for the indices
    UINT uiIndexBufSize = 6 * sizeof( DWORD );
    DWORD* pIndices = new DWORD[ uiIndexBufSize ];
    if( !pIndices )
        return E_OUTOFMEMORY;

    // Fill the indices
    pIndices[0] = 0;
    pIndices[1] = 1;
    pIndices[2] = 2;
    pIndices[3] = 1;
    pIndices[4] = 3;
    pIndices[5] = 2;

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
    hr = pd3dDevice->CreateBuffer( &ibdesc, &InitData, &g_pQuadIB );
    SAFE_DELETE_ARRAY( pIndices );
    if( FAILED( hr ) )
        return hr;

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CreateBuffers( ID3D10Device* pd3dDevice, UINT uiTexX, UINT uiTexY )
{
    HRESULT hr = S_OK;

    // Create Source and Dest textures
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = uiTexX;
    dstex.Height = uiTexY;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R32G32_FLOAT;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 1;
    hr = pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pSourceTexture );
    if( FAILED( hr ) )
        return hr;
    hr = pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pDestTexture );
    if( FAILED( hr ) )
        return hr;

    // Create Source and Dest Resource Views
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;
    hr = pd3dDevice->CreateShaderResourceView( g_pSourceTexture, &SRVDesc, &g_pSourceTexRV );
    if( FAILED( hr ) )
        return hr;
    hr = pd3dDevice->CreateShaderResourceView( g_pDestTexture, &SRVDesc, &g_pDestTexRV );
    if( FAILED( hr ) )
        return hr;

    // Create Position and Velocity RenderTarget Views
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = DXGI_FORMAT_R32G32_FLOAT;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    DescRT.Texture2D.MipSlice = 0;
    hr = pd3dDevice->CreateRenderTargetView( g_pSourceTexture, &DescRT, &g_pSourceRTV );
    if( FAILED( hr ) )
        return hr;
    hr = pd3dDevice->CreateRenderTargetView( g_pDestTexture, &DescRT, &g_pDestRTV );
    if( FAILED( hr ) )
        return hr;

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT LoadAudioIntoBuffer( ID3D10Device* pd3dDevice, UINT uiTexSizeX, LPCTSTR szFileName )
{
    HRESULT hr = S_OK;

    // Load the wave file
    CAudioData audioData;
    if( !audioData.LoadWaveFile( ( TCHAR* )szFileName ) )
        return E_FAIL;

    // Normalize the data
    audioData.NormalizeData();

    // If we have data, get the number of samples
    unsigned long ulNumSamples = audioData.GetNumSamples();

    // Find out how much Y space (time) our spectogram will need
    g_uiTexY = ( ulNumSamples / uiTexSizeX ) - 1;

    // Create a texture large enough to hold our data
    hr = CreateBuffers( pd3dDevice, uiTexSizeX, g_uiTexY );
    if( FAILED( hr ) )
        return hr;

    // Create temp storage with space for imaginary data
    unsigned long size = uiTexSizeX * g_uiTexY;
    D3DXVECTOR2* pvData = new D3DXVECTOR2[ size ];
    if( !pvData )
        return E_OUTOFMEMORY;

    float* pDataPtr = audioData.GetChannelPtr( 0 );
    for( unsigned long s = 0; s < size; s++ )
    {
        if( s < ulNumSamples )
        {
            pvData[s].x = pDataPtr[ s ];
            pvData[s].y = 0.0f;
        }
        else
        {
            pvData[s] = D3DXVECTOR2( 0.0f, 0.0f );
        }
    }

    // Update the texture with this information
    pd3dDevice->UpdateSubresource( g_pSourceTexture, D3D10CalcSubresource( 0, 0, 1 ), NULL,
                                   pvData, uiTexSizeX * sizeof( D3DXVECTOR2 ), 0 );

    SAFE_DELETE_ARRAY( pvData );

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT SaveSpectogramToFile( ID3D10Device* pd3dDevice, LPCTSTR szFileName, ID3D10Texture2D* pTex )
{
    HRESULT hr = S_OK;
    DWORD dwBytesWritten = 0;

    // Open the file
    HANDLE hFile = CreateFile( szFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        return E_FAIL;

    // Get the size from the texture
    D3D10_TEXTURE2D_DESC desc;
    pTex->GetDesc( &desc );
    UINT iWidth = ( UINT )desc.Width;
    UINT iHeight = ( UINT )desc.Height;

    // Fill out the BMP header
    BITMAPINFOHEADER bih;
    ZeroMemory( &bih, sizeof( BITMAPINFOHEADER ) );
    bih.biSize = sizeof( BITMAPINFOHEADER );
    bih.biWidth = iHeight;
    bih.biHeight = iWidth;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = 0;

    // Find our pad amount
    int iPadAmt = 0;
    if( 0 != ( ( iHeight * 3 ) % 4 ) )
        iPadAmt = 4 - ( ( iHeight * 3 ) % 4 );

    BITMAPFILEHEADER bfh;
    ZeroMemory( &bfh, sizeof( BITMAPFILEHEADER ) );
    bfh.bfType = MAKEWORD( 'B', 'M' );
    bfh.bfSize = sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER ) + ( iWidth * 3 + iPadAmt ) *
        iHeight * sizeof( unsigned char );
    bfh.bfOffBits = sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER );

    // Write out the file header
    WriteFile( hFile, &bfh, sizeof( BITMAPFILEHEADER ), &dwBytesWritten, NULL );
    WriteFile( hFile, &bih, sizeof( BITMAPINFOHEADER ), &dwBytesWritten, NULL );

    // Create a staging resource to get our data back to the CPU

    ID3D10Texture2D* pStagingResource = NULL;
    D3D10_TEXTURE2D_DESC dstex;
    pTex->GetDesc( &dstex );
    dstex.Usage = D3D10_USAGE_STAGING;                      // Staging allows us to copy to and from the GPU
    dstex.BindFlags = 0;                                    // Staging resources cannot be bound to ANY part of the pipeline
    dstex.CPUAccessFlags = D3D10_CPU_ACCESS_READ;           // We want to read from this resource
    hr = pd3dDevice->CreateTexture2D( &dstex, NULL, &pStagingResource );
    if( FAILED( hr ) )
    {
        CloseHandle( hFile );
        return hr;
    }

    // Copy the data from the GPU resource to the CPU resource
    pd3dDevice->CopyResource( pStagingResource, pTex );

    // Map the CPU staging resource
    D3D10_MAPPED_TEXTURE2D map;
    hr = pStagingResource->Map( 0, D3D10_MAP_READ, NULL, &map );
    if( FAILED( hr ) )
    {
        CloseHandle( hFile );
        return hr;
    }
    float* pColors = ( float* )map.pData;

    // Find the maximum data component
    unsigned long ulIndex = 0;
    float fMaxReal = 0.0f;
    float fMaxImag = 0.0f;
    for( unsigned long h = 0; h < iHeight; h++ )
    {
        ulIndex = ( unsigned long )( h * map.RowPitch / sizeof( float ) );
        for( unsigned long w = 0; w < iWidth; w++ )
        {
            if( fMaxReal < fabs( pColors[ ulIndex ] ) )
                fMaxReal = fabs( pColors[ ulIndex ] );
            if( fMaxImag < fabs( pColors[ ulIndex + 1 ] ) )
                fMaxImag = fabs( pColors[ ulIndex + 1 ] );

            ulIndex += 2;
        }
    }

    fMaxReal = 2.0f;
    fMaxImag = 2.0f;

    // Write out the bits in a more familiar frequency vs time format
    // Basically, swap x and y
    float fBlue, fGreen;
    unsigned char ucRed, ucGreen, ucBlue;
    for( unsigned long w = 0; w < iWidth * 2; w += 2 )
    {
        for( unsigned long h = 0; h < iHeight; h++ )
        {
            ulIndex = ( unsigned long )( h * map.RowPitch / sizeof( float ) ) + w;

            fBlue = fabs( pColors[ ulIndex ] ) / fMaxReal;
            fGreen = fabs( pColors[ ulIndex + 1 ] ) / fMaxImag;
            if( fBlue > 1.0f )
                fBlue = 1.0f;
            if( fGreen > 1.0f )
                fGreen = 1.0f;

            ucBlue = ( unsigned char )( 255.0f * fBlue );
            ucGreen = ( unsigned char )( 255.0f * fGreen );
            ucRed = 0;

            WriteFile( hFile, &ucRed, sizeof( unsigned char ), &dwBytesWritten, NULL );
            WriteFile( hFile, &ucGreen, sizeof( unsigned char ), &dwBytesWritten, NULL );
            WriteFile( hFile, &ucBlue, sizeof( unsigned char ), &dwBytesWritten, NULL );
        }

        ucRed = 0;
        for( int i = 0; i < iPadAmt; i++ )
        {
            WriteFile( hFile, &ucRed, sizeof( unsigned char ), &dwBytesWritten, NULL );
        }
    }

    CloseHandle( hFile );

    pStagingResource->Unmap( 0 );
    SAFE_RELEASE( pStagingResource );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Helper function to try to find the location of a media file
//--------------------------------------------------------------------------------------
HRESULT FindMediaFileCch( WCHAR* strDestPath, int cchDest, LPCWSTR strFilename )
{
    bool bFound = false;

    if( NULL == strFilename || strFilename[0] == 0 || NULL == strDestPath || cchDest < 10 )
        return E_INVALIDARG;

    // Get the exe name, and exe path
    WCHAR strExePath[MAX_PATH] = {0};
    WCHAR strExeName[MAX_PATH] = {0};
    WCHAR* strLastSlash = NULL;
    GetModuleFileName( NULL, strExePath, MAX_PATH );
    strExePath[MAX_PATH - 1] = 0;
    strLastSlash = wcsrchr( strExePath, TEXT( '\\' ) );
    if( strLastSlash )
    {
        wcscpy_s( strExeName, MAX_PATH, &strLastSlash[1] );

        // Chop the exe name from the exe path
        *strLastSlash = 0;

        // Chop the .exe from the exe name
        strLastSlash = wcsrchr( strExeName, TEXT( '.' ) );
        if( strLastSlash )
            *strLastSlash = 0;
    }

    wcscpy_s( strDestPath, cchDest, strFilename );
    if( GetFileAttributes( strDestPath ) != 0xFFFFFFFF )
        return S_OK;

    // Search all parent directories starting at .\ and using strFilename as the leaf name
    WCHAR strLeafName[MAX_PATH] = {0};
    wcscpy_s( strLeafName, MAX_PATH, strFilename );

    WCHAR strFullPath[MAX_PATH] = {0};
    WCHAR strFullFileName[MAX_PATH] = {0};
    WCHAR strSearch[MAX_PATH] = {0};
    WCHAR* strFilePart = NULL;

    GetFullPathName( L".", MAX_PATH, strFullPath, &strFilePart );
    if( strFilePart == NULL )
        return E_FAIL;

    while( strFilePart != NULL && *strFilePart != '\0' )
    {
        swprintf_s( strFullFileName, MAX_PATH, L"%s\\%s", strFullPath, strLeafName );
        if( GetFileAttributes( strFullFileName ) != 0xFFFFFFFF )
        {
            wcscpy_s( strDestPath, cchDest, strFullFileName );
            bFound = true;
            break;
        }

        swprintf_s( strFullFileName, MAX_PATH, L"..\\%s\\%s", strExeName, strLeafName );
        if( GetFileAttributes( strFullFileName ) != 0xFFFFFFFF )
        {
            wcscpy_s( strDestPath, cchDest, strFullFileName );
            bFound = true;
            break;
        }

        swprintf_s( strFullFileName, MAX_PATH, L"..\\..\\%s\\%s", strExeName, strLeafName );
        if( GetFileAttributes( strFullFileName ) != 0xFFFFFFFF )
        {
            wcscpy_s( strDestPath, cchDest, strFullFileName );
            bFound = true;
            break;
        }

        swprintf_s( strSearch, MAX_PATH, L"%s\\..", strFullPath );
        GetFullPathName( strSearch, MAX_PATH, strFullPath, &strFilePart );
    }
    if( bFound )
        return S_OK;

    // On failure, return the file as the path but also return an error code
    wcscpy_s( strDestPath, cchDest, strFilename );

    return HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND );
}
