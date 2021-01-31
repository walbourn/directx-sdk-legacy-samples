//--------------------------------------------------------------------------------------
// File: AntiAlias.cpp
//
// Multisampling attempts to reduce aliasing by mimicking a higher resolution display; 
// multiple sample points are used to determine each pixel's color. This sample shows 
// how the various multisampling techniques supported by your video card affect the 
// scene's rendering. Although multisampling effectively combats aliasing, under 
// particular situations it can introduce visual artifacts of it's own. As illustrated 
// by the sample, centroid sampling seeks to eliminate one common type of multisampling 
// artifact. Support for centroid sampling is supported under Pixel Shader 2.0 in the 
// latest version of the DirectX runtime.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "DXUTres.h"
#include "SDKmisc.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


//--------------------------------------------------------------------------------------
// Custom vertex
//--------------------------------------------------------------------------------------
struct Vertex
{
    D3DXVECTOR3 Position;
    D3DCOLOR Diffuse;
    D3DXVECTOR2 TexCoord;
};

D3DVERTEXELEMENT9 VertexElements[] =
{
    { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};



//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
const int                       GUI_WIDTH = 305;

 char g_strActiveTechnique[ MAX_PATH+1 ] = {0};
D3DXMATRIXA16                   g_mRotation;
IDirect3DVertexDeclaration9*    g_pVertexDecl = NULL;
IDirect3DVertexBuffer9*         g_pVBTriangles = NULL;
ID3DXMesh*                      g_pMeshSphereHigh = NULL;
ID3DXMesh*                      g_pMeshSphereLow = NULL;
ID3DXMesh*                      g_pMeshQuadHigh = NULL;
ID3DXMesh*                      g_pMeshQuadLow = NULL;
ID3DXFont*                      g_pFont = NULL;            // Font for drawing text
ID3DXSprite*                    g_pTextSprite = NULL;      // Sprite for batching draw text calls
ID3DXEffect*                    g_pEffect = NULL;          // D3DX effect interface
CDXUTDialogResourceManager      g_DialogResourceManager;   // Manager for shared resources of dialogs
CD3DSettingsDlg                 g_SettingsDlg;             // Device settings dialog
CDXUTDialog                     g_HUD;                     // Dialog for standard controls
CDXUTDialog                     g_DialogLabels;            // Labels within the current scene
bool                            g_bCentroid = false;
IDirect3DTexture9*              g_pTriangleTexture = NULL;
IDirect3DTexture9*              g_pCheckerTexture = NULL;
DXUTDeviceSettings              g_D3DDeviceSettings;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_MULTISAMPLE_TYPE    4
#define IDC_MULTISAMPLE_QUALITY 5
#define IDC_CENTROID            6
#define IDC_FILTERGROUP         7
#define IDC_FILTER_POINT        8
#define IDC_FILTER_LINEAR       9
#define IDC_FILTER_ANISOTROPIC  10
#define IDC_SCENE               11
#define IDC_ROTATIONGROUP       12
#define IDC_ROTATION_RPM        13
#define IDC_RPM                 14
#define IDC_RPM_SUFFIX          15
#define IDC_ROTATION_DEGREES    16
#define IDC_DEGREES             17
#define IDC_DEGREES_SUFFIX      18
#define IDC_LABEL0              19
#define IDC_LABEL1              20


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

void RenderSceneTriangles( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime );
void RenderSceneQuads( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime );
void RenderSceneSpheres( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime );

void InitApp();
HRESULT FillVertexBuffer();
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh );
void RenderText();

