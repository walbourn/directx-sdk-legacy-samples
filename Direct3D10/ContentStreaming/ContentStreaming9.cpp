//--------------------------------------------------------------------------------------
// File: ContentStreaming9.cpp
//
// Illustrates streaming content using Direct3D 9
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
#include "ResourceReuseCache.h"
#include "PackedFile.h"
#include "Terrain.h"
#include <oleauto.h>
#include <wbemidl.h>

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------
#define DEG2RAD(p) ( D3DX_PI*(p/180.0f) )

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
extern CFirstPersonCamera           g_Camera;               // A first person camera
extern CDXUTDialogResourceManager   g_DialogResourceManager; // manager for shared resources of dialogs
extern CD3DSettingsDlg              g_SettingsDlg;          // Device settings dialog
extern CDXUTTextHelper*             g_pTxtHelper;
extern CDXUTDialog                  g_HUD;                  // dialog for standard controls
extern CDXUTDialog                  g_SampleUI;             // dialog for sample specific controls
extern CDXUTDialog                  g_StartUpUI;
extern CAsyncLoader*                g_pAsyncLoader;
extern CResourceReuseCache*         g_pResourceReuseCache;
extern UINT                         g_NumProcessingThreads;
extern UINT                         g_MaxOutstandingResources;
extern CResourceReuseCache*         g_pResourceReuseCache;
extern CPackedFile                  g_PackFile;

// Direct3D 9 resources
ID3DXFont*                          g_pFont9 = NULL;
ID3DXSprite*                        g_pSprite9 = NULL;
ID3DXEffect*                        g_pEffect9 = NULL;
IDirect3DVertexDeclaration9*        g_pDeclTile = NULL;

// Effect variable handles
D3DXHANDLE                          g_hTimeShift = 0;
D3DXHANDLE                          g_hmWorld = 0;
D3DXHANDLE                          g_hmWorldViewProjection = 0;
D3DXHANDLE                          g_hEyePt = 0;
D3DXHANDLE                          g_htxDiffuse = 0;
D3DXHANDLE                          g_htxNormal = 0;
D3DXHANDLE                          g_hRenderTileDiff = 0;
D3DXHANDLE                          g_hRenderTileBump = 0;
D3DXHANDLE                          g_hRenderTileWire = 0;

extern CGrowableArray <LEVEL_ITEM*> g_VisibleItemArray;
extern CTerrain                     g_Terrain;

extern float                        g_fFOV;
extern float                        g_fAspectRatio;
extern float                        g_fInitialLoadTime;
extern float                        g_LoadingRadius;
extern int                          g_NumResourceToLoadPerFrame;
extern int                          g_UploadToVRamEveryNthFrame;
extern UINT                         g_SkipMips;
extern UINT                         g_NumMeshPlacements;
extern UINT                         g_NumLevelPieces;
extern UINT                         g_NumLevelItems;
extern bool                         g_bStartupResourcesLoaded;
extern bool                         g_bDrawUI;
extern bool                         g_bWireframe;
extern UINT64                       g_AvailableVideoMem;

enum LOAD_TYPE
{
    LOAD_TYPE_MULTITHREAD = 0,
    LOAD_TYPE_SINGLETHREAD,
};
extern LOAD_TYPE                    g_LoadType;

enum APP_STATE
{
    APP_STATE_STARTUP = 0,
    APP_STATE_RENDER_SCENE
};
extern APP_STATE                    g_AppState;

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                      bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                    void* pUserContext );
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D9LostDevice( void* pUserContext );
void CALLBACK OnD3D9DestroyDevice( void* pUserContext );

void CALLBACK CreateTextureFromFile9_Serial( IDirect3DDevice9* pDev, WCHAR* szFileName, IDirect3DTexture9** ppTexture,
                                             void* pContext );
void CALLBACK CreateVertexBuffer9_Serial( IDirect3DDevice9* pDev, IDirect3DVertexBuffer9** ppBuffer, UINT iSizeBytes,
                                          DWORD Usage, DWORD FVF, D3DPOOL Pool, void* pData, void* pContext );
void CALLBACK CreateIndexBuffer9_Serial( IDirect3DDevice9* pDev, IDirect3DIndexBuffer9** ppBuffer, UINT iSizeBytes,
                                         DWORD Usage, D3DFORMAT ibFormat, D3DPOOL Pool, void* pData, void* pContext );
