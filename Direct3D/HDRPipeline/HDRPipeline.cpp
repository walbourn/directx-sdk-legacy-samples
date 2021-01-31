//======================================================================
//
//      HIGH DYNAMIC RANGE RENDERING DEMONSTRATION
//      Written by Jack Hoxley, November 2005
//
//======================================================================

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "resource.h"

#include "HDRScene.h"
#include "Luminance.h"
#include "PostProcess.h"
#include "Enumeration.h"


//--------------------------------------------------------------------------------------
// Data Structure Definitions
//--------------------------------------------------------------------------------------
struct TLVertex
{
    D3DXVECTOR4 p;
    D3DXVECTOR2 t;
};

static const DWORD          FVF_TLVERTEX = D3DFVF_XYZRHW | D3DFVF_TEX1;



//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                  g_pFont = NULL;     // Font for drawing text
ID3DXSprite*                g_pTextSprite = NULL;     // Sprite for batching draw text calls
LPDIRECT3DTEXTURE9          g_pArrowTex = NULL;     // Used to indicate flow between cells
CModelViewerCamera          g_Camera;                               // A model viewing camera
CDXUTDialog                 g_HUD;                                  // Dialog for standard controls
CDXUTDialog                 g_Config;                               // Dialog for the shader/render parameters
CD3DSettingsDlg             g_SettingsDlg;                          // Used for the "change device" button
CDXUTDialogResourceManager  g_DialogResourceManager;                // Manager for shared resources of dialogs
LPDIRECT3DPIXELSHADER9      g_pFinalPassPS = NULL;     // The pixel shader that maps HDR->LDR
LPD3DXCONSTANTTABLE         g_pFinalPassPSConsts = NULL;     // The interface for setting param/const data for the above PS
DWORD                       g_dwLastFPSCheck = 0;        // The time index of the last frame rate check
DWORD                       g_dwFrameCount = 0;        // How many frames have elapsed since the last check
DWORD                       g_dwFrameRate = 0;        // How many frames rendered during the PREVIOUS interval
float                       g_fExposure = 0.50f;    // The exposure bias fed into the FinalPass.psh shader (OnFrameRender )



//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_THRESHOLDSLIDER     4
#define IDC_THRESHOLDLABEL      5
#define IDC_EXPOSURESLIDER      6
#define IDC_EXPOSURELABEL       7
#define IDC_MULTIPLIERSLIDER    8
#define IDC_MULTIPLIERLABEL     9
#define IDC_MEANSLIDER          10
#define IDC_MEANLABEL           11
#define IDC_STDDEVSLIDER        12
#define IDC_STDDEVLABEL         13



//--------------------------------------------------------------------------------------
// HIGH DYNAMIC RANGE VARIABLES
//--------------------------------------------------------------------------------------
D3DFORMAT                   g_FloatRenderTargetFmt = D3DFMT_UNKNOWN;   // 128 or 64 bit rendering...
D3DFORMAT                   g_DepthFormat = D3DFMT_UNKNOWN;   // How many bits of depth buffer are we using?
DWORD                       g_dwApproxMemory = 0;                // How many bytes of VRAM are we using up?
LPDIRECT3DTEXTURE9          g_pFinalTexture = NULL;



//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
void RenderText();
void DrawHDRTextureToScreen();


//--------------------------------------------------------------------------------------
// Debug output helper
//--------------------------------------------------------------------------------------
WCHAR g_szStaticOutput[ MAX_PATH ];
WCHAR* DebugHelper( WCHAR* szFormat, UINT cSizeBytes )
{
    swprintf_s( g_szStaticOutput, MAX_PATH, szFormat, cSizeBytes );
    return g_szStaticOutput;
}

//--------------------------------------------------------------------------------------
// Rejects any devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                  D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{

    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    // Also need post pixel processing support
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat,
                                         D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_SURFACE, BackBufferFormat ) ) )
    {
        return false;
    }

    // We must have SM2.0 support for this sample to run. We
    // don't need to worry about the VS version as we can shift
    // that to the CPU if necessary...
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    // We must have floating point render target support, where available we'll use 128bit,
    // but we can also use 64bit. We'll store the best one we can use...
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F ) ) )
    {

        //we don't support 128bit render targets :-(
        if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                             AdapterFormat, D3DUSAGE_RENDERTARGET,
                                             D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) ) )
        {

            //we don't support 128 or 64 bit render targets. This demo
            //will not work with this device.
            return false;

        }

    }

    return true;

}