HRESULT InitializeUI();
HRESULT OnMultisampleTypeChanged();
HRESULT OnMultisampleQualityChanged();
void AddMultisampleType( D3DMULTISAMPLE_TYPE type );
D3DMULTISAMPLE_TYPE GetSelectedMultisampleType();
void AddMultisampleQuality( DWORD dwQuality );
DWORD GetSelectedMultisampleQuality();
WCHAR*                          DXUTMultisampleTypeToString( D3DMULTISAMPLE_TYPE MultiSampleType );
CD3D9EnumDeviceSettingsCombo*   GetCurrentDeviceSettingsCombo();



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

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );

    InitApp();
    DXUTInit( true, true ); // Parse the command line, handle the default hotkeys, and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );
    DXUTCreateWindow( L"AntiAlias" );
    DXUTCreateDevice( true, 640, 480 );
    DXUTMainLoop();

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_DialogLabels.Init( &g_DialogResourceManager );

    // Title font for comboboxes
    g_HUD.SetFont( 1, L"Arial", 14, FW_BOLD );
    CDXUTElement* pElement = g_HUD.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;

        // Scene label font
        g_DialogLabels.SetFont( 1, L"Arial", 16, FW_BOLD );
    }

    pElement = g_DialogLabels.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->FontColor.Init( D3DCOLOR_RGBA( 200, 200, 200, 255 ) );
        pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;
    }

    // Initialize dialogs
    int iX = 25, iY = 10;

    g_HUD.SetCallback( OnGUIEvent );
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", iX + 135, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", iX + 135, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", iX + 135, iY += 24, 125, 22, VK_F2 );

    iY += 10;
    g_HUD.AddStatic( -1, L"Scene", iX, iY += 24, 260, 22 );
    g_HUD.AddComboBox( IDC_SCENE, iX, iY += 24, 260, 22 );
    g_HUD.GetComboBox( IDC_SCENE )->AddItem( L"Triangles", NULL );
    g_HUD.GetComboBox( IDC_SCENE )->AddItem( L"Quads", NULL );
    g_HUD.GetComboBox( IDC_SCENE )->AddItem( L"Spheres", NULL );

    iY += 10;
    g_HUD.AddStatic( -1, L"Multisample Type", iX, iY += 24, 260, 22 );
    g_HUD.AddComboBox( IDC_MULTISAMPLE_TYPE, iX, iY += 24, 260, 22 );
    g_HUD.AddStatic( -1, L"Multisample Quality", iX, iY += 24, 260, 22 );
    g_HUD.AddComboBox( IDC_MULTISAMPLE_QUALITY, iX, iY += 24, 260, 22 );

    iY += 10;
    g_HUD.AddStatic( -1, L"Texture Filtering", iX, iY += 24, 260, 22 );

    iY += 10;
    g_HUD.AddCheckBox( IDC_CENTROID, L"Centroid sampling", iX + 150, iY + 20, 260, 18, false );

    g_HUD.AddRadioButton( IDC_FILTER_POINT, IDC_FILTERGROUP, L"Point", iX + 10, iY += 20, 130, 18 );
    g_HUD.AddRadioButton( IDC_FILTER_LINEAR, IDC_FILTERGROUP, L"Linear", iX + 10, iY += 20, 130, 18, true );
    g_HUD.AddRadioButton( IDC_FILTER_ANISOTROPIC, IDC_FILTERGROUP, L"Anisotropic", iX + 10, iY += 20, 130, 18 );

    iY += 10;
    g_HUD.AddStatic( -1, L"Mesh Rotation", iX, iY += 24, 260, 22 );

    g_HUD.AddEditBox( IDC_RPM, L"4.0", iX + 86, iY + 32, 50, 30 );
    g_HUD.AddRadioButton( IDC_ROTATION_RPM, IDC_ROTATIONGROUP, L"Rotating at", iX + 10, iY + 38, 76, 18, true );
    g_HUD.AddStatic( IDC_RPM_SUFFIX, L"rpm", iX + 141, iY += 38, 100, 18 );
    pElement = g_HUD.GetStatic( IDC_RPM_SUFFIX )->GetElement( 0 );
    if( pElement )
        pElement->iFont = 0;
    g_HUD.AddEditBox( IDC_DEGREES, L"90.0", iX + 74, iY + 36, 50, 30 );
    g_HUD.AddRadioButton( IDC_ROTATION_DEGREES, IDC_ROTATIONGROUP, L"Fixed at", iX + 10, iY + 42, 64, 18 );
    g_HUD.AddStatic( IDC_DEGREES_SUFFIX, L"degrees", iX + 129, iY += 42, 100, 18 );
    pElement = g_HUD.GetStatic( IDC_DEGREES_SUFFIX )->GetElement( 0 );
    if( pElement )
        pElement->iFont = 0;

    // Add labels
    g_DialogLabels.AddStatic( IDC_LABEL0, L"", 0, 0, 200, 25 );
    g_DialogLabels.AddStatic( IDC_LABEL1, L"", 0, 0, 200, 25 );
}