void CALLBACK CreateTextureFromFile9_Async( IDirect3DDevice9* pDev, WCHAR* szFileName, IDirect3DTexture9** ppTexture,
                                            void* pContext );
void CALLBACK CreateVertexBuffer9_Async( IDirect3DDevice9* pDev, IDirect3DVertexBuffer9** ppBuffer, UINT iSizeBytes,
                                         DWORD Usage, DWORD FVF, D3DPOOL Pool, void* pData, void* pContext );
void CALLBACK CreateIndexBuffer9_Async( IDirect3DDevice9* pDev, IDirect3DIndexBuffer9** ppBuffer, UINT iSizeBytes,
                                        DWORD Usage, D3DFORMAT ibFormat, D3DPOOL Pool, void* pData, void* pContext );

extern void LoadStartupResources( IDirect3DDevice9* pDev9, ID3D10Device* pDev10, double fTime );
extern void RenderText();
extern void DestroyAllMeshes( LOADER_DEVICE_TYPE ldt );

//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
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
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont9 ) );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ContentStreaming.fx" ) );
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, D3DXFX_NOT_CLONEABLE | D3DXFX_LARGEADDRESSAWARE,
                                        NULL, &g_pEffect9, NULL ) );

    // Get the effect variable handles
    g_hmWorld = g_pEffect9->GetParameterByName( NULL, "g_mWorld" );
    g_hmWorldViewProjection = g_pEffect9->GetParameterByName( NULL, "g_mWorldViewProj" );
    g_hEyePt = g_pEffect9->GetParameterByName( NULL, "g_vEyePt" );
    g_htxDiffuse = g_pEffect9->GetParameterByName( NULL, "g_txDiffuse" );
    g_htxNormal = g_pEffect9->GetParameterByName( NULL, "g_txNormal" );

    g_hRenderTileDiff = g_pEffect9->GetTechniqueByName( "RenderTileDiff" );
    g_hRenderTileBump = g_pEffect9->GetTechniqueByName( "RenderTileBump" );
    g_hRenderTileWire = g_pEffect9->GetTechniqueByName( "RenderTileWire" );

    // Create a decl for the object data.
    D3DVERTEXELEMENT9 declDesc[] =
    {
        {0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
        {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        {0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
    };
    pd3dDevice->CreateVertexDeclaration( declDesc, &g_pDeclTile );

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

    return S_OK;
}


typedef BOOL ( WINAPI* PfnCoSetProxyBlanket )(
IUnknown* pProxy,
DWORD dwAuthnSvc,
DWORD dwAuthzSvc,
OLECHAR* pServerPrincName,
DWORD dwAuthnLevel,
DWORD dwImpLevel,
RPC_AUTH_IDENTITY_HANDLE pAuthInfo,
DWORD dwCapabilities );

UINT32 GetAdapterRAMFromWMI( IDirect3DDevice9* pd3dDevice )
{
    HRESULT hr;
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    BSTR pNamespace = NULL;

    UINT32 dwAdapterRAM = 0;
    bool bFoundViaWMI = false;

    CoInitialize( 0 );

    hr = CoCreateInstance( CLSID_WbemLocator,
                           NULL,
                           CLSCTX_INPROC_SERVER,
                           IID_IWbemLocator,
                           ( LPVOID* )&pIWbemLocator );

    if( SUCCEEDED( hr ) && pIWbemLocator )
    {
        // Using the locator, connect to WMI in the given namespace.
        pNamespace = SysAllocString( L"\\\\.\\root\\cimv2" );

        hr = pIWbemLocator->ConnectServer( pNamespace, NULL, NULL, 0L,
                                           0L, NULL, NULL, &pIWbemServices );
        if( SUCCEEDED( hr ) && pIWbemServices != NULL )
        {
            HINSTANCE hinstOle32 = NULL;

            hinstOle32 = LoadLibraryW( L"ole32.dll" );
            if( hinstOle32 )
            {
                PfnCoSetProxyBlanket pfnCoSetProxyBlanket = NULL;

                pfnCoSetProxyBlanket = ( PfnCoSetProxyBlanket )GetProcAddress( hinstOle32, "CoSetProxyBlanket" );
                if( pfnCoSetProxyBlanket != NULL )
                {
                    // Switch security level to IMPERSONATE. 
                    pfnCoSetProxyBlanket( pIWbemServices,               // proxy
                                          RPC_C_AUTHN_WINNT,            // authentication service
                                          RPC_C_AUTHZ_NONE,             // authorization service
                                          NULL,                         // server principle name
                                          RPC_C_AUTHN_LEVEL_CALL,       // authentication level
                                          RPC_C_IMP_LEVEL_IMPERSONATE,  // impersonation level
                                          NULL,                         // identity of the client
                                          EOAC_NONE );                  // capability flags
                }

                FreeLibrary( hinstOle32 );
            }

            IEnumWbemClassObject* pEnumProcessorDevs = NULL;
            BSTR pClassName = NULL;

            pClassName = SysAllocString( L"Win32_VideoController" );

            hr = pIWbemServices->CreateInstanceEnum( pClassName,
                                                     0,
                                                     NULL, &pEnumProcessorDevs );

            if( SUCCEEDED( hr ) && pEnumProcessorDevs )
            {
                IWbemClassObject* pProcessorDev = NULL;
                DWORD uReturned = 0;
                BSTR pPropName = NULL;

                // Get the first one in the list
                pEnumProcessorDevs->Reset();
                hr = pEnumProcessorDevs->Next( 5000,             // timeout in 5 seconds
                                               1,                // return just one storage device
                                               &pProcessorDev,   // pointer to storage device
                                               &uReturned );     // number obtained: one or zero

                if( SUCCEEDED( hr ) && uReturned != 0 && pProcessorDev != NULL )
                {
                    pPropName = SysAllocString( L"AdapterRAM" );

                    VARIANT var;
                    hr = pProcessorDev->Get( pPropName, 0L, &var, NULL, NULL );
                    if( SUCCEEDED( hr ) )
                    {
                        dwAdapterRAM = var.lVal;
                        bFoundViaWMI = true;
                    }

                    if( pPropName )
                        SysFreeString( pPropName );
                }

                SAFE_RELEASE( pProcessorDev );
            }

            if( pClassName )
                SysFreeString( pClassName );
            SAFE_RELEASE( pEnumProcessorDevs );
        }

        if( pNamespace )
            SysFreeString( pNamespace );
        SAFE_RELEASE( pIWbemServices );
    }

    SAFE_RELEASE( pIWbemLocator );

    if( bFoundViaWMI )
        return dwAdapterRAM;

    return pd3dDevice->GetAvailableTextureMem();
}




//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice,
                                    const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont9 ) V_RETURN( g_pFont9->OnResetDevice() );
    if( g_pEffect9 ) V_RETURN( g_pEffect9->OnResetDevice() );

    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite9 ) );
    g_pTxtHelper = new CDXUTTextHelper( g_pFont9, g_pSprite9, NULL, NULL, 15 );

    // Setup the camera's projection parameters
    g_fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( DEG2RAD(g_fFOV), g_fAspectRatio, 0.1f, 1500.0f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 500 );
    g_SampleUI.SetSize( 170, 300 );
    g_StartUpUI.SetLocation( ( pBackBufferSurfaceDesc->Width - 300 ) / 2 - 40,
                             ( pBackBufferSurfaceDesc->Height - 200 ) / 2 );
    g_StartUpUI.SetSize( 300, 200 );

    // Get availble texture memory
    g_AvailableVideoMem = GetAdapterRAMFromWMI( pd3dDevice );

    // Load resources if they haven't been already (for a device reset)
    if( APP_STATE_RENDER_SCENE == g_AppState )
        LoadStartupResources( pd3dDevice, NULL, DXUTGetTime() );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Render the scene using the programmable pipeline