//--------------------------------------------------------------------------------------
// Before a device is created, modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( DXUT_D3D9_DEVICE == pDeviceSettings->ver );

    HRESULT hr;
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    assert( pD3D != NULL );
    D3DCAPS9 caps;
    V( pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal,
                            pDeviceSettings->d3d9.DeviceType,
                            &caps ) );

    // If device doesn't support HW T&L or doesn't support 2.0 vertex shaders in HW 
    // then switch to SWVP.
    if( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
        caps.VertexShaderVersion < D3DVS_VERSION( 2, 0 ) )
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }
    else
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    }

    // This application is designed to work on a pure device by not using 
    // IDirect3D9::Get*() methods, so create a pure device if supported and using HWVP.
    if( ( caps.DevCaps & D3DDEVCAPS_PUREDEVICE ) != 0 &&
        ( pDeviceSettings->d3d9.BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING ) != 0 )
        pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_PUREDEVICE;

    g_DepthFormat = pDeviceSettings->d3d9.pp.AutoDepthStencilFormat;

    // TODO: Some devices **WILL** allow multisampled HDR targets, this can be
    // checked via a device format enumeration. If multisampling is a requirement, an
    // enumeration/check could be added here.
    if( pDeviceSettings->d3d9.pp.MultiSampleType != D3DMULTISAMPLE_NONE )
    {
        // Multisampling and HDRI don't play nice!
        OutputDebugString( L"Multisampling is enabled. Disabling now!\n" );
        pDeviceSettings->d3d9.pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3DPOOL_MANAGED resources here 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{

    HRESULT hr = S_OK;

    // Allow the GUI to set itself up..
    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    // Determine any necessary enumerations
    V_RETURN( HDREnumeration::FindBestHDRFormat( &g_FloatRenderTargetFmt ) );

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 12, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    WCHAR str[ MAX_PATH ];
    V( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"misc\\Arrows.bmp" ) );
    if( FAILED( hr ) )
    {
        // Couldn't find the sprites
        OutputDebugString( L"OnCreateDevice() - Could not locate 'Arrows.bmp'.\n" );
        return hr;
    }

    V( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pArrowTex ) );
    if( FAILED( hr ) )
    {

        // Couldn't load the sprites!
        OutputDebugString( L"OnCreateDevice() - Could not load 'Arrows.bmp'.\n" );
        return hr;

    }

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );


    return S_OK;

}


//--------------------------------------------------------------------------------------
// Create any D3DPOOL_DEFAULT resources here 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice,
                                const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{

    HRESULT hr = S_OK;
    LPD3DXBUFFER pCode;

    // Allow the GUI time to reset itself..
    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont )
        V_RETURN( g_pFont->OnResetDevice() );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    // Configure the GUI
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 90 );
    g_HUD.SetSize( 170, 170 );

    g_Config.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_Config.SetSize( 170, 210 );

    // Recreate the floating point resources
    V_RETURN( pd3dDevice->CreateTexture( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height,
                                         1, D3DUSAGE_RENDERTARGET, pBackBufferSurfaceDesc->Format,
                                         D3DPOOL_DEFAULT, &g_pFinalTexture, NULL ) );

    //Attempt to recalulate some memory statistics:
    int iMultiplier = 0;
    switch( pBackBufferSurfaceDesc->Format )
    {

            //32bit modes:
        case D3DFMT_A2R10G10B10:
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
            iMultiplier = 4;
            break;

            //24bit modes:
        case D3DFMT_R8G8B8:
            iMultiplier = 3;
            break;

            //16bit modes:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_R5G6B5:
            iMultiplier = 2;
            break;

        default:
            iMultiplier = 1;
    }

    //the *3 constant due to having double buffering AND the composition render target..
    g_dwApproxMemory = ( pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height * 3 * iMultiplier );

    switch( g_DepthFormat )
    {

            //16 bit formats
        case D3DFMT_D16:
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D15S1:
            g_dwApproxMemory += ( pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height * 2 );
            break;

            //32bit formats:
        case D3DFMT_D32:
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D24X8:
        case D3DFMT_D24S8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D24FS8:
            g_dwApproxMemory += ( pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height * 4 );
            break;

        default:
            break;
    }