//--------------------------------------------------------------------------------------
// Called during device initialization, this code checks the device for some 
// minimum set of capabilities, and rejects those that don't pass by returning false.
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                  D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
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
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( DXUT_D3D9_DEVICE == pDeviceSettings->ver );

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
    WCHAR strPath[MAX_PATH];

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );


    // Create the vertex declaration
    V_RETURN( pd3dDevice->CreateVertexDeclaration( VertexElements, &g_pVertexDecl ) );

    // Create the vertex buffer for the triangles / quads
    V_RETURN( pd3dDevice->CreateVertexBuffer( 3 * sizeof( Vertex ), D3DUSAGE_WRITEONLY,
                                              0, D3DPOOL_MANAGED, &g_pVBTriangles, NULL ) );

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    // Create the spheres
    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"spherehigh.x" ) );
    V_RETURN( LoadMesh( pd3dDevice, strPath, &g_pMeshSphereHigh ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"spherelow.x" ) );
    V_RETURN( LoadMesh( pd3dDevice, strPath, &g_pMeshSphereLow ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"quadhigh.x" ) );
    V_RETURN( LoadMesh( pd3dDevice, strPath, &g_pMeshQuadHigh ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"quadlow.x" ) );
    V_RETURN( LoadMesh( pd3dDevice, strPath, &g_pMeshQuadLow ) );


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
    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"AntiAlias.fx" ) );
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, strPath, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );


    // Load the textures
    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"checker.tga" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, strPath, &g_pCheckerTexture ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"triangle.tga" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, strPath, &g_pTriangleTexture ) );

    return S_OK;
}