//--------------------------------------------------------------------------------------
static int                          iFrameNum = 0;
void RenderScene( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr;
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
    V( g_pEffect9->SetVector( g_hEyePt, &vEyePt4 ) );

    int NewTextureUploadsToVidMem = 0;
    if( iFrameNum % g_UploadToVRamEveryNthFrame > 0 )
        NewTextureUploadsToVidMem = g_NumResourceToLoadPerFrame;
    iFrameNum ++;

    // Render the level
    pd3dDevice->SetVertexDeclaration( g_pDeclTile );
    for( int i = 0; i < g_VisibleItemArray.GetSize(); i++ )
    {
        LEVEL_ITEM* pItem = g_VisibleItemArray.GetAt( i );

        D3DXMatrixIdentity( &mWorld );
        mWorldViewProjection = mWorld * mView * mProj;

        V( g_pEffect9->SetMatrix( g_hmWorldViewProjection, &mWorldViewProjection ) );
        V( g_pEffect9->SetMatrix( g_hmWorld, &mWorld ) );

        if( pItem->bLoaded )
        {
            pd3dDevice->SetStreamSource( 0, pItem->VB.pVB9, 0, sizeof( TERRAIN_VERTEX ) );
            pd3dDevice->SetIndices( pItem->IB.pIB9 );

            bool bDiff = pItem->Diffuse.pTexture9 ? true : false;
            if( bDiff && pItem->CurrentCountdownDiff > 0 )
            {
                bDiff = false;
                pItem->CurrentCountdownDiff --;
            }
            bool bNorm = pItem->Normal.pTexture9 ? true : false;
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
            if( g_bWireframe )
            {
                g_pEffect9->SetTechnique( g_hRenderTileWire );
                g_pEffect9->SetTexture( g_htxDiffuse, pItem->Diffuse.pTexture9 );
            }
            else if( bCanRenderNorm )
            {
                pItem->bHasBeenRenderedNormal = true;
                g_pEffect9->SetTechnique( g_hRenderTileBump );
                g_pEffect9->SetTexture( g_htxDiffuse, pItem->Diffuse.pTexture9 );
                g_pEffect9->SetTexture( g_htxNormal, pItem->Normal.pTexture9 );
            }
            else if( bCanRenderDiff )
            {
                pItem->bHasBeenRenderedDiffuse = true;
                g_pEffect9->SetTechnique( g_hRenderTileDiff );
                g_pEffect9->SetTexture( g_htxDiffuse, pItem->Diffuse.pTexture9 );
            }

            // Apply the technique contained in the effect 
            UINT cPasses = 0;
            V( g_pEffect9->Begin( &cPasses, 0 ) );

            for( UINT iPass = 0; iPass < cPasses; iPass++ )
            {
                V( g_pEffect9->BeginPass( iPass ) );

                pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 0, g_Terrain.GetNumTileVertices(), 0,
                                                  g_Terrain.GetNumIndices() - 2 );

                V( g_pEffect9->EndPass() );
            }
            V( g_pEffect9->End() );
        }
        else
        {
            // TODO: draw default object here
        }
    }

    g_pEffect9->SetTechnique( NULL );
    g_pEffect9->SetTexture( g_htxDiffuse, NULL );
    g_pEffect9->SetTexture( g_htxNormal, NULL );
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr = S_OK;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 160, 160, 250 ), 1.0f, 0 ) );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
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

        V( pd3dDevice->EndScene() );
    }

    // Load in up to g_NumResourceToLoadPerFrame resources at the end of every frame
    if( LOAD_TYPE_MULTITHREAD == g_LoadType && APP_STATE_RENDER_SCENE == g_AppState )
    {
        UINT NumResToProcess = g_NumResourceToLoadPerFrame;
        g_pAsyncLoader->ProcessDeviceWorkItems( NumResToProcess );
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9LostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    if( g_pFont9 ) g_pFont9->OnLostDevice();
    if( g_pEffect9 ) g_pEffect9->OnLostDevice();
    SAFE_RELEASE( g_pSprite9 );
    SAFE_DELETE( g_pTxtHelper );

    DestroyAllMeshes( LDT_D3D9 );

    g_bStartupResourcesLoaded = false;
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pEffect9 );
    SAFE_RELEASE( g_pFont9 );
    SAFE_RELEASE( g_pDeclTile );
    SAFE_DELETE( g_pAsyncLoader );

    g_pResourceReuseCache->OnDestroy();
    SAFE_DELETE( g_pResourceReuseCache );
}

