//--------------------------------------------------------------------------------------
// File: Text3D.cpp
//
// Desc: Example code showing how to do text in a Direct3D scene.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include <commdlg.h>
#include "resource.h"


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXSprite*                g_pTextSprite = NULL;   // Sprite for batching draw text calls
CFirstPersonCamera          g_Camera;               // A model viewing camera
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls

ID3DXFont*                  g_pFont = NULL;
ID3DXFont*                  g_pFont2 = NULL;
ID3DXFont*                  g_pStatsFont = NULL;
ID3DXMesh*                  g_pMesh3DText = NULL;
WCHAR*                      g_strTextBuffer = NULL;

TCHAR g_strFont[LF_FACESIZE];
int                         g_nFontSize;

D3DXMATRIXA16               g_matObj1;
D3DXMATRIXA16               g_matObj2;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_CHANGEFONT          5



//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed,
                                  void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext );
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnLostDevice( void* pUserContext );
void CALLBACK OnDestroyDevice( void* pUserContext );

void InitApp();
HRESULT CreateD3DXTextMesh( IDirect3DDevice9* pd3dDevice, LPD3DXMESH* ppMesh, TCHAR* pstrFont, DWORD dwSize,
                            BOOL bBold, BOOL bItalic );
HRESULT CreateD3DXFont( LPD3DXFONT* ppd3dxFont, TCHAR* pstrFont, DWORD dwSize );


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Set the callback functions. These functions allow DXUT to notify
    // the application about device changes, user input, and windows messages.  The 
    // callbacks are optional so you need only set callbacks for events you're interested 
    // in. However, if you don't handle the device reset/lost callbacks then the sample 
    // framework won't be able to reset your device since the application must first 
    // release all device resources before resetting.  Likewise, if you don't handle the 
    // device created/destroyed callbacks then DXUT won't be able to 
    // recreate your device resources.
    DXUTSetCallbackD3D9DeviceAcceptable( IsDeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnCreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnFrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnLostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );
    InitApp();

    // Initialize DXUT and create the desired Win32 window and Direct3D 
    // device for the application. Calling each of these functions is optional, but they
    // allow you to set several options which control the behavior of the framework.
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the defaul hotkeys
    DXUTCreateWindow( L"Text3D" );
    DXUTCreateDevice( true, 640, 480 );

    // Pass control to DXUT for handling the message pump and 
    // dispatching render calls. DXUT will call your FrameMove 
    // and FrameRender callback when there is idle time between handling window messages.
    DXUTMainLoop();

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.
    SAFE_DELETE_ARRAY( g_strTextBuffer );

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    wcscpy_s( g_strFont, 32, L"Arial" );

    HRSRC rsrc;
    HGLOBAL hgData;
    LPVOID pvData;
    DWORD cbData;

    rsrc = FindResource( NULL, MAKEINTRESOURCE( IDR_TXT ), L"TEXT" );
    if( rsrc != NULL )
    {
        cbData = SizeofResource( NULL, rsrc );
        if( cbData > 0 )
        {
            hgData = LoadResource( NULL, rsrc );
            if( hgData != NULL )
            {
                pvData = LockResource( hgData );
                if( pvData != NULL )
                {
                    int nSize = cbData / sizeof( WCHAR ) + 1;
                    g_strTextBuffer = new WCHAR[nSize];
                    memcpy( g_strTextBuffer, ( WCHAR* )pvData, cbData );
                    g_strTextBuffer[nSize - 1] = 0;
                }
            }
        }
    }

    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.AddButton( IDC_CHANGEFONT, L"Change Font", 35, iY += 24, 125, 22 );
}


//--------------------------------------------------------------------------------------
// Called during device initialization, this code checks the device for some 
// minimum set of capabilities, and rejects those that don't pass by returning false.
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

    return true;
}


