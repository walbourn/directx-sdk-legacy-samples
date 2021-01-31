//--------------------------------------------------------------------------------------
// File: HDRFormats9.cpp
//
// Basic starting point for new Direct3D samples
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"
#include "skybox.h"

#define NUM_TONEMAP_TEXTURES  5       // Number of stages in the 3x3 down-scaling 
// of average luminance textures
#define NUM_BLOOM_TEXTURES    2
#define RGB16_MAX             100
#define RGB32_MAX             10000


enum ENCODING_MODE
{
    FP16,
    FP32,
    RGB16,
    RGBE8,
    RGB32,
    RGB9E5,
    RG11B10,
    NUM_ENCODING_MODES
};

enum RENDER_MODE
{
    DECODED,
    RGB_ENCODED,
    ALPHA_ENCODED,
    NUM_RENDER_MODES
};

enum NORMAL_MODE
{
    NORM_RGB8,
    NORM_FP32,
    NORM_FP16,
    NORM_FPRG11B10,
    NORM_BC5S,
    NORM_BC5U,
    NUM_NORMAL_MODES
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
extern CModelViewerCamera           g_Camera;               // A model viewing camera
extern bool                         g_bShowHelp;     // If true, it renders the UI control text
extern bool                         g_bShowText;
extern CDXUTDialogResourceManager   g_DialogResourceManager; // manager for shared resources of dialogs
extern CD3DSettingsDlg              g_SettingsDlg;          // Device settings dialog
extern CDXUTTextHelper*             g_pTxtHelper;
extern CDXUTDialog                  g_HUD;                  // dialog for standard controls
extern CDXUTDialog                  g_SampleUI;             // dialog for sample specific controls
extern bool                         g_bBloom;             // Bloom effect on/off
extern ENCODING_MODE                g_eEncodingMode;
extern NORMAL_MODE                  g_eNormalMode;
extern RENDER_MODE                  g_eRenderMode;
extern CSkybox g_aSkybox[NUM_ENCODING_MODES];
extern bool                         g_bSupportsR16F;
extern bool                         g_bSupportsR32F;
extern bool                         g_bSupportsD16;
extern bool                         g_bSupportsD32;
extern bool                         g_bSupportsD24X8;
extern bool                         g_bUseMultiSample; // True when using multisampling on a supported back buffer
extern double g_aPowsOfTwo[257]; // Lookup table for log calculations

struct SCREEN_VERTEX
{
    D3DXVECTOR4 pos;
    D3DXVECTOR2 tex;

    static const DWORD FVF;
};

IDirect3DDevice9*                   g_pd3dDevice9 = NULL;         // Direct3D device

struct TECH_HANDLES9
{
    D3DXHANDLE Scene;
    D3DXHANDLE DownScale2x2_Lum;
    D3DXHANDLE DownScale3x3;
    D3DXHANDLE DownScale3x3_BrightPass;
    D3DXHANDLE FinalPass;
};

TECH_HANDLES9 g_aTechHandles9[NUM_ENCODING_MODES];
TECH_HANDLES9*                      g_pCurTechnique9;

D3DXHANDLE                          g_hmWorldViewProj;
D3DXHANDLE                          g_hmWorld;
D3DXHANDLE                          g_hvEyePt;
D3DXHANDLE                          g_hFinalPassEncoded_RGB;
D3DXHANDLE                          g_hFinalPassEncoded_A;
D3DXHANDLE                          g_htCube;
D3DXHANDLE                          g_havSampleOffsets;
D3DXHANDLE                          g_hBloom;
D3DXHANDLE                          g_havSampleWeights;

ID3DXFont*                          g_pFont9 = NULL;              // Font for drawing text   
ID3DXSprite*                        g_pSprite9 = NULL;       // Sprite for batching draw text calls
LPD3DXEFFECT                        g_pEffect9 = NULL;
PDIRECT3DSURFACE9                   g_pMSRT9 = NULL;       // Multi-Sample float render target
PDIRECT3DSURFACE9                   g_pMSDS9 = NULL;       // Depth Stencil surface for the float RT
LPDIRECT3DTEXTURE9                  g_pTexRender9 = NULL;         // Render target texture
LPDIRECT3DTEXTURE9                  g_pTexBrightPass9 = NULL;     // Bright pass filter
LPD3DXMESH                          g_pMesh9 = NULL;
LPDIRECT3DTEXTURE9 g_apTexToneMap9[NUM_TONEMAP_TEXTURES]; // Tone mapping calculation textures
LPDIRECT3DTEXTURE9 g_apTexBloom9[NUM_BLOOM_TEXTURES];     // Blooming effect intermediate texture
D3DMULTISAMPLE_TYPE                 g_MaxMultiSampleType9 = D3DMULTISAMPLE_NONE;  // Non-Zero when g_bUseMultiSample is true
DWORD                               g_dwMultiSampleQuality9 = 0; // Used when we have multisampling on a backbuffer

#define IDC_ENCODING_MODE       5
#define IDC_NORMAL_MODE         6
#define IDC_RENDER_MODE         7
#define IDC_BLOOM               8


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
//d3d9 callbacks
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                      bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                    void* pUserContext );
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D9LostDevice( void* pUserContext );
void CALLBACK OnD3D9DestroyDevice( void* pUserContext );

HRESULT RetrieveD3D9TechHandles();
HRESULT LoadMesh( WCHAR* strFileName, LPD3DXMESH* ppMesh );
void DrawFullScreenQuad( float fLeftU=0.0f, float fTopV=0.0f, float fRightU=1.0f, float fBottomV=1.0f );

HRESULT RenderModel();

float GaussianDistribution( float x, float y, float rho );
HRESULT GetSampleOffsets_DownScale3x3( DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 avSampleOffsets[] );
HRESULT GetSampleOffsets_DownScale2x2_Lum( DWORD dwSrcWidth, DWORD dwSrcHeight, DWORD dwDestWidth, DWORD dwDestHeight,
                                           D3DXVECTOR2 avSampleOffsets[] );
HRESULT GetSampleOffsets_Bloom( DWORD dwD3DTexSize, float afTexCoordOffset[15], D3DXVECTOR4* avColorWeight,
                                float fDeviation, float fMultiplier=1.0f );
HRESULT RenderBloom9();
HRESULT MeasureLuminance9();
HRESULT BrightPassFilter9();

HRESULT CreateEncodedTexture( IDirect3DCubeTexture9* pTexSrc, IDirect3DCubeTexture9** ppTexDest,
                              ENCODING_MODE eTarget );

