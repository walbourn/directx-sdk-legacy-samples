//--------------------------------------------------------------------------------------
// File: ContentStreaming10.cpp
//
// Illustrates streaming content using Direct3D 10
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
#include "AsyncLoader.h"
#include "ContentLoaders.h"
#include "PackedFile.h"
#include "Terrain.h"

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------
#define DEG2RAD(p) ( D3DX_PI*(p/180.0f) )
#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x)/sizeof(x[0]))
#endif


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CFirstPersonCamera                  g_Camera;               // A first person camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*                    g_pTxtHelper = NULL;
CDXUTDialog                         g_HUD;                  // dialog for standard controls
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
CDXUTDialog                         g_StartUpUI;            // dialog for startup

// Direct3D 9 resources
extern ID3DXFont*                   g_pFont9;
extern ID3DXSprite*                 g_pSprite9;
extern ID3DXEffect*                 g_pEffect9;
extern D3DXHANDLE                   g_htxDiffuse;
extern D3DXHANDLE                   g_htxNormal;

// Direct3D 10 resources
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10InputLayout*                  g_pLayoutObject = NULL;
CAsyncLoader*                       g_pAsyncLoader = NULL;
CResourceReuseCache*                g_pResourceReuseCache = NULL;
CPackedFile                         g_PackFile;

// Effect variables
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProjection = NULL;
ID3D10EffectVectorVariable*         g_pEyePt = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDiffuse = NULL;
ID3D10EffectShaderResourceVariable* g_ptxNormal = NULL;

ID3D10EffectTechnique*              g_pRenderTileDiff = NULL;
ID3D10EffectTechnique*              g_pRenderTileBump = NULL;
ID3D10EffectTechnique*              g_pRenderTileWire = NULL;

float                               g_fFOV = 70.0f;
float                               g_fAspectRatio = 0.0f;
float                               g_fInitialLoadTime = 0.0f;
UINT                                g_NumModelsInUse = 0;
float                               g_fLoadingRadius;
float                               g_fVisibleRadius;
float                               g_fViewHeight = 7.5f;
float                               g_fRotationFrequency = 20.0f;
float                               g_fRotateInLeadTime = 2.0f;
int                                 g_NumResourceToLoadPerFrame = 1;
int                                 g_UploadToVRamEveryNthFrame = 3;
UINT                                g_SkipMips = 0;
UINT                                g_NumProcessingThreads = 1;
UINT                                g_MaxOutstandingResources = 1500;
UINT64                              g_AvailableVideoMem = 0;
bool                                g_bUseWDDMPaging = false;
bool                                g_bStartupResourcesLoaded = false;
bool                                g_bDrawUI = true;
bool                                g_bWireframe = false;

CGrowableArray <LEVEL_ITEM*>        g_LevelItemArray;
CGrowableArray <LEVEL_ITEM*>        g_VisibleItemArray;
CGrowableArray <LEVEL_ITEM*>        g_LoadedItemArray;
CTerrain                            g_Terrain;

enum LOAD_TYPE
{
    LOAD_TYPE_MULTITHREAD = 0,
    LOAD_TYPE_SINGLETHREAD,
};
LOAD_TYPE                           g_LoadType = LOAD_TYPE_MULTITHREAD;

enum APP_STATE
{
    APP_STATE_STARTUP = 0,
    APP_STATE_RENDER_SCENE
};
APP_STATE                           g_AppState = APP_STATE_STARTUP; // Current state of app

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
// General HUD
#define IDC_TOGGLEFULLSCREEN		1
#define IDC_TOGGLEREF				2
#define IDC_CHANGEDEVICE			3
// StartupUI
#define IDC_LOAD_TYPE_GROUP			4
#define IDC_ONDEMANDSINGLETHREAD	5
#define IDC_ONDEMANDMULTITHREAD		8
#define IDC_RUN						9
#define IDC_DELETE_PACK_FILE		10
// SampleUI
#define IDC_VIEWHEIGHT_STATIC		20
#define IDC_VIEWHEIGHT				21
#define IDC_VISIBLERADIUS_STATIC	22
#define IDC_VISIBLERADIUS			23
#define IDC_NUMPERFRAMERES_STATIC	24
#define IDC_NUMPERFRAMERES			25
#define IDC_UPLOADTOVRAMFREQ_STATIC 26
#define IDC_UPLOADTOVRAMFREQ		27
#define IDC_WIREFRAME				28
#define IDC_STARTOVER				29

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

extern bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                             bool bWindowed, void* pUserContext );
extern HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice,
                                            const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
extern HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                           void* pUserContext );
extern void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime,
                                        void* pUserContext );
extern void CALLBACK OnD3D9LostDevice( void* pUserContext );
extern void CALLBACK OnD3D9DestroyDevice( void* pUserContext );

bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );

extern void CALLBACK CreateTextureFromFile9_Serial( IDirect3DDevice9* pDev, WCHAR* szFileName,
                                                    IDirect3DTexture9** ppTexture, void* pContext );
extern void CALLBACK CreateVertexBuffer9_Serial( IDirect3DDevice9* pDev, IDirect3DVertexBuffer9** ppBuffer,
                                                 UINT iSizeBytes, DWORD Usage, DWORD FVF, D3DPOOL Pool, void* pData,
                                                 void* pContext );
extern void CALLBACK CreateIndexBuffer9_Serial( IDirect3DDevice9* pDev, IDirect3DIndexBuffer9** ppBuffer,
                                                UINT iSizeBytes, DWORD Usage, D3DFORMAT ibFormat, D3DPOOL Pool,
                                                void* pData, void* pContext );
extern void CALLBACK CreateTextureFromFile9_Async( IDirect3DDevice9* pDev, WCHAR* szFileName,
                                                   IDirect3DTexture9** ppTexture, void* pContext );
extern void CALLBACK CreateVertexBuffer9_Async( IDirect3DDevice9* pDev, IDirect3DVertexBuffer9** ppBuffer,
                                                UINT iSizeBytes, DWORD Usage, DWORD FVF, D3DPOOL Pool, void* pData,
                                                void* pContext );
extern void CALLBACK CreateIndexBuffer9_Async( IDirect3DDevice9* pDev, IDirect3DIndexBuffer9** ppBuffer,
                                               UINT iSizeBytes, DWORD Usage, D3DFORMAT ibFormat, D3DPOOL Pool,
                                               void* pData, void* pContext );

void CALLBACK CreateTextureFromFile10_Serial( ID3D10Device* pDev, WCHAR* szFileName, ID3D10ShaderResourceView** ppRV,
                                              void* pContext );
void CALLBACK CreateVertexBuffer10_Serial( ID3D10Device* pDev, ID3D10Buffer** ppBuffer, D3D10_BUFFER_DESC BufferDesc,
                                           void* pData, void* pContext );
void CALLBACK CreateIndexBuffer10_Serial( ID3D10Device* pDev, ID3D10Buffer** ppBuffer, D3D10_BUFFER_DESC BufferDesc,
                                          void* pData, void* pContext );
void CALLBACK CreateTextureFromFile10_Async( ID3D10Device* pDev, WCHAR* szFileName, ID3D10ShaderResourceView** ppRV,
                                             void* pContext );
void CALLBACK CreateVertexBuffer10_Async( ID3D10Device* pDev, ID3D10Buffer** ppBuffer, D3D10_BUFFER_DESC BufferDesc,
                                          void* pData, void* pContext );
