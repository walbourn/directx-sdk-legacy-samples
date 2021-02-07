//--------------------------------------------------------------------------------------
// File: ContactHardeningShadows11.cpp
//
// This file implements the C++ part of contact hardening shadows technique
// which uses sm5.0 instructions to run fast without dithering artefacts
//
// Contributed by AMD Developer Relations Team
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include <D3DX11tex.h>
#include <D3DX11.h>
#include <D3DX11core.h>
#include <D3DX11async.h>

bool    g_bLeftButtonDown = false;
bool    g_bRightButtonDown = false;
bool    g_bMiddleButtonDown = false;
bool    g_bGuiVisible = true;

CDXUTDialogResourceManager  g_DialogResourceManager;    // Manager for shared resources of dialogs
CFirstPersonCamera          g_Camera;                   // The first person viewer camera
CModelViewerCamera          g_LCamera;                  // A model viewing camera for the light
CD3DSettingsDlg             g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                 g_HUD;                      // Dialog for standard controls
CDXUTDialog                 g_SampleUI;                 // Dialog for sample specific controls
CDXUTTextHelper*            g_pTxtHelper = NULL;
CDXUTTextHelper*            g_pTxtHelper1 = NULL;
float                       g_fShadowMapWidth = 1024.0f;
float                       g_fShadowMapHeight = 1024.0f;
float                       g_fSunWidth = 2.0f;

// blend states
ID3D11BlendState*    g_pBlendStateNoBlend = NULL;
ID3D11BlendState*    g_pBlendStateColorWritesOff = NULL;

// scene mesh
CDXUTSDKMesh                g_SceneMesh;
CDXUTSDKMesh                g_Poles;
static ID3D11InputLayout*   g_pSceneVertexLayout = NULL;

// Samplers
ID3D11SamplerState*   g_pSamplePoint    = NULL;
ID3D11SamplerState*   g_pSampleLinear   = NULL;
ID3D11SamplerState*   g_pSamplePointCmp = NULL;

// Shaders
ID3D11VertexShader*       g_pSceneVS = NULL;
ID3D11PixelShader*        g_pScenePS = NULL;
ID3D11VertexShader*       g_pSM_VS = NULL;

// Constant buffer layout for transferring data to the utility HLSL functions
struct CB_CONSTANTS
{
    D3DXMATRIX    f4x4WorldViewProjection;      // World * View * Projection matrix  
    D3DXMATRIX    f4x4WorldViewProjLight;       // World * ViewLight * Projection Light matrix  
    D3DXVECTOR4   vShadowMapDimensions;         // shadow map dimensions
    D3DXVECTOR4   vLightDir;                    // shadow map dimensions
    float         fSunWidth;
    float         f3Pad[3];
};
UINT                        g_iCONSTANTSCBBind = 0;

// Various Constant buffers
ID3D11Buffer*               g_pcbConstants = NULL;                 

// SM depth stencil texture data
static ID3D11Texture2D*             g_pRSMDepthStencilTexture = NULL;
// SRV of the SM
static ID3D11ShaderResourceView*    g_pDepthTextureSRV = NULL;
// depth stencil view of the SM
static ID3D11DepthStencilView*      g_pDepthStencilTextureDSV = NULL;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_SUNWIDTH_SLIDER     5
#define IDC_SUNWIDTH_TEXT       6

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, 
                                       const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, 
                                       void* pUserContext );