void RenderD3D9Text( double fTime );
extern VOID EncodeRGB16( D3DXFLOAT16* pSrc, BYTE** ppDest );
extern int log2_ceiling( float val );

//--------------------------------------------------------------------------------------
VOID EncodeRGBE8_D3D9( D3DXFLOAT16* pSrc, BYTE** ppDest )
{
    FLOAT r, g, b;

    r = ( FLOAT )*( pSrc + 0 );
    g = ( FLOAT )*( pSrc + 1 );
    b = ( FLOAT )*( pSrc + 2 );

    // Determine the largest color component
    float maxComponent = max( max( r, g ), b );

    // Round to the nearest integer exponent
    int nExp = log2_ceiling( maxComponent );

    // Divide the components by the shared exponent
    FLOAT fDivisor = ( FLOAT )g_aPowsOfTwo[ nExp + 128 ];

    r /= fDivisor;
    g /= fDivisor;
    b /= fDivisor;

    // Constrain the color components
    r = max( 0, min( 1, r ) );
    g = max( 0, min( 1, g ) );
    b = max( 0, min( 1, b ) );

    // Store the shared exponent in the alpha channel
    D3DCOLOR* pDestColor = ( D3DCOLOR* )*ppDest;
    *pDestColor = D3DCOLOR_RGBA( ( BYTE )( r * 255 ), ( BYTE )( g * 255 ), ( BYTE )( b * 255 ), nExp + 128 );
    *ppDest += sizeof( D3DCOLOR );
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_DownScale3x3
// Desc: Get the texture coordinate offsets to be used inside the DownScale3x3
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_DownScale3x3( DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 avSampleOffsets[] )
{
    if( NULL == avSampleOffsets )
        return E_INVALIDARG;

    float tU = 1.0f / dwWidth;
    float tV = 1.0f / dwHeight;

    // Sample from the 9 surrounding points. 
    int index = 0;
    for( int y = -1; y <= 1; y++ )
    {
        for( int x = -1; x <= 1; x++ )
        {
            avSampleOffsets[ index ].x = x * tU;
            avSampleOffsets[ index ].y = y * tV;

            index++;
        }
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_DownScale2x2_Lum
// Desc: Get the texture coordinate offsets to be used inside the DownScale2x2_Lum
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_DownScale2x2_Lum( DWORD dwSrcWidth, DWORD dwSrcHeight, DWORD dwDestWidth, DWORD dwDestHeight,
                                           D3DXVECTOR2 avSampleOffsets[] )
{
    if( NULL == avSampleOffsets )
        return E_INVALIDARG;

    float tU = 1.0f / dwSrcWidth;
    float tV = 1.0f / dwSrcHeight;

    float deltaU = ( float )dwSrcWidth / dwDestWidth / 2.0f;
    float deltaV = ( float )dwSrcHeight / dwDestHeight / 2.0f;

    // Sample from 4 surrounding points. 
    int index = 0;
    for( int y = 0; y < 2; y++ )
    {
        for( int x = 0; x <= 2; x++ )
        {
            avSampleOffsets[ index ].x = ( x - 0.5f ) * deltaU * tU;
            avSampleOffsets[ index ].y = ( y - 0.5f ) * deltaV * tV;

            index++;
        }
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_Bloom()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_Bloom( DWORD dwD3DTexSize,
                                float afTexCoordOffset[15],
                                D3DXVECTOR4* avColorWeight,
                                float fDeviation, float fMultiplier )
{
    int i = 0;
    float tu = 1.0f / ( float )dwD3DTexSize;

    // Fill the center texel
    float weight = 1.0f * GaussianDistribution( 0, 0, fDeviation );
    avColorWeight[0] = D3DXVECTOR4( weight, weight, weight, 1.0f );

    afTexCoordOffset[0] = 0.0f;

    // Fill the right side
    for( i = 1; i < 8; i++ )
    {
        weight = fMultiplier * GaussianDistribution( ( float )i, 0, fDeviation );
        afTexCoordOffset[i] = i * tu;

        avColorWeight[i] = D3DXVECTOR4( weight, weight, weight, 1.0f );
    }

    // Copy to the left side
    for( i = 8; i < 15; i++ )
    {
        avColorWeight[i] = avColorWeight[i - 7];
        afTexCoordOffset[i] = -afTexCoordOffset[i - 7];
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Called during device initialization, this code checks the device for some 
// minimum set of capabilities, and rejects those that don't pass by returning false.
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                      D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    UNREFERENCED_PARAMETER( bWindowed );

    // No fallback defined by this app, so reject any device that 
    // doesn't support at least ps2.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnDestroyDevice callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr = S_OK;
    WCHAR str[MAX_PATH] = {0};

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    g_pd3dDevice9 = pd3dDevice;

    // Initialize the font
    HDC hDC = GetDC( NULL );
    int nHeight = -MulDiv( 9, GetDeviceCaps( hDC, LOGPIXELSY ), 72 );
    ReleaseDC( NULL, hDC );
    if( FAILED( hr = D3DXCreateFont( pd3dDevice, nHeight, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     TEXT( "Arial" ), &g_pFont9 ) ) )
        return hr;

    // Define DEBUG_VS and/or DEBUG_PS to debug vertex and/or pixel shaders with the shader debugger.  
    // Debugging vertex shaders requires either REF or software vertex processing, and debugging 
    // pixel shaders requires REF.  The D3DXSHADER_FORCE_*_SOFTWARE_NOOPT flag improves the debug 
    // experience in the shader debugger.  It enables source level debugging, prevents instruction 
    // reordering, prevents dead code elimination, and forces the compiler to compile against the next 
    // higher available software target, which ensures that the unoptimized shaders do not exceed 
    // the shader model limitations.  Setting these flags will cause slower rendering since the shaders 
    // will be unoptimized and forced into software.  See the DirectX documentation for more information 
    // about using the shader debugger.
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE | D3DXFX_LARGEADDRESSAWARE;
#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif

    hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"HDRFormats.fx" );
    if( FAILED( hr ) )
        return hr;

    hr = D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags, NULL, &g_pEffect9, NULL );

    // If this fails, there should be debug output
    if( FAILED( hr ) )
        return hr;

    ZeroMemory( g_apTexToneMap9, sizeof( g_apTexToneMap9 ) );
    ZeroMemory( g_apTexBloom9, sizeof( g_apTexBloom9 ) );
    ZeroMemory( g_aTechHandles9, sizeof( g_aTechHandles9 ) );

    RetrieveD3D9TechHandles();

    g_hmWorldViewProj = g_pEffect9->GetParameterByName( NULL, "g_mWorldViewProj" );
    g_hmWorld = g_pEffect9->GetParameterByName( NULL, "g_mWorld" );
    g_hvEyePt = g_pEffect9->GetParameterByName( NULL, "g_vEyePt" );
    g_hFinalPassEncoded_RGB = g_pEffect9->GetTechniqueByName( "FinalPassEncoded_RGB" );
    g_hFinalPassEncoded_A = g_pEffect9->GetTechniqueByName( "FinalPassEncoded_A" );
    g_htCube = g_pEffect9->GetParameterByName( NULL, "g_tCube" );
    g_havSampleOffsets = g_pEffect9->GetParameterByName( NULL, "g_avSampleOffsets" );
    g_hBloom = g_pEffect9->GetTechniqueByName( "Bloom" );
    g_havSampleWeights = g_pEffect9->GetParameterByName( NULL, "g_avSampleWeights" );

    // Determine which encoding modes this device can support
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    DXUTDeviceSettings settings = DXUTGetDeviceSettings();

    bool bCreatedTexture = false;
    IDirect3DCubeTexture9* pCubeTexture = NULL;
    IDirect3DCubeTexture9* pEncodedTexture = NULL;


    WCHAR strPath[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"Light Probes\\uffizi_cross.dds" ) );

    g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->RemoveAllItems();


    g_bSupportsR16F = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_RENDERTARGET,
                                            D3DRTYPE_TEXTURE, D3DFMT_R16F ) ) )
        g_bSupportsR16F = true;

    g_bSupportsR32F = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_RENDERTARGET,
                                            D3DRTYPE_TEXTURE, D3DFMT_R32F ) ) )
        g_bSupportsR32F = true;

    bool bSupports128FCube = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, 0,
                                            D3DRTYPE_CUBETEXTURE, D3DFMT_A16B16G16R16F ) ) )
        bSupports128FCube = true;

    // FP16
    if( g_bSupportsR16F && bSupports128FCube )
    {
        // Device supports floating-point textures. 
        V_RETURN( D3DXCreateCubeTextureFromFileEx( pd3dDevice, strPath, D3DX_DEFAULT, 1, 0, D3DFMT_A16B16G16R16F,
                                                   D3DPOOL_MANAGED, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL,
                                                   NULL, &pCubeTexture ) );

        V_RETURN( g_aSkybox[FP16].OnD3D9CreateDevice( pd3dDevice, 50, pCubeTexture, L"skybox.fx" ) );

        g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->AddItem( L"FP16", ( void* )FP16 );
    }

    // FP32
    if( g_bSupportsR32F && bSupports128FCube )
    {
        // Device supports floating-point textures. 
        V_RETURN( D3DXCreateCubeTextureFromFileEx( pd3dDevice, strPath, D3DX_DEFAULT, 1, 0, D3DFMT_A16B16G16R16F,
                                                   D3DPOOL_MANAGED, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL,
                                                   NULL, &pCubeTexture ) );

        V_RETURN( g_aSkybox[FP32].OnD3D9CreateDevice( pd3dDevice, 50, pCubeTexture, L"skybox.fx" ) );

        g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->AddItem( L"FP32", ( void* )FP32 );
    }

    if( ( !g_bSupportsR32F && !g_bSupportsR16F ) || !bSupports128FCube )
    {
        // Device doesn't support floating-point textures. Use the scratch pool to load it temporarily
        // in order to create encoded textures from it.
        bCreatedTexture = true;
        V_RETURN( D3DXCreateCubeTextureFromFileEx( pd3dDevice, strPath, D3DX_DEFAULT, 1, 0, D3DFMT_A16B16G16R16F,
                                                   D3DPOOL_SCRATCH, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL,
                                                   NULL, &pCubeTexture ) );
    }

    // RGB16
    if( D3D_OK == pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal,
                                           settings.d3d9.DeviceType,
                                           settings.d3d9.AdapterFormat, 0,
                                           D3DRTYPE_CUBETEXTURE,
                                           D3DFMT_A16B16G16R16 ) )
    {
        V_RETURN( CreateEncodedTexture( pCubeTexture, &pEncodedTexture, RGB16 ) );
        V_RETURN( g_aSkybox[RGB16].OnD3D9CreateDevice( pd3dDevice, 50, pEncodedTexture, L"skybox.fx" ) );

        g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->AddItem( L"RGB16", ( void* )RGB16 );
    }


    // RGBE8
    V_RETURN( CreateEncodedTexture( pCubeTexture, &pEncodedTexture, RGBE8 ) );
    V_RETURN( g_aSkybox[RGBE8].OnD3D9CreateDevice( pd3dDevice, 50, pEncodedTexture, L"skybox.fx" ) );

    g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->AddItem( L"RGBE8", ( void* )RGBE8 );
    g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->SetSelectedByText( L"RGBE8" );

    if( bCreatedTexture )
        SAFE_RELEASE( pCubeTexture );

    hr = LoadMesh( L"misc\\teapot.x", &g_pMesh9 );
    if( FAILED( hr ) )
        return hr;

    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    g_bBloom = TRUE;
    g_eEncodingMode = RGBE8;
    g_eRenderMode = DECODED;
    g_pCurTechnique9 = &g_aTechHandles9[ g_eEncodingMode ];

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// reset, which will happen after a lost device scenario. This is the best location to 
// create D3DPOOL_DEFAULT resources since these resources need to be reloaded whenever 
// the device is lost. Resources created here should be released in the OnLostDevice 
// callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice,
                                    const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr = S_OK;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont9 ) V_RETURN( g_pFont9->OnResetDevice() );
    if( g_pEffect9 ) V_RETURN( g_pEffect9->OnResetDevice() );
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite9 ) );
    g_pTxtHelper = new CDXUTTextHelper( g_pFont9, g_pSprite9, NULL, NULL, 15 );

    if( pd3dDevice == NULL )
        return S_OK;

    int i;
    for( i = 0; i < 4; i++ )
    {
        g_aSkybox[i].OnD3D9ResetDevice( pBackBufferSurfaceDesc );
    }

    if( g_pFont9 )
        g_pFont9->OnResetDevice();

    if( g_pEffect9 )
        g_pEffect9->OnResetDevice();

    D3DFORMAT fmt = D3DFMT_UNKNOWN;
    switch( g_eEncodingMode )
    {
        case FP16:
            fmt = D3DFMT_A16B16G16R16F; break;
        case FP32:
            fmt = D3DFMT_A16B16G16R16F; break;
        case RGBE8:
            fmt = D3DFMT_A8R8G8B8; break;
        case RGB16:
            fmt = D3DFMT_A16B16G16R16; break;
    }

    hr = pd3dDevice->CreateTexture( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height,
                                    1, D3DUSAGE_RENDERTARGET, fmt,
                                    D3DPOOL_DEFAULT, &g_pTexRender9, NULL );
    if( FAILED( hr ) )
        return hr;

    hr = pd3dDevice->CreateTexture( pBackBufferSurfaceDesc->Width / 8, pBackBufferSurfaceDesc->Height / 8,
                                    1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                    D3DPOOL_DEFAULT, &g_pTexBrightPass9, NULL );
    if( FAILED( hr ) )
        return hr;

    // Determine whether we can and should support a multisampling on the HDR render target
    g_bUseMultiSample = false;
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( !pD3D )
        return E_FAIL;

    DXUTDeviceSettings settings = DXUTGetDeviceSettings();

    g_bSupportsD16 = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE, D3DFMT_D16 ) ) )
    {
        if( SUCCEEDED( pD3D->CheckDepthStencilMatch( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                                     settings.d3d9.AdapterFormat, fmt,
                                                     D3DFMT_D16 ) ) )
        {
            g_bSupportsD16 = true;
        }
    }
    g_bSupportsD32 = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE, D3DFMT_D32 ) ) )
    {
        if( SUCCEEDED( pD3D->CheckDepthStencilMatch( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                                     settings.d3d9.AdapterFormat, fmt,
                                                     D3DFMT_D32 ) ) )
        {
            g_bSupportsD32 = true;
        }
    }
    g_bSupportsD24X8 = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE, D3DFMT_D24X8 ) ) )
    {
        if( SUCCEEDED( pD3D->CheckDepthStencilMatch( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                                     settings.d3d9.AdapterFormat, fmt,
                                                     D3DFMT_D24X8 ) ) )
        {
            g_bSupportsD24X8 = true;
        }
    }

    D3DFORMAT dfmt = D3DFMT_UNKNOWN;
    if( g_bSupportsD16 )
        dfmt = D3DFMT_D16;
    else if( g_bSupportsD32 )
        dfmt = D3DFMT_D32;
    else if( g_bSupportsD24X8 )
        dfmt = D3DFMT_D24X8;

    if( dfmt != D3DFMT_UNKNOWN )
    {
        D3DCAPS9 Caps;
        pd3dDevice->GetDeviceCaps( &Caps );

        g_MaxMultiSampleType9 = D3DMULTISAMPLE_NONE;
        for( D3DMULTISAMPLE_TYPE imst = D3DMULTISAMPLE_2_SAMPLES; imst <= D3DMULTISAMPLE_16_SAMPLES;
             imst = ( D3DMULTISAMPLE_TYPE )( imst + 1 ) )
        {
            DWORD msQuality = 0;
            if( SUCCEEDED( pD3D->CheckDeviceMultiSampleType( Caps.AdapterOrdinal,
                                                             Caps.DeviceType,
                                                             fmt,
                                                             settings.d3d9.pp.Windowed,
                                                             imst, &msQuality ) ) )
            {
                g_bUseMultiSample = true;
                g_MaxMultiSampleType9 = imst;
                if( msQuality > 0 )
                    g_dwMultiSampleQuality9 = msQuality - 1;
                else
                    g_dwMultiSampleQuality9 = msQuality;
            }
        }

        // Create the Multi-Sample floating point render target
        if( g_bUseMultiSample )
        {
            const D3DSURFACE_DESC* pBackBufferDesc = DXUTGetD3D9BackBufferSurfaceDesc();
            hr = g_pd3dDevice9->CreateRenderTarget( pBackBufferDesc->Width, pBackBufferDesc->Height,
                                                    fmt,
                                                    g_MaxMultiSampleType9, g_dwMultiSampleQuality9,
                                                    FALSE, &g_pMSRT9, NULL );
            if( FAILED( hr ) )
                g_bUseMultiSample = false;
            else
            {
                hr = g_pd3dDevice9->CreateDepthStencilSurface( pBackBufferDesc->Width, pBackBufferDesc->Height,
                                                               dfmt,
                                                               g_MaxMultiSampleType9, g_dwMultiSampleQuality9,
                                                               TRUE, &g_pMSDS9, NULL );
                if( FAILED( hr ) )
                {
                    g_bUseMultiSample = false;
                    SAFE_RELEASE( g_pMSRT9 );
                }
            }
        }
    }

    // For each scale stage, create a texture to hold the intermediate results
    // of the luminance calculation
    int nSampleLen = 1;
    for( i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
    {
        fmt = D3DFMT_UNKNOWN;
        switch( g_eEncodingMode )
        {
            case FP16:
                fmt = D3DFMT_R16F; break;
            case FP32:
                fmt = D3DFMT_R32F; break;
            case RGBE8:
                fmt = D3DFMT_A8R8G8B8; break;
            case RGB16:
                fmt = D3DFMT_A16B16G16R16; break;
        }

        hr = pd3dDevice->CreateTexture( nSampleLen, nSampleLen, 1, D3DUSAGE_RENDERTARGET,
                                        fmt, D3DPOOL_DEFAULT, &g_apTexToneMap9[i], NULL );
        if( FAILED( hr ) )
            return hr;

        nSampleLen *= 3;
    }

    // Create the temporary blooming effect textures
    for( i = 0; i < NUM_BLOOM_TEXTURES; i++ )
    {
        hr = pd3dDevice->CreateTexture( pBackBufferSurfaceDesc->Width / 8, pBackBufferSurfaceDesc->Height / 8,
                                        1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                        D3DPOOL_DEFAULT, &g_apTexBloom9[i], NULL );
        if( FAILED( hr ) )
            return hr;
    }

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );

    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 150, pBackBufferSurfaceDesc->Height - 250 );
    g_SampleUI.SetSize( 150, 110 );

    CDXUTCheckBox* pCheckBox = g_SampleUI.GetCheckBox( IDC_BLOOM );
    pCheckBox->SetChecked( g_bBloom );

    CDXUTComboBox* pComboBox = g_SampleUI.GetComboBox( IDC_ENCODING_MODE );
    pComboBox->SetSelectedByData( ( void* )g_eEncodingMode );

    pComboBox = g_SampleUI.GetComboBox( IDC_RENDER_MODE );
    pComboBox->SetSelectedByData( ( void* )g_eRenderMode );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// This callback function will be called at the end of every frame to perform all the 
