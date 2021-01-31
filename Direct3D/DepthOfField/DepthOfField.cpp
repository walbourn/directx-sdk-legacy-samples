//--------------------------------------------------------------------------------------
// File: DepthOfField.cpp
//
// Depth of field
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


//--------------------------------------------------------------------------------------
// Vertex format
//--------------------------------------------------------------------------------------
struct VERTEX
{
    D3DXVECTOR4 pos;
    DWORD clr;
    D3DXVECTOR2 tex1;
    D3DXVECTOR2 tex2;
    D3DXVECTOR2 tex3;
    D3DXVECTOR2 tex4;
    D3DXVECTOR2 tex5;
    D3DXVECTOR2 tex6;

    static const DWORD FVF;
};
const DWORD                 VERTEX::FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX6;


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                  g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                g_pTextSprite = NULL;   // Sprite for batching draw text calls
CFirstPersonCamera          g_Camera;               // A model viewing camera
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls

VERTEX g_Vertex[4];

LPDIRECT3DTEXTURE9          g_pFullScreenTexture;
LPD3DXRENDERTOSURFACE       g_pRenderToSurface;
LPDIRECT3DSURFACE9          g_pFullScreenTextureSurf;

LPD3DXMESH                  g_pScene1Mesh;
LPDIRECT3DTEXTURE9          g_pScene1MeshTexture;
LPD3DXMESH                  g_pScene2Mesh;
LPDIRECT3DTEXTURE9          g_pScene2MeshTexture;
int                         g_nCurrentScene;
LPD3DXEFFECT                g_pEffect;

D3DXVECTOR4                 g_vFocalPlane;
double                      g_fChangeTime;
int                         g_nShowMode;
DWORD                       g_dwBackgroundColor;

D3DVIEWPORT9                g_ViewportFB;
D3DVIEWPORT9                g_ViewportOffscreen;

FLOAT                       g_fBlurConst;
DWORD                       g_TechniqueIndex;

D3DXHANDLE                  g_hFocalPlane;
D3DXHANDLE                  g_hWorld;
D3DXHANDLE                  g_hWorldView;
D3DXHANDLE                  g_hWorldViewProjection;
D3DXHANDLE                  g_hMeshTexture;
D3DXHANDLE                  g_hTechWorldWithBlurFactor;
D3DXHANDLE                  g_hTechShowBlurFactor;
D3DXHANDLE                  g_hTechShowUnmodified;
D3DXHANDLE g_hTech[5];

static CHAR*                g_TechniqueNames[] =
{
    "UsePS20ThirteenLookups",
    "UsePS20SevenLookups",
    "UsePS20SixTexcoords"
};
const DWORD                 g_TechniqueCount = sizeof( g_TechniqueNames ) / sizeof( LPCSTR );