#ifdef _DEBUG
    {
        OutputDebugString( DebugHelper( L"HDRPipeline.cpp : Basic Swap Chain/Depth Buffer Occupy %d bytes.\n", g_dwApproxMemory ) );
    }
    #endif

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Shader Code\\FinalPass.psh" ) );
    V_RETURN( D3DXCompileShaderFromFile( str,
                                         NULL, NULL,
                                         "main",
                                         "ps_2_0",
                                         0,
                                         &pCode,
                                         NULL,
                                         &g_pFinalPassPSConsts
                                         ) );

    V_RETURN( pd3dDevice->CreatePixelShader( reinterpret_cast< DWORD* >( pCode->GetBufferPointer() ),
                                             &g_pFinalPassPS ) );

    pCode->Release();

    // Hand over execution to the 'HDRScene' module so it can perform it's
    // resource creation:
    V_RETURN( HDRScene::CreateResources( pd3dDevice, pBackBufferSurfaceDesc ) );
    g_dwApproxMemory += HDRScene::CalculateResourceUsage();
#ifdef _DEBUG
    {
        OutputDebugString( DebugHelper( L"HDRPipeline.cpp : HDR Scene Resources Occupy %d bytes.\n", HDRScene::CalculateResourceUsage( ) ) );
    }
    #endif

    // Hand over execution to the 'Luminance' module so it can perform it's
    // resource creation:
    V_RETURN( Luminance::CreateResources( pd3dDevice, pBackBufferSurfaceDesc ) );
    g_dwApproxMemory += Luminance::ComputeResourceUsage();
#ifdef _DEBUG
    {
        OutputDebugString( DebugHelper( L"HDRPipeline.cpp : Luminance Resources Occupy %d bytes.\n", Luminance::ComputeResourceUsage( ) ) );
    }
    #endif

    // Hand over execution to the 'PostProcess' module so it can perform it's
    // resource creation:
    V_RETURN( PostProcess::CreateResources( pd3dDevice, pBackBufferSurfaceDesc ) );
    g_dwApproxMemory += PostProcess::CalculateResourceUsage();
#ifdef _DEBUG
    {
        OutputDebugString( DebugHelper( L"HDRPipeline.cpp : Post Processing Resources Occupy %d bytes.\n", PostProcess::CalculateResourceUsage( ) ) );

        OutputDebugString( DebugHelper( L"HDRPipeline.cpp : Total Graphics Resources Occupy %d bytes.\n", g_dwApproxMemory ) );
    }
    #endif

    return S_OK;

}


//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{

    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    // Compute the frame rate based on a 1/4 second update cycle
    if( ( GetTickCount() - g_dwLastFPSCheck ) >= 250 )
    {
        g_dwFrameRate = g_dwFrameCount * 4;
        g_dwFrameCount = 0;
        g_dwLastFPSCheck = GetTickCount();
    }

    ++g_dwFrameCount;

    // The original HDR scene needs to update some of it's internal structures, so
    // pass the message along...
    HDRScene::UpdateScene( DXUTGetD3D9Device(), static_cast< float >( fTime ), &g_Camera );

}