//--------------------------------------------------------------------------------------
// Serial create texture
//--------------------------------------------------------------------------------------
void	CALLBACK CreateTextureFromFile9_Serial( IDirect3DDevice9* pDev, WCHAR* szFileName,
                                                IDirect3DTexture9** ppTexture, void* pContext )
{
    CTextureLoader* pLoader = new CTextureLoader( szFileName, &g_PackFile );
    CTextureProcessor* pProcessor = new CTextureProcessor( pDev, ppTexture, g_pResourceReuseCache, g_SkipMips );

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
// Serial create VB
//--------------------------------------------------------------------------------------
void	CALLBACK CreateVertexBuffer9_Serial( IDirect3DDevice9* pDev, IDirect3DVertexBuffer9** ppBuffer,
                                             UINT iSizeBytes, DWORD Usage, DWORD FVF, D3DPOOL Pool, void* pData,
                                             void* pContext )
{
    CVertexBufferLoader* pLoader = new CVertexBufferLoader();
    CVertexBufferProcessor* pProcessor = new CVertexBufferProcessor( pDev, ppBuffer, iSizeBytes, Usage, FVF, Pool,
                                                                     pData, g_pResourceReuseCache );

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
// Serial create IB
//--------------------------------------------------------------------------------------
void	CALLBACK CreateIndexBuffer9_Serial( IDirect3DDevice9* pDev, IDirect3DIndexBuffer9** ppBuffer, UINT iSizeBytes,
                                            DWORD Usage, D3DFORMAT ibFormat, D3DPOOL Pool, void* pData,
                                            void* pContext )
{
    CIndexBufferLoader* pLoader = new CIndexBufferLoader();
    CIndexBufferProcessor* pProcessor = new CIndexBufferProcessor( pDev, ppBuffer, iSizeBytes, Usage, ibFormat, Pool,
                                                                   pData, g_pResourceReuseCache );

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
void	CALLBACK CreateTextureFromFile9_Async( IDirect3DDevice9* pDev, WCHAR* szFileName,
                                               IDirect3DTexture9** ppTexture, void* pContext )
{
    CAsyncLoader* pAsyncLoader = ( CAsyncLoader* )pContext;
    if( pAsyncLoader )
    {
        CTextureLoader* pLoader = new CTextureLoader( szFileName, &g_PackFile );
        CTextureProcessor* pProcessor = new CTextureProcessor( pDev, ppTexture, g_pResourceReuseCache, g_SkipMips );

        pAsyncLoader->AddWorkItem( pLoader, pProcessor, NULL, ( void** )ppTexture );
    }
}

//--------------------------------------------------------------------------------------
// Async create VB
//--------------------------------------------------------------------------------------
void	CALLBACK CreateVertexBuffer9_Async( IDirect3DDevice9* pDev, IDirect3DVertexBuffer9** ppBuffer, UINT iSizeBytes,
                                            DWORD Usage, DWORD FVF, D3DPOOL Pool, void* pData, void* pContext )
{
    CAsyncLoader* pAsyncLoader = ( CAsyncLoader* )pContext;
    if( pAsyncLoader )
    {
        CVertexBufferLoader* pLoader = new CVertexBufferLoader();
        CVertexBufferProcessor* pProcessor = new CVertexBufferProcessor( pDev, ppBuffer, iSizeBytes, Usage, FVF, Pool,
                                                                         pData, g_pResourceReuseCache );

        pAsyncLoader->AddWorkItem( pLoader, pProcessor, NULL, ( void** )ppBuffer );
    }
}

//--------------------------------------------------------------------------------------
// Async create IB
//--------------------------------------------------------------------------------------
void	CALLBACK CreateIndexBuffer9_Async( IDirect3DDevice9* pDev, IDirect3DIndexBuffer9** ppBuffer, UINT iSizeBytes,
                                           DWORD Usage, D3DFORMAT ibFormat, D3DPOOL Pool, void* pData, void* pContext )
{
    CAsyncLoader* pAsyncLoader = ( CAsyncLoader* )pContext;
    if( pAsyncLoader )
    {
        CIndexBufferLoader* pLoader = new CIndexBufferLoader();
        CIndexBufferProcessor* pProcessor = new CIndexBufferProcessor( pDev, ppBuffer, iSizeBytes, Usage, ibFormat,
                                                                       Pool, pData, g_pResourceReuseCache );

        pAsyncLoader->AddWorkItem( pLoader, pProcessor, NULL, ( void** )ppBuffer );
    }
}