void    CALLBACK MouseProc( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, 
                            bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta, 
                            int xPos, int yPos, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

//--------------------------------------------------------------------------------------
// Find and compile the specified shader
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, 
                               LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile( str, NULL, NULL, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL );
    if( FAILED(hr) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                     LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackMouse( MouseProc );
    DXUTSetCallbackKeyboard( OnKeyboard );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();
    
    DXUTInit( true, true );

    DXUTSetCursorSettings( true, true );// Show the cursor and clip when in full screen
    DXUTCreateWindow( L"Contact Hardening Shadows - Direct3D 11" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 800, 600 );
    DXUTMainLoop();                         // Enter into the DXUT render loop

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Handle mouse buttons
//--------------------------------------------------------------------------------------
void CALLBACK MouseProc( bool bLeftButtonDown, bool bRightButtonDown, 
                         bool bMiddleButtonDown, bool bSideButton1Down, 
                         bool bSideButton2Down, int nMouseWheelDelta, 
                         int xPos, int yPos, void* pUserContext )
{
    bool bOldLeftButtonDown = g_bLeftButtonDown;
    bool bOldRightButtonDown = g_bRightButtonDown;
    bool bOldMiddleButtonDown = g_bMiddleButtonDown;
    g_bLeftButtonDown = bLeftButtonDown;
    g_bMiddleButtonDown = bMiddleButtonDown;
    g_bRightButtonDown = bRightButtonDown;

    if( bOldLeftButtonDown && !g_bLeftButtonDown )
        g_Camera.SetEnablePositionMovement( false );
    else if( !bOldLeftButtonDown && g_bLeftButtonDown )
        g_Camera.SetEnablePositionMovement( true );

    if( !bOldRightButtonDown && g_bRightButtonDown )
    {
        g_Camera.SetEnablePositionMovement( false );
    }

    if( bOldMiddleButtonDown && !g_bMiddleButtonDown )
    {
        g_LCamera.SetEnablePositionMovement( false );
    } 
    else if( !bOldMiddleButtonDown && g_bMiddleButtonDown )
    {
        g_LCamera.SetEnablePositionMovement( true );
        g_Camera.SetEnablePositionMovement( false );
    }

    // If no mouse button is down at all, enable camera movement.
    if( !g_bLeftButtonDown && !g_bRightButtonDown && !g_bMiddleButtonDown )
        g_Camera.SetEnablePositionMovement( true );
}

void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown && nChar == 'G' )
    {
        g_bGuiVisible = ( g_bGuiVisible ? false : true );
    }
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    WCHAR temp[64];
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 30;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );

    iY += 180;
    swprintf_s( temp, L"SunWidth = %2.2f", g_fSunWidth );
    g_HUD.AddStatic( IDC_SUNWIDTH_TEXT, temp, 0, iY += 25, 100, 24 );
    g_HUD.AddSlider( IDC_SUNWIDTH_SLIDER, 0, iY += 25, 150, 24, 0, 100, int( ( g_fSunWidth / 3.0 ) * 100 ) ); 

    g_Camera.SetRotateButtons( true, false, false );
    g_LCamera.SetButtonMasks( MOUSE_RIGHT_BUTTON, 0, 0 );

    g_SampleUI.SetCallback( OnGUIEvent ); 
}

//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( pDeviceSettings->ver == DXUT_D3D11_DEVICE );
    
    // Force D32 depth buffer
    pDeviceSettings->d3d11.AutoDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;

    // For the first device created if it is a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && 
              pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
    g_LCamera.FrameMove( fElapsedTime );
}

//--------------------------------------------------------------------------------------
// Render text part of the GUI
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 1.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->DrawTextLine( L"'G' key toggles display of GUI" );

    g_pTxtHelper->End();
 
    if( g_pScenePS == NULL )
    {
        g_pTxtHelper1->Begin();
        g_pTxtHelper1->SetInsertionPos( ( DXUTGetDXGIBackBufferSurfaceDesc()->Width - 300 )/2,
                                          DXUTGetDXGIBackBufferSurfaceDesc()->Height / 2 );
        g_pTxtHelper1->SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
        g_pTxtHelper1->DrawTextLine( L"Compiling shaders ..." );
        g_pTxtHelper1->End();
    }
 }