//--------------------------------------------------------------------------------------
//  OnFrameRender()
//  ---------------
//  NOTES: This function makes particular use of the 'D3DPERF_BeginEvent()' and
//         'D3DPERF_EndEvent()' functions. See the documentation for more details,
//         but these are essentially used to provide better output from PIX. If you
//         perform a full call-stream capture on this sample, PIX will group together
//         related calls, making it much easier to interpret the results.
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{

    // 'hr' is used by the 'V()' and 'V_RETURN()' macros
    HRESULT hr;

    // If the user is currently in the process of selecting
    // a new device and/or configuration then we hand execution
    // straight back to the framework...
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    //Cache some results for later use
    LPDIRECT3DSURFACE9 pLDRSurface = NULL;

    //Configure the render targets
    V( pd3dDevice->GetRenderTarget( 0, &pLDRSurface ) );        //This is the output surface - a standard 32bit device

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0xFF ), 1.0f,
                          0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {

        // RENDER THE COMPLETE SCENE
        //--------------------------
        // The first part of each frame is to actually render the "core"
        // resources - those that would be required for an HDR-based pipeline.

        // HDRScene creates an unprocessed, raw, image to work with.
        D3DPERF_BeginEvent( D3DCOLOR_XRGB( 255, 0, 0 ), L"HDRScene Rendering" );
        HDRScene::RenderScene( pd3dDevice );
        D3DPERF_EndEvent();

        // Luminance attempts to calculate what sort of tone/mapping should
        // be done as part of the post-processing step.
        D3DPERF_BeginEvent( D3DCOLOR_XRGB( 0, 0, 255 ), L"Luminance Rendering" );
        Luminance::MeasureLuminance( pd3dDevice );
        D3DPERF_EndEvent();

        // The post-processing adds the blur to the bright (over-exposed) areas
        // of the image.
        D3DPERF_BeginEvent( D3DCOLOR_XRGB( 255, 0, 0 ), L"Post-Processing Rendering" );
        PostProcess::PerformPostProcessing( pd3dDevice );
        D3DPERF_EndEvent();

        // When all the individual components have been created we have the final
        // composition. The output of this is the image that will be displayed
        // on screen.
        D3DPERF_BeginEvent( D3DCOLOR_XRGB( 255, 0, 255 ), L"Final Image Composition" );
        {
            LPDIRECT3DSURFACE9 pFinalSurf = NULL;
            g_pFinalTexture->GetSurfaceLevel( 0, &pFinalSurf );
            pd3dDevice->SetRenderTarget( 0, pFinalSurf );

            LPDIRECT3DTEXTURE9 pHDRTex = NULL;
            HDRScene::GetOutputTexture( &pHDRTex );

            LPDIRECT3DTEXTURE9 pLumTex = NULL;
            Luminance::GetLuminanceTexture( &pLumTex );

            LPDIRECT3DTEXTURE9 pBloomTex = NULL;
            PostProcess::GetTexture( &pBloomTex );

            pd3dDevice->SetTexture( 0, pHDRTex );
            pd3dDevice->SetTexture( 1, pLumTex );
            pd3dDevice->SetTexture( 2, pBloomTex );

            pd3dDevice->SetPixelShader( g_pFinalPassPS );

            D3DSURFACE_DESC d;
            pBloomTex->GetLevelDesc( 0, &d );
            g_pFinalPassPSConsts->SetFloat( pd3dDevice, "g_rcp_bloom_tex_w", 1.0f / static_cast< float >( d.Width ) );
            g_pFinalPassPSConsts->SetFloat( pd3dDevice, "g_rcp_bloom_tex_h", 1.0f / static_cast< float >( d.Height ) );
            g_pFinalPassPSConsts->SetFloat( pd3dDevice, "fExposure", g_fExposure );
            g_pFinalPassPSConsts->SetFloat( pd3dDevice, "fGaussianScalar", PostProcess::GetGaussianMultiplier() );

            DrawHDRTextureToScreen();

            pd3dDevice->SetPixelShader( NULL );

            pd3dDevice->SetTexture( 2, NULL );
            pd3dDevice->SetTexture( 1, NULL );
            pd3dDevice->SetTexture( 0, NULL );

            pd3dDevice->SetRenderTarget( 0, pLDRSurface );

            SAFE_RELEASE( pBloomTex );
            SAFE_RELEASE( pLumTex );
            SAFE_RELEASE( pHDRTex );
            SAFE_RELEASE( pFinalSurf );
        }
        D3DPERF_EndEvent();



        // RENDER THE GUI COMPONENTS
        //--------------------------
        // The remainder of the rendering is specific to this example rather
        // than to a general-purpose HDRI pipeline. The following code outputs
        // each individual stage of the above process so that, in real-time, you
        // can see exactly what is happening...
        D3DPERF_BeginEvent( D3DCOLOR_XRGB( 255, 0, 255 ), L"GUI Rendering" );
        {
            D3DSURFACE_DESC d;
            pLDRSurface->GetDesc( &d );

            TLVertex v[4];

            v[0].t = D3DXVECTOR2( 0.0f, 0.0f );
            v[1].t = D3DXVECTOR2( 1.0f, 0.0f );
            v[2].t = D3DXVECTOR2( 0.0f, 1.0f );
            v[3].t = D3DXVECTOR2( 1.0f, 1.0f );

            pd3dDevice->SetPixelShader( NULL );
            pd3dDevice->SetVertexShader( NULL );
            pd3dDevice->SetFVF( FVF_TLVERTEX );

            float fCellWidth = ( static_cast< float >( d.Width ) - 48.0f ) / 4.0f;
            float fCellHeight = ( static_cast< float >( d.Height ) - 36.0f ) / 4.0f;
            float fFinalHeight = static_cast< float >( d.Height ) - ( fCellHeight + 16.0f );
            float fFinalWidth = fFinalHeight / 0.75f;

            v[0].p = D3DXVECTOR4( fCellWidth + 16.0f, fCellHeight + 16.0f, 0.0f, 1.0f );
            v[1].p = D3DXVECTOR4( fCellWidth + 16.0f + fFinalWidth, fCellHeight + 16.0f, 0.0f, 1.0f );
            v[2].p = D3DXVECTOR4( fCellWidth + 16.0f, fCellHeight + 16.0f + fFinalHeight, 0.0f, 1.0f );
            v[3].p = D3DXVECTOR4( fCellWidth + 16.0f + fFinalWidth, fCellHeight + 16.0f + fFinalHeight, 0.0f, 1.0f );

            pd3dDevice->SetTexture( 0, g_pFinalTexture );
            pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( TLVertex ) );

            // Draw the original HDR scene to the GUI
            D3DPERF_BeginEvent( D3DCOLOR_XRGB( 255, 0, 0 ), L"GUI: HDR Scene" );
            HDRScene::DrawToScreen( pd3dDevice, g_pFont, g_pTextSprite, g_pArrowTex );
            D3DPERF_EndEvent();

            // Draw the various down-sampling stages to the luminance measurement
            D3DPERF_BeginEvent( D3DCOLOR_XRGB( 255, 0, 0 ), L"GUI: Luminance Measurements" );
            Luminance::DisplayLuminance( pd3dDevice, g_pFont, g_pTextSprite, g_pArrowTex );
            D3DPERF_EndEvent();

            // Draw the bright-pass, downsampling and bloom steps
            D3DPERF_BeginEvent( D3DCOLOR_XRGB( 255, 0, 0 ), L"GUI: Post-Processing Effects" );
            PostProcess::DisplaySteps( pd3dDevice, g_pFont, g_pTextSprite, g_pArrowTex );
            D3DPERF_EndEvent();
        }
        D3DPERF_EndEvent();

        D3DPERF_BeginEvent( D3DCOLOR_XRGB( 64, 0, 64 ), L"User Interface Rendering" );
        RenderText();
        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_Config.OnRender( fElapsedTime ) );
        D3DPERF_EndEvent();

        // We've finished all of the rendering, so make sure that D3D
        // is aware of this...
        V( pd3dDevice->EndScene() );
    }

    //Remove any memory involved in the render target switching
    SAFE_RELEASE( pLDRSurface );

}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    DWORD xOff = 16 + ( ( pd3dsdBackBuffer->Width - 48 ) / 4 );
    txtHelper.SetInsertionPos( xOff + 5, pd3dsdBackBuffer->Height - 40 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.5f, 0.0f, 1.0f ) );

    //fill out the text...
    WCHAR strRT[MAX_PATH];
    if( g_FloatRenderTargetFmt == D3DFMT_A16B16G16R16F )
    {
        swprintf_s( strRT, MAX_PATH, L"%s\n", L"Using 64bit floating-point render targets." );
    }
    else if( g_FloatRenderTargetFmt == D3DFMT_A32B32G32R32F )
    {
        swprintf_s( strRT, MAX_PATH, L"%s\n", L"Using 128bit floating-point render targets." );
    }

    static const UINT iMaxStringSize = 1024;
    WCHAR str[iMaxStringSize];
    swprintf_s( str, iMaxStringSize, L"Final Composed Image (%dx%d) @ %dfps.\n"
                     L"%s Approx Memory Consumption is %d bytes.\n",
                     pd3dsdBackBuffer->Width,
                     pd3dsdBackBuffer->Height,
                     g_dwFrameRate,
                     strRT,
                     g_dwApproxMemory );

    txtHelper.DrawTextLine( str );

    txtHelper.SetInsertionPos( xOff + 5, ( ( pd3dsdBackBuffer->Height - 36 ) / 4 ) + 20 );

    txtHelper.DrawTextLine( L"Drag with LEFT mouse button  : Rotate occlusion cube." );
    txtHelper.DrawTextLine( L"Drag with RIGHT mouse button : Rotate view of scene." );

    txtHelper.End();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{

    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give on screen UI a chance to handle this message
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    *pbNoFurtherProcessing = g_Config.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;

}