void CALLBACK CreateIndexBuffer10_Async( ID3D10Device* pDev, ID3D10Buffer** ppBuffer, D3D10_BUFFER_DESC BufferDesc,
                                         void* pData, void* pContext );

void InitApp();
void LoadStartupResources( IDirect3DDevice9* pDev9, ID3D10Device* pDev10, double fTime );
UINT EnsureResourcesLoaded( IDirect3DDevice9* pDev9, ID3D10Device* pDev10, float visradius, float loadradius );
UINT EnsureUnusedResourcesUnloaded( IDirect3DDevice9* pDev9, ID3D10Device* pDev10, double fTime );
void CheckForLoadDone( IDirect3DDevice9* pDev9, ID3D10Device* pDev10 );
void SetTextureLOD( IDirect3DDevice9* pDev9, ID3D10Device* pDev10 );
void RenderText();
void DestroyAllMeshes( LOADER_DEVICE_TYPE ldt );
void ClearD3D10State();

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D10) 
    // that is available on the system depending on which D3D callbacks are set below

    // Disable gamma correction in DXUT
    DXUTSetIsInGammaCorrectMode( false );

    // Set DXUT callbacks
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnD3D9CreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnD3D9ResetDevice );
    DXUTSetCallbackD3D9DeviceLost( OnD3D9LostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnD3D9DestroyDevice );
    DXUTSetCallbackD3D9FrameRender( OnD3D9FrameRender );

    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"ContentStreaming" );
    DXUTCreateDevice( true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );
    g_StartUpUI.Init( &g_DialogResourceManager );

    int iY = 10;
    g_HUD.SetCallback( OnGUIEvent );
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    iY += 100;

    WCHAR str[MAX_PATH];

    // SampleUI
    swprintf_s( str, MAX_PATH, L"View Height: %0.2f", g_fViewHeight );
    g_SampleUI.AddStatic( IDC_VIEWHEIGHT_STATIC, str, 5, iY += 24, 150, 22 );
    g_SampleUI.AddSlider( IDC_VIEWHEIGHT, 15, iY += 24, 135, 22, 0, 10000, ( int )( g_fViewHeight * 100.0f ) );

    swprintf_s( str, MAX_PATH, L"Visible Radius: %0.2f", g_fVisibleRadius );
    g_SampleUI.AddStatic( IDC_VISIBLERADIUS_STATIC, str, 5, iY += 24, 150, 22 );
    g_SampleUI.AddSlider( IDC_VISIBLERADIUS, 15, iY += 24, 135, 22, 0, 1000, ( int )( g_fVisibleRadius * 100.0f ) );

    swprintf_s( str, MAX_PATH, L"Create up to\n%d Items Per-Frame", g_NumResourceToLoadPerFrame );
    g_SampleUI.AddStatic( IDC_NUMPERFRAMERES_STATIC, str, 5, iY += 24, 150, 22 );
    g_SampleUI.AddSlider( IDC_NUMPERFRAMERES, 15, iY += 24, 135, 22, 0, 30, g_NumResourceToLoadPerFrame );

    swprintf_s( str, MAX_PATH, L"Upload to VRam every\n%d frames", g_UploadToVRamEveryNthFrame );
    g_SampleUI.AddStatic( IDC_UPLOADTOVRAMFREQ_STATIC, str, 5, iY += 24, 150, 22 );
    g_SampleUI.AddSlider( IDC_UPLOADTOVRAMFREQ, 15, iY += 24, 135, 22, 1, 10, g_UploadToVRamEveryNthFrame );

    g_SampleUI.AddCheckBox( IDC_WIREFRAME, L"Wireframe", 35, iY += 24, 125, 22, g_bWireframe );

    g_SampleUI.AddButton( IDC_STARTOVER, L"Start Over", 35, iY += 24, 125, 22 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;

    // StartupUI
    // Title font for comboboxes
    g_StartUpUI.SetFont( 1, L"Arial", 24, FW_BOLD );
    CDXUTElement* pElement = g_StartUpUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_CENTER | DT_BOTTOM;
    }

    int iX1 = 100;
    iY = 0;
    g_StartUpUI.AddStatic( -1, L"How would you like to load the scene?", 0, iY, 400, 22 );
    g_StartUpUI.AddRadioButton( IDC_ONDEMANDMULTITHREAD, IDC_LOAD_TYPE_GROUP, L"On Demand Multi-Threaded", iX1,
                                iY += 42, 250, 22 );
    g_StartUpUI.AddRadioButton( IDC_ONDEMANDSINGLETHREAD, IDC_LOAD_TYPE_GROUP, L"On Demand Single-Threaded", iX1,
                                iY += 22, 250, 22 );
    g_StartUpUI.AddButton( IDC_RUN, L"Run", 70, iY += 40, 250, 40 );
    g_StartUpUI.AddButton( IDC_DELETE_PACK_FILE, L"Delete Packfile", 70, iY += 40, 250, 40 );

    // Check the defaults
    g_StartUpUI.GetRadioButton( IDC_ONDEMANDMULTITHREAD )->SetChecked( true );
    g_StartUpUI.SetCallback( OnGUIEvent );

    // camera setup
    g_Camera.SetScalers( 0.01f, 100.0f );

    // Constrain the camera to movement within the horizontal plane
    g_Camera.SetEnableYAxisMovement( false );
}