//-----------------------------------------------------------------------------
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, LPD3DXMESH* ppMesh )
{
    LPD3DXMESH pMesh = NULL;
    WCHAR str[MAX_PATH];
    HRESULT hr;

    if( ppMesh == NULL )
        return E_INVALIDARG;

    DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFileName );
    hr = D3DXLoadMeshFromX( str, D3DXMESH_MANAGED,
                            pd3dDevice, NULL, NULL, NULL, NULL, &pMesh );
    if( FAILED( hr ) || ( pMesh == NULL ) )
        return hr;

    DWORD* rgdwAdjacency = NULL;

    // Make sure there are normals which are required for lighting
    if( !( pMesh->GetFVF() & D3DFVF_NORMAL ) )
    {
        LPD3DXMESH pTempMesh;
        hr = pMesh->CloneMeshFVF( pMesh->GetOptions(),
                                  pMesh->GetFVF() | D3DFVF_NORMAL,
                                  pd3dDevice, &pTempMesh );
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



//--------------------------------------------------------------------------------------
// Set the vertices for the triangles scene
//--------------------------------------------------------------------------------------
HRESULT FillVertexBuffer()
{
    HRESULT hr;

    Vertex* pVertex = NULL;
    V_RETURN( g_pVBTriangles->Lock( 0, 0, ( void** )&pVertex, 0 ) );

    float fTexel = 1.0f / 128;
    int nBorder = 5;

    pVertex->Position = D3DXVECTOR3( 1, 1, 0 );
    pVertex->Diffuse = D3DCOLOR_ARGB( 255, 0, 0, 0 );
    pVertex->TexCoord = D3DXVECTOR2( ( 128 - nBorder ) * fTexel, ( 128 - nBorder ) * fTexel );
    pVertex++;

    pVertex->Position = D3DXVECTOR3( 0, 1, 0 );
    pVertex->Diffuse = D3DCOLOR_ARGB( 255, 0, 0, 0 );
    pVertex->TexCoord = D3DXVECTOR2( nBorder * fTexel, ( 128 - nBorder ) * fTexel );
    pVertex++;

    pVertex->Position = D3DXVECTOR3( 0, 0, 0 );
    pVertex->Diffuse = D3DCOLOR_ARGB( 255, 0, 0, 0 );
    pVertex->TexCoord = D3DXVECTOR2( nBorder * fTexel, nBorder * fTexel );
    pVertex++;

    g_pVBTriangles->Unlock();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Moves the world-space XY plane into screen-space for pixel-perfect perspective
//--------------------------------------------------------------------------------------
HRESULT CalculateViewAndProjection( D3DXMATRIX* pViewOut, D3DXMATRIX* pProjectionOut, float fovy, float zNearOffset,
                                    float zFarOffset )
{
    if( pViewOut == NULL ||
        pProjectionOut == NULL )
        return E_INVALIDARG;

    // Get back buffer description and determine aspect ratio
    const D3DSURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetD3D9BackBufferSurfaceDesc();
    float Width = ( float )pBackBufferSurfaceDesc->Width;
    float Height = ( float )pBackBufferSurfaceDesc->Height;
    float fAspectRatio = Width / Height;

    // Determine the correct Z depth to completely fill the frustum
    float yScale = 1 / tanf( fovy / 2 );
    float z = yScale * Height / 2;

    // Calculate perspective projection
    D3DXMatrixPerspectiveFovLH( pProjectionOut, fovy, fAspectRatio, z + zNearOffset, z + zFarOffset );

    // Initialize the view matrix as a rotation and translation from "screen-coordinates"
    // in world space (the XY plane from the perspective of Z+) to a plane that's centered
    // along Z+
    D3DXMatrixIdentity( pViewOut );
    pViewOut->_22 = -1;
    pViewOut->_33 = -1;
    pViewOut->_41 = -( Width / 2 );
    pViewOut->_42 = ( Height / 2 );
    pViewOut->_43 = z;


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

    V_RETURN( FillVertexBuffer() );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    const D3DCOLOR backColor = D3DCOLOR_ARGB( 255, 150, 150, 150 );
    const D3DCOLOR bottomColor = D3DCOLOR_ARGB( 255, 100, 100, 100 );

    g_HUD.SetBackgroundColors( bottomColor, bottomColor, backColor, backColor );
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - GUI_WIDTH, 0 );
    g_HUD.SetSize( GUI_WIDTH, pBackBufferSurfaceDesc->Height );

    InitializeUI();

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT InitializeUI()
{
    HRESULT hr;

    g_D3DDeviceSettings = DXUTGetDeviceSettings();

    CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo = GetCurrentDeviceSettingsCombo();
    if( pDeviceSettingsCombo == NULL )
        return E_FAIL;

    CDXUTComboBox* pMultisampleTypeCombo = g_HUD.GetComboBox( IDC_MULTISAMPLE_TYPE );
    pMultisampleTypeCombo->RemoveAllItems();

    for( int ims = 0; ims < pDeviceSettingsCombo->multiSampleTypeList.GetSize(); ims++ )
    {
        D3DMULTISAMPLE_TYPE msType = pDeviceSettingsCombo->multiSampleTypeList.GetAt( ims );

        bool bConflictFound = false;
        for( int iConf = 0; iConf < pDeviceSettingsCombo->DSMSConflictList.GetSize(); iConf++ )
        {
            CD3D9EnumDSMSConflict DSMSConf = pDeviceSettingsCombo->DSMSConflictList.GetAt( iConf );
            if( DSMSConf.DSFormat == g_D3DDeviceSettings.d3d9.pp.AutoDepthStencilFormat &&
                DSMSConf.MSType == msType )
            {
                bConflictFound = true;
                break;
            }
        }

        if( !bConflictFound )
            AddMultisampleType( msType );
    }

    CDXUTComboBox* pMultisampleQualityCombo = g_HUD.GetComboBox( IDC_MULTISAMPLE_TYPE );
    pMultisampleQualityCombo->SetSelectedByData( ULongToPtr( g_D3DDeviceSettings.d3d9.pp.MultiSampleType ) );

    hr = OnMultisampleTypeChanged();
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//-------------------------------------------------------------------------------------
CD3D9EnumDeviceSettingsCombo* GetCurrentDeviceSettingsCombo()
{
    CD3D9Enumeration* pD3DEnum = DXUTGetD3D9Enumeration();
    return pD3DEnum->GetDeviceSettingsCombo( g_D3DDeviceSettings.d3d9.AdapterOrdinal,
                                             g_D3DDeviceSettings.d3d9.DeviceType,
                                             g_D3DDeviceSettings.d3d9.AdapterFormat,
                                             g_D3DDeviceSettings.d3d9.pp.BackBufferFormat,
                                             ( g_D3DDeviceSettings.d3d9.pp.Windowed == TRUE ) );
}


//-------------------------------------------------------------------------------------
HRESULT OnMultisampleTypeChanged()
{
    HRESULT hr = S_OK;

    D3DMULTISAMPLE_TYPE multisampleType = GetSelectedMultisampleType();
    g_D3DDeviceSettings.d3d9.pp.MultiSampleType = multisampleType;

    // If multisampling is enabled, then centroid is a meaningful option.
    g_HUD.GetCheckBox( IDC_CENTROID )->SetEnabled( multisampleType != D3DMULTISAMPLE_NONE );
    g_HUD.GetCheckBox( IDC_CENTROID )->SetChecked( multisampleType != D3DMULTISAMPLE_NONE && g_bCentroid );

    CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo = GetCurrentDeviceSettingsCombo();
    if( pDeviceSettingsCombo == NULL )
        return E_FAIL;

    DWORD dwMaxQuality = 0;
    for( int iType = 0; iType < pDeviceSettingsCombo->multiSampleTypeList.GetSize(); iType++ )
    {
        D3DMULTISAMPLE_TYPE msType = pDeviceSettingsCombo->multiSampleTypeList.GetAt( iType );
        if( msType == multisampleType )
        {
            dwMaxQuality = pDeviceSettingsCombo->multiSampleQualityList.GetAt( iType );
            break;
        }
    }

    // DXUTSETTINGSDLG_MULTISAMPLE_QUALITY
    CDXUTComboBox* pMultisampleQualityCombo = g_HUD.GetComboBox( IDC_MULTISAMPLE_QUALITY );
    pMultisampleQualityCombo->RemoveAllItems();

    for( UINT iQuality = 0; iQuality < dwMaxQuality; iQuality++ )
    {
        AddMultisampleQuality( iQuality );
    }

    pMultisampleQualityCombo->SetSelectedByData( ULongToPtr( g_D3DDeviceSettings.d3d9.pp.MultiSampleQuality ) );

    hr = OnMultisampleQualityChanged();
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//-------------------------------------------------------------------------------------
HRESULT OnMultisampleQualityChanged()
{
    g_D3DDeviceSettings.d3d9.pp.MultiSampleQuality = GetSelectedMultisampleQuality();

    // Change the device if current settings don't match the UI settings
    DXUTDeviceSettings curDeviceSettings = DXUTGetDeviceSettings();
    if( curDeviceSettings.d3d9.pp.MultiSampleQuality != g_D3DDeviceSettings.d3d9.pp.MultiSampleQuality ||
        curDeviceSettings.d3d9.pp.MultiSampleType != g_D3DDeviceSettings.d3d9.pp.MultiSampleType )
    {
        if( g_D3DDeviceSettings.d3d9.pp.MultiSampleType != D3DMULTISAMPLE_NONE )
            g_D3DDeviceSettings.d3d9.pp.Flags &= ~D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

        DXUTCreateDeviceFromSettings( &g_D3DDeviceSettings );
    }

    return S_OK;
}

//-------------------------------------------------------------------------------------
void AddMultisampleType( D3DMULTISAMPLE_TYPE type )
{
    CDXUTComboBox* pComboBox = g_HUD.GetComboBox( IDC_MULTISAMPLE_TYPE );

    if( !pComboBox->ContainsItem( DXUTMultisampleTypeToString( type ) ) )
        pComboBox->AddItem( DXUTMultisampleTypeToString( type ), ULongToPtr( type ) );
}


//-------------------------------------------------------------------------------------
D3DMULTISAMPLE_TYPE GetSelectedMultisampleType()
{
    CDXUTComboBox* pComboBox = g_HUD.GetComboBox( IDC_MULTISAMPLE_TYPE );

    return ( D3DMULTISAMPLE_TYPE )PtrToUlong( pComboBox->GetSelectedData() );
}


//-------------------------------------------------------------------------------------
void AddMultisampleQuality( DWORD dwQuality )
{
    CDXUTComboBox* pComboBox = g_HUD.GetComboBox( IDC_MULTISAMPLE_QUALITY );

    WCHAR strQuality[50];
    swprintf_s( strQuality, 50, L"%d", dwQuality );
    strQuality[49] = 0;

    if( !pComboBox->ContainsItem( strQuality ) )
        pComboBox->AddItem( strQuality, ULongToPtr( dwQuality ) );
}


//-------------------------------------------------------------------------------------
DWORD GetSelectedMultisampleQuality()
{
    CDXUTComboBox* pComboBox = g_HUD.GetComboBox( IDC_MULTISAMPLE_QUALITY );

    return PtrToUlong( pComboBox->GetSelectedData() );
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    HRESULT hr;

    // Determine the active technique from the UI settings
    strcpy_s( g_strActiveTechnique, MAX_PATH, "Texture" );

    if( g_HUD.GetRadioButton( IDC_FILTER_POINT )->GetChecked() )
        strcat_s( g_strActiveTechnique, MAX_PATH, "Point" );
    else if( g_HUD.GetRadioButton( IDC_FILTER_LINEAR )->GetChecked() )
        strcat_s( g_strActiveTechnique, MAX_PATH, "Linear" );
    else if( g_HUD.GetRadioButton( IDC_FILTER_ANISOTROPIC )->GetChecked() )
        strcat_s( g_strActiveTechnique, MAX_PATH, "Anisotropic" );
    else
        return; //error

    if( g_HUD.GetCheckBox( IDC_CENTROID )->GetChecked() )
        strcat_s( g_strActiveTechnique, MAX_PATH, "Centroid" );

    // Get the current rotation matrix
    if( g_HUD.GetRadioButton( IDC_ROTATION_RPM )->GetChecked() )
    {
        float fRotate = ( float )_wtof( g_HUD.GetEditBox( IDC_RPM )->GetText() );
        D3DXMatrixRotationY( &g_mRotation, ( ( float )fTime * 2.0f * D3DX_PI * fRotate ) / ( 60.0f ) );
    }
    else if( g_HUD.GetRadioButton( IDC_ROTATION_DEGREES )->GetChecked() )
    {
        float fRotate = ( float )_wtof( g_HUD.GetEditBox( IDC_DEGREES )->GetText() );
        D3DXMatrixRotationY( &g_mRotation, fRotate * ( D3DX_PI / 180.0f ) );
    }

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 255, 255, 255 ), 1.0f, 0 ) );


    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        V( pd3dDevice->SetStreamSource( 0, g_pVBTriangles, 0, sizeof( Vertex ) ) );
        V( pd3dDevice->SetVertexDeclaration( g_pVertexDecl ) );

        DXUTComboBoxItem* pSelectedItem = g_HUD.GetComboBox( IDC_SCENE )->GetSelectedItem();

        if( 0 == lstrcmp( L"Triangles", pSelectedItem->strText ) )
            RenderSceneTriangles( pd3dDevice, fTime, fElapsedTime );
        else if( 0 == lstrcmp( L"Quads", pSelectedItem->strText ) )
            RenderSceneQuads( pd3dDevice, fTime, fElapsedTime );
        else if( 0 == lstrcmp( L"Spheres", pSelectedItem->strText ) )
            RenderSceneSpheres( pd3dDevice, fTime, fElapsedTime );

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" ); // These events are to help PIX identify what the code is doing
        RenderText();
        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_DialogLabels.OnRender( fElapsedTime ) );
        DXUT_EndPerfEvent();

        V( pd3dDevice->EndScene() );
    }
}