//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise IDirect3D9::CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( DXUT_D3D9_DEVICE == pDeviceSettings->ver );

    HRESULT hr;
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    D3DCAPS9 caps;

    V( pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal,
                            pDeviceSettings->d3d9.DeviceType,
                            &caps ) );

    // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
    // then switch to SWVP.
    if( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
        caps.VertexShaderVersion < D3DVS_VERSION( 1, 1 ) )
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // Debugging vertex shaders requires either REF or software vertex processing 
    // and debugging pixel shaders requires REF.  
#ifdef DEBUG_VS
    if( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF )
    {
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
        pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }
#endif
#ifdef DEBUG_PS
    pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
#endif
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnDestroyDevice callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{
    HRESULT hr;


    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );
    HDC hDC = GetDC( NULL );
    int nLogPixelsY = GetDeviceCaps( hDC, LOGPIXELSY );
    ReleaseDC( NULL, hDC );

    int nHeight = -g_nFontSize * nLogPixelsY / 72;
    hr = D3DXCreateFont( pd3dDevice,            // D3D device
                         nHeight,               // Height
                         0,                     // Width
                         FW_BOLD,               // Weight
                         1,                     // MipLevels, 0 = autogen mipmaps
                         FALSE,                 // Italic
                         DEFAULT_CHARSET,       // CharSet
                         OUT_DEFAULT_PRECIS,    // OutputPrecision
                         DEFAULT_QUALITY,       // Quality
                         DEFAULT_PITCH | FF_DONTCARE, // PitchAndFamily
                         g_strFont,              // pFaceName
                         &g_pFont );              // ppFont
    if( FAILED( hr ) )
        return hr;

    if( FAILED( hr = D3DXCreateFont( pd3dDevice, -12, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     L"System", &g_pFont2 ) ) )
        return hr;

    if( FAILED( hr = D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     L"Arial", &g_pStatsFont ) ) )
        return hr;

    if( FAILED( hr = D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// reset, which will happen after a lost device scenario. This is the best location to 
// create D3DPOOL_DEFAULT resources since these resources need to be reloaded whenever 
// the device is lost. Resources created here should be released in the OnLostDevice 
// callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice,
                                const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    // Restore the fonts
    g_pStatsFont->OnResetDevice();
    g_pFont->OnResetDevice();
    g_pFont2->OnResetDevice();
    g_pTextSprite->OnResetDevice();

    // Restore the textures
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x80808080 );
    D3DLIGHT9 light;
    D3DXVECTOR3 vecLightDirUnnormalized( 10.0f, -10.0f, 10.0f );
    ZeroMemory( &light, sizeof( D3DLIGHT9 ) );
    light.Type = D3DLIGHT_DIRECTIONAL;
    light.Diffuse.r = 1.0f;
    light.Diffuse.g = 1.0f;
    light.Diffuse.b = 1.0f;
    D3DXVec3Normalize( ( D3DXVECTOR3* )&light.Direction, &vecLightDirUnnormalized );
    light.Position.x = 10.0f;
    light.Position.y = -10.0f;
    light.Position.z = 10.0f;
    light.Range = 1000.0f;
    pd3dDevice->SetLight( 0, &light );
    pd3dDevice->LightEnable( 0, TRUE );

    // Set the transform matrices
    D3DXMATRIXA16 matWorld;
    D3DXMatrixIdentity( &matWorld );
    pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    // Setup the camera with view & projection matrix
    D3DXVECTOR3 vecEye( 0.0f,-5.0f, -10.0f );
    D3DXVECTOR3 vecAt ( 0.2f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 1.0f, 1000.0f );

    SAFE_RELEASE( g_pMesh3DText );
    if( FAILED( CreateD3DXTextMesh( pd3dDevice, &g_pMesh3DText, g_strFont, g_nFontSize, FALSE, FALSE ) ) )
        return E_FAIL;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CreateD3DXTextMesh()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CreateD3DXTextMesh( IDirect3DDevice9* pd3dDevice,
                            LPD3DXMESH* ppMesh,
                            TCHAR* pstrFont, DWORD dwSize,
                            BOOL bBold, BOOL bItalic )
{
    HRESULT hr;
    LPD3DXMESH pMeshNew = NULL;
    HDC hdc = CreateCompatibleDC( NULL );
    if( hdc == NULL )
        return E_OUTOFMEMORY;
    INT nHeight = -MulDiv( dwSize, GetDeviceCaps( hdc, LOGPIXELSY ), 72 );
    HFONT hFont;
    HFONT hFontOld;

    hFont = CreateFont( nHeight, 0, 0, 0, bBold ? FW_BOLD : FW_NORMAL, bItalic, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                        pstrFont );

    hFontOld = ( HFONT )SelectObject( hdc, hFont );

    hr = D3DXCreateText( pd3dDevice, hdc, L"This is calling D3DXCreateText",
                         0.001f, 0.4f, &pMeshNew, NULL, NULL );

    SelectObject( hdc, hFontOld );
    DeleteObject( hFont );
    DeleteDC( hdc );

    if( SUCCEEDED( hr ) )
        *ppMesh = pMeshNew;

    return hr;
}




//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Setup five rotation matrices (for rotating text strings)
    D3DXVECTOR3 vAxis1( 1.0f,2.0f,0.0f );
    D3DXVECTOR3 vAxis2( 1.0f,0.0f,0.0f );
    D3DXMatrixRotationAxis( &g_matObj1, &vAxis1, ( float )fTime / 2.0f );
    D3DXMatrixRotationAxis( &g_matObj2, &vAxis2, ( float )fTime );

    D3DXMATRIX mScale;
    D3DXMatrixScaling( &mScale, 0.5f, 0.5f, 0.5f );
    g_matObj2 *= mScale;

    // Add some translational values to the matrices
    g_matObj1._41 = 1.0f;   g_matObj1._42 = 6.0f;   g_matObj1._43 = 20.0f;
    g_matObj2._41 = -4.0f;  g_matObj2._42 = -1.0f;  g_matObj2._43 = 0.0f;
}