//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, 
                          LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );
    g_LCamera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, 
                          void* pUserContext )
{
    if( g_bGuiVisible )
    {
        switch( nControlID )
        {
            case IDC_TOGGLEFULLSCREEN:
                DXUTToggleFullScreen(); break;
            case IDC_TOGGLEREF:
                DXUTToggleREF(); break;
            case IDC_CHANGEDEVICE:
                g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
            case IDC_SUNWIDTH_SLIDER:
                WCHAR temp[64];
                int iVal = g_HUD.GetSlider( IDC_SUNWIDTH_SLIDER )->GetValue();

                g_fSunWidth = ( float( iVal ) / 100.0f ) * 3.0f;
                swprintf_s( temp, L"SunWidth = %2.2f", g_fSunWidth );
                g_HUD.GetStatic( IDC_SUNWIDTH_TEXT )->SetText( temp );

                break;
        }
    }
}

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, 
                                       UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, 
                                       void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );
    g_pTxtHelper1 = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 35 );

    // textures / rts
    D3D11_TEXTURE2D_DESC TDesc;
      
    TDesc.Width  = UINT(g_fShadowMapWidth);
    TDesc.Height = UINT(g_fShadowMapHeight);
    TDesc.MipLevels = 1;
    TDesc.ArraySize = 1;
    TDesc.Format = DXGI_FORMAT_R16_TYPELESS;
    TDesc.SampleDesc.Count = 1;
    TDesc.SampleDesc.Quality = 0;
    TDesc.Usage = D3D11_USAGE_DEFAULT;
    TDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    TDesc.CPUAccessFlags = 0;
    TDesc.MiscFlags = 0;
    pd3dDevice->CreateTexture2D( &TDesc, 0, &g_pRSMDepthStencilTexture);
    DXUT_SetDebugName( g_pRSMDepthStencilTexture, "RSM" );

    D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
    
    DSVDesc.Format = DXGI_FORMAT_D16_UNORM;
    DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    DSVDesc.Flags = 0;
    DSVDesc.Texture2D.MipSlice = 0;
    pd3dDevice->CreateDepthStencilView( g_pRSMDepthStencilTexture, &DSVDesc, & g_pDepthStencilTextureDSV );
    DXUT_SetDebugName( g_pDepthStencilTextureDSV, "RSM DSV" );

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    
    SRVDesc.Format =  DXGI_FORMAT_R16_UNORM;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    pd3dDevice->CreateShaderResourceView( g_pRSMDepthStencilTexture, &SRVDesc, &g_pDepthTextureSRV );
    DXUT_SetDebugName( g_pDepthTextureSRV, "RSM SRV" );

    // Setup constant buffers
    D3D11_BUFFER_DESC Desc;
    
    // Utility
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;    
    Desc.ByteWidth = sizeof( CB_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbConstants ) );
    DXUT_SetDebugName( g_pcbConstants, "CB_CONSTANTS"  );

    // Load the scene mesh
    WCHAR str[256];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, 256,  L"ColumnScene\\scene.sdkmesh" ) );
    g_SceneMesh.Create( pd3dDevice, str, false );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, 256,  L"ColumnScene\\poles.sdkmesh" ) );
    g_Poles.Create( pd3dDevice, str, false );

    // Setup the camera   
    D3DXVECTOR3 vecEye( 0.95f, 5.83f, -14.48f );
    D3DXVECTOR3 vecAt ( 0.90f, 5.44f, -13.56f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    D3DXVECTOR3 vecEyeL = D3DXVECTOR3( 0,0,0 );
    D3DXVECTOR3 vecAtL ( 0, -0.5, 1 );
    g_LCamera.SetViewParams( &vecEyeL, &vecAtL );

    // Create sampler states for point and linear

    // PointCmp
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 1.0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSamplePointCmp ) );
    DXUT_SetDebugName( g_pSamplePointCmp, "PointCmp" );

    // Point
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSamplePoint ) );
    DXUT_SetDebugName( g_pSamplePoint, "Point" );

    // Linear
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSampleLinear ) );
    DXUT_SetDebugName( g_pSampleLinear, "Linear" );

    // Create a blend state to disable alpha blending
    D3D11_BLEND_DESC BlendState;
    ZeroMemory(&BlendState, sizeof(D3D11_BLEND_DESC));
    BlendState.IndependentBlendEnable = FALSE;
    BlendState.RenderTarget[0].BlendEnable = FALSE;
    BlendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = pd3dDevice->CreateBlendState(&BlendState, &g_pBlendStateNoBlend);
    DXUT_SetDebugName( g_pBlendStateNoBlend, "No Blend" );
    
    BlendState.RenderTarget[0].RenderTargetWriteMask = 0;
    hr = pd3dDevice->CreateBlendState(&BlendState, &g_pBlendStateColorWritesOff);
    DXUT_SetDebugName( g_pBlendStateColorWritesOff, "Color Writes Off");

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Callback function for changed window size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters    
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.5f, 100.0f );
    g_LCamera.SetProjParams( D3DX_PI / 4, fAspectRatio, 10.0f, 100.0f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 240 );
    g_SampleUI.SetSize( 150, 110 );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Renders the depth only shadow map