//-------------------------------------------------------------------------------------
void RenderSceneTriangles( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;

    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProjection;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXMATRIXA16 mRotation;
    D3DXMATRIXA16 mScaling;
    D3DXMATRIXA16 mTranslation;

    CalculateViewAndProjection( &mView, &mProjection, D3DX_PI / 4, -100, 100 );

    // Place labels within the scene
    g_DialogLabels.GetStatic( IDC_LABEL0 )->SetLocation( 25, 75 );
    g_DialogLabels.GetStatic( IDC_LABEL0 )->SetText( L"Solid Color Fill" );

    g_DialogLabels.GetStatic( IDC_LABEL1 )->SetLocation( 175, 75 );
    g_DialogLabels.GetStatic( IDC_LABEL1 )->SetText( L"Texturing Artifacts" );

    for( int iTriangle = 0; iTriangle < 4; iTriangle++ )
    {
        switch( iTriangle )
        {
            case 0:
                D3DXMatrixIdentity( &mRotation );
                D3DXMatrixTranslation( &mTranslation, 75.0f + 0.1f, 125.0f - 0.5f, 0 );
                V( g_pEffect->SetTechnique( "Color" ) );
                break;

            case 1:
                mRotation = g_mRotation;
                D3DXMatrixTranslation( &mTranslation, 75.0f + 0.1f, 275.0f - 0.5f, 0 );
                V( g_pEffect->SetTechnique( "Color" ) );
                break;

            case 2:
                D3DXMatrixIdentity( &mRotation );
                D3DXMatrixTranslation( &mTranslation, 225.0f + 0.1f, 125.0f - 0.5f, 0 );
                V( g_pEffect->SetTechnique( g_strActiveTechnique ) );
                break;

            case 3:
                mRotation = g_mRotation;
                D3DXMatrixTranslation( &mTranslation, 225.0f + 0.1f, 275.0f - 0.5f, 0 );
                V( g_pEffect->SetTechnique( g_strActiveTechnique ) );
                break;
        }

        const float Width = 40;
        const float Height = 100;

        D3DXMatrixScaling( &mScaling, Width, Height, 1 );
        mWorld = mScaling * mRotation * mTranslation;
        mWorldViewProjection = mWorld * mView * mProjection;

        V( g_pEffect->SetMatrix( "g_mWorldViewProjection", &mWorldViewProjection ) );
        V( g_pEffect->SetTexture( "g_tDiffuse", g_pTriangleTexture ) );


        UINT NumPasses;
        V( g_pEffect->Begin( &NumPasses, 0 ) );

        for( UINT iPass = 0; iPass < NumPasses; iPass++ )
        {
            V( g_pEffect->BeginPass( iPass ) );

            V( pd3dDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 1 ) );

            V( g_pEffect->EndPass() );
        }

        V( g_pEffect->End() );
    }
}