// rendering calls for the scene, and it will also be called if the window needs to be 
// repainted. After this function has returned, DXUT will call 
// IDirect3DDevice9::Present to display the contents of the next buffer in the swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    UNREFERENCED_PARAMETER( fElapsedTime );

    LPDIRECT3DSURFACE9 pSurfBackBuffer = NULL;
    PDIRECT3DSURFACE9 pSurfDS = NULL;
    pd3dDevice->GetRenderTarget( 0, &pSurfBackBuffer );
    pd3dDevice->GetDepthStencilSurface( &pSurfDS );

    LPDIRECT3DSURFACE9 pSurfRenderTarget = NULL;
    g_pTexRender9->GetSurfaceLevel( 0, &pSurfRenderTarget );

    // Setup the HDR render target
    if( g_bUseMultiSample )
    {
        pd3dDevice->SetRenderTarget( 0, g_pMSRT9 );
        pd3dDevice->SetDepthStencilSurface( g_pMSDS9 );
    }
    else
    {
        pd3dDevice->SetRenderTarget( 0, pSurfRenderTarget );
    }

    // Clear the render target
    pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x000000FF, 1.0f, 0L );

    // Update matrices
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mWorldView;
    D3DXMATRIXA16 mWorldViewProj;

    D3DXMatrixMultiply( &mWorldViewProj, g_Camera.GetViewMatrix(), g_Camera.GetProjMatrix() );
    g_pEffect9->SetMatrix( g_hmWorldViewProj, &mWorldViewProj );
    g_pEffect9->SetValue( g_hvEyePt, g_Camera.GetEyePt(), sizeof( D3DXVECTOR3 ) );

    // For the first pass we'll draw the screen to the full screen render target
    // and to update the velocity render target with the velocity of each pixel
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Draw the skybox
        g_aSkybox[g_eEncodingMode].D3D9Render( &mWorldViewProj );

        RenderModel();

        // If using floating point multi sampling, stretchrect to the rendertarget
        if( g_bUseMultiSample )
        {
            pd3dDevice->StretchRect( g_pMSRT9, NULL, pSurfRenderTarget, NULL, D3DTEXF_NONE );
            pd3dDevice->SetRenderTarget( 0, pSurfRenderTarget );
            pd3dDevice->SetDepthStencilSurface( pSurfDS );
        }

        MeasureLuminance9();
        BrightPassFilter9();

        if( g_bBloom )
            RenderBloom9();

        //---------------------------------------------------------------------
        // Final pass
        pd3dDevice->SetRenderTarget( 0, pSurfBackBuffer );
        pd3dDevice->SetTexture( 0, g_pTexRender9 );
        pd3dDevice->SetTexture( 1, g_apTexToneMap9[0] );
        pd3dDevice->SetTexture( 2, g_bBloom ? g_apTexBloom9[0] : NULL );

        pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );

        switch( g_eRenderMode )
        {
            case DECODED:
                g_pEffect9->SetTechnique( g_pCurTechnique9->FinalPass ); break;
            case RGB_ENCODED:
                g_pEffect9->SetTechnique( g_hFinalPassEncoded_RGB ); break;
            case ALPHA_ENCODED:
                g_pEffect9->SetTechnique( g_hFinalPassEncoded_A ); break;
        }

        UINT uiPass, uiNumPasses;
        g_pEffect9->Begin( &uiNumPasses, 0 );

        for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
        {
            g_pEffect9->BeginPass( uiPass );

            DrawFullScreenQuad();

            g_pEffect9->EndPass();
        }

        g_pEffect9->End();
        pd3dDevice->SetTexture( 0, NULL );
        pd3dDevice->SetTexture( 1, NULL );
        pd3dDevice->SetTexture( 2, NULL );

        if( g_bShowText )
            RenderD3D9Text( fTime );

        g_HUD.OnRender( fElapsedTime );
        g_SampleUI.OnRender( fElapsedTime );

        pd3dDevice->EndScene();
    }

    SAFE_RELEASE( pSurfRenderTarget );
    SAFE_RELEASE( pSurfBackBuffer );
    SAFE_RELEASE( pSurfDS );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderD3D9Text( double fTime )
{
    UNREFERENCED_PARAMETER( fTime );

    const WCHAR* ENCODING_MODE_NAMES[] =
    {
        L"16-Bit floating-point (FP16)",
        L"32-Bit floating-point (FP32)",
        L"16-Bit integer (RGB16)",
        L"8-Bit integer w/ shared exponent (RGBE8)",
        L"32-Bit integer (RGB32)",
    };

    const WCHAR* RENDER_MODE_NAMES[] =
    {
        L"Decoded scene",
        L"RGB channels of encoded textures",
        L"Alpha channel of encoded textures",
    };

    // Output statistics
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.90f, 0.90f, 1.0f, 1.0f ) );
    g_pTxtHelper->DrawFormattedTextLine( ENCODING_MODE_NAMES[ g_eEncodingMode ] );
    g_pTxtHelper->DrawFormattedTextLine( RENDER_MODE_NAMES[ g_eRenderMode ] );

    if( g_bUseMultiSample )
    {
        g_pTxtHelper->DrawTextLine( L"Using MultiSample Render Target" );
        g_pTxtHelper->DrawFormattedTextLine( L"Number of Samples: %d", ( int )g_MaxMultiSampleType9 );
        g_pTxtHelper->DrawFormattedTextLine( L"Quality: %d", g_dwMultiSampleQuality9 );
    }

    // Draw help
    if( g_bShowHelp )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
        g_pTxtHelper->SetInsertionPos( 2, pd3dsdBackBuffer->Height - 15 * 6 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Controls:" );

        g_pTxtHelper->SetInsertionPos( 20, pd3dsdBackBuffer->Height - 15 * 5 );
        g_pTxtHelper->DrawTextLine( L"Rotate model: Left mouse button\n"
                                    L"Rotate camera: Right mouse button\n"
                                    L"Zoom camera: Mouse wheel scroll\n"
                                    L"Quit: ESC" );

        g_pTxtHelper->SetInsertionPos( 250, pd3dsdBackBuffer->Height - 15 * 5 );
        g_pTxtHelper->DrawTextLine( L"Cycle encoding: E\n"
                                    L"Cycle render mode: R\n"
                                    L"Toggle bloom: B\n"
                                    L"Hide text: T\n" );

        g_pTxtHelper->SetInsertionPos( 410, pd3dsdBackBuffer->Height - 15 * 5 );
        g_pTxtHelper->DrawTextLine( L"Hide help: F1\n"
                                    L"Change Device: F2\n"
                                    L"Toggle HAL/REF: F3\n"
                                    L"View readme: F9\n" );
    }
    else
    {
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Press F1 for help" );
    }
    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// entered a lost state and before IDirect3DDevice9::Reset is called. Resources created