//--------------------------------------------------------------------------------------
// Load a mesh using one of three techniques
//--------------------------------------------------------------------------------------
void SmartLoadMesh( IDirect3DDevice9* pDev9, ID3D10Device* pDev10, LEVEL_ITEM* pItem )
{
    if( pDev9 )
    {
        if( LOAD_TYPE_SINGLETHREAD == g_LoadType )
        {
            BYTE* pData;
            UINT DataBytes;

            if( !g_PackFile.GetPackedFile( pItem->szVBName, &pData, &DataBytes ) )
                return;
            CreateVertexBuffer9_Serial( pDev9, &pItem->VB.pVB9, DataBytes, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED,
                                        pData, NULL );
            if( !g_PackFile.GetPackedFile( pItem->szIBName, &pData, &DataBytes ) )
                return;
            CreateIndexBuffer9_Serial( pDev9, &pItem->IB.pIB9, DataBytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                       D3DPOOL_MANAGED, pData, NULL );
            CreateTextureFromFile9_Serial( pDev9, pItem->szDiffuseName, &pItem->Diffuse.pTexture9, NULL );
            CreateTextureFromFile9_Serial( pDev9, pItem->szNormalName, &pItem->Normal.pTexture9, NULL );
        }
        else if( LOAD_TYPE_MULTITHREAD == g_LoadType )
        {
            BYTE* pData;
            UINT DataBytes;

            if( !g_PackFile.GetPackedFile( pItem->szVBName, &pData, &DataBytes ) )
                return;
            CreateVertexBuffer9_Async( pDev9, &pItem->VB.pVB9, DataBytes, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED,
                                       pData, ( void* )g_pAsyncLoader );
            if( !g_PackFile.GetPackedFile( pItem->szIBName, &pData, &DataBytes ) )
                return;
            CreateIndexBuffer9_Async( pDev9, &pItem->IB.pIB9, DataBytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                      D3DPOOL_MANAGED, pData, ( void* )g_pAsyncLoader );
            CreateTextureFromFile9_Async( pDev9, pItem->szDiffuseName, &pItem->Diffuse.pTexture9,
                                          ( void* )g_pAsyncLoader );
            CreateTextureFromFile9_Async( pDev9, pItem->szNormalName, &pItem->Normal.pTexture9,
                                          ( void* )g_pAsyncLoader );
        }
    }
    else if( pDev10 )
    {
        if( LOAD_TYPE_SINGLETHREAD == g_LoadType )
        {
            BYTE* pData;
            UINT DataBytes;

            if( !g_PackFile.GetPackedFile( pItem->szVBName, &pData, &DataBytes ) )
                return;
            D3D10_BUFFER_DESC bufferDesc;
            bufferDesc.ByteWidth = DataBytes;
            bufferDesc.Usage = D3D10_USAGE_DEFAULT;
            bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;
            bufferDesc.MiscFlags = 0;
            CreateVertexBuffer10_Serial( pDev10, &pItem->VB.pVB10, bufferDesc, pData, NULL );

            if( !g_PackFile.GetPackedFile( pItem->szIBName, &pData, &DataBytes ) )
                return;
            bufferDesc.ByteWidth = DataBytes;
            bufferDesc.Usage = D3D10_USAGE_DEFAULT;
            bufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;
            bufferDesc.MiscFlags = 0;
            CreateIndexBuffer10_Serial( pDev10, &pItem->IB.pIB10, bufferDesc, pData, NULL );

            CreateTextureFromFile10_Serial( pDev10, pItem->szDiffuseName, &pItem->Diffuse.pRV10, NULL );
            CreateTextureFromFile10_Serial( pDev10, pItem->szNormalName, &pItem->Normal.pRV10, NULL );
        }
        else if( LOAD_TYPE_MULTITHREAD == g_LoadType )
        {
            BYTE* pData;
            UINT DataBytes;

            if( !g_PackFile.GetPackedFile( pItem->szVBName, &pData, &DataBytes ) )
                return;
            D3D10_BUFFER_DESC bufferDesc;
            bufferDesc.ByteWidth = DataBytes;
            bufferDesc.Usage = D3D10_USAGE_DEFAULT;
            bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;
            bufferDesc.MiscFlags = 0;
            CreateVertexBuffer10_Async( pDev10, &pItem->VB.pVB10, bufferDesc, pData, ( void* )g_pAsyncLoader );

            if( !g_PackFile.GetPackedFile( pItem->szIBName, &pData, &DataBytes ) )
                return;
            bufferDesc.ByteWidth = DataBytes;
            bufferDesc.Usage = D3D10_USAGE_DEFAULT;
            bufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;
            bufferDesc.MiscFlags = 0;
            CreateIndexBuffer10_Async( pDev10, &pItem->IB.pIB10, bufferDesc, pData, ( void* )g_pAsyncLoader );

            CreateTextureFromFile10_Async( pDev10, pItem->szDiffuseName, &pItem->Diffuse.pRV10,
                                           ( void* )g_pAsyncLoader );
            CreateTextureFromFile10_Async( pDev10, pItem->szNormalName, &pItem->Normal.pRV10,
                                           ( void* )g_pAsyncLoader );
        }
    }
}

//--------------------------------------------------------------------------------------
float RPercent()
{
    float ret = ( float )( ( rand() % 20000 ) - 10000 );
    return ret / 10000.0f;
}

//-------------------------------------------------------------------------------------
const WCHAR g_strFile[MAX_PATH] = L"\\ContentPackedFile.packedfile";
const UINT64                        g_PackedFileSize = 3408789504u;
void GetPackedFilePath( WCHAR* strPath, UINT cchPath )
{
    WCHAR strFolder[MAX_PATH] = L"\\ContentStreaming";

    SHGetFolderPath( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, strPath );
    wcscat_s( strPath, cchPath, strFolder );
}

//--------------------------------------------------------------------------------------
// Load the resources necessary at the beginning of a level
//--------------------------------------------------------------------------------------
void LoadStartupResources( IDirect3DDevice9* pDev9, ID3D10Device* pDev10, double fTime )
{
    if( g_bStartupResourcesLoaded )
        return;

    WCHAR strPath[MAX_PATH] = {0};
    WCHAR strDirectory[MAX_PATH] = {0};
    GetPackedFilePath( strDirectory, MAX_PATH );

    if( 0xFFFFFFFF == GetFileAttributes( strDirectory ) )
    {
        // create the directory
        if( !CreateDirectory( strDirectory, NULL ) )
        {
            MessageBox( NULL, L"There was an error creating the pack file.  ContentStreaming will now exit.", L"Error",
                        MB_OK );
            PostQuitMessage( 0 );
            return;
        }
    }

    // Find the pack file
    UINT SqrtNumTiles = 20;
    UINT SidesPerTile = 50;
    float fWorldScale = 6667.0f;
    float fHeightScale = 300.0f;
    wcscpy_s( strPath, MAX_PATH, strDirectory );
    wcscat_s( strPath, MAX_PATH, g_strFile );
    bool bCreatePackedFile = false;
    if( 0xFFFFFFFF == GetFileAttributes( strPath ) )
    {
        bCreatePackedFile = true;
    }
    else
    {
        HANDLE hFile = CreateFile( strPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                   FILE_FLAG_SEQUENTIAL_SCAN, NULL );
        if( INVALID_HANDLE_VALUE != hFile )
        {
            // Check against file size in case the process was killed trying to create the file
            LARGE_INTEGER FileSize;
            GetFileSizeEx( hFile, &FileSize );
            UINT64 Size = FileSize.QuadPart;
            CloseHandle( hFile );

            if( Size != g_PackedFileSize )
                bCreatePackedFile = true;
        }
        else
        {
            bCreatePackedFile = true;
        }
    }

    if( bCreatePackedFile )
    {
        // Check for necessary disk space
        ULARGE_INTEGER FreeBytesAvailable;
        ULARGE_INTEGER TotalNumberOfBytes;
        ULARGE_INTEGER TotalNumberOfFreeBytes;
        if( !GetDiskFreeSpaceEx( strDirectory, &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes ) ||
            FreeBytesAvailable.QuadPart < g_PackedFileSize )
        {
            WCHAR strSizeMessage[1024];
            swprintf_s( strSizeMessage, 1024,
                             L"There is not enough free disk space to create the file %s.  ContentStreaming will now exit.", strPath );
            MessageBox( NULL, strSizeMessage, L"Error", MB_OK );
            PostQuitMessage( 0 );
            return;
        }

        if( IDNO == MessageBox( NULL,
                                L"This is the first time ContentStreaming has been run.  The sample will need to create a 3.3 gigabyte pack file in order to demonstrate loading assets from a packed file format.  Do you wish to continue?", L"Warning", MB_YESNO ) )
        {
            PostQuitMessage( 0 );
            return;
        }

        if( !g_PackFile.CreatePackedFile( pDev10, pDev9, strPath, SqrtNumTiles, SidesPerTile, fWorldScale,
                                          fHeightScale ) )
        {
            MessageBox( NULL, L"There was an error creating the pack file.  ContentStreaming will now exit.", L"Error",
                        MB_OK );
            PostQuitMessage( 0 );
            return;
        }
    }

    // Create a pseudo terrain
    WCHAR str[MAX_PATH];
    if( FAILED( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"contentstreaming\\terrain1.bmp" ) ) )
        return;
    if( FAILED( g_Terrain.LoadTerrain( str, SqrtNumTiles, SidesPerTile, fWorldScale, fHeightScale, false ) ) )
        return;

    bool b64Bit = false;
    if( !g_PackFile.LoadPackedFile( strPath, b64Bit, &g_LevelItemArray ) )
    {
        MessageBox( NULL, L"There was an error loading the pack file.  ContentStreaming will now exit.", L"Error",
                    MB_OK );
        PostQuitMessage( 0 );
        return;
    }