//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR str[100];

    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_THRESHOLDSLIDER:
        {
            float fNewThreshold = static_cast< float >( g_Config.GetSlider( IDC_THRESHOLDSLIDER )->GetValue() ) /
                10.0f;
            PostProcess::SetBrightPassThreshold( fNewThreshold );
            swprintf_s( str, 100, L"Bright-Pass Threshold: (%.2f)", fNewThreshold );
            g_Config.GetStatic( IDC_THRESHOLDLABEL )->SetText( str );
        }
            break;
        case IDC_EXPOSURESLIDER:
        {
            g_fExposure = static_cast< float >( g_Config.GetSlider( IDC_EXPOSURESLIDER )->GetValue() ) / 100.0f;
            swprintf_s( str, 100, L"Exposure: (%.2f)", g_fExposure );
            g_Config.GetStatic( IDC_EXPOSURELABEL )->SetText( str );
        }
            break;
        case IDC_MULTIPLIERSLIDER:
        {
            float mul = static_cast< float >( g_Config.GetSlider( IDC_MULTIPLIERSLIDER )->GetValue() ) / 10.0f;
            PostProcess::SetGaussianMultiplier( mul );
            swprintf_s( str, 100, L"Gaussian Multiplier: (%.2f)", mul );
            g_Config.GetStatic( IDC_MULTIPLIERLABEL )->SetText( str );
        }
            break;
        case IDC_MEANSLIDER:
        {
            float mean = static_cast< float >( g_Config.GetSlider( IDC_MEANSLIDER )->GetValue() ) / 10.0f;
            PostProcess::SetGaussianMean( mean );
            swprintf_s( str, 100, L"Gaussian Mean: (%.2f)", mean );
            g_Config.GetStatic( IDC_MEANLABEL )->SetText( str );
        }
            break;
        case IDC_STDDEVSLIDER:
        {
            float sd = static_cast< float >( g_Config.GetSlider( IDC_STDDEVSLIDER )->GetValue() ) / 10.0f;
            PostProcess::SetGaussianStandardDeviation( sd );
            swprintf_s( str, 100, L"Gaussian Std. Dev: (%.2f)", sd );
            g_Config.GetStatic( IDC_STDDEVLABEL )->SetText( str );
        }
            break;

    }

}