// in the OnResetDevice callback should be released here, which generally includes all 
// D3DPOOL_DEFAULT resources. See the "Lost Devices" section of the documentation for 
// information about lost devices.
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9LostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    if( g_pFont9 ) g_pFont9->OnLostDevice();
    if( g_pEffect9 ) g_pEffect9->OnLostDevice();
    SAFE_RELEASE( g_pSprite9 );
    SAFE_DELETE( g_pTxtHelper );

    int i = 0;

    for( i = 0; i < 4; i++ )
    {
        g_aSkybox[i].OnD3D9LostDevice();
    }

    if( g_pFont9 )
        g_pFont9->OnLostDevice();

    if( g_pEffect9 )
        g_pEffect9->OnLostDevice();

    SAFE_RELEASE( g_pMSRT9 );
    SAFE_RELEASE( g_pMSDS9 );

    SAFE_RELEASE( g_pTexRender9 );
    SAFE_RELEASE( g_pTexBrightPass9 );

    for( i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
    {
        SAFE_RELEASE( g_apTexToneMap9[i] );
    }

    for( i = 0; i < NUM_BLOOM_TEXTURES; i++ )
    {
        SAFE_RELEASE( g_apTexBloom9[i] );
    }
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnCreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    for( int i = 0; i < 4; i++ )
    {
        g_aSkybox[i].OnD3D9DestroyDevice();
    }

    SAFE_RELEASE( g_pFont9 );
    SAFE_RELEASE( g_pEffect9 );
    SAFE_RELEASE( g_pMesh9 );
}