#if defined(_AMD64_)
    // We don't have a VS space limit on X64
    UINT maxChunks = (UINT)g_PackFile.GetNumChunks();
    g_PackFile.SetMaxChunksMapped( maxChunks );
    for(UINT i=0; i<maxChunks; i++)
        g_PackFile.EnsureChunkMapped( i );
#else
    // This ensure that the loading radius can cover at most 9 chunks
    UINT maxChunks = g_PackFile.GetMaxChunksInVA();
    g_PackFile.SetMaxChunksMapped( maxChunks );
#endif
    g_fLoadingRadius = g_PackFile.GetLoadingRadius();
    g_fVisibleRadius = g_fLoadingRadius;
    g_Camera.SetProjParams( DEG2RAD(g_fFOV), g_fAspectRatio, 0.5f, g_fVisibleRadius );

    // Determine our available texture memory and try to skip mip levels to fit into it
    g_pResourceReuseCache->SetMaxManagedMemory( g_AvailableVideoMem );
    g_SkipMips = 0;
    UINT64 FullUsage = g_PackFile.GetVideoMemoryUsageAtFullMips();
    while( FullUsage > g_AvailableVideoMem )
    {	
        FullUsage = FullUsage >> 2;
        g_SkipMips ++;
    }

    swprintf_s( str, MAX_PATH, L"Visible Radius: %0.2f", g_fVisibleRadius );
    g_SampleUI.GetStatic( IDC_VISIBLERADIUS_STATIC )->SetText( str );
    g_SampleUI.GetSlider( IDC_VISIBLERADIUS )->SetRange( 0, ( int )( g_fLoadingRadius * 100.0f ) );
    g_SampleUI.GetSlider( IDC_VISIBLERADIUS )->SetValue( ( int )( g_fVisibleRadius * 100.0f ) );
    g_pResourceReuseCache->OnDestroy();

    // Tell the resource cache to create all resources
    g_pResourceReuseCache->SetDontCreateResources( FALSE );

    // Use the same random seed each time for consistent repros
    srand( 100 );

    g_bStartupResourcesLoaded = true;
}

//--------------------------------------------------------------------------------------
// GetCameraCullPlanes
//--------------------------------------------------------------------------------------
void GetCameraCullPlanes( D3DXVECTOR3* p1Normal, D3DXVECTOR3* p2Normal, float* p1D, float* p2D )
{
    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    vEye = *g_Camera.GetEyePt();
    vDir = *g_Camera.GetLookAtPt() - vEye;
    D3DXVec3Normalize( &vDir, &vDir );

    // setup clip planes
    D3DXVECTOR3 vLeftNormal;
    D3DXVECTOR3 vRightNormal;
    D3DXMATRIX mRotLeft;
    D3DXMATRIX mRotRight;
    float fAngle = D3DX_PI / 2.0f + ( DEG2RAD(g_fFOV) / 2.0f ) * 1.3333f;
    D3DXMatrixRotationY( &mRotLeft, -fAngle );
    D3DXMatrixRotationY( &mRotRight, fAngle );
    D3DXVec3TransformNormal( &vLeftNormal, &vDir, &mRotLeft );
    D3DXVec3TransformNormal( &vRightNormal, &vDir, &mRotRight );
    *p1D = D3DXVec3Dot( &vLeftNormal, &vEye );
    *p2D = D3DXVec3Dot( &vRightNormal, &vEye );
    *p1Normal = vLeftNormal;
    *p2Normal = vRightNormal;
}

//--------------------------------------------------------------------------------------
// Calculate our visible and potentially visible items
//--------------------------------------------------------------------------------------
void CalculateVisibleItems( D3DXVECTOR3 vEye, float fVisRadius, float fLoadRadius )
{
    g_VisibleItemArray.Reset();
    g_LoadedItemArray.Reset();

    // setup cull planes
    D3DXVECTOR3 vLeftNormal;
    D3DXVECTOR3 vRightNormal;
    float leftD;
    float rightD;
    GetCameraCullPlanes( &vLeftNormal, &vRightNormal, &leftD, &rightD );
    float fTileSize = g_PackFile.GetTileSideSize();

    for( int i = 0; i < g_LevelItemArray.GetSize(); i++ )
    {
        LEVEL_ITEM* pItem = g_LevelItemArray.GetAt( i );

        // Default is not in loading radius
        pItem->bInLoadRadius = false;

        D3DXVECTOR3 vDelta = vEye - pItem->vCenter;
        float len2 = D3DXVec3LengthSq( &vDelta );
        if( len2 < fVisRadius * fVisRadius )
        {
            pItem->bInFrustum = false;

            if( len2 < fTileSize * fTileSize ||
                ( D3DXVec3Dot( &pItem->vCenter, &vLeftNormal ) < leftD + fTileSize &&
                  D3DXVec3Dot( &pItem->vCenter, &vRightNormal ) < rightD + fTileSize )
                )
            {
                pItem->bInFrustum = true;
            }

            g_VisibleItemArray.Add( pItem );
        }
        if( len2 < fLoadRadius * fLoadRadius )
        {
            pItem->bInLoadRadius = true;
            g_LoadedItemArray.Add( pItem );
        }
    }
}

//--------------------------------------------------------------------------------------
// Ensure resources within a certain radius are loaded
//--------------------------------------------------------------------------------------
UINT EnsureResourcesLoaded( IDirect3DDevice9* pDev9, ID3D10Device* pDev10, float fVisRadius, float fLoadRadius )
{
    UINT NumToLoad = 0;
    for( int i = 0; i < g_LoadedItemArray.GetSize(); i++ )
    {
        LEVEL_ITEM* pItem = g_LoadedItemArray.GetAt( i );

        if( !pItem->bLoaded && !pItem->bLoading )
        {
            pItem->bLoading = true;
            NumToLoad ++;
            SmartLoadMesh( pDev9, pDev10, pItem );
        }
    }

    return NumToLoad;
}

//--------------------------------------------------------------------------------------
void FreeUpMeshResources( LEVEL_ITEM* pItem, IDirect3DDevice9* pDev9, ID3D10Device* pDev10 )
{
    if( pDev9 )
    {
        g_pResourceReuseCache->UnuseDeviceTexture9( pItem->Diffuse.pTexture9 );
        g_pResourceReuseCache->UnuseDeviceTexture9( pItem->Normal.pTexture9 );
        g_pResourceReuseCache->UnuseDeviceVB9( pItem->VB.pVB9 );
        g_pResourceReuseCache->UnuseDeviceIB9( pItem->IB.pIB9 );
    }
    else if( pDev10 )
    {
        g_pResourceReuseCache->UnuseDeviceTexture10( pItem->Diffuse.pRV10 );
        g_pResourceReuseCache->UnuseDeviceTexture10( pItem->Normal.pRV10 );
        g_pResourceReuseCache->UnuseDeviceVB10( pItem->VB.pVB10 );
        g_pResourceReuseCache->UnuseDeviceIB10( pItem->IB.pIB10 );
    }
}