//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_CHANGE_SCENE        5
#define IDC_CHANGE_TECHNIQUE    6
#define IDC_SHOW_BLUR           7
#define IDC_CHANGE_BLUR         8
#define IDC_CHANGE_FOCAL        9
#define IDC_CHANGE_BLUR_STATIC  10
#define IDC_SHOW_UNBLURRED      11
#define IDC_SHOW_NORMAL         12
#define IDC_CHANGE_FOCAL_STATIC 13



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
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh );
void RenderText();
void SetupQuad( const D3DSURFACE_DESC* pBackBufferSurfaceDesc );
HRESULT UpdateTechniqueSpecificVariables( const D3DSURFACE_DESC* pBackBufferSurfaceDesc );


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
    DXUTCreateWindow( L"DepthOfField" );
    DXUTCreateDevice( true, 640, 480 );
    DXUTMainLoop();

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_pFont = NULL;

    g_pFullScreenTexture = NULL;
    g_pFullScreenTextureSurf = NULL;
    g_pRenderToSurface = NULL;
    g_pEffect = NULL;

    g_vFocalPlane = D3DXVECTOR4( 0.0f, 0.0f, 1.0f, -2.5f );
    g_fChangeTime = 0.0f;

    g_pScene1Mesh = NULL;
    g_pScene1MeshTexture = NULL;
    g_pScene2Mesh = NULL;
    g_pScene2MeshTexture = NULL;
    g_nCurrentScene = 1;

    g_nShowMode = 0;
    g_bShowHelp = TRUE;
    g_dwBackgroundColor = 0x00003F3F;

    g_fBlurConst = 4.0f;
    g_TechniqueIndex = 0;

    g_hFocalPlane = NULL;
    g_hWorld = NULL;
    g_hWorldView = NULL;
    g_hWorldViewProjection = NULL;
    g_hMeshTexture = NULL;
    g_hTechWorldWithBlurFactor = NULL;
    g_hTechShowBlurFactor = NULL;
    g_hTechShowUnmodified = NULL;
    ZeroMemory( g_hTech, sizeof( g_hTech ) );

    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.AddButton( IDC_CHANGE_SCENE, L"Change Scene", 35, iY += 24, 125, 22, 'P' );
    g_SampleUI.AddButton( IDC_CHANGE_TECHNIQUE, L"Change Technique", 35, iY += 24, 125, 22, 'N' );
    g_SampleUI.AddRadioButton( IDC_SHOW_NORMAL, 1, L"Show Normal", 35, iY += 24, 125, 22, true );
    g_SampleUI.AddRadioButton( IDC_SHOW_BLUR, 1, L"Show Blur Factor", 35, iY += 24, 125, 22 );
    g_SampleUI.AddRadioButton( IDC_SHOW_UNBLURRED, 1, L"Show Unblurred", 35, iY += 24, 125, 22 );

    iY += 24;
    WCHAR sz[100];
    swprintf_s( sz, 100, L"Focal Distance: %0.2f", -g_vFocalPlane.w ); sz[99] = 0;
    g_SampleUI.AddStatic( IDC_CHANGE_FOCAL_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_CHANGE_FOCAL, 50, iY += 24, 100, 22, 0, 100, ( int )( -g_vFocalPlane.w * 10.0f ) );

    iY += 24;
    swprintf_s( sz, 100, L"Blur Factor: %0.2f", g_fBlurConst ); sz[99] = 0;
    g_SampleUI.AddStatic( IDC_CHANGE_BLUR_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_CHANGE_BLUR, 50, iY += 24, 100, 22, 0, 100, ( int )( g_fBlurConst * 10.0f ) );
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
    WCHAR str[MAX_PATH];
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    // Load the meshs
    V_RETURN( LoadMesh( pd3dDevice, TEXT( "tiger\\tiger.x" ), &g_pScene1Mesh ) );
    V_RETURN( LoadMesh( pd3dDevice, TEXT( "misc\\sphere.x" ), &g_pScene2Mesh ) );

    // Load the mesh textures
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"tiger\\tiger.bmp" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pScene1MeshTexture ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"earth\\earth.bmp" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pScene2MeshTexture ) );

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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DepthOfField.fx" ) );
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This function loads the mesh and ensures the mesh has normals; it also optimizes the 
// mesh for the graphics card's vertex cache, which improves performance by organizing 
// the internal triangle list for less cache misses.
//--------------------------------------------------------------------------------------
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh )
{
    ID3DXMesh* pMesh = NULL;
    WCHAR str[MAX_PATH];
    HRESULT hr;

    // Load the mesh with D3DX and get back a ID3DXMesh*.  For this
    // sample we'll ignore the X file's embedded materials since we know 
    // exactly the model we're loading.  See the mesh samples such as
    // "OptimizedMesh" for a more generic mesh loading example.
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFileName ) );

    V_RETURN( D3DXLoadMeshFromX( str, D3DXMESH_MANAGED, pd3dDevice, NULL, NULL, NULL, NULL, &pMesh ) );

    DWORD* rgdwAdjacency = NULL;

    // Make sure there are normals which are required for lighting
    if( !( pMesh->GetFVF() & D3DFVF_NORMAL ) )
    {
        ID3DXMesh* pTempMesh;
        V( pMesh->CloneMeshFVF( pMesh->GetOptions(),
                                pMesh->GetFVF() | D3DFVF_NORMAL,
                                pd3dDevice, &pTempMesh ) );
        V( D3DXComputeNormals( pTempMesh, NULL ) );

        SAFE_RELEASE( pMesh );
        pMesh = pTempMesh;
    }

    // Optimize the mesh for this graphics card's vertex cache 
    // so when rendering the mesh's triangle list the vertices will 
    // cache hit more often so it won't have to re-execute the vertex shader 
    // on those vertices so it will improve perf.     
    rgdwAdjacency = new DWORD[pMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
        return E_OUTOFMEMORY;
    V( pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    V( pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE, rgdwAdjacency, NULL, NULL, NULL ) );
    delete []rgdwAdjacency;

    *ppMesh = pMesh;

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

    // Setup the camera with view & projection matrix
    D3DXVECTOR3 vecEye( 1.3f, 1.1f, -3.3f );
    D3DXVECTOR3 vecAt ( 0.75f, 0.9f, -2.5f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DXToRadian( 60.0f ), fAspectRatio, 0.5f, 100.0f );

    pd3dDevice->GetViewport( &g_ViewportFB );

    // Backbuffer viewport is identical to frontbuffer, except starting at 0, 0
    g_ViewportOffscreen = g_ViewportFB;
    g_ViewportOffscreen.X = 0;
    g_ViewportOffscreen.Y = 0;

    // Create fullscreen renders target texture
    hr = D3DXCreateTexture( pd3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height,
                            1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_pFullScreenTexture );
    if( FAILED( hr ) )
    {
        // Fallback to a non-RT texture
        V_RETURN( D3DXCreateTexture( pd3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height,
                                     1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_pFullScreenTexture ) );
    }

    D3DSURFACE_DESC desc;
    g_pFullScreenTexture->GetSurfaceLevel( 0, &g_pFullScreenTextureSurf );
    g_pFullScreenTextureSurf->GetDesc( &desc );

    // Create a ID3DXRenderToSurface to help render to a texture on cards 
    // that don't support render targets
    V_RETURN( D3DXCreateRenderToSurface( pd3dDevice, desc.Width, desc.Height,
                                         desc.Format, TRUE, D3DFMT_D16, &g_pRenderToSurface ) );

    // clear the surface alpha to 0 so that it does not bleed into a "blurry" background
    //   this is possible because of the avoidance of blurring in a non-blurred texel
    if( SUCCEEDED( g_pRenderToSurface->BeginScene( g_pFullScreenTextureSurf, NULL ) ) )
    {
        pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0x00, 1.0f, 0 );
        g_pRenderToSurface->EndScene( 0 );
    }

    D3DXCOLOR colorWhite( 1.0f, 1.0f, 1.0f, 1.0f );
    D3DXCOLOR colorBlack( 0.0f, 0.0f, 0.0f, 1.0f );
    D3DXCOLOR colorAmbient( 0.25f, 0.25f, 0.25f, 1.0f );

    // Get D3DXHANDLEs to the parameters/techniques that are set every frame so 
    // D3DX doesn't spend time doing string compares.  Doing this likely won't affect
    // the perf of this simple sample but it should be done in complex engine.
    g_hFocalPlane = g_pEffect->GetParameterByName( NULL, "vFocalPlane" );
    g_hWorld = g_pEffect->GetParameterByName( NULL, "mWorld" );
    g_hWorldView = g_pEffect->GetParameterByName( NULL, "mWorldView" );
    g_hWorldViewProjection = g_pEffect->GetParameterByName( NULL, "mWorldViewProjection" );
    g_hMeshTexture = g_pEffect->GetParameterByName( NULL, "MeshTexture" );
    g_hTechWorldWithBlurFactor = g_pEffect->GetTechniqueByName( "WorldWithBlurFactor" );
    g_hTechShowBlurFactor = g_pEffect->GetTechniqueByName( "ShowBlurFactor" );
    g_hTechShowUnmodified = g_pEffect->GetTechniqueByName( "ShowUnmodified" );
    for( int i = 0; i < g_TechniqueCount; i++ )
        g_hTech[i] = g_pEffect->GetTechniqueByName( g_TechniqueNames[i] );

    // Set the vars in the effect that doesn't change each frame
    V_RETURN( g_pEffect->SetVector( "MaterialAmbientColor", ( D3DXVECTOR4* )&colorAmbient ) );
    V_RETURN( g_pEffect->SetVector( "MaterialDiffuseColor", ( D3DXVECTOR4* )&colorWhite ) );
    V_RETURN( g_pEffect->SetTexture( "RenderTargetTexture", g_pFullScreenTexture ) );

    // Check if the current technique is valid for the new device/settings
    // Start from the current technique, increment until we find one we can use.
    DWORD OriginalTechnique = g_TechniqueIndex;
    do
    {
        D3DXHANDLE hTech = g_pEffect->GetTechniqueByName( g_TechniqueNames[g_TechniqueIndex] );
        if( SUCCEEDED( g_pEffect->ValidateTechnique( hTech ) ) )
            break;

        g_TechniqueIndex++;

        if( g_TechniqueIndex == g_TechniqueCount )
            g_TechniqueIndex = 0;
    } while( OriginalTechnique != g_TechniqueIndex );

    V_RETURN( UpdateTechniqueSpecificVariables( pBackBufferSurfaceDesc ) );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 250 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Certain parameters need to be specified for specific techniques