//--------------------------------------------------------------------------------------
// This callback function will be called at the end of every frame to perform all the 
// rendering calls for the scene, and it will also be called if the window needs to be 
// repainted. After this function has returned, DXUT will call 
// IDirect3DDevice9::Present to display the contents of the next buffer in the swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    HRESULT hr;
    RECT rc;
    D3DMATERIAL9 mtrl;
    D3DXMATRIXA16 matWorld;

    // Get the view & projection matrix from camera.
    // User can't control camera for this simple sample
    D3DXMATRIXA16 matView = *g_Camera.GetViewMatrix();
    D3DXMATRIXA16 matProj = *g_Camera.GetProjMatrix();
    D3DXMATRIXA16 matViewProj = matView * matProj;

    pd3dDevice->SetTransform( D3DTS_VIEW, &matView );
    pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Clear the viewport
    pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0L );

    // Begin the scene 
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        UINT nHeight = DXUTGetD3D9PresentParameters().BackBufferHeight;

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Demonstration Code" );

        // Demonstration 1:
        // Draw a simple line using ID3DXFont::DrawText
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Demonstration Part 1" );
#if 1
            // Pass in DT_NOCLIP so we don't have to calc the bottom/right of the rect
            SetRect( &rc, 150, 200, 0, 0 );
            g_pFont->DrawText( NULL, L"This is a trivial call to ID3DXFont::DrawText", -1, &rc, DT_NOCLIP,
                               D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
#else
            // If you wanted to calc the bottom/rect of the rect make these 2 calls
            SetRect( &rc, 150, 200, 0, 0 );
            g_pFont->DrawText( NULL, L"This is a trivial call to ID3DXFont::DrawText", -1, &rc, DT_CALCRECT, D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ));
            g_pFont->DrawText( NULL, L"This is a trivial call to ID3DXFont::DrawText", -1, &rc, 0, D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ));