//-------------------------------------------------------------------------------------
void RenderSceneQuads( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;

    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProjection;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXMATRIXA16 mScaling;
    D3DXMATRIXA16 mTranslation;

    const D3DSURFACE_DESC* pDesc = DXUTGetD3D9BackBufferSurfaceDesc();
    const float totalWidth = ( float )pDesc->Width - GUI_WIDTH;
    const float radius = .2f * totalWidth;

    // Place labels within the scene
    g_DialogLabels.GetStatic( IDC_LABEL0 )->SetLocation( ( int )( .25f * totalWidth - 50 ), ( int )
                                                         ( pDesc->Height / 2.0f - radius - 50 ) );
    g_DialogLabels.GetStatic( IDC_LABEL0 )->SetText( L"2 Triangles" );

    g_DialogLabels.GetStatic( IDC_LABEL1 )->SetLocation( ( int )( .75f * totalWidth - 50 ), ( int )
                                                         ( pDesc->Height / 2.0f - radius - 50 ) );
    g_DialogLabels.GetStatic( IDC_LABEL1 )->SetText( L"8192 Triangles" );

    CalculateViewAndProjection( &mView, &mProjection, D3DX_PI / 4, -300, 300 );

    V( g_pEffect->SetTechnique( g_strActiveTechnique ) );
    V( g_pEffect->SetTexture( "g_tDiffuse", g_pCheckerTexture ) );

    for( int iQuad = 0; iQuad < 2; iQuad++ )
    {
        ID3DXMesh* pMesh = NULL;

        switch( iQuad )
        {
            case 0:
                D3DXMatrixTranslation( &mTranslation, .25f * totalWidth, pDesc->Height / 2.0f, 0 );
                pMesh = g_pMeshQuadLow;
                break;

            case 1:
                D3DXMatrixTranslation( &mTranslation, .75f * totalWidth, pDesc->Height / 2.0f, 0 );
                pMesh = g_pMeshQuadHigh;
                break;
        }

        D3DXMatrixScaling( &mScaling, 2 * radius, 2 * radius, 2 * radius );
        mWorld = mScaling * g_mRotation * mTranslation;
        mWorldViewProjection = mWorld * mView * mProjection;

        V( g_pEffect->SetMatrix( "g_mWorldViewProjection", &mWorldViewProjection ) );

        UINT NumPasses;
        V( g_pEffect->Begin( &NumPasses, 0 ) );

        for( UINT iPass = 0; iPass < NumPasses; iPass++ )
        {
            V( g_pEffect->BeginPass( iPass ) );
            V( pMesh->DrawSubset( 0 ) );
            V( g_pEffect->EndPass() );
        }

        V( g_pEffect->End() );
    }
}