//-----------------------------------------------------------------------------
// Name: RetrieveD3D9TechHandles()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT RetrieveD3D9TechHandles()
{
    CHAR* modes[] = { "FP16", "FP16", "RGB16", "RGBE8" };
    CHAR* techniques[] = { "Scene", "DownScale2x2_Lum", "DownScale3x3", "DownScale3x3_BrightPass", "FinalPass" };
    DWORD dwNumTechniques = sizeof( TECH_HANDLES9 ) / sizeof( D3DXHANDLE );

    CHAR strBuffer[MAX_PATH] = {0};

    D3DXHANDLE* pHandle = ( D3DXHANDLE* )g_aTechHandles9;

    for( UINT m = 0; m < ( UINT )4; m++ )
    {
        for( UINT t = 0; t < dwNumTechniques; t++ )
        {
            sprintf_s( strBuffer, MAX_PATH - 1, "%s_%s", techniques[t], modes[m] );

            *pHandle++ = g_pEffect9->GetTechniqueByName( strBuffer );
        }
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: MeasureLuminance()
// Desc: Measure the average log luminance in the scene.
//-----------------------------------------------------------------------------
HRESULT MeasureLuminance9()
{
    HRESULT hr = S_OK;
    UINT uiNumPasses, uiPass;
    D3DXVECTOR2 avSampleOffsets[16];

    LPDIRECT3DTEXTURE9 pTexSrc = NULL;
    LPDIRECT3DTEXTURE9 pTexDest = NULL;
    LPDIRECT3DSURFACE9 pSurfDest = NULL;

    //-------------------------------------------------------------------------
    // Initial sampling pass to convert the image to the log of the grayscale
    //-------------------------------------------------------------------------
    pTexSrc = g_pTexRender9;
    pTexDest = g_apTexToneMap9[NUM_TONEMAP_TEXTURES - 1];

    D3DSURFACE_DESC descSrc;
    pTexSrc->GetLevelDesc( 0, &descSrc );

    D3DSURFACE_DESC descDest;
    pTexDest->GetLevelDesc( 0, &descDest );

    GetSampleOffsets_DownScale2x2_Lum( descSrc.Width, descSrc.Height, descDest.Width, descDest.Height,
                                       avSampleOffsets );

    g_pEffect9->SetValue( g_havSampleOffsets, avSampleOffsets, sizeof( avSampleOffsets ) );
    g_pEffect9->SetTechnique( g_pCurTechnique9->DownScale2x2_Lum );

    hr = pTexDest->GetSurfaceLevel( 0, &pSurfDest );
    if( FAILED( hr ) )
        return hr;

    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

    pd3dDevice->SetRenderTarget( 0, pSurfDest );
    pd3dDevice->SetTexture( 0, pTexSrc );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );

    hr = g_pEffect9->Begin( &uiNumPasses, 0 );
    if( FAILED( hr ) )
        return hr;

    for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
    {
        g_pEffect9->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad();

        g_pEffect9->EndPass();
    }

    g_pEffect9->End();
    pd3dDevice->SetTexture( 0, NULL );

    SAFE_RELEASE( pSurfDest );

    //-------------------------------------------------------------------------
    // Iterate through the remaining tone map textures
    //------------------------------------------------------------------------- 
    for( int i = NUM_TONEMAP_TEXTURES - 1; i > 0; i-- )
    {
        // Cycle the textures
        pTexSrc = g_apTexToneMap9[i];
        pTexDest = g_apTexToneMap9[i - 1];

        hr = pTexDest->GetSurfaceLevel( 0, &pSurfDest );
        if( FAILED( hr ) )
            return hr;

        D3DSURFACE_DESC desc;
        pTexSrc->GetLevelDesc( 0, &desc );
        GetSampleOffsets_DownScale3x3( desc.Width, desc.Height, avSampleOffsets );

        g_pEffect9->SetValue( g_havSampleOffsets, avSampleOffsets, sizeof( avSampleOffsets ) );
        g_pEffect9->SetTechnique( g_pCurTechnique9->DownScale3x3 );

        pd3dDevice->SetRenderTarget( 0, pSurfDest );
        pd3dDevice->SetTexture( 0, pTexSrc );
        pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );

        hr = g_pEffect9->Begin( &uiNumPasses, 0 );
        if( FAILED( hr ) )
            return hr;

        for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
        {
            g_pEffect9->BeginPass( uiPass );

            // Draw a fullscreen quad to sample the RT
            DrawFullScreenQuad();

            g_pEffect9->EndPass();
        }

        g_pEffect9->End();
        pd3dDevice->SetTexture( 0, NULL );

        SAFE_RELEASE( pSurfDest );
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderModel()
// Desc: Render the model
//-----------------------------------------------------------------------------
HRESULT RenderModel()
{
    HRESULT hr = S_OK;

    // Set the transforms
    D3DXMATRIXA16 mWorldViewProj;
    D3DXMatrixMultiply( &mWorldViewProj, g_Camera.GetWorldMatrix(), g_Camera.GetViewMatrix() );
    D3DXMatrixMultiply( &mWorldViewProj, &mWorldViewProj, g_Camera.GetProjMatrix() );
    g_pEffect9->SetMatrix( g_hmWorld, g_Camera.GetWorldMatrix() );
    g_pEffect9->SetMatrix( g_hmWorldViewProj, &mWorldViewProj );

    // Draw the mesh
    g_pEffect9->SetTechnique( g_pCurTechnique9->Scene );
    g_pEffect9->SetTexture( g_htCube, g_aSkybox[g_eEncodingMode].GetD3D9EnvironmentMap() );

    UINT uiPass, uiNumPasses;
    hr = g_pEffect9->Begin( &uiNumPasses, 0 );
    if( FAILED( hr ) )
        return hr;

    for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
    {
        g_pEffect9->BeginPass( uiPass );

        g_pMesh9->DrawSubset( 0 );

        g_pEffect9->EndPass();
    }

    g_pEffect9->End();

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: BrightPassFilter
// Desc: Prepare for the bloom pass by removing dark information from the scene
//-----------------------------------------------------------------------------
HRESULT BrightPassFilter9()
{
    HRESULT hr = S_OK;

    const D3DSURFACE_DESC* pBackBufDesc = DXUTGetD3D9BackBufferSurfaceDesc();

    D3DXVECTOR2 avSampleOffsets[16];
    GetSampleOffsets_DownScale3x3( pBackBufDesc->Width / 2, pBackBufDesc->Height / 2, avSampleOffsets );

    g_pEffect9->SetValue( g_havSampleOffsets, avSampleOffsets, sizeof( avSampleOffsets ) );

    LPDIRECT3DSURFACE9 pSurfBrightPass = NULL;
    hr = g_pTexBrightPass9->GetSurfaceLevel( 0, &pSurfBrightPass );
    if( FAILED( hr ) )
        return hr;

    g_pEffect9->SetTechnique( g_pCurTechnique9->DownScale3x3_BrightPass );
    g_pd3dDevice9->SetRenderTarget( 0, pSurfBrightPass );
    g_pd3dDevice9->SetTexture( 0, g_pTexRender9 );
    g_pd3dDevice9->SetTexture( 1, g_apTexToneMap9[0] );

    g_pd3dDevice9->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );

    UINT uiPass, uiNumPasses;
    hr = g_pEffect9->Begin( &uiNumPasses, 0 );
    if( FAILED( hr ) )
        return hr;

    for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
    {
        g_pEffect9->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad();

        g_pEffect9->EndPass();
    }

    g_pEffect9->End();
    g_pd3dDevice9->SetTexture( 0, NULL );
    g_pd3dDevice9->SetTexture( 1, NULL );

    SAFE_RELEASE( pSurfBrightPass );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderBloom()
// Desc: Render the blooming effect
//-----------------------------------------------------------------------------
HRESULT RenderBloom9()
{
    HRESULT hr = S_OK;
    UINT uiPassCount, uiPass;
    int i = 0;

    D3DXVECTOR2 avSampleOffsets[16];
    float afSampleOffsets[16];
    D3DXVECTOR4 avSampleWeights[16];

    LPDIRECT3DSURFACE9 pSurfDest = NULL;
    hr = g_apTexBloom9[1]->GetSurfaceLevel( 0, &pSurfDest );
    if( FAILED( hr ) )
        return hr;

    D3DSURFACE_DESC desc;
    hr = g_pTexBrightPass9->GetLevelDesc( 0, &desc );
    if( FAILED( hr ) )
        return hr;

    hr = GetSampleOffsets_Bloom( desc.Width, afSampleOffsets, avSampleWeights, 3.0f, 1.25f );
    for( i = 0; i < 16; i++ )
    {
        avSampleOffsets[i] = D3DXVECTOR2( afSampleOffsets[i], 0.0f );
    }

    g_pEffect9->SetTechnique( g_hBloom );
    g_pEffect9->SetValue( g_havSampleOffsets, avSampleOffsets, sizeof( avSampleOffsets ) );
    g_pEffect9->SetValue( g_havSampleWeights, avSampleWeights, sizeof( avSampleWeights ) );

    g_pd3dDevice9->SetRenderTarget( 0, pSurfDest );
    g_pd3dDevice9->SetTexture( 0, g_pTexBrightPass9 );
    g_pd3dDevice9->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice9->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );


    g_pEffect9->Begin( &uiPassCount, 0 );
    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect9->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad();

        g_pEffect9->EndPass();
    }
    g_pEffect9->End();
    g_pd3dDevice9->SetTexture( 0, NULL );

    SAFE_RELEASE( pSurfDest );
    hr = g_apTexBloom9[0]->GetSurfaceLevel( 0, &pSurfDest );
    if( FAILED( hr ) )
        return hr;

    hr = GetSampleOffsets_Bloom( desc.Height, afSampleOffsets, avSampleWeights, 3.0f, 1.25f );
    for( i = 0; i < 16; i++ )
    {
        avSampleOffsets[i] = D3DXVECTOR2( 0.0f, afSampleOffsets[i] );
    }

    g_pEffect9->SetTechnique( g_hBloom );
    g_pEffect9->SetValue( g_havSampleOffsets, avSampleOffsets, sizeof( avSampleOffsets ) );
    g_pEffect9->SetValue( g_havSampleWeights, avSampleWeights, sizeof( avSampleWeights ) );

    g_pd3dDevice9->SetRenderTarget( 0, pSurfDest );
    g_pd3dDevice9->SetTexture( 0, g_apTexBloom9[1] );
    g_pd3dDevice9->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice9->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );


    g_pEffect9->Begin( &uiPassCount, 0 );

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect9->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad();

        g_pEffect9->EndPass();
    }

    g_pEffect9->End();
    g_pd3dDevice9->SetTexture( 0, NULL );

    SAFE_RELEASE( pSurfDest );

    return hr;
}


