//--------------------------------------------------------------------------------------
// File: CustomUI.cpp
//
// Sample to show usage of DXUT's GUI system
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTguiIME.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmesh.h"
#include "SDKmisc.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                  g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                g_pTextSprite = NULL;   // Sprite for batching draw text calls
ID3DXEffect*                g_pEffect = NULL;       // D3DX effect interface
CDXUTXFileMesh              g_Mesh;                 // Background mesh
D3DXMATRIXA16               g_mView;
CModelViewerCamera          g_Camera;               // A model viewing camera
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_EDITBOX1            5
#define IDC_EDITBOX2            6
#define IDC_ENABLEIME           7
#define IDC_DISABLEIME          8
#define IDC_COMBOBOX            9
#define IDC_STATIC              10
#define IDC_OUTPUT              12
#define IDC_SLIDER              13
#define IDC_CHECKBOX            14
#define IDC_CLEAREDIT           15
#define IDC_RADIO1A             16
#define IDC_RADIO1B             17
#define IDC_RADIO1C             18
#define IDC_RADIO2A             19
#define IDC_RADIO2B             20
#define IDC_RADIO2C             21
#define IDC_LISTBOX             22
#define IDC_LISTBOXM            23


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
void RenderText();


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

    // DXUT will create and use the best device (either D3D9 or D3D10) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
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

    DXUTSetCursorSettings( true, true );
    InitApp();
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );
    DXUTCreateWindow( L"CustomUI" );
    DXUTCreateDevice( true, 640, 480 );
    DXUTMainLoop();

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent );

    g_SampleUI.SetFont( 1, L"Comic Sans MS", 24, FW_NORMAL );
    g_SampleUI.SetFont( 2, L"Courier New", 16, FW_NORMAL );

    // Static
    g_SampleUI.AddStatic( IDC_STATIC, L"This is a static control.", 0, 0, 200, 30 );
    g_SampleUI.AddStatic( IDC_OUTPUT,
                          L"This static control provides feedback for your action.  It will change as you interact with the UI controls.", 20, 50, 620, 300 );
    g_SampleUI.GetStatic( IDC_OUTPUT )->SetTextColor( D3DCOLOR_ARGB( 255, 255, 0, 0 ) ); // Change color to red
    g_SampleUI.GetStatic( IDC_STATIC )->SetTextColor( D3DCOLOR_ARGB( 255, 0, 255, 0 ) ); // Change color to green
    g_SampleUI.GetControl( IDC_OUTPUT )->GetElement( 0 )->dwTextFormat = DT_LEFT | DT_TOP | DT_WORDBREAK;
    g_SampleUI.GetControl( IDC_OUTPUT )->GetElement( 0 )->iFont = 2;
    g_SampleUI.GetControl( IDC_STATIC )->GetElement( 0 )->dwTextFormat = DT_CENTER | DT_VCENTER | DT_WORDBREAK;

    // Buttons
    g_SampleUI.AddButton( IDC_ENABLEIME, L"Enable (I)ME", 30, 390, 80, 35, L'I' );
    g_SampleUI.AddButton( IDC_DISABLEIME, L"Disable I(M)E", 30, 430, 80, 35, L'M' );

    // Edit box
    g_SampleUI.AddEditBox( IDC_EDITBOX1, L"Edit control with default styles. Type text here and press Enter", 20, 440,
                           600, 32 );

    // IME-enabled edit box
    CDXUTIMEEditBox* pIMEEdit;
    CDXUTIMEEditBox::InitDefaultElements( &g_SampleUI );
    if( SUCCEEDED( CDXUTIMEEditBox::CreateIMEEditBox( &g_SampleUI, IDC_EDITBOX2,
                                                      L"IME-capable edit control with custom styles. Type and press Enter", 20, 390, 600, 45, false, &pIMEEdit ) ) )
    {
        g_SampleUI.AddControl( pIMEEdit );
        pIMEEdit->GetElement( 0 )->iFont = 1;
        pIMEEdit->GetElement( 1 )->iFont = 1;
        pIMEEdit->GetElement( 9 )->iFont = 1;
        pIMEEdit->GetElement( 0 )->TextureColor.Init( D3DCOLOR_ARGB( 128, 255, 255, 255 ) );  // Transparent center
        pIMEEdit->SetBorderWidth( 7 );
        pIMEEdit->SetTextColor( D3DCOLOR_ARGB( 255, 64, 64, 64 ) );
        pIMEEdit->SetCaretColor( D3DCOLOR_ARGB( 255, 64, 64, 64 ) );
        pIMEEdit->SetSelectedTextColor( D3DCOLOR_ARGB( 255, 255, 255, 255 ) );
        pIMEEdit->SetSelectedBackColor( D3DCOLOR_ARGB( 255, 40, 72, 72 ) );
    }

    // Slider
    g_SampleUI.AddSlider( IDC_SLIDER, 200, 450, 200, 24, 0, 100, 50, false );

    // Checkbox
    g_SampleUI.AddCheckBox( IDC_CHECKBOX, L"This is a checkbox with hotkey. Press 'C' to toggle the check state.",
                            170, 450, 350, 24, false, L'C', false );
    g_SampleUI.AddCheckBox( IDC_CLEAREDIT,
                            L"This checkbox controls whether edit control text is cleared when Enter is pressed. (T)",
                            170, 460, 450, 24, false, L'T', false );

    // Combobox
    CDXUTComboBox* pCombo;
    g_SampleUI.AddComboBox( IDC_COMBOBOX, 0, 0, 200, 24, L'O', false, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 100 );
        pCombo->AddItem( L"Combobox item (O)", ( LPVOID )0x11111111 );
        pCombo->AddItem( L"Placeholder (O)", ( LPVOID )0x12121212 );
        pCombo->AddItem( L"One more (O)", ( LPVOID )0x13131313 );
        pCombo->AddItem( L"I can't get enough (O)", ( LPVOID )0x14141414 );
        pCombo->AddItem( L"Ok, last one, I promise (O)", ( LPVOID )0x15151515 );
    }

    // Radio buttons
    g_SampleUI.AddRadioButton( IDC_RADIO1A, 1, L"Radio group 1 Amy (1)", 0, 50, 220, 24, false, L'1' );
    g_SampleUI.AddRadioButton( IDC_RADIO1B, 1, L"Radio group 1 Brian (2)", 0, 50, 220, 24, false, L'2' );
    g_SampleUI.AddRadioButton( IDC_RADIO1C, 1, L"Radio group 1 Clark (3)", 0, 50, 220, 24, false, L'3' );

    g_SampleUI.AddRadioButton( IDC_RADIO2A, 2, L"Single (4)", 0, 50, 90, 24, false, L'4' );
    g_SampleUI.AddRadioButton( IDC_RADIO2B, 2, L"Double (5)", 0, 50, 90, 24, false, L'5' );
    g_SampleUI.AddRadioButton( IDC_RADIO2C, 2, L"Triple (6)", 0, 50, 90, 24, false, L'6' );

    // List box
    g_SampleUI.AddListBox( IDC_LISTBOX, 30, 400, 200, 150, 0 );
    for( int i = 0; i < 15; ++i )
    {
        WCHAR wszText[50];
        swprintf_s( wszText, 50, L"Single-selection listbox item %d", i );
        g_SampleUI.GetListBox( IDC_LISTBOX )->AddItem( wszText, ( LPVOID )( size_t )i );
    }
    g_SampleUI.AddListBox( IDC_LISTBOXM, 30, 400, 200, 150, CDXUTListBox::MULTISELECTION );
    for( int i = 0; i < 30; ++i )
    {
        WCHAR wszText[50];
        swprintf_s( wszText, 50, L"Multi-selection listbox item %d", i );
        g_SampleUI.GetListBox( IDC_LISTBOXM )->AddItem( wszText, ( LPVOID )( size_t )i );
    }
}