//--------------------------------------------------------------------------------------
// Release resources created in the OnResetDevice callback here 
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{

    // Let the GUI do it's lost-device work:
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();

    if( g_pFont )
        g_pFont->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );
    SAFE_RELEASE( g_pArrowTex );

    // Free up the HDR resources
    SAFE_RELEASE( g_pFinalTexture );

    // Free up the final screen pass resources
    SAFE_RELEASE( g_pFinalPassPS );
    SAFE_RELEASE( g_pFinalPassPSConsts );

    HDRScene::DestroyResources();
    Luminance::DestroyResources();
    PostProcess::DestroyResources();

}


//--------------------------------------------------------------------------------------
// Release resources created in the OnCreateDevice callback here
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{

    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();

    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pTextSprite );
    SAFE_RELEASE( g_pArrowTex );

    // Free up the HDR resources
    SAFE_RELEASE( g_pFinalTexture );

    // Free up the final screen pass resources
    SAFE_RELEASE( g_pFinalPassPS );
    SAFE_RELEASE( g_pFinalPassPSConsts );

    HDRScene::DestroyResources();
    Luminance::DestroyResources();
    PostProcess::DestroyResources();

}



//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{

    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    //    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
    #endif

    // Set the callback functions
    DXUTSetCallbackD3D9DeviceAcceptable( IsDeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnCreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnFrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnLostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_Config.Init( &g_DialogResourceManager );

    g_HUD.SetFont( 0, L"Arial", 14, FW_NORMAL );
    g_HUD.SetCallback( OnGUIEvent );

    g_Config.SetFont( 0, L"Arial", 12, FW_NORMAL );
    g_Config.SetCallback( OnGUIEvent );

    int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );


    g_Config.AddStatic( IDC_MULTIPLIERLABEL, L"Gaussian Multiplier: (0.4)", 35, 0, 125, 15 );
    g_Config.AddSlider( IDC_MULTIPLIERSLIDER, 35, 20, 125, 16, 0, 20, 4 );

    g_Config.AddStatic( IDC_MEANLABEL, L"Gaussian Mean: (0.0)", 35, 40, 125, 15 );
    g_Config.AddSlider( IDC_MEANSLIDER, 35, 60, 125, 16, -10, 10, 0 );

    g_Config.AddStatic( IDC_STDDEVLABEL, L"Gaussian Std. Dev: (0.8)", 35, 80, 125, 15 );
    g_Config.AddSlider( IDC_STDDEVSLIDER, 35, 100, 125, 16, 0, 10, 8 );

    g_Config.AddStatic( IDC_THRESHOLDLABEL, L"Bright-Pass Threshold: (0.8)", 35, 120, 125, 15 );
    g_Config.AddSlider( IDC_THRESHOLDSLIDER, 35, 140, 125, 16, 0, 25, 8 );

    g_Config.AddStatic( IDC_EXPOSURELABEL, L"Exposure: (0.5)", 35, 160, 125, 15 );
    g_Config.AddSlider( IDC_EXPOSURESLIDER, 35, 180, 125, 16, 0, 200, 50 );

    // Initialize DXUT and create the desired Win32 window and Direct3D device for the application
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the default hotkeys
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"HDR Pipeline Demonstration" );
    DXUTCreateDevice( true, 640, 480 );

    // Start the render loop
    g_dwLastFPSCheck = GetTickCount();
    DXUTMainLoop();

    return DXUTGetExitCode();

}