//-----------------------------------------------------------------------------
// Name: DrawFullScreenQuad
// Desc: Draw a properly aligned quad covering the entire render target
//-----------------------------------------------------------------------------
void DrawFullScreenQuad( float fLeftU, float fTopV, float fRightU, float fBottomV )
{
    D3DSURFACE_DESC dtdsdRT;
    PDIRECT3DSURFACE9 pSurfRT;

    // Acquire render target width and height
    g_pd3dDevice9->GetRenderTarget( 0, &pSurfRT );
    pSurfRT->GetDesc( &dtdsdRT );
    pSurfRT->Release();

    // Ensure that we're directly mapping texels to pixels by offset by 0.5
    // For more info see the doc page titled "Directly Mapping Texels to Pixels"
    FLOAT fWidth5 = ( FLOAT )dtdsdRT.Width - 0.5f;
    FLOAT fHeight5 = ( FLOAT )dtdsdRT.Height - 0.5f;

    // Draw the quad
    SCREEN_VERTEX svQuad[4];

    svQuad[0].pos = D3DXVECTOR4( -0.5f, -0.5f, 0.5f, 1.0f );
    svQuad[0].tex = D3DXVECTOR2( fLeftU, fTopV );

    svQuad[1].pos = D3DXVECTOR4( fWidth5, -0.5f, 0.5f, 1.0f );
    svQuad[1].tex = D3DXVECTOR2( fRightU, fTopV );

    svQuad[2].pos = D3DXVECTOR4( -0.5f, fHeight5, 0.5f, 1.0f );
    svQuad[2].tex = D3DXVECTOR2( fLeftU, fBottomV );

    svQuad[3].pos = D3DXVECTOR4( fWidth5, fHeight5, 0.5f, 1.0f );
    svQuad[3].tex = D3DXVECTOR2( fRightU, fBottomV );

    g_pd3dDevice9->SetRenderState( D3DRS_ZENABLE, FALSE );
    g_pd3dDevice9->SetFVF( SCREEN_VERTEX::FVF );
    g_pd3dDevice9->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, svQuad, sizeof( SCREEN_VERTEX ) );
    g_pd3dDevice9->SetRenderState( D3DRS_ZENABLE, TRUE );
}