//--------------------------------------------------------------------------------------
// Ensure resources that are unused are unloaded
//--------------------------------------------------------------------------------------
UINT EnsureUnusedResourcesUnloaded( IDirect3DDevice9* pDev9, ID3D10Device* pDev10, double fTime )
{
    UINT NumToUnload = 0;

    for( int i = 0; i < g_LevelItemArray.GetSize(); i++ )
    {
        LEVEL_ITEM* pItem = g_LevelItemArray.GetAt( i );

        if( pItem->bLoaded && !pItem->bInLoadRadius )
        {
            // Unload the mesh textures from the texture cache
            FreeUpMeshResources( pItem, pDev9, pDev10 );
            pItem->bLoading = false;
            pItem->bLoaded = false;
            pItem->bHasBeenRenderedDiffuse = false;
            pItem->bHasBeenRenderedNormal = false;
        }
    }

    return NumToUnload;
}


//--------------------------------------------------------------------------------------
// If an item is done loading, label it as loaded
//--------------------------------------------------------------------------------------
void CheckForLoadDone( IDirect3DDevice9* pDev9, ID3D10Device* pDev10 )
{
    if( pDev9 )
    {
        for( int i = 0; i < g_LevelItemArray.GetSize(); i++ )
        {
            LEVEL_ITEM* pItem = g_LevelItemArray.GetAt( i );

            if( pItem->bLoading )
            {
                if( pItem->VB.pVB9 &&
                    pItem->IB.pIB9 )
                {
                    pItem->bLoading = false;
                    pItem->bLoaded = true;

                    pItem->CurrentCountdownDiff = 5;
                    pItem->CurrentCountdownNorm = 10;
                }
            }
        }
    }
    else if( pDev10 )
    {
        for( int i = 0; i < g_LevelItemArray.GetSize(); i++ )
        {
            LEVEL_ITEM* pItem = g_LevelItemArray.GetAt( i );

            if( pItem->bLoading )
            {
                if( pItem->VB.pVB10 &&
                    pItem->IB.pIB10 )
                {
                    pItem->bLoading = false;
                    pItem->bLoaded = true;

                    pItem->CurrentCountdownDiff = 5;
                    pItem->CurrentCountdownNorm = 10;
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    WCHAR str[MAX_PATH];

    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( true ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    swprintf_s( str, MAX_PATH, L"InitialLevel Loaded in %.02f seconds", g_fInitialLoadTime );
    g_pTxtHelper->DrawTextLine( str );
    swprintf_s( str, MAX_PATH, L"Models in Use: %d", g_NumModelsInUse );
    g_pTxtHelper->DrawTextLine( str );
    g_pTxtHelper->DrawTextLine( L"" );
    if( g_pResourceReuseCache )
    {
        int NumTextures = g_pResourceReuseCache->GetNumTextures();
        UINT NumUsed = 0;
        UINT64 EstimatedManagedMemory = g_pResourceReuseCache->GetUsedManagedMemory();

        for( int i = 0; i < NumTextures; i++ )
        {
            DEVICE_TEXTURE* tex = g_pResourceReuseCache->GetTexture( i );
            if( tex->bInUse )
                NumUsed ++;
        }

        swprintf_s( str, MAX_PATH, L"Estimated video memory used: %d (mb) of %d (mb)", ( int )
                         ( EstimatedManagedMemory / ( 1024 * 1024 ) ), ( int )( g_AvailableVideoMem /
                                                                                ( 1024 * 1024 ) ) );
        g_pTxtHelper->DrawTextLine( str );

        swprintf_s( str, MAX_PATH, L"TextureCache: Total textures: %d", NumTextures );
        g_pTxtHelper->DrawTextLine( str );
        swprintf_s( str, MAX_PATH, L"TextureCache: Used textures: %d", NumUsed );
        g_pTxtHelper->DrawTextLine( str );

        // LOD list
        int TextureSize = ( int )powf( 2.0f, 11.0f - g_SkipMips );
        swprintf_s( str, MAX_PATH, L"Texture LOD: %d x %d", TextureSize, TextureSize );
        g_pTxtHelper->DrawTextLine( str );


        int NumVBs = g_pResourceReuseCache->GetNumVBs();
        int NumIBs = g_pResourceReuseCache->GetNumIBs();
        UINT NumUsedVBs = 0;
        UINT NumUsedIBs = 0;

        for( int i = 0; i < NumVBs; i++ )
        {
            DEVICE_VERTEX_BUFFER* vb = g_pResourceReuseCache->GetVB( i );
            if( vb->bInUse )
                NumUsedVBs ++;
        }
        for( int i = 0; i < NumIBs; i++ )
        {
            DEVICE_INDEX_BUFFER* ib = g_pResourceReuseCache->GetIB( i );
            if( ib->bInUse )
                NumUsedIBs ++;
        }

        swprintf_s( str, MAX_PATH, L"BufferCache: Total buffers: %d", NumVBs + NumIBs );
        g_pTxtHelper->DrawTextLine( str );
        swprintf_s( str, MAX_PATH, L"    VBs: %d", NumVBs );
        g_pTxtHelper->DrawTextLine( str );
        swprintf_s( str, MAX_PATH, L"    IBs: %d", NumIBs );
        g_pTxtHelper->DrawTextLine( str );
        swprintf_s( str, MAX_PATH, L"BufferCache: Used buffers: %d", NumUsedVBs + NumUsedIBs );
        g_pTxtHelper->DrawTextLine( str );
        swprintf_s( str, MAX_PATH, L"    VBs: %d", NumUsedVBs );
        g_pTxtHelper->DrawTextLine( str );
        swprintf_s( str, MAX_PATH, L"    IBs: %d", NumUsedIBs );
        g_pTxtHelper->DrawTextLine( str );

    }

    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Reject any D3D10 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite10 ) );
    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ContentStreaming.fx" ) );
    D3D10_SHADER_MACRO Defines[] =
    {
        { "D3D10", "TRUE" },
        { NULL, NULL }
    };
    V_RETURN( D3DX10CreateEffectFromFile( str, Defines, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect10, NULL, NULL ) );

    // Get the effect variable handles
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmWorldViewProjection = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pEyePt = g_pEffect10->GetVariableByName( "g_vEyePt" )->AsVector();
    g_ptxDiffuse = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_ptxNormal = g_pEffect10->GetVariableByName( "g_txNormal" )->AsShaderResource();

    g_pRenderTileDiff = g_pEffect10->GetTechniqueByName( "RenderTileDiff10" );
    g_pRenderTileBump = g_pEffect10->GetTechniqueByName( "RenderTileBump10" );
    g_pRenderTileWire = g_pEffect10->GetTechniqueByName( "RenderTileWire10" );

    // Create a layout for the object data.
    const D3D10_INPUT_ELEMENT_DESC layoutObject[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC PassDesc;
    g_pRenderTileBump->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layoutObject, ARRAYSIZE(layoutObject), PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pLayoutObject ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 2.0f, 0.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 2.0f, 1.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    // Create the async loader
    g_pAsyncLoader = new CAsyncLoader( g_NumProcessingThreads );
    if( !g_pAsyncLoader )
        return E_OUTOFMEMORY;

    // Create the texture reuse cache
    g_pResourceReuseCache = new CResourceReuseCache( pd3dDevice );
    if( !g_pResourceReuseCache )
        return E_OUTOFMEMORY;

    // Load resources if they haven't been already (coming from a device recreate)
    if( APP_STATE_RENDER_SCENE == g_AppState )
        LoadStartupResources( NULL, pd3dDevice, DXUTGetTime() );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    g_fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( DEG2RAD(70.0f), g_fAspectRatio, 0.1f, 1500.0f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 500 );
    g_SampleUI.SetSize( 170, 300 );
    g_StartUpUI.SetLocation( ( pBackBufferSurfaceDesc->Width - 300 ) / 2 - 40,
                             ( pBackBufferSurfaceDesc->Height - 200 ) / 2 );
    g_StartUpUI.SetSize( 300, 200 );

    // Get availble texture memory
    IDXGIDevice* pDXGIDevice = NULL;
    IDXGIAdapter* pDXGIAdapter = NULL;
    V_RETURN( pd3dDevice->QueryInterface( IID_IDXGIDevice, ( void** )&pDXGIDevice ) );
    V_RETURN( pDXGIDevice->GetAdapter( &pDXGIAdapter ) );
    DXGI_ADAPTER_DESC AdapterDesc;
    pDXGIAdapter->GetDesc( &AdapterDesc );
    if( AdapterDesc.DedicatedVideoMemory )
        g_AvailableVideoMem = AdapterDesc.DedicatedVideoMemory;
    else
        g_AvailableVideoMem = AdapterDesc.SharedSystemMemory;
    SAFE_RELEASE( pDXGIDevice );
    SAFE_RELEASE( pDXGIAdapter );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Render the scene using the programmable pipeline
//--------------------------------------------------------------------------------------
static int                          iFrameNum = 0;
void RenderScene( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXMATRIX mWorldViewProjection;

    // Get the projection & view matrix from the camera class
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    // Set the eye vector
    D3DXVECTOR3 vEyePt = *g_Camera.GetEyePt();
    D3DXVECTOR4 vEyePt4;
    vEyePt4.x = vEyePt.x;
    vEyePt4.y = vEyePt.y;
    vEyePt4.z = vEyePt.z;
    g_pEyePt->SetFloatVector( ( float* )&vEyePt4 );

    int NewTextureUploadsToVidMem = 0;
    if( iFrameNum % g_UploadToVRamEveryNthFrame > 0 )
        NewTextureUploadsToVidMem = g_NumResourceToLoadPerFrame;
    iFrameNum ++;

    // Render the level
    pd3dDevice->IASetInputLayout( g_pLayoutObject );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
    for( int i = 0; i < g_VisibleItemArray.GetSize(); i++ )
    {
        LEVEL_ITEM* pItem = g_VisibleItemArray.GetAt( i );

        D3DXMatrixIdentity( &mWorld );
        mWorldViewProjection = mWorld * mView * mProj;

        g_pmWorldViewProjection->SetMatrix( ( float* )&mWorldViewProjection );
        g_pmWorld->SetMatrix( ( float* )&mWorld );

        if( pItem->bLoaded )
        {
            UINT Stride = sizeof( TERRAIN_VERTEX );
            UINT Offset = 0;
            pd3dDevice->IASetVertexBuffers( 0, 1, &pItem->VB.pVB10, &Stride, &Offset );
            pd3dDevice->IASetIndexBuffer( pItem->IB.pIB10, DXGI_FORMAT_R16_UINT, 0 );

            bool bDiff = pItem->Diffuse.pRV10 ? true : false;
            if( bDiff && pItem->CurrentCountdownDiff > 0 )
            {
                bDiff = false;
                pItem->CurrentCountdownDiff --;
            }
            bool bNorm = pItem->Normal.pRV10 ? true : false;
            if( bNorm && pItem->CurrentCountdownNorm > 0 )
            {
                bNorm = false;
                pItem->CurrentCountdownNorm --;
            }

            bool bCanRenderDiff = bDiff;
            bool bCanRenderNorm = bDiff && bNorm && pItem->bHasBeenRenderedDiffuse;
            if( bDiff && !pItem->bHasBeenRenderedDiffuse )
            {
                if( NewTextureUploadsToVidMem >= g_NumResourceToLoadPerFrame )
                    bCanRenderDiff = false;
                else
                    NewTextureUploadsToVidMem ++;
            }
            if( bCanRenderDiff && bNorm && !pItem->bHasBeenRenderedNormal )
            {
                if( NewTextureUploadsToVidMem >= g_NumResourceToLoadPerFrame )
                    bCanRenderNorm = false;
                else
                    NewTextureUploadsToVidMem ++;
            }

            if( !bCanRenderDiff && !bCanRenderNorm )
                continue;

            // Render the scene with this technique 
            ID3D10EffectTechnique* pTechnique = NULL;
            if( g_bWireframe )
            {
                pTechnique = g_pRenderTileWire;
                g_ptxDiffuse->SetResource( pItem->Diffuse.pRV10 );
            }
            else if( bCanRenderNorm )
            {
                pItem->bHasBeenRenderedNormal = true;
                pTechnique = g_pRenderTileBump;
                g_ptxDiffuse->SetResource( pItem->Diffuse.pRV10 );
                g_ptxNormal->SetResource( pItem->Normal.pRV10 );
            }
            else if( bCanRenderDiff )
            {
                pItem->bHasBeenRenderedDiffuse = true;
                pTechnique = g_pRenderTileDiff;
                g_ptxDiffuse->SetResource( pItem->Diffuse.pRV10 );
            }

            // Apply the technique contained in the effect 
            D3D10_TECHNIQUE_DESC Desc;
            pTechnique->GetDesc( &Desc );

            for( UINT iPass = 0; iPass < Desc.Passes; iPass++ )
            {
                pTechnique->GetPassByIndex( iPass )->Apply( 0 );

                pd3dDevice->DrawIndexed( g_Terrain.GetNumIndices(), 0, 0 );
            }
        }
        else
        {
            // TODO: draw default object here
        }
    }

    g_ptxDiffuse->SetResource( NULL );
    g_ptxNormal->SetResource( NULL );
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr = S_OK;

    D3DXMATRIX mWorldViewProjection;
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;

    float ClearColor[4] = { 0.627f, 0.627f, 0.980f, 0.0f };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );

    // Clear the depth stencil
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Render the scene
    if( APP_STATE_RENDER_SCENE == g_AppState )
        RenderScene( pd3dDevice, fTime, fElapsedTime, pUserContext );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" ); // These events are to help PIX identify what the code is doing

    if( APP_STATE_STARTUP == g_AppState )
    {
        V( g_StartUpUI.OnRender( fElapsedTime ) );
        V( g_HUD.OnRender( fElapsedTime ) );
    }
    if( g_bDrawUI )
    {
        if( APP_STATE_RENDER_SCENE == g_AppState )
            V( g_SampleUI.OnRender( fElapsedTime ) );
        RenderText();
    }

    DXUT_EndPerfEvent();

    // Load in up to g_NumResourceToLoadPerFrame resources at the end of every frame
    if( LOAD_TYPE_MULTITHREAD == g_LoadType && APP_STATE_RENDER_SCENE == g_AppState )
    {
        UINT NumResToProcess = g_NumResourceToLoadPerFrame;
        g_pAsyncLoader->ProcessDeviceWorkItems( NumResToProcess );
    }
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_SettingsDlg.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pLayoutObject );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );

    DestroyAllMeshes( LDT_D3D10 );

    SAFE_DELETE( g_pAsyncLoader );
    SAFE_DELETE( g_pResourceReuseCache );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    if( pDeviceSettings->ver == DXUT_D3D9_DEVICE )
    {
        IDirect3D9* pD3D = DXUTGetD3D9Object();
        D3DCAPS9 Caps;
        pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType, &Caps );

        // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
        // then switch to SWVP.
        if( ( Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
            Caps.VertexShaderVersion < D3DVS_VERSION( 1, 1 ) )
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
    }

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D10_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE ) )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    if( APP_STATE_RENDER_SCENE == g_AppState )
    {
        // Update the camera's position based on user input 
        g_Camera.FrameMove( fElapsedTime );

        // Keep us close to the terrain
        D3DXVECTOR3 vEye = *g_Camera.GetEyePt();
        D3DXVECTOR3 vAt = *g_Camera.GetLookAtPt();
        D3DXVECTOR3 vDir = vAt - vEye;
        float fHeight = g_Terrain.GetHeightOnMap( &vEye );
        vEye.y = fHeight + g_fViewHeight;
        vAt = vEye + vDir;
        g_Camera.SetViewParams( &vEye, &vAt );

        IDirect3DDevice9* pDev9 = NULL;
        ID3D10Device* pDev10 = NULL;

        if( DXUTIsAppRenderingWithD3D9() )
            pDev9 = DXUTGetD3D9Device();
        else
            pDev10 = DXUTGetD3D10Device();

        // Find visible sets
        CalculateVisibleItems( vEye, g_fVisibleRadius, g_fLoadingRadius );

        // Ensure resources within a certian radius are loaded
        EnsureResourcesLoaded( pDev9, pDev10, g_fVisibleRadius, g_fLoadingRadius );

        // Never unload when using WDDM paging
        if( !g_bUseWDDMPaging )
            EnsureUnusedResourcesUnloaded( pDev9, pDev10, fTime );

        CheckForLoadDone( pDev9, pDev10 );
    }
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    if( APP_STATE_RENDER_SCENE == g_AppState )
    {
        *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
        if( *pbNoFurtherProcessing )
            return 0;
        // Pass all remaining windows messages to camera so it can respond to user input
        g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );
    }
    else if( APP_STATE_STARTUP == g_AppState )
    {
        *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
        if( *pbNoFurtherProcessing )
            return 0;
        *pbNoFurtherProcessing = g_StartUpUI.MsgProc( hWnd, uMsg, wParam, lParam );
        if( *pbNoFurtherProcessing )
            return 0;
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    switch( nChar )
    {
        case VK_F1:
            if( !bKeyDown )
                g_bDrawUI = !g_bDrawUI;
            break;
    }
}