#endif
        }

        // Demonstration 2:
        // Allow multiple draw calls to sort by texture changes by ID3DXSprite
        // When drawing 2D text use flags: D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE
        // When drawing 3D text use flags: D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_BACKTOFRONT
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Demonstration Part 2" );
            g_pTextSprite->Begin( D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE );
            SetRect( &rc, 10, nHeight - 15 * 6, 0, 0 );
            g_pFont2->DrawText( g_pTextSprite,
                                L"These multiple calls to ID3DXFont::DrawText() using the same ID3DXSprite.", -1, &rc,
                                DT_NOCLIP, D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
            SetRect( &rc, 10, nHeight - 15 * 5, 0, 0 );
            g_pFont2->DrawText( g_pTextSprite, L"ID3DXFont now caches letters on one or more textures.", -1, &rc,
                                DT_NOCLIP, D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
            SetRect( &rc, 10, nHeight - 15 * 4, 0, 0 );
            g_pFont2->DrawText( g_pTextSprite, L"In order to sort by texture state changes on multiple", -1, &rc,
                                DT_NOCLIP, D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
            SetRect( &rc, 10, nHeight - 15 * 3, 0, 0 );
            g_pFont2->DrawText( g_pTextSprite, L"draw calls to DrawText() pass a ID3DXSprite and use", -1, &rc,
                                DT_NOCLIP, D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
            SetRect( &rc, 10, nHeight - 15 * 2, 0, 0 );
            g_pFont2->DrawText( g_pTextSprite, L"flags D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE", -1, &rc,
                                DT_NOCLIP, D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
            g_pTextSprite->End();
        }

        // Demonstration 3:
        // Word wrapping and unicode text.  
        // Note that not all fonts support dynamic font linking. 
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Demonstration Part 3" );
            rc.left = 10;
            rc.top = 60;
            rc.right = DXUTGetD3D9PresentParameters().BackBufferWidth - 150;
            rc.bottom = DXUTGetD3D9PresentParameters().BackBufferHeight - 10;
            g_pFont2->DrawTextW( NULL, g_strTextBuffer, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS,
                                 D3DXCOLOR( 1.0f, 0.0f, 1.0f, 1.0f ) );
        }

        // Demonstration 4:
        // Draw 3D extruded text using a mesh created with D3DXFont
        if( g_pMesh3DText != NULL )
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Demonstration Part 4" );
            ZeroMemory( &mtrl, sizeof( D3DMATERIAL9 ) );
            mtrl.Diffuse.r = mtrl.Ambient.r = 0.0f;
            mtrl.Diffuse.g = mtrl.Ambient.g = 0.0f;
            mtrl.Diffuse.b = mtrl.Ambient.b = 1.0f;
            mtrl.Diffuse.a = mtrl.Ambient.a = 1.0f;
            pd3dDevice->SetMaterial( &mtrl );
            pd3dDevice->SetTransform( D3DTS_WORLD, &g_matObj2 );
            g_pMesh3DText->DrawSubset( 0 );
        }

        DXUT_EndPerfEvent(); // end of demonstration code

        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
            V( g_HUD.OnRender( fElapsedTime ) );
            V( g_SampleUI.OnRender( fElapsedTime ) );

            // Show frame rate
            SetRect( &rc, 2, 0, 0, 0 );
            g_pStatsFont->DrawText( NULL, DXUTGetFrameStats( DXUTIsVsyncEnabled() ), -1, &rc, DT_NOCLIP,
                                    D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
            SetRect( &rc, 2, 15, 0, 0 );
            g_pStatsFont->DrawText( NULL, DXUTGetDeviceStats(), -1, &rc, DT_NOCLIP,
                                    D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
        }

        // End the scene
        pd3dDevice->EndScene();
    }
}


//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Always allow dialog resource manager calls to handle global messages
    // so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// As a convenience, DXUT inspects the incoming windows messages for
// keystroke messages and decodes the message parameters to pass relevant keyboard
// messages to the application.  The framework does not remove the underlying keystroke 
// messages, which are still passed to the application's MsgProc callback.
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_CHANGEFONT:
        {
            bool bWindowed = DXUTIsWindowed();
            if( !bWindowed )
                DXUTToggleFullScreen();

            HDC hdc;
            LONG lHeight;
            hdc = GetDC( DXUTGetHWND() );
            lHeight = -MulDiv( g_nFontSize, GetDeviceCaps( hdc, LOGPIXELSY ), 72 );
            ReleaseDC( DXUTGetHWND(), hdc );
            hdc = NULL;

            LOGFONT lf;
            wcscpy_s( lf.lfFaceName, 32, g_strFont );
            lf.lfHeight = lHeight;

            CHOOSEFONT cf;
            ZeroMemory( &cf, sizeof( cf ) );
            cf.lStructSize = sizeof( cf );
            cf.hwndOwner = DXUTGetHWND();
            cf.lpLogFont = &lf;
            cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_TTONLY;

            if( ChooseFont( &cf ) )
            {
                LPD3DXFONT pFontNew = NULL;
                LPD3DXMESH pMesh3DTextNew = NULL;
                TCHAR* pstrFontNameNew = lf.lfFaceName;
                int dwFontSizeNew = cf.iPointSize / 10;
                bool bSuccess = false;
                bool bBold = ( ( cf.nFontType & BOLD_FONTTYPE ) == BOLD_FONTTYPE );
                bool bItalic = ( ( cf.nFontType & ITALIC_FONTTYPE ) == ITALIC_FONTTYPE );

                HDC hDC = GetDC( NULL );
                int nLogPixelsY = GetDeviceCaps( hDC, LOGPIXELSY );
                ReleaseDC( NULL, hDC );

                int nHeight = -MulDiv( dwFontSizeNew, nLogPixelsY, 72 );
                if( SUCCEEDED( D3DXCreateFont( DXUTGetD3D9Device(), nHeight, 0, bBold ? FW_BOLD : FW_NORMAL, 1,
                                               bItalic, DEFAULT_CHARSET,
                                               OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                               pstrFontNameNew, &pFontNew ) ) )
                {
                    if( SUCCEEDED( CreateD3DXTextMesh( DXUTGetD3D9Device(), &pMesh3DTextNew, pstrFontNameNew,
                                                       dwFontSizeNew, bBold, bItalic ) ) )
                    {
                        bSuccess = true;
                        SAFE_RELEASE( g_pFont );
                        g_pFont = pFontNew;
                        pFontNew = NULL;

                        SAFE_RELEASE( g_pMesh3DText );
                        g_pMesh3DText = pMesh3DTextNew;
                        pMesh3DTextNew = NULL;

                        wcscpy_s( g_strFont, 32, pstrFontNameNew );
                        g_nFontSize = dwFontSizeNew;
                    }
                }

                SAFE_RELEASE( pMesh3DTextNew );
                SAFE_RELEASE( pFontNew );

                if( !bSuccess )
                {
                    MessageBox( DXUTGetHWND(), TEXT( "Could not create that font." ),
                                L"Text3D", MB_OK );
                }
            }

            if( !bWindowed )
                DXUTToggleFullScreen();

            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// entered a lost state and before IDirect3DDevice9::Reset is called. Resources created
// in the OnResetDevice callback should be released here, which generally includes all 
// D3DPOOL_DEFAULT resources. See the "Lost Devices" section of the documentation for 
// information about lost devices.
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    if( g_pStatsFont )
        g_pStatsFont->OnLostDevice();
    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pFont2 )
        g_pFont2->OnLostDevice();
    if( g_pTextSprite )
        g_pTextSprite->OnLostDevice();
    SAFE_RELEASE( g_pMesh3DText );
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnCreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pFont2 );
    SAFE_RELEASE( g_pStatsFont );
    SAFE_RELEASE( g_pTextSprite );
}