//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
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

    // Must support pixel shader 2.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
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
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{
    HRESULT hr;

    CDXUTIMEEditBox::Initialize( DXUTGetHWND() );

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );
    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;
#if defined( DEBUG ) || defined( _DEBUG )
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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"CustomUI.fx" ) );
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );

    g_Mesh.Create( pd3dDevice, L"misc\\cell.x" );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 1.5f, -7.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.2f, 0.0f );
    D3DXVECTOR3 vecUp ( 0.0f, 1.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    D3DXMatrixLookAtLH( &g_mView, &vecEye, &vecAt, &vecUp );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice,
                                const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont )
        V_RETURN( g_pFont->OnResetDevice() );
    if( g_pEffect )
        V_RETURN( g_pEffect->OnResetDevice() );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( 0, 0 );
    g_SampleUI.SetSize( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_SampleUI.GetControl( IDC_STATIC )->SetSize( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height * 6 /
                                                  10 );
    g_SampleUI.GetControl( IDC_OUTPUT )->SetSize( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height /
                                                  4 );
    g_SampleUI.GetControl( IDC_EDITBOX1 )->SetLocation( 20, pBackBufferSurfaceDesc->Height - 230 );
    g_SampleUI.GetControl( IDC_EDITBOX1 )->SetSize( pBackBufferSurfaceDesc->Width - 40, 32 );
    if( g_SampleUI.GetControl( IDC_EDITBOX2 ) )
    {
        g_SampleUI.GetControl( IDC_EDITBOX2 )->SetLocation( 20, pBackBufferSurfaceDesc->Height - 280 );
        g_SampleUI.GetControl( IDC_EDITBOX2 )->SetSize( pBackBufferSurfaceDesc->Width - 40, 45 );
    }
    g_SampleUI.GetControl( IDC_ENABLEIME )->SetLocation( 130, pBackBufferSurfaceDesc->Height - 80 );
    g_SampleUI.GetControl( IDC_DISABLEIME )->SetLocation( 220, pBackBufferSurfaceDesc->Height - 80 );
    g_SampleUI.GetControl( IDC_SLIDER )->SetLocation( 10, pBackBufferSurfaceDesc->Height - 140 );
    g_SampleUI.GetControl( IDC_CHECKBOX )->SetLocation( 120, pBackBufferSurfaceDesc->Height - 50 );
    g_SampleUI.GetControl( IDC_CLEAREDIT )->SetLocation( 120, pBackBufferSurfaceDesc->Height - 25 );
    g_SampleUI.GetControl( IDC_COMBOBOX )->SetLocation( 20, pBackBufferSurfaceDesc->Height - 180 );
    g_SampleUI.GetControl( IDC_RADIO1A )->SetLocation( pBackBufferSurfaceDesc->Width - 160, 100 );
    g_SampleUI.GetControl( IDC_RADIO1B )->SetLocation( pBackBufferSurfaceDesc->Width - 160, 124 );
    g_SampleUI.GetControl( IDC_RADIO1C )->SetLocation( pBackBufferSurfaceDesc->Width - 160, 148 );
    g_SampleUI.GetControl( IDC_RADIO2A )->SetLocation( 20, pBackBufferSurfaceDesc->Height - 100 );
    g_SampleUI.GetControl( IDC_RADIO2B )->SetLocation( 20, pBackBufferSurfaceDesc->Height - 76 );
    g_SampleUI.GetControl( IDC_RADIO2C )->SetLocation( 20, pBackBufferSurfaceDesc->Height - 52 );
    g_SampleUI.GetControl( IDC_LISTBOX )->SetLocation( pBackBufferSurfaceDesc->Width - 400,
                                                       pBackBufferSurfaceDesc->Height - 180 );
    g_SampleUI.GetControl( IDC_LISTBOX )->SetSize( 190, 96 );
    g_SampleUI.GetControl( IDC_LISTBOXM )->SetLocation( pBackBufferSurfaceDesc->Width - 200,
                                                        pBackBufferSurfaceDesc->Height - 180 );
    g_SampleUI.GetControl( IDC_LISTBOXM )->SetSize( 190, 124 );
    
    g_Mesh.RestoreDeviceObjects( pd3dDevice );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    D3DXMATRIXA16 m;
    D3DXMatrixRotationY( &m, D3DX_PI * fElapsedTime / 40.0f );
    D3DXMatrixMultiply( &g_mView, &m, &g_mView );
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    HRESULT hr;
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldViewProjection;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Get the projection & view matrix from the camera class
        mWorld = *g_Camera.GetWorldMatrix();
        mProj = *g_Camera.GetProjMatrix();
        mView = g_mView;

        mWorldViewProjection = mWorld * mView * mProj;

        // Update the effect's variables.  Instead of using strings, it would 
        // be more efficient to cache a handle to the parameter by calling 
        // ID3DXEffect::GetParameterByName
        V( g_pEffect->SetMatrix( "g_mWorldViewProjection", &mWorldViewProjection ) );
        V( g_pEffect->SetMatrix( "g_mWorld", &mWorld ) );
        V( g_pEffect->SetFloat( "g_fTime", ( float )fTime ) );

        g_pEffect->SetTechnique( "RenderScene" );
        UINT cPasses;
        g_pEffect->Begin( &cPasses, 0 );
        ID3DXMesh* pMesh = g_Mesh.GetMesh();
        for( UINT p = 0; p < cPasses; ++p )
        {
            g_pEffect->BeginPass( p );
            for( UINT m = 0; m < g_Mesh.m_dwNumMaterials; ++m )
            {
                g_pEffect->SetTexture( "g_txScene", g_Mesh.m_pTextures[m] );
                g_pEffect->CommitChanges();
                pMesh->DrawSubset( m );
            }
            g_pEffect->EndPass();
        }
        g_pEffect->End();

        RenderText();
        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_SampleUI.OnRender( fElapsedTime ) );

        V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    txtHelper.DrawTextLine( L"Press ESC to quit" );
    txtHelper.End();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Always allow dialog resource manager calls to handle global messages
    // so GUI state is updated correctly
    *pbNoFurtherProcessing = CDXUTIMEEditBox::StaticMsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
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
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR wszOutput[1024];

    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_ENABLEIME:
            CDXUTIMEEditBox::SetImeEnableFlag( true );
            g_SampleUI.GetStatic( IDC_OUTPUT )->SetText(
                L"You clicked the 'Enable IME' button.\nIME text input is enabled for IME-capable edit controls." );
            break;
        case IDC_DISABLEIME:
            CDXUTIMEEditBox::SetImeEnableFlag( false );
            g_SampleUI.GetStatic( IDC_OUTPUT )->SetText(
                L"You clicked the 'Disable IME' button.\nIME text input is disabled for IME-capable edit controls." );
            break;
        case IDC_EDITBOX1:
        case IDC_EDITBOX2:
            switch( nEvent )
            {
                case EVENT_EDITBOX_STRING:
                {
                    swprintf_s( wszOutput, 1024,
                                     L"You have pressed Enter in edit control (ID %u).\nThe content of the edit control is:\n\"%s\"",
                                     nControlID, ( ( CDXUTEditBox* )pControl )->GetText() );
                    g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );

                    // Clear the text if needed
                    if( g_SampleUI.GetCheckBox( IDC_CLEAREDIT )->GetChecked() )
                        ( ( CDXUTEditBox* )pControl )->SetText( L"" );
                    break;
                }

                case EVENT_EDITBOX_CHANGE:
                {
                    swprintf_s( wszOutput, 1024,
                                     L"You have changed the content of an edit control (ID %u).\nIt is now:\n\"%s\"",
                                     nControlID, ( ( CDXUTEditBox* )pControl )->GetText() );
                    g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );

                    break;
                }
            }
            break;
        case IDC_SLIDER:
            swprintf_s( wszOutput, 1024, L"You adjusted the slider control.\nThe new value reported is %d",
                             ( ( CDXUTSlider* )pControl )->GetValue() );
            g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );
            break;
        case IDC_CHECKBOX:
            swprintf_s( wszOutput, 1024, L"You %s the upper check box.",
                             ( ( CDXUTCheckBox* )pControl )->GetChecked() ? L"checked" : L"cleared" );
            g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );
            break;
        case IDC_CLEAREDIT:
            swprintf_s( wszOutput, 1024, L"You %s the lower check box.\nNow edit controls will %s",
                             ( ( CDXUTCheckBox* )pControl )->GetChecked() ? L"checked" : L"cleared",
                             ( ( CDXUTCheckBox* )pControl )->GetChecked() ?
                             L"be cleared when you press Enter to send the text" :
                             L"retain the text context when you press Enter to send the text" );
            g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );
            break;
        case IDC_COMBOBOX:
        {
            DXUTComboBoxItem* pItem = ( ( CDXUTComboBox* )pControl )->GetSelectedItem();
            if( pItem )
            {
                swprintf_s( wszOutput, 1024,
                                 L"You selected a new item in the combobox.\nThe new item is \"%s\" and the associated data value is 0x%p",
                                 pItem->strText, pItem->pData );
                g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );
            }
            break;
        }
        case IDC_RADIO1A:
        case IDC_RADIO1B:
        case IDC_RADIO1C:
            swprintf_s( wszOutput, 1024,
                             L"You selected a new radio button in the UPPER radio group.\nThe new button is \"%s\"",
                             ( ( CDXUTRadioButton* )pControl )->GetText() );
            g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );
            break;
        case IDC_RADIO2A:
        case IDC_RADIO2B:
        case IDC_RADIO2C:
            swprintf_s( wszOutput, 1024,
                             L"You selected a new radio button in the LOWER radio group.\nThe new button is \"%s\"",
                             ( ( CDXUTRadioButton* )pControl )->GetText() );
            g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );
            break;

        case IDC_LISTBOX:
            switch( nEvent )
            {
                case EVENT_LISTBOX_ITEM_DBLCLK:
                {
                    DXUTListBoxItem* pItem = ( ( CDXUTListBox* )pControl )->GetItem(
                        ( ( CDXUTListBox* )pControl )->GetSelectedIndex( -1 ) );

                    swprintf_s( wszOutput, 1024,
                                     L"You double clicked an item in the left list box.  The item is\n\"%s\"",
                                     pItem ? pItem->strText : L"" );
                    g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );
                    break;
                }

                case EVENT_LISTBOX_SELECTION:
                {
                    swprintf_s( wszOutput, 1024,
                                     L"You changed the selection in the left list box.  The selected item is %d",
                                     ( ( CDXUTListBox* )pControl )->GetSelectedIndex() );
                    g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );
                    break;
                }
            }
            break;

        case IDC_LISTBOXM:
            switch( nEvent )
            {
                case EVENT_LISTBOX_ITEM_DBLCLK:
                {
                    DXUTListBoxItem* pItem = ( ( CDXUTListBox* )pControl )->GetItem(
                        ( ( CDXUTListBox* )pControl )->GetSelectedIndex( -1 ) );

                    swprintf_s( wszOutput, 1024,
                                     L"You double clicked an item in the right list box.  The item is\n\"%s\"",
                                     pItem ? pItem->strText : L"" );
                    g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );
                    break;
                }

                case EVENT_LISTBOX_SELECTION:
                {
                    swprintf_s( wszOutput, 1024,
                                     L"You changed the selection in the right list box.  The selected item(s) are\n" );
                    int nSelected = -1;
                    while( ( nSelected = ( ( CDXUTListBox* )pControl )->GetSelectedIndex( nSelected ) ) != -1 )
                    {
                        swprintf_s( wszOutput + lstrlenW( wszOutput ), 1024 - lstrlenW( wszOutput ), L"%d,",
                                         nSelected );
                    }
                    // Remove the trailing comma if one exists.
                    if( wszOutput[lstrlenW( wszOutput ) - 1] == L',' )
                        wszOutput[lstrlenW( wszOutput ) - 1] = L'\0';
                    g_SampleUI.GetStatic( IDC_OUTPUT )->SetText( wszOutput );
                    break;
                }
            }
            break;
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    g_Mesh.InvalidateDeviceObjects();

    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pEffect )
        g_pEffect->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    CDXUTIMEEditBox::Uninitialize( );

    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    g_Mesh.Destroy();

    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );
}