//--------------------------------------------------------------------------------------
void DestroyAllMeshes( LOADER_DEVICE_TYPE ldt )
{
    // Wait for everything to load
    if( LOAD_TYPE_MULTITHREAD == g_LoadType )
        g_pAsyncLoader->WaitForAllItems();

    g_pResourceReuseCache->OnDestroy();

    // Destroy the level-item array
    for( int i = 0; i < g_LevelItemArray.GetSize(); i++ )
    {
        LEVEL_ITEM* pItem = g_LevelItemArray.GetAt( i );
        SAFE_DELETE( pItem );
    }
    g_LevelItemArray.RemoveAll();
    g_VisibleItemArray.RemoveAll();
    g_LoadedItemArray.RemoveAll();
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR str[MAX_PATH] = {0};

    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;

            // StartupUI
        case IDC_ONDEMANDSINGLETHREAD:
        {
            g_LoadType = LOAD_TYPE_SINGLETHREAD;
            g_bUseWDDMPaging = false;
        }
            break;
        case IDC_ONDEMANDMULTITHREAD:
        {
            g_LoadType = LOAD_TYPE_MULTITHREAD;
            g_bUseWDDMPaging = false;
        }
            break;
        case IDC_RUN:
        {
            if( DXUTIsAppRenderingWithD3D9() )
                LoadStartupResources( DXUTGetD3D9Device(), NULL, DXUTGetTime() );
            else if( DXUTIsAppRenderingWithD3D10() )
                LoadStartupResources( NULL, DXUTGetD3D10Device(), DXUTGetTime() );

            g_AppState = APP_STATE_RENDER_SCENE;
            break;
        }

            // SampleUI
        case IDC_VIEWHEIGHT:
        {
            g_fViewHeight = ( float )g_SampleUI.GetSlider( IDC_VIEWHEIGHT )->GetValue() / 100.0f;

            swprintf_s( str, MAX_PATH, L"View Height: %.02f", g_fViewHeight );
            g_SampleUI.GetStatic( IDC_VIEWHEIGHT_STATIC )->SetText( str );
            break;
        }
        case IDC_VISIBLERADIUS:
        {
            g_fVisibleRadius = ( float )g_SampleUI.GetSlider( IDC_VISIBLERADIUS )->GetValue() / 100.0f;
            g_fLoadingRadius = g_fVisibleRadius;
            swprintf_s( str, MAX_PATH, L"Visible Radius: %.02f", g_fVisibleRadius );
            g_SampleUI.GetStatic( IDC_VISIBLERADIUS_STATIC )->SetText( str );
            break;
        }
        case IDC_NUMPERFRAMERES:
        {
            g_NumResourceToLoadPerFrame = g_SampleUI.GetSlider( IDC_NUMPERFRAMERES )->GetValue();

            swprintf_s( str, MAX_PATH, L"Create up to\n%d Items Per-Frame", g_NumResourceToLoadPerFrame );
            g_SampleUI.GetStatic( IDC_NUMPERFRAMERES_STATIC )->SetText( str );
            break;
        }
        case IDC_UPLOADTOVRAMFREQ:
        {
            g_UploadToVRamEveryNthFrame = g_SampleUI.GetSlider( IDC_UPLOADTOVRAMFREQ )->GetValue();

            swprintf_s( str, MAX_PATH, L"Upload to VRam every\n%d frames", g_UploadToVRamEveryNthFrame );
            g_SampleUI.GetStatic( IDC_UPLOADTOVRAMFREQ_STATIC )->SetText( str );
            break;
        }
        case IDC_WIREFRAME:
        {
            g_bWireframe = !g_bWireframe;
            break;
        }
        case IDC_STARTOVER:
        {
            g_AppState = APP_STATE_STARTUP;
            g_bStartupResourcesLoaded = false;

            // Clear state
            if( DXUTIsAppRenderingWithD3D10() )
            {
                ClearD3D10State();
                DestroyAllMeshes( LDT_D3D10 );
            }
            else
            {
                DestroyAllMeshes( LDT_D3D9 );
            }

            // Clear out any old packed file we may have
            g_PackFile.UnloadPackedFile();

            break;
        }
        case IDC_DELETE_PACK_FILE:
        {
            WCHAR strPath[MAX_PATH] = {0};
            GetPackedFilePath( strPath, MAX_PATH );
            wcscat_s( strPath, MAX_PATH, g_strFile );
            if( 0xFFFFFFFF == GetFileAttributes( strPath ) )
            {
                MessageBox( NULL, L"No PackFile exists.", L"Error", MB_OK );
            }
            else
            {
                if( IDYES == MessageBox( NULL,
                                         L"You are about to delete the packfile.  Content Streaming will need to recreate this file the next time it runs.", L"Warning", MB_YESNO ) )
                {
                    DeleteFile( strPath );
                }
            }
            break;
        }
    }
}