//-----------------------------------------------------------------------------
// Name: CreateEncodedTexture
// Desc: Create a copy of the input floating-point texture with RGBE8 or RGB16 
//       encoding
//-----------------------------------------------------------------------------
HRESULT CreateEncodedTexture( IDirect3DCubeTexture9* pTexSrc, IDirect3DCubeTexture9** ppTexDest,
                              ENCODING_MODE eTarget )
{
    HRESULT hr = S_OK;

    D3DSURFACE_DESC desc;
    V_RETURN( pTexSrc->GetLevelDesc( 0, &desc ) );

    // Create a texture with equal dimensions to store the encoded texture
    D3DFORMAT fmt = D3DFMT_UNKNOWN;
    switch( eTarget )
    {
        case RGBE8:
            fmt = D3DFMT_A8R8G8B8; break;
        case RGB16:
            fmt = D3DFMT_A16B16G16R16; break;
    }

    V_RETURN( g_pd3dDevice9->CreateCubeTexture( desc.Width, 1, 0,
                                                fmt, D3DPOOL_MANAGED,
                                                ppTexDest, NULL ) );

    for( UINT iFace = 0; iFace < 6; iFace++ )
    {
        // Lock the source texture for reading
        D3DLOCKED_RECT rcSrc;
        V_RETURN( pTexSrc->LockRect( ( D3DCUBEMAP_FACES )iFace, 0, &rcSrc, NULL, D3DLOCK_READONLY ) );

        // Lock the destination texture for writing
        D3DLOCKED_RECT rcDest;
        V_RETURN( ( *ppTexDest )->LockRect( ( D3DCUBEMAP_FACES )iFace, 0, &rcDest, NULL, 0 ) );

        BYTE* pSrcBytes = ( BYTE* )rcSrc.pBits;
        BYTE* pDestBytes = ( BYTE* )rcDest.pBits;


        for( UINT y = 0; y < desc.Height; y++ )
        {
            D3DXFLOAT16* pSrc = ( D3DXFLOAT16* )pSrcBytes;
            BYTE* pDest = pDestBytes;

            for( UINT x = 0; x < desc.Width; x++ )
            {
                switch( eTarget )
                {
                    case RGBE8:
                        EncodeRGBE8_D3D9( pSrc, &pDest ); break;
                    case RGB16:
                        EncodeRGB16( pSrc, &pDest ); break;
                    default:
                        return E_FAIL;
                }

                pSrc += 4;
            }

            pSrcBytes += rcSrc.Pitch;
            pDestBytes += rcDest.Pitch;
        }

        // Release the locks
        ( *ppTexDest )->UnlockRect( ( D3DCUBEMAP_FACES )iFace, 0 );
        pTexSrc->UnlockRect( ( D3DCUBEMAP_FACES )iFace, 0 );
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: LoadMesh()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT LoadMesh( WCHAR* strFileName, LPD3DXMESH* ppMesh )
{
    LPD3DXMESH pMesh = NULL;
    WCHAR str[MAX_PATH];
    HRESULT hr;

    if( ppMesh == NULL )
        return E_INVALIDARG;

    DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFileName );
    hr = D3DXLoadMeshFromX( str, D3DXMESH_MANAGED,
                            g_pd3dDevice9, NULL, NULL, NULL, NULL, &pMesh );
    if( FAILED( hr ) || ( pMesh == NULL ) )
        return hr;

    DWORD* rgdwAdjacency = NULL;

    // Make sure there are normals which are required for lighting
    if( !( pMesh->GetFVF() & D3DFVF_NORMAL ) )
    {
        LPD3DXMESH pTempMesh;
        hr = pMesh->CloneMeshFVF( pMesh->GetOptions(),
                                  pMesh->GetFVF() | D3DFVF_NORMAL,
                                  g_pd3dDevice9, &pTempMesh );
        if( FAILED( hr ) )
            return hr;

        D3DXComputeNormals( pTempMesh, NULL );

        SAFE_RELEASE( pMesh );
        pMesh = pTempMesh;
    }

    // Optimze the mesh to make it fast for the user's graphics card
    rgdwAdjacency = new DWORD[pMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
        return E_OUTOFMEMORY;
    V( pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE, rgdwAdjacency, NULL, NULL, NULL );
    delete []rgdwAdjacency;

    *ppMesh = pMesh;

    return S_OK;
}