//-------------------------------------------------------------------------------------
void RenderSceneSpheres( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;

    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProjection;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXMATRIXA16 mScaling;
    D3DXMATRIXA16 mTranslation;

    const D3DSURFACE_DESC* pDesc = DXUTGetD3D9BackBufferSurfaceDesc();
    const float totalWidth = ( float )pDesc->Width - GUI_WIDTH;
    const float radius = .2f * totalWidth;

    // Place labels within the scene
    g_DialogLabels.GetStatic( IDC_LABEL0 )->SetLocation( ( int )( .25f * totalWidth - 50 ), ( int )
                                                         ( pDesc->Height / 2.0f - radius - 50 ) );
    g_DialogLabels.GetStatic( IDC_LABEL0 )->SetText( L"180 Triangles" );

    g_DialogLabels.GetStatic( IDC_LABEL1 )->SetLocation( ( int )( .75f * totalWidth - 50 ), ( int )
                                                         ( pDesc->Height / 2.0f - radius - 50 ) );
    g_DialogLabels.GetStatic( IDC_LABEL1 )->SetText( L"8064 Triangles" );

    CalculateViewAndProjection( &mView, &mProjection, D3DX_PI / 4, -300, 300 );

    V( g_pEffect->SetTechnique( g_strActiveTechnique ) );
    V( g_pEffect->SetTexture( "g_tDiffuse", g_pCheckerTexture ) );

    for( int iSphere = 0; iSphere < 2; iSphere++ )
    {
        ID3DXMesh* pMesh = NULL;

        switch( iSphere )
        {
            case 0:
                D3DXMatrixTranslation( &mTranslation, .25f * totalWidth, pDesc->Height / 2.0f, 0 );
                pMesh = g_pMeshSphereLow;
                break;

            case 1:
                D3DXMatrixTranslation( &mTranslation, .75f * totalWidth, pDesc->Height / 2.0f, 0 );
                pMesh = g_pMeshSphereHigh;
                break;
        }

        D3DXMatrixScaling( &mScaling, radius, radius, radius );
        mWorld = mScaling * g_mRotation * mTranslation;
        mWorldViewProjection = mWorld * mView * mProjection;

        V( g_pEffect->SetMatrix( "g_mWorldViewProjection", &mWorldViewProjection ) );

        UINT NumPasses;
        V( g_pEffect->Begin( &NumPasses, 0 ) );

        for( UINT iPass = 0; iPass < NumPasses; iPass++ )
        {
            V( g_pEffect->BeginPass( iPass ) );
            V( pMesh->DrawSubset( 0 ) );
            V( g_pEffect->EndPass() );
        }

        V( g_pEffect->End() );
    }
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 0.3f, 0.3f, 0.7f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );
    txtHelper.SetForegroundColor( D3DXCOLOR( 0.4f, 0.4f, 0.6f, 1.0f ) );
    txtHelper.DrawTextLine( L"Please see the AntiAlias documentation" );
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
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_MULTISAMPLE_TYPE:
            OnMultisampleTypeChanged(); break;
        case IDC_MULTISAMPLE_QUALITY:
            OnMultisampleQualityChanged(); break;
        case IDC_CENTROID:
            g_bCentroid = ( ( CDXUTCheckBox* )pControl )->GetChecked(); break;
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
    SAFE_RELEASE( g_pVertexDecl );
    SAFE_RELEASE( g_pVBTriangles );
    SAFE_RELEASE( g_pMeshSphereHigh );
    SAFE_RELEASE( g_pMeshSphereLow );
    SAFE_RELEASE( g_pMeshQuadHigh );
    SAFE_RELEASE( g_pMeshQuadLow );
    SAFE_RELEASE( g_pCheckerTexture );
    SAFE_RELEASE( g_pTriangleTexture );
}