//--------------------------------------------------------------------------------------
// Serial create texture
//--------------------------------------------------------------------------------------
void	CALLBACK CreateTextureFromFile10_Serial( ID3D10Device* pDev, WCHAR* szFileName,
                                                 ID3D10ShaderResourceView** ppRV, void* pContext )
{
    CTextureLoader* pLoader = new CTextureLoader( szFileName, &g_PackFile );
    CTextureProcessor* pProcessor = new CTextureProcessor( pDev, ppRV, g_pResourceReuseCache, g_SkipMips );

    void* pLocalData;
    SIZE_T Bytes;
    if( FAILED( pLoader->Load() ) )
        goto Error;
    if( FAILED( pLoader->Decompress( &pLocalData, &Bytes ) ) )
        goto Error;
    if( FAILED( pProcessor->Process( pLocalData, Bytes ) ) )
        goto Error;
    if( FAILED( pProcessor->LockDeviceObject() ) )
        goto Error;
    if( FAILED( pProcessor->CopyToResource() ) )
        goto Error;
    if( FAILED( pProcessor->UnLockDeviceObject() ) )
        goto Error;

    goto Finish;

Error:
    pProcessor->SetResourceError();
Finish:
    pProcessor->Destroy();
    pLoader->Destroy();
    SAFE_DELETE( pLoader );
    SAFE_DELETE( pProcessor );
}