//--------------------------------------------------------------------------------------
void RenderShadowMap( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                      D3DXMATRIXA16& mViewProjLight, D3DXVECTOR3& vLightDir )
{
    D3D11_RECT oldrects[1];
    D3D11_VIEWPORT oldvp[2];
    UINT num = 1;
    pd3dImmediateContext->RSGetScissorRects( &num, oldrects );
    num = 1;
    pd3dImmediateContext->RSGetViewports( &num, oldvp );
    oldvp[ 1 ] = oldvp[ 0 ];

    D3D11_RECT rects[1] = { { 0, UINT(g_fShadowMapWidth), 0, UINT(g_fShadowMapHeight) } };
    pd3dImmediateContext->RSSetScissorRects( 1, rects );

    D3D11_VIEWPORT vp[1] = { { 0, 0, g_fShadowMapWidth, g_fShadowMapHeight, 0.0f, 1.0f } };
    pd3dImmediateContext->RSSetViewports( 1, vp );

    // Set our scene render target & keep original depth buffer
    ID3D11RenderTargetView* pRTVs[2] = {0,0};
    pd3dImmediateContext->OMSetRenderTargets( 2, pRTVs, g_pDepthStencilTextureDSV );
    
    // Clear the render target
    pd3dImmediateContext->ClearDepthStencilView( g_pDepthStencilTextureDSV, 
                                                 D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 
                                                 1.0, 0 );

    // Get the projection & view matrix from the camera class
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;

    D3DXVECTOR3 up(0,1,0);
    D3DXVECTOR4 vLight( 0.0f,0.0f,0.0f, 1.0f );
    D3DXVECTOR4 vLightLookAt4d( 0.0f,-0.5f,1.0f, 0.0f );
    vLightLookAt4d = vLight + vLightLookAt4d;
    D3DXVec4Transform( &vLight, &vLight, g_LCamera.GetWorldMatrix() );
    D3DXVec4Transform( &vLightLookAt4d, &vLightLookAt4d, g_LCamera.GetWorldMatrix() );
    D3DXMatrixOrthoOffCenterLH( &mProj, -8.5, 9, -15, 11, -20, 20 );
    D3DXVECTOR3 vLight3d( vLight.x, vLight.y, vLight.z );
    D3DXVECTOR3 vLightLookAt3d( vLightLookAt4d.x, vLightLookAt4d.y, vLightLookAt4d.z );
    vLightDir = vLightLookAt3d - vLight3d;

    D3DXMatrixLookAtLH( &mView, &vLight3d, &vLightLookAt3d, &up);
    
    mViewProjLight = mView * mProj;

    // Setup the constant buffer for the scene vertex shader
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dImmediateContext->Map( g_pcbConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_CONSTANTS* pConstants = ( CB_CONSTANTS* )MappedResource.pData;
    D3DXMatrixTranspose( &pConstants->f4x4WorldViewProjection, &mViewProjLight );
    D3DXMatrixTranspose( &pConstants->f4x4WorldViewProjLight,  &mViewProjLight );
    pConstants->vShadowMapDimensions =    D3DXVECTOR4(g_fShadowMapWidth, 
                                                      g_fShadowMapHeight, 
                                                      1.0f/g_fShadowMapWidth, 
                                                      1.0f/g_fShadowMapHeight);
    pd3dImmediateContext->Unmap( g_pcbConstants, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( g_iCONSTANTSCBBind, 1, &g_pcbConstants );
    pd3dImmediateContext->PSSetConstantBuffers( g_iCONSTANTSCBBind, 1, &g_pcbConstants );

    // Set the shaders
    pd3dImmediateContext->VSSetShader( g_pSM_VS, NULL, 0 );
    pd3dImmediateContext->PSSetShader( NULL, NULL, 0 );

    // Set the vertex buffer format
    pd3dImmediateContext->IASetInputLayout( g_pSceneVertexLayout );

    // Render the scene
    g_Poles.Render( pd3dImmediateContext, 0 );

    // reset the old viewport etc.
    pd3dImmediateContext->RSSetScissorRects( 1, oldrects );
    pd3dImmediateContext->RSSetViewports( 1, oldvp );
}

//--------------------------------------------------------------------------------------
// render callback
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, 
                                  ID3D11DeviceContext* pd3dImmediateContext, 
                                  double fTime,
                                  float fElapsedTime, void* pUserContext )
{
    static int s_iCounter = 0;

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    if( g_pScenePS == NULL && s_iCounter == 0 )
    {
        s_iCounter = 4;
    }

    if( s_iCounter > 0 )
        s_iCounter --;

    if( s_iCounter == 1 && g_pScenePS == NULL )
    {
        HRESULT hr = S_OK;

        // Create the shaders
        ID3DBlob* pBlob = NULL;

        // VS
        hr = CompileShaderFromFile( L"ContactHardeningShadows11.hlsl", "VS_RenderScene", "vs_5_0", &pBlob ); 
        hr = pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSceneVS );
        DXUT_SetDebugName( g_pSceneVS, "VS_RenderScene" );
        // Define our scene vertex data layout
        const D3D11_INPUT_ELEMENT_DESC SceneLayout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = pd3dDevice->CreateInputLayout( SceneLayout, ARRAYSIZE( SceneLayout ), pBlob->GetBufferPointer(),
                                                    pBlob->GetBufferSize(), &g_pSceneVertexLayout );
        SAFE_RELEASE( pBlob );
        DXUT_SetDebugName( g_pSceneVertexLayout, "SceneLayout" );

        hr = CompileShaderFromFile( L"ContactHardeningShadows11.hlsl", "VS_RenderSceneSM", "vs_5_0", &pBlob ); 
        hr = pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSM_VS );
        SAFE_RELEASE( pBlob );
        DXUT_SetDebugName( g_pSM_VS, "VS_RenderSceneSM" );

        // PS
        hr = CompileShaderFromFile( L"ContactHardeningShadows11.hlsl", "PS_RenderScene", "ps_5_0", &pBlob ); 
        hr = pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pScenePS );
        SAFE_RELEASE( pBlob );
        DXUT_SetDebugName( g_pScenePS, "PS_RenderScene" );

        s_iCounter = 0;
    }
    else if( g_pScenePS != NULL )
    {
        ID3D11RenderTargetView*      pRTV[2] = { NULL,NULL };
        ID3D11ShaderResourceView*    pSRV[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

        // Array of our samplers
        ID3D11SamplerState* ppSamplerStates[3] = { g_pSamplePoint, g_pSampleLinear, g_pSamplePointCmp };

        pd3dImmediateContext->PSSetSamplers( 0, 3, ppSamplerStates );
                
            // Store off original render target, this is the back buffer of the swap chain
        ID3D11RenderTargetView* pOrigRTV = DXUTGetD3D11RenderTargetView();
        ID3D11DepthStencilView* pOrigDSV = DXUTGetD3D11DepthStencilView();
            
        // Clear the render target
        float ClearColor[4] = { 0.0f, 0.25f, 0.25f, 0.55f };
        pd3dImmediateContext->ClearRenderTargetView( DXUTGetD3D11RenderTargetView(), 
                                                     ClearColor );
        pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), 
                                                     D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 
                                                     1.0, 0 );

            // Get the projection & view matrix from the camera class
        D3DXMATRIXA16 mWorld;
        D3DXMATRIXA16 mView;
        D3DXMATRIXA16 mProj;
        D3DXMATRIXA16 mViewProjLight;
        D3DXMATRIXA16 mWorldViewProjection;
        D3DXVECTOR3   vLightDir;

        // disable color writes
        pd3dImmediateContext->OMSetBlendState(g_pBlendStateColorWritesOff, 0, 0xffffffff);

        RenderShadowMap( pd3dDevice, pd3dImmediateContext, mViewProjLight, vLightDir );

        // enable color writes
        pd3dImmediateContext->OMSetBlendState(g_pBlendStateNoBlend, 0, 0xffffffff);

        mView  = *g_Camera.GetViewMatrix();
        mProj  = *g_Camera.GetProjMatrix();
        mWorldViewProjection = mView * mProj;

        // Setup the constant buffer for the scene vertex shader
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dImmediateContext->Map( g_pcbConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_CONSTANTS* pConstants = ( CB_CONSTANTS* )MappedResource.pData;
        D3DXMatrixTranspose( &pConstants->f4x4WorldViewProjection, &mWorldViewProjection );
        D3DXMatrixTranspose( &pConstants->f4x4WorldViewProjLight,  &mViewProjLight );
        pConstants->vShadowMapDimensions =    D3DXVECTOR4(g_fShadowMapWidth, g_fShadowMapHeight, 
                                                          1.0f/g_fShadowMapWidth, 
                                                          1.0f/g_fShadowMapHeight);
        pConstants->vLightDir            =    D3DXVECTOR4( vLightDir.x, vLightDir.y,
                                                           vLightDir.z, 0.0f );
        pConstants->fSunWidth            =    g_fSunWidth;
        pd3dImmediateContext->Unmap( g_pcbConstants, 0 );
        pd3dImmediateContext->VSSetConstantBuffers( g_iCONSTANTSCBBind, 1, &g_pcbConstants );
        pd3dImmediateContext->PSSetConstantBuffers( g_iCONSTANTSCBBind, 1, &g_pcbConstants );

        // Set the shaders
        pd3dImmediateContext->VSSetShader( g_pSceneVS, NULL, 0 );
        pd3dImmediateContext->PSSetShader( g_pScenePS, NULL, 0 );

        // Set the vertex buffer format
        pd3dImmediateContext->IASetInputLayout( g_pSceneVertexLayout );
        
        // Rebind to original back buffer and depth buffer
        pRTV[0] = pOrigRTV;
        pd3dImmediateContext->OMSetRenderTargets(1, pRTV, pOrigDSV );

        // set the shadow map
        pd3dImmediateContext->PSSetShaderResources( 1, 1, &g_pDepthTextureSRV );

        // Render the scene
        g_SceneMesh.Render( pd3dImmediateContext, 0 );
        g_Poles.Render( pd3dImmediateContext, 0 );

        // restore resources
        pd3dImmediateContext->PSSetShaderResources( 0, 8, pSRV );
    }

    // Render GUI
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    if( g_bGuiVisible )
    {
        g_HUD.OnRender( fElapsedTime );
        g_SampleUI.OnRender( fElapsedTime );
    }
    RenderText();
    DXUT_EndPerfEvent();
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );
    SAFE_DELETE( g_pTxtHelper1 );

    g_SceneMesh.Destroy();
    g_Poles.Destroy();

    SAFE_RELEASE( g_pSceneVS );
    SAFE_RELEASE( g_pScenePS );
    SAFE_RELEASE( g_pSM_VS );
    SAFE_RELEASE( g_pcbConstants );
    SAFE_RELEASE( g_pRSMDepthStencilTexture );
    SAFE_RELEASE( g_pDepthStencilTextureDSV );
    SAFE_RELEASE( g_pDepthTextureSRV );
    SAFE_RELEASE( g_pSceneVertexLayout );
    SAFE_RELEASE( g_pBlendStateNoBlend );
    SAFE_RELEASE( g_pBlendStateColorWritesOff );
    SAFE_RELEASE( g_pSamplePoint );
    SAFE_RELEASE( g_pSamplePointCmp );
    SAFE_RELEASE( g_pSampleLinear );
}

//--------------------------------------------------------------------------------------
// Callback called when releasing the swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