//--------------------------------------------------------------------------------------
//  The last stage of the HDR pipeline requires us to take the image that was rendered
//  to an HDR-format texture and map it onto a LDR render target that can be displayed
//  on screen. This is done by rendering a screen-space quad and setting a pixel shader
//  up to map HDR->LDR.
//--------------------------------------------------------------------------------------
void DrawHDRTextureToScreen()
{

    // Find out what dimensions the screen is
    IDirect3DDevice9* pDev = DXUTGetD3D9Device();
    assert( pDev != NULL );
    D3DSURFACE_DESC desc;
    LPDIRECT3DSURFACE9 pSurfRT;

    pDev->GetRenderTarget( 0, &pSurfRT );
    pSurfRT->GetDesc( &desc );
    pSurfRT->Release();

    // To correctly map from texels->pixels we offset the coordinates
    // by -0.5f:
    float fWidth = static_cast< float >( desc.Width ) - 0.5f;
    float fHeight = static_cast< float >( desc.Height ) - 0.5f;

    // Now we can actually assemble the screen-space geometry
    TLVertex v[4];

    v[0].p = D3DXVECTOR4( -0.5f, -0.5f, 0.0f, 1.0f );
    v[0].t = D3DXVECTOR2( 0.0f, 0.0f );

    v[1].p = D3DXVECTOR4( fWidth, -0.5f, 0.0f, 1.0f );
    v[1].t = D3DXVECTOR2( 1.0f, 0.0f );

    v[2].p = D3DXVECTOR4( -0.5f, fHeight, 0.0f, 1.0f );
    v[2].t = D3DXVECTOR2( 0.0f, 1.0f );

    v[3].p = D3DXVECTOR4( fWidth, fHeight, 0.0f, 1.0f );
    v[3].t = D3DXVECTOR2( 1.0f, 1.0f );

    // Configure the device and render..
    pDev->SetVertexShader( NULL );
    pDev->SetFVF( FVF_TLVERTEX );
    pDev->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDev->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDev->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( TLVertex ) );

}