//--------------------------------------------------------------------------------------
HRESULT UpdateTechniqueSpecificVariables( const D3DSURFACE_DESC* pBackBufferSurfaceDesc )
{
    LPCSTR strInputArrayName, strOutputArrayName;
    int nNumKernelEntries;
    HRESULT hr;
    D3DXHANDLE hAnnotation;

    // Create the post-process quad and set the texcoords based on the blur factor
    SetupQuad( pBackBufferSurfaceDesc );

    // Get the handle to the current technique
    D3DXHANDLE hTech = g_pEffect->GetTechniqueByName( g_TechniqueNames[g_TechniqueIndex] );
    if( hTech == NULL )
        return S_FALSE; // This will happen if the technique doesn't have this annotation

    // Get the value of the annotation int named "NumKernelEntries" inside the technique
    hAnnotation = g_pEffect->GetAnnotationByName( hTech, "NumKernelEntries" );
    if( hAnnotation == NULL )
        return S_FALSE; // This will happen if the technique doesn't have this annotation
    V_RETURN( g_pEffect->GetInt( hAnnotation, &nNumKernelEntries ) );

    // Get the value of the annotation string named "KernelInputArray" inside the technique
    hAnnotation = g_pEffect->GetAnnotationByName( hTech, "KernelInputArray" );
    if( hAnnotation == NULL )
        return S_FALSE; // This will happen if the technique doesn't have this annotation
    V_RETURN( g_pEffect->GetString( hAnnotation, &strInputArrayName ) );

    // Get the value of the annotation string named "KernelOutputArray" inside the technique
    hAnnotation = g_pEffect->GetAnnotationByName( hTech, "KernelOutputArray" );
    if( hAnnotation == NULL )
        return S_FALSE; // This will happen if the technique doesn't have this annotation
    if( FAILED( hr = g_pEffect->GetString( hAnnotation, &strOutputArrayName ) ) )
        return hr;

    // Create a array to store the input array
    D3DXVECTOR2* aKernel = new D3DXVECTOR2[nNumKernelEntries];
    if( aKernel == NULL )
        return E_OUTOFMEMORY;

    // Get the input array
    V_RETURN( g_pEffect->GetValue( strInputArrayName, aKernel, sizeof( D3DXVECTOR2 ) * nNumKernelEntries ) );

    // Get the size of the texture
    D3DSURFACE_DESC desc;
    g_pFullScreenTextureSurf->GetDesc( &desc );

    // Calculate the scale factor to convert the input array to screen space
    FLOAT fWidthMod = g_fBlurConst / ( FLOAT )desc.Width;
    FLOAT fHeightMod = g_fBlurConst / ( FLOAT )desc.Height;

    // Scale the effect's kernel from pixel space to tex coord space
    // In pixel space 1 unit = one pixel and in tex coord 1 unit = width/height of texture
    for( int iEntry = 0; iEntry < nNumKernelEntries; iEntry++ )
    {
        aKernel[iEntry].x *= fWidthMod;
        aKernel[iEntry].y *= fHeightMod;
    }

    // Pass the updated array values to the effect file
    V_RETURN( g_pEffect->SetValue( strOutputArrayName, aKernel, sizeof( D3DXVECTOR2 ) * nNumKernelEntries ) );

    SAFE_DELETE_ARRAY( aKernel );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Sets up a quad to render the fullscreen render target to the backbuffer
// so it can run a fullscreen pixel shader pass that blurs based
// on the depth of the objects.  It set the texcoords based on the blur factor
//--------------------------------------------------------------------------------------
void SetupQuad( const D3DSURFACE_DESC* pBackBufferSurfaceDesc )
{
    D3DSURFACE_DESC desc;
    g_pFullScreenTextureSurf->GetDesc( &desc );

    FLOAT fWidth5 = ( FLOAT )pBackBufferSurfaceDesc->Width - 0.5f;
    FLOAT fHeight5 = ( FLOAT )pBackBufferSurfaceDesc->Height - 0.5f;

    FLOAT fHalf = g_fBlurConst;
    FLOAT fOffOne = fHalf * 0.5f;
    FLOAT fOffTwo = fOffOne * sqrtf( 3.0f );

    FLOAT fTexWidth1 = ( FLOAT )pBackBufferSurfaceDesc->Width / ( FLOAT )desc.Width;
    FLOAT fTexHeight1 = ( FLOAT )pBackBufferSurfaceDesc->Height / ( FLOAT )desc.Height;

    FLOAT fWidthMod = 1.0f / ( FLOAT )desc.Width;
    FLOAT fHeightMod = 1.0f / ( FLOAT )desc.Height;

    // Create vertex buffer.  
    // g_Vertex[0].tex1 == full texture coverage
    // g_Vertex[0].tex2 == full texture coverage, but shifted y by -fHalf*fHeightMod
    // g_Vertex[0].tex3 == full texture coverage, but shifted x by -fOffTwo*fWidthMod & y by -fOffOne*fHeightMod
    // g_Vertex[0].tex4 == full texture coverage, but shifted x by +fOffTwo*fWidthMod & y by -fOffOne*fHeightMod
    // g_Vertex[0].tex5 == full texture coverage, but shifted x by -fOffTwo*fWidthMod & y by +fOffOne*fHeightMod
    // g_Vertex[0].tex6 == full texture coverage, but shifted x by +fOffTwo*fWidthMod & y by +fOffOne*fHeightMod
    g_Vertex[0].pos = D3DXVECTOR4( fWidth5, -0.5f, 0.0f, 1.0f );
    g_Vertex[0].clr = D3DXCOLOR( 0.5f, 0.5f, 0.5f, 0.66666f );
    g_Vertex[0].tex1 = D3DXVECTOR2( fTexWidth1, 0.0f );
    g_Vertex[0].tex2 = D3DXVECTOR2( fTexWidth1, 0.0f - fHalf * fHeightMod );
    g_Vertex[0].tex3 = D3DXVECTOR2( fTexWidth1 - fOffTwo * fWidthMod, 0.0f - fOffOne * fHeightMod );
    g_Vertex[0].tex4 = D3DXVECTOR2( fTexWidth1 + fOffTwo * fWidthMod, 0.0f - fOffOne * fHeightMod );
    g_Vertex[0].tex5 = D3DXVECTOR2( fTexWidth1 - fOffTwo * fWidthMod, 0.0f + fOffOne * fHeightMod );
    g_Vertex[0].tex6 = D3DXVECTOR2( fTexWidth1 + fOffTwo * fWidthMod, 0.0f + fOffOne * fHeightMod );

    g_Vertex[1].pos = D3DXVECTOR4( fWidth5, fHeight5, 0.0f, 1.0f );
    g_Vertex[1].clr = D3DXCOLOR( 0.5f, 0.5f, 0.5f, 0.66666f );
    g_Vertex[1].tex1 = D3DXVECTOR2( fTexWidth1, fTexHeight1 );
    g_Vertex[1].tex2 = D3DXVECTOR2( fTexWidth1, fTexHeight1 - fHalf * fHeightMod );
    g_Vertex[1].tex3 = D3DXVECTOR2( fTexWidth1 - fOffTwo * fWidthMod, fTexHeight1 - fOffOne * fHeightMod );
    g_Vertex[1].tex4 = D3DXVECTOR2( fTexWidth1 + fOffTwo * fWidthMod, fTexHeight1 - fOffOne * fHeightMod );
    g_Vertex[1].tex5 = D3DXVECTOR2( fTexWidth1 - fOffTwo * fWidthMod, fTexHeight1 + fOffOne * fHeightMod );
    g_Vertex[1].tex6 = D3DXVECTOR2( fTexWidth1 + fOffTwo * fWidthMod, fTexHeight1 + fOffOne * fHeightMod );

    g_Vertex[2].pos = D3DXVECTOR4( -0.5f, -0.5f, 0.0f, 1.0f );
    g_Vertex[2].clr = D3DXCOLOR( 0.5f, 0.5f, 0.5f, 0.66666f );
    g_Vertex[2].tex1 = D3DXVECTOR2( 0.0f, 0.0f );
    g_Vertex[2].tex2 = D3DXVECTOR2( 0.0f, 0.0f - fHalf * fHeightMod );
    g_Vertex[2].tex3 = D3DXVECTOR2( 0.0f - fOffTwo * fWidthMod, 0.0f - fOffOne * fHeightMod );
    g_Vertex[2].tex4 = D3DXVECTOR2( 0.0f + fOffTwo * fWidthMod, 0.0f - fOffOne * fHeightMod );
    g_Vertex[2].tex5 = D3DXVECTOR2( 0.0f - fOffTwo * fWidthMod, 0.0f + fOffOne * fHeightMod );
    g_Vertex[2].tex6 = D3DXVECTOR2( 0.0f + fOffTwo * fWidthMod, 0.0f + fOffOne * fHeightMod );

    g_Vertex[3].pos = D3DXVECTOR4( -0.5f, fHeight5, 0.0f, 1.0f );
    g_Vertex[3].clr = D3DXCOLOR( 0.5f, 0.5f, 0.5f, 0.66666f );
    g_Vertex[3].tex1 = D3DXVECTOR2( 0.0f, fTexHeight1 );
    g_Vertex[3].tex2 = D3DXVECTOR2( 0.0f, fTexHeight1 - fHalf * fHeightMod );
    g_Vertex[3].tex3 = D3DXVECTOR2( 0.0f - fOffTwo * fWidthMod, fTexHeight1 - fOffOne * fHeightMod );
    g_Vertex[3].tex4 = D3DXVECTOR2( 0.0f + fOffTwo * fWidthMod, fTexHeight1 - fOffOne * fHeightMod );
    g_Vertex[3].tex5 = D3DXVECTOR2( 0.0f + fOffTwo * fWidthMod, fTexHeight1 + fOffOne * fHeightMod );
    g_Vertex[3].tex6 = D3DXVECTOR2( 0.0f - fOffTwo * fWidthMod, fTexHeight1 + fOffOne * fHeightMod );
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
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
    UINT iPass, cPasses;

    // First render the world on the rendertarget g_pFullScreenTexture. 
    if( SUCCEEDED( g_pRenderToSurface->BeginScene( g_pFullScreenTextureSurf, &g_ViewportOffscreen ) ) )
    {
        V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, g_dwBackgroundColor, 1.0f, 0 ) );

        // Get the view & projection matrix from camera
        D3DXMATRIXA16 matWorld;
        D3DXMATRIXA16 matView = *g_Camera.GetViewMatrix();
        D3DXMATRIXA16 matProj = *g_Camera.GetProjMatrix();
        D3DXMATRIXA16 matViewProj = matView * matProj;

        // Update focal plane
        g_pEffect->SetVector( g_hFocalPlane, &g_vFocalPlane );

        // Set world render technique
        V( g_pEffect->SetTechnique( g_hTechWorldWithBlurFactor ) );

        // Set the mesh texture 
        LPD3DXMESH pSceneMesh;
        int nNumObjectsInScene;
        if( g_nCurrentScene == 1 )
        {
            V( g_pEffect->SetTexture( g_hMeshTexture, g_pScene1MeshTexture ) );
            pSceneMesh = g_pScene1Mesh;
            nNumObjectsInScene = 25;
        }
        else
        {
            V( g_pEffect->SetTexture( g_hMeshTexture, g_pScene2MeshTexture ) );
            pSceneMesh = g_pScene2Mesh;
            nNumObjectsInScene = 3;
        }

        static const D3DXVECTOR3 mScene2WorldPos[3] =
        {
            D3DXVECTOR3( -0.5f, -0.5f, -0.5f ),
            D3DXVECTOR3( 1.0f, 1.0f, 2.0f ),
            D3DXVECTOR3( 3.0f, 3.0f, 5.0f )
        };

        for( int iObject = 0; iObject < nNumObjectsInScene; iObject++ )
        {
            // setup the world matrix for the current world
            if( g_nCurrentScene == 1 )
            {
                D3DXMatrixTranslation( &matWorld, -( iObject % 5 ) * 1.0f, 0.0f, ( iObject / 5 ) * 3.0f );
            }
            else
            {
                D3DXMATRIXA16 matRot, matPos;
                D3DXMatrixRotationY( &matRot, ( float )fTime * 0.66666f );
                D3DXMatrixTranslation( &matPos, mScene2WorldPos[iObject].x, mScene2WorldPos[iObject].y,
                                       mScene2WorldPos[iObject].z );
                matWorld = matRot * matPos;
            }

            // Update effect vars
            D3DXMATRIXA16 matWorldViewProj = matWorld * matViewProj;
            D3DXMATRIXA16 matWorldView = matWorld * matView;
            V( g_pEffect->SetMatrix( g_hWorld, &matWorld ) );
            V( g_pEffect->SetMatrix( g_hWorldView, &matWorldView ) );
            V( g_pEffect->SetMatrix( g_hWorldViewProjection, &matWorldViewProj ) );

            // Draw the mesh on the rendertarget
            V( g_pEffect->Begin( &cPasses, 0 ) );
            for( iPass = 0; iPass < cPasses; iPass++ )
            {
                V( g_pEffect->BeginPass( iPass ) );
                V( pSceneMesh->DrawSubset( 0 ) );
                V( g_pEffect->EndPass() );
            }
            V( g_pEffect->End() );
        }

        V( g_pRenderToSurface->EndScene( 0 ) );
    }

    // Clear the backbuffer 
    V( pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L ) );

    // Begin the scene, rendering to the backbuffer
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        pd3dDevice->SetViewport( &g_ViewportFB );

        // Set the post process technique
        switch( g_nShowMode )
        {
            case 0:
                V( g_pEffect->SetTechnique( g_hTech[g_TechniqueIndex] ) ); break;
            case 1:
                V( g_pEffect->SetTechnique( g_hTechShowBlurFactor ) ); break;
            case 2:
                V( g_pEffect->SetTechnique( g_hTechShowUnmodified ) ); break;
        }

        // Render the fullscreen quad on to the backbuffer
        V( g_pEffect->Begin( &cPasses, 0 ) );
        for( iPass = 0; iPass < cPasses; iPass++ )
        {
            V( g_pEffect->BeginPass( iPass ) );
            V( pd3dDevice->SetFVF( VERTEX::FVF ) );
            V( pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, g_Vertex, sizeof( VERTEX ) ) );
            V( g_pEffect->EndPass() );
        }
        V( g_pEffect->End() );

        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_SampleUI.OnRender( fElapsedTime ) );

        // Render the text
        RenderText();

        // End the scene.
        pd3dDevice->EndScene();
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
    switch( g_nShowMode )
    {
        case 0:
            txtHelper.DrawFormattedTextLine( L"Technique: %S", g_TechniqueNames[g_TechniqueIndex] ); break;
        case 1:
            txtHelper.DrawTextLine( L"Technique: ShowBlurFactor" ); break;
        case 2:
            txtHelper.DrawTextLine( L"Technique: ShowUnmodified" ); break;
    }

    txtHelper.DrawFormattedTextLine( L"Focal Plane: (%0.1f,%0.1f,%0.1f,%0.1f)", g_vFocalPlane.x, g_vFocalPlane.y,
                                     g_vFocalPlane.z, g_vFocalPlane.w );

    // Draw help
    if( g_bShowHelp )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
        txtHelper.SetInsertionPos( 2, pd3dsdBackBuffer->Height - 15 * 6 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls (F1 to hide):" );

        txtHelper.SetInsertionPos( 20, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Look: Left drag mouse\n"
                                L"Move: A,W,S,D or Arrow Keys\n"
                                L"Move up/down: Q,E or PgUp,PgDn\n"
                                L"Reset camera: Home\n" );
    }
    else
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Press F1 for help" );
    }
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

        case IDC_CHANGE_TECHNIQUE:
        {
            DWORD OriginalTechnique = g_TechniqueIndex;
            do
            {
                g_TechniqueIndex++;

                if( g_TechniqueIndex == g_TechniqueCount )
                {
                    g_TechniqueIndex = 0;
                }

                D3DXHANDLE hTech = g_pEffect->GetTechniqueByName( g_TechniqueNames[g_TechniqueIndex] );
                if( SUCCEEDED( g_pEffect->ValidateTechnique( hTech ) ) )
                {
                    break;
                }
            } while( OriginalTechnique != g_TechniqueIndex );

            UpdateTechniqueSpecificVariables( DXUTGetD3D9BackBufferSurfaceDesc() );
            break;
        }

        case IDC_CHANGE_SCENE:
        {
            g_nCurrentScene %= 2;
            g_nCurrentScene++;

            switch( g_nCurrentScene )
            {
                case 1:
                {
                    D3DXVECTOR3 vecEye( 0.75f, 0.8f, -2.3f );
                    D3DXVECTOR3 vecAt ( 0.2f, 0.75f, -1.5f );
                    g_Camera.SetViewParams( &vecEye, &vecAt );
                    break;
                }

                case 2:
                {
                    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -3.0f );
                    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
                    g_Camera.SetViewParams( &vecEye, &vecAt );
                    break;
                }
            }
            break;
        }

        case IDC_CHANGE_FOCAL:
        {
            WCHAR sz[100];
            g_vFocalPlane.w = -g_SampleUI.GetSlider( IDC_CHANGE_FOCAL )->GetValue() / 10.0f;
            swprintf_s( sz, 100, L"Focal Distance: %0.2f", -g_vFocalPlane.w ); sz[99] = 0;
            g_SampleUI.GetStatic( IDC_CHANGE_FOCAL_STATIC )->SetText( sz );
            UpdateTechniqueSpecificVariables( DXUTGetD3D9BackBufferSurfaceDesc() );
            break;
        }

        case IDC_SHOW_NORMAL:
            g_nShowMode = 0;
            break;

        case IDC_SHOW_BLUR:
            g_nShowMode = 1;
            break;

        case IDC_SHOW_UNBLURRED:
            g_nShowMode = 2;
            break;

        case IDC_CHANGE_BLUR:
        {
            WCHAR sz[100];
            g_fBlurConst = g_SampleUI.GetSlider( IDC_CHANGE_BLUR )->GetValue() / 10.0f;
            swprintf_s( sz, 100, L"Blur Factor: %0.2f", g_fBlurConst ); sz[99] = 0;
            g_SampleUI.GetStatic( IDC_CHANGE_BLUR_STATIC )->SetText( sz );
            UpdateTechniqueSpecificVariables( DXUTGetD3D9BackBufferSurfaceDesc() );
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pEffect )
        g_pEffect->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );
    SAFE_RELEASE( g_pFullScreenTextureSurf );
    SAFE_RELEASE( g_pFullScreenTexture );
    SAFE_RELEASE( g_pRenderToSurface );
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );

    SAFE_RELEASE( g_pFullScreenTextureSurf );
    SAFE_RELEASE( g_pFullScreenTexture );
    SAFE_RELEASE( g_pRenderToSurface );

    SAFE_RELEASE( g_pScene1Mesh );
    SAFE_RELEASE( g_pScene1MeshTexture );
    SAFE_RELEASE( g_pScene2Mesh );
    SAFE_RELEASE( g_pScene2MeshTexture );
}