//--------------------------------------------------------------------------------------
// Serial create buffer
//--------------------------------------------------------------------------------------
void	CALLBACK CreateVertexBuffer10_Serial( ID3D10Device* pDev, ID3D10Buffer** ppBuffer,
                                              D3D10_BUFFER_DESC BufferDesc, void* pData, void* pContext )
{
    CVertexBufferLoader* pLoader = new CVertexBufferLoader();
    CVertexBufferProcessor* pProcessor = new CVertexBufferProcessor( pDev, ppBuffer, &BufferDesc, pData,
                                                                     g_pResourceReuseCache );

    void* pLocalData;
    SIZE_T Bytes;
    if( FAILED( pLoader->Load() ) )
        goto Error;
    if( FAILED( pLoader->Decompress( &pLocalData, &Bytes ) ) )
        goto Error;
    if( FAILED( pProcessor->Process( pLocalData, Bytes ) ) )
        goto Error;
    if( FAILED( pProcessor->LockDeviceObject() ) )
        goto Error;
    if( FAILED( pProcessor->CopyToResource() ) )
        goto Error;
    if( FAILED( pProcessor->UnLockDeviceObject() ) )
        goto Error;

    goto Finish;

Error:
    pProcessor->SetResourceError();
Finish:
    pProcessor->Destroy();
    pLoader->Destroy();
    SAFE_DELETE( pLoader );
    SAFE_DELETE( pProcessor );
}

//--------------------------------------------------------------------------------------
// Serial create buffer
//--------------------------------------------------------------------------------------
void	CALLBACK CreateIndexBuffer10_Serial( ID3D10Device* pDev, ID3D10Buffer** ppBuffer, D3D10_BUFFER_DESC BufferDesc,
                                             void* pData, void* pContext )
{
    CIndexBufferLoader* pLoader = new CIndexBufferLoader();
    CIndexBufferProcessor* pProcessor = new CIndexBufferProcessor( pDev, ppBuffer, &BufferDesc, pData,
                                                                   g_pResourceReuseCache );

    void* pLocalData;
    SIZE_T Bytes;
    if( FAILED( pLoader->Load() ) )
        goto Error;
    if( FAILED( pLoader->Decompress( &pLocalData, &Bytes ) ) )
        goto Error;
    if( FAILED( pProcessor->Process( pLocalData, Bytes ) ) )
        goto Error;
    if( FAILED( pProcessor->LockDeviceObject() ) )
        goto Error;
    if( FAILED( pProcessor->CopyToResource() ) )
        goto Error;
    if( FAILED( pProcessor->UnLockDeviceObject() ) )
        goto Error;

    goto Finish;

Error:
    pProcessor->SetResourceError();
Finish:
    pProcessor->Destroy();
    pLoader->Destroy();
    SAFE_DELETE( pLoader );
    SAFE_DELETE( pProcessor );
}

//--------------------------------------------------------------------------------------
// Async create texture
//--------------------------------------------------------------------------------------
void	CALLBACK CreateTextureFromFile10_Async( ID3D10Device* pDev, WCHAR* szFileName, ID3D10ShaderResourceView** ppRV,
                                                void* pContext )
{
    CAsyncLoader* pAsyncLoader = ( CAsyncLoader* )pContext;
    if( pAsyncLoader )
    {
        CTextureLoader* pLoader = new CTextureLoader( szFileName, &g_PackFile );
        CTextureProcessor* pProcessor = new CTextureProcessor( pDev, ppRV, g_pResourceReuseCache, g_SkipMips );

        pAsyncLoader->AddWorkItem( pLoader, pProcessor, NULL, ( void** )ppRV );
    }
}

//--------------------------------------------------------------------------------------
// Async create buffer
//--------------------------------------------------------------------------------------
void	CALLBACK CreateVertexBuffer10_Async( ID3D10Device* pDev, ID3D10Buffer** ppBuffer, D3D10_BUFFER_DESC BufferDesc,
                                             void* pData, void* pContext )
{
    CAsyncLoader* pAsyncLoader = ( CAsyncLoader* )pContext;
    if( pAsyncLoader )
    {
        CVertexBufferLoader* pLoader = new CVertexBufferLoader();
        CVertexBufferProcessor* pProcessor = new CVertexBufferProcessor( pDev, ppBuffer, &BufferDesc, pData,
                                                                         g_pResourceReuseCache );

        pAsyncLoader->AddWorkItem( pLoader, pProcessor, NULL, ( void** )ppBuffer );
    }
}

//--------------------------------------------------------------------------------------
// Async create buffer
//--------------------------------------------------------------------------------------
void	CALLBACK CreateIndexBuffer10_Async( ID3D10Device* pDev, ID3D10Buffer** ppBuffer, D3D10_BUFFER_DESC BufferDesc,
                                            void* pData, void* pContext )
{
    CAsyncLoader* pAsyncLoader = ( CAsyncLoader* )pContext;
    if( pAsyncLoader )
    {
        CIndexBufferLoader* pLoader = new CIndexBufferLoader();
        CIndexBufferProcessor* pProcessor = new CIndexBufferProcessor( pDev, ppBuffer, &BufferDesc, pData,
                                                                       g_pResourceReuseCache );

        pAsyncLoader->AddWorkItem( pLoader, pProcessor, NULL, ( void** )ppBuffer );
    }
}


//-------------------------------------------------------------------------------------
void ClearD3D10State()
{
    OnD3D10ReleasingSwapChain( NULL );
    OnD3D10DestroyDevice( NULL );
    ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
    OnD3D10CreateDevice( pd3dDevice, DXUTGetDXGIBackBufferSurfaceDesc(), NULL );
    OnD3D10ResizedSwapChain( pd3dDevice, DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), NULL );
}
