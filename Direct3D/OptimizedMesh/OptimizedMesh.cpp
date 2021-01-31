//--------------------------------------------------------------------------------------
// File: OptimizedMesh.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 

#define MESHFILENAME L"misc\\knot.x"


struct SStripData
{
    LPDIRECT3DINDEXBUFFER9 m_pStrips;          // strip indices (single strip)
    LPDIRECT3DINDEXBUFFER9 m_pStripsMany;      // strip indices (many strips)

    DWORD m_cStripIndices;
    DWORD* m_rgcStripLengths;
    DWORD m_cStrips;

    SStripData() : m_pStrips( NULL ),
                   m_pStripsMany( NULL ),
                   m_cStripIndices( 0 ),
                   m_rgcStripLengths( NULL )
    {
    }
};


struct SMeshData
{
    LPD3DXMESH m_pMeshSysMem;      // System memory copy of mesh

    LPD3DXMESH m_pMesh;            // Local version of mesh, copied on resize
    LPDIRECT3DVERTEXBUFFER9 m_pVertexBuffer;    // vertex buffer of mesh

    SStripData* m_rgStripData;      // strip indices split by attribute
    DWORD m_cStripDatas;

            SMeshData() : m_pMeshSysMem( NULL ),
                          m_pMesh( NULL ),
                          m_pVertexBuffer( NULL ),
                          m_rgStripData( NULL ),
                          m_cStripDatas( 0 )
            {
            }

    void    ReleaseLocalMeshes()
    {
        SAFE_RELEASE( m_pMesh );
        SAFE_RELEASE( m_pVertexBuffer );
    }

    void    ReleaseAll()
    {
        SAFE_RELEASE( m_pMeshSysMem );
        SAFE_RELEASE( m_pMesh );
        SAFE_RELEASE( m_pVertexBuffer );

        for( DWORD iStripData = 0; iStripData < m_cStripDatas; iStripData++ )
        {
            SAFE_RELEASE( m_rgStripData[iStripData].m_pStrips );
            SAFE_RELEASE( m_rgStripData[iStripData].m_pStripsMany );
            delete[] m_rgStripData[iStripData].m_rgcStripLengths;
        }

        delete[] m_rgStripData;
        m_rgStripData = NULL;
        m_cStripDatas = 0;
    }
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                  g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                g_pTextSprite = NULL;   // Sprite for batching draw text calls
ID3DXEffect*                g_pEffect = NULL;       // D3DX effect interface
CModelViewerCamera          g_Camera;               // A model viewing camera
IDirect3DTexture9*          g_pDefaultTex = NULL;   // A default texture
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls
bool                        g_bShowVertexCacheOptimized = true;
bool                        g_bShowStripReordered = false;
bool                        g_bShowStrips = false;
bool                        g_bShowSingleStrip = false;
bool                        g_bForce32ByteFVF = true;
bool                        g_bCantDoSingleStrip = false;// Single strip would be too many primitives
D3DXVECTOR3                 g_vObjectCenter;        // Center of bounding sphere of object
FLOAT                       g_fObjectRadius;        // Radius of bounding sphere of object
D3DXMATRIXA16               g_matWorld;
int                         g_cObjectsPerSide = 1;  // sqrt of the number of objects to draw
DWORD                       g_dwMemoryOptions = D3DXMESH_MANAGED;
// various forms of mesh data
SMeshData                   g_MeshAttrSorted;
SMeshData                   g_MeshStripReordered;
SMeshData                   g_MeshVertexCacheOptimized;

DWORD                       g_dwNumMaterials = 0;   // Number of materials
IDirect3DTexture9**         g_ppMeshTextures = NULL;
D3DMATERIAL9*               g_pMeshMaterials = NULL;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_MESHTYPE            5
#define IDC_GRIDSIZE            6
#define IDC_PRIMTYPE            7


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
HRESULT LoadMeshData( IDirect3DDevice9* pd3dDevice, LPCWSTR wszMeshFile, LPD3DXMESH* pMeshSysMemLoaded,
                      LPD3DXBUFFER* ppAdjacencyBuffer );
HRESULT OptimizeMeshData( LPD3DXMESH pMeshSysMem, LPD3DXBUFFER pAdjacencyBuffer, DWORD dwOptFlags,
                          SMeshData* pMeshData );
HRESULT UpdateLocalMeshes( IDirect3DDevice9* pd3dDevice, SMeshData* pMeshData );
HRESULT DrawMeshData( ID3DXEffect* pEffect, SMeshData* pMeshData );


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
    DXUTCreateWindow( L"OptimizedMesh: Optimizing Meshes in D3D" );
    DXUTCreateDevice( true, 640, 480 );

    // Pass control to DXUT for handling the message pump and 
    // dispatching render calls. DXUT will call your FrameMove 
    // and FrameRender callback when there is idle time between handling window messages.
    DXUTMainLoop();

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.

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

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.AddComboBox( IDC_MESHTYPE, 0, iY, 200, 20, L'M' );
    g_SampleUI.GetComboBox( IDC_MESHTYPE )->AddItem( L"(M)esh type: VCache optimized", ( void* )0 );
    g_SampleUI.GetComboBox( IDC_MESHTYPE )->AddItem( L"(M)esh type: Strip reordered", ( void* )1 );
    g_SampleUI.GetComboBox( IDC_MESHTYPE )->AddItem( L"(M)esh type: Unoptimized", ( void* )2 );
    g_SampleUI.AddComboBox( IDC_PRIMTYPE, 0, iY += 24, 200, 20, L'P' );
    g_SampleUI.GetComboBox( IDC_PRIMTYPE )->AddItem( L"(P)rimitive: Triangle list", ( void* )0 );
    g_SampleUI.GetComboBox( IDC_PRIMTYPE )->AddItem( L"(P)rimitive: Single tri strip", ( void* )1 );
    g_SampleUI.GetComboBox( IDC_PRIMTYPE )->AddItem( L"(P)rimitive: Many tri strips", ( void* )2 );
    g_SampleUI.AddComboBox( IDC_GRIDSIZE, 0, iY += 24, 200, 20, L'G' );
    g_SampleUI.GetComboBox( IDC_GRIDSIZE )->AddItem( L"(G)rid size: 1 mesh", ( void* )1 );
    g_SampleUI.GetComboBox( IDC_GRIDSIZE )->AddItem( L"(G)rid size: 4 mesh", ( void* )2 );
    g_SampleUI.GetComboBox( IDC_GRIDSIZE )->AddItem( L"(G)rid size: 9 mesh", ( void* )3 );
    g_SampleUI.GetComboBox( IDC_GRIDSIZE )->AddItem( L"(G)rid size: 16 mesh", ( void* )4 );
    g_SampleUI.GetComboBox( IDC_GRIDSIZE )->AddItem( L"(G)rid size: 25 mesh", ( void* )5 );
    g_SampleUI.GetComboBox( IDC_GRIDSIZE )->AddItem( L"(G)rid size: 36 mesh", ( void* )6 );

    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, 0 );
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

    // No fallback defined by this app, so reject any device that 
    // doesn't support at least ps2.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
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


HRESULT LoadMeshData( IDirect3DDevice9* pd3dDevice, LPCWSTR wszMeshFile, LPD3DXMESH* pMeshSysMemLoaded,
                      LPD3DXBUFFER* ppAdjacencyBuffer )
{
    LPDIRECT3DVERTEXBUFFER9 pMeshVB = NULL;
    LPD3DXBUFFER pD3DXMtrlBuffer = NULL;
    void* pVertices;
    WCHAR strMesh[512];
    HRESULT hr = S_OK;
    LPD3DXMESH pMeshSysMem = NULL;
    LPD3DXMESH pMeshTemp;
    D3DXMATERIAL* d3dxMaterials;

    // Get a path to the media file
    if( FAILED( hr = DXUTFindDXSDKMediaFileCch( strMesh, 512, wszMeshFile ) ) )
        goto End;

    // Load the mesh from the specified file
    hr = D3DXLoadMeshFromX( strMesh, D3DXMESH_SYSTEMMEM, pd3dDevice,
                            ppAdjacencyBuffer, &pD3DXMtrlBuffer, NULL,
                            &g_dwNumMaterials, &pMeshSysMem );
    if( FAILED( hr ) )
        goto End;

    // Get the array of materials out of the returned buffer, and allocate a texture array
    d3dxMaterials = ( D3DXMATERIAL* )pD3DXMtrlBuffer->GetBufferPointer();
    g_pMeshMaterials = new D3DMATERIAL9[g_dwNumMaterials];
    if( g_pMeshMaterials == NULL )
    {
        hr = E_OUTOFMEMORY;
        goto End;
    }
    g_ppMeshTextures = new LPDIRECT3DTEXTURE9[g_dwNumMaterials];
    if( g_ppMeshTextures == NULL )
    {
        hr = E_OUTOFMEMORY;
        goto End;
    }

    for( DWORD i = 0; i < g_dwNumMaterials; i++ )
    {
        g_pMeshMaterials[i] = d3dxMaterials[i].MatD3D;
        g_pMeshMaterials[i].Ambient = g_pMeshMaterials[i].Diffuse;
        g_ppMeshTextures[i] = NULL;

        // Get a path to the texture
        WCHAR strPath[512];
        if( d3dxMaterials[i].pTextureFilename != NULL )
        {
            WCHAR wszBuf[MAX_PATH];
            MultiByteToWideChar( CP_ACP, 0, d3dxMaterials[i].pTextureFilename, -1, wszBuf, MAX_PATH );
            wszBuf[MAX_PATH - 1] = L'\0';
            DXUTFindDXSDKMediaFileCch( strPath, 512, wszBuf );

            // Load the texture
            D3DXCreateTextureFromFile( pd3dDevice, strPath, &g_ppMeshTextures[i] );
        }
        else
        {
            // Use the default texture
            g_ppMeshTextures[i] = g_pDefaultTex;
            g_ppMeshTextures[i]->AddRef();
        }
    }

    // Done with the material buffer
    SAFE_RELEASE( pD3DXMtrlBuffer );

    // Lock the vertex buffer, to generate a simple bounding sphere
    hr = pMeshSysMem->GetVertexBuffer( &pMeshVB );
    if( SUCCEEDED( hr ) )
    {
        hr = pMeshVB->Lock( 0, 0, &pVertices, D3DLOCK_NOSYSLOCK );
        if( SUCCEEDED( hr ) )
        {
            D3DXComputeBoundingSphere( ( D3DXVECTOR3* )pVertices, pMeshSysMem->GetNumVertices(),
                                       D3DXGetFVFVertexSize( pMeshSysMem->GetFVF() ),
                                       &g_vObjectCenter, &g_fObjectRadius );
            pMeshVB->Unlock();
        }
        pMeshVB->Release();
    }
    else
        goto End;

    // remember if there were normals in the file, before possible clone operation
    bool bNormalsInFile = ( pMeshSysMem->GetFVF() & D3DFVF_NORMAL ) != 0;

    // if using 32byte vertices, check fvf
    if( g_bForce32ByteFVF )
    {
        // force 32 byte vertices
        if( pMeshSysMem->GetFVF() != ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 ) )
        {
            hr = pMeshSysMem->CloneMeshFVF( pMeshSysMem->GetOptions(), D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1,
                                            pd3dDevice, &pMeshTemp );
            if( FAILED( hr ) )
                goto End;

            pMeshSysMem->Release();
            pMeshSysMem = pMeshTemp;
        }
    }
        // otherwise, just make sure that there are normals in mesh
    else if( !( pMeshSysMem->GetFVF() & D3DFVF_NORMAL ) )
    {
        hr = pMeshSysMem->CloneMeshFVF( pMeshSysMem->GetOptions(), pMeshSysMem->GetFVF() | D3DFVF_NORMAL,
                                        pd3dDevice, &pMeshTemp );
        if( FAILED( hr ) )
            goto End;

        pMeshSysMem->Release();
        pMeshSysMem = pMeshTemp;
    }

    // Compute normals for the mesh, if not present
    if( !bNormalsInFile )
        D3DXComputeNormals( pMeshSysMem, NULL );

    *pMeshSysMemLoaded = pMeshSysMem;
    pMeshSysMem = NULL;

End:
    SAFE_RELEASE( pMeshSysMem );

    return hr;
}


HRESULT OptimizeMeshData( LPD3DXMESH pMeshSysMem, LPD3DXBUFFER pAdjacencyBuffer, DWORD dwOptFlags,
                          SMeshData* pMeshData )
{
    HRESULT hr = S_OK;
    LPD3DXBUFFER pbufTemp = NULL;

    // Attribute sort - the un-optimized mesh option
    // remember the adjacency for the vertex cache optimization
    hr = pMeshSysMem->Optimize( dwOptFlags | D3DXMESH_SYSTEMMEM,
                                ( DWORD* )pAdjacencyBuffer->GetBufferPointer(),
                                NULL, NULL, NULL, &pMeshData->m_pMeshSysMem );
    if( FAILED( hr ) )
        goto End;

    pMeshData->m_cStripDatas = g_dwNumMaterials;
    pMeshData->m_rgStripData = new SStripData[ pMeshData->m_cStripDatas ];
    if( pMeshData->m_rgStripData == NULL )
    {
        hr = E_OUTOFMEMORY;
        goto End;
    }

    g_bCantDoSingleStrip = false;
    for( DWORD iMaterial = 0; iMaterial < g_dwNumMaterials; iMaterial++ )
    {
        hr = D3DXConvertMeshSubsetToSingleStrip( pMeshData->m_pMeshSysMem, iMaterial,
                                                 D3DXMESH_IB_MANAGED, &pMeshData->m_rgStripData[iMaterial].m_pStrips,
                                                 &pMeshData->m_rgStripData[iMaterial].m_cStripIndices );
        if( FAILED( hr ) )
            goto End;

        UINT primCount = pMeshData->m_rgStripData[iMaterial].m_cStripIndices - 2;

        IDirect3DDevice9* pd3dDevice;
        D3DCAPS9 d3dCaps;
        pMeshSysMem->GetDevice( &pd3dDevice );
        pd3dDevice->GetDeviceCaps( &d3dCaps );
        SAFE_RELEASE( pd3dDevice );
        if( primCount > d3dCaps.MaxPrimitiveCount )
        {
            g_bCantDoSingleStrip = true;
        }

        hr = D3DXConvertMeshSubsetToStrips( pMeshData->m_pMeshSysMem, iMaterial,
                                            D3DXMESH_IB_MANAGED, &pMeshData->m_rgStripData[iMaterial].m_pStripsMany,
                                            NULL, &pbufTemp, &pMeshData->m_rgStripData[iMaterial].m_cStrips );
        if( FAILED( hr ) )
            goto End;

        pMeshData->m_rgStripData[iMaterial].m_rgcStripLengths = new
            DWORD[pMeshData->m_rgStripData[iMaterial].m_cStrips];
        if( pMeshData->m_rgStripData[iMaterial].m_rgcStripLengths == NULL )
        {
            hr = E_OUTOFMEMORY;
            goto End;
        }
        memcpy( pMeshData->m_rgStripData[iMaterial].m_rgcStripLengths,
                pbufTemp->GetBufferPointer(),
                sizeof( DWORD ) * pMeshData->m_rgStripData[iMaterial].m_cStrips );
    }

End:
    SAFE_RELEASE( pbufTemp );

    return hr;
}


HRESULT UpdateLocalMeshes( IDirect3DDevice9* pd3dDevice, SMeshData* pMeshData )
{
    HRESULT hr = S_OK;

    // if a mesh was loaded, update the local meshes
    if( pMeshData->m_pMeshSysMem != NULL )
    {
        hr = pMeshData->m_pMeshSysMem->CloneMeshFVF( g_dwMemoryOptions | D3DXMESH_VB_WRITEONLY,
                                                     pMeshData->m_pMeshSysMem->GetFVF(),
                                                     pd3dDevice, &pMeshData->m_pMesh );
        if( FAILED( hr ) )
            return hr;

        hr = pMeshData->m_pMesh->GetVertexBuffer( &pMeshData->m_pVertexBuffer );
        if( FAILED( hr ) )
            return hr;
    }

    return hr;
}


HRESULT DrawMeshData( IDirect3DDevice9* pd3dDevice, ID3DXEffect* pEffect, SMeshData* pMeshData )
{
    HRESULT hr;
    DWORD iCurFace;

    V( pEffect->SetTechnique( "RenderScene" ) );
    UINT cPasses;
    V( pEffect->Begin( &cPasses, 0 ) );
    for( UINT p = 0; p < cPasses; ++p )
    {
        V( pEffect->BeginPass( p ) );

        // Set and draw each of the materials in the mesh
        for( DWORD iMaterial = 0; iMaterial < g_dwNumMaterials; iMaterial++ )
        {
            V( pEffect->SetVector( "g_vDiffuse", ( D3DXVECTOR4* )&g_pMeshMaterials[iMaterial].Diffuse ) );
            V( pEffect->SetTexture( "g_txScene", g_ppMeshTextures[iMaterial] ) );
            V( pEffect->CommitChanges() );
            //            V( pd3dDevice->SetMaterial( &g_pMeshMaterials[iMaterial] ) );
            //            V( pd3dDevice->SetTexture( 0, g_ppMeshTextures[iMaterial] ) );

            if( !g_bShowStrips && !g_bShowSingleStrip )
            {
                V( pMeshData->m_pMesh->DrawSubset( iMaterial ) );
            }
            else  // drawing strips
            {
                DWORD dwFVF;
                DWORD cBytesPerVertex;
                DWORD iStrip;

                dwFVF = pMeshData->m_pMesh->GetFVF();
                cBytesPerVertex = D3DXGetFVFVertexSize( dwFVF );

                V( pd3dDevice->SetFVF( dwFVF ) );
                V( pd3dDevice->SetStreamSource( 0, pMeshData->m_pVertexBuffer, 0, cBytesPerVertex ) );

                if( g_bShowSingleStrip )
                {
                    if( !g_bCantDoSingleStrip )
                    {
                        V( pd3dDevice->SetIndices( pMeshData->m_rgStripData[iMaterial].m_pStrips ) );

                        V( pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0,
                                                             0, pMeshData->m_pMesh->GetNumVertices(),
                                                             0, pMeshData->m_rgStripData[iMaterial].m_cStripIndices -
                                                             2 ) );
                    }
                }
                else
                {
                    V( pd3dDevice->SetIndices( pMeshData->m_rgStripData[iMaterial].m_pStripsMany ) );

                    iCurFace = 0;
                    for( iStrip = 0; iStrip < pMeshData->m_rgStripData[iMaterial].m_cStrips; iStrip++ )
                    {
                        V( pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0,
                                                             0, pMeshData->m_pMesh->GetNumVertices(),
                                                             iCurFace,
                                                             pMeshData->m_rgStripData[iMaterial].m_rgcStripLengths[iStrip] ) );

                        iCurFace += 2 + pMeshData->m_rgStripData[iMaterial].m_rgcStripLengths[iStrip];
                    }
                }
            }
        }
        V( pEffect->EndPass() );
    }
    V( pEffect->End() );

    return S_OK;
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
    // Create the 1x1 white default texture
    V_RETURN( pd3dDevice->CreateTexture( 1, 1, 1, 0, D3DFMT_A8R8G8B8,
                                         D3DPOOL_MANAGED, &g_pDefaultTex, NULL ) );
    D3DLOCKED_RECT lr;
    V_RETURN( g_pDefaultTex->LockRect( 0, &lr, NULL, 0 ) );
    *( LPDWORD )lr.pBits = D3DCOLOR_RGBA( 255, 255, 255, 255 );
    V_RETURN( g_pDefaultTex->UnlockRect( 0 ) );

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    // Define DEBUG_VS and/or DEBUG_PS to debug vertex and/or pixel shaders with the 
    // shader debugger. Debugging vertex shaders requires either REF or software vertex 
    // processing, and debugging pixel shaders requires REF.  The 
    // D3DXSHADER_FORCE_*_SOFTWARE_NOOPT flag improves the debug experience in the 
    // shader debugger.  It enables source level debugging, prevents instruction 
    // reordering, prevents dead code elimination, and forces the compiler to compile 
    // against the next higher available software target, which ensures that the 
    // unoptimized shaders do not exceed the shader model limitations.  Setting these 
    // flags will cause slower rendering since the shaders will be unoptimized and 
    // forced into software.  See the DirectX documentation for more information about 
    // using the shader debugger.
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;

#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DXSHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"OptimizedMesh.fx" ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );

    // Load mesh
    LPD3DXMESH pMeshSysMem = NULL;
    LPD3DXBUFFER pAdjacencyBuffer = NULL;

    hr = LoadMeshData( pd3dDevice, MESHFILENAME, &pMeshSysMem, &pAdjacencyBuffer );
    if( SUCCEEDED( hr ) )
    {
        hr = OptimizeMeshData( pMeshSysMem, pAdjacencyBuffer, D3DXMESHOPT_ATTRSORT, &g_MeshAttrSorted );
        if( SUCCEEDED( hr ) )
            hr = OptimizeMeshData( pMeshSysMem, pAdjacencyBuffer, D3DXMESHOPT_STRIPREORDER, &g_MeshStripReordered );

        if( SUCCEEDED( hr ) )
            hr = OptimizeMeshData( pMeshSysMem, pAdjacencyBuffer,
                                   D3DXMESHOPT_VERTEXCACHE, &g_MeshVertexCacheOptimized );

        SAFE_RELEASE( pMeshSysMem );
        SAFE_RELEASE( pAdjacencyBuffer );
    }
    else
        // ignore load errors, just draw blank screen if mesh is invalid
        hr = S_OK;

    D3DXMatrixTranslation( &g_matWorld, -g_vObjectCenter.x,
                           -g_vObjectCenter.y,
                           -g_vObjectCenter.z );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

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

    // update the local copies of the meshes
    UpdateLocalMeshes( pd3dDevice, &g_MeshAttrSorted );
    UpdateLocalMeshes( pd3dDevice, &g_MeshStripReordered );
    UpdateLocalMeshes( pd3dDevice, &g_MeshVertexCacheOptimized );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 200, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 200, 300 );

    return S_OK;
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
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldViewProjection;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 66, 75, 121 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Get the projection & view matrix from the camera class
        //        mWorld = *g_Camera.GetWorldMatrix();
        mProj = *g_Camera.GetProjMatrix();
        mView = *g_Camera.GetViewMatrix();

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Draw mesh" );

        for( int xOffset = 0; xOffset < g_cObjectsPerSide; xOffset++ )
        {
            for( int yOffset = 0; yOffset < g_cObjectsPerSide; yOffset++ )
            {
                D3DXMatrixTranslation( &mWorld, g_fObjectRadius * ( xOffset * 2 - g_cObjectsPerSide + 1 ),
                                       g_fObjectRadius * ( yOffset * 2 - g_cObjectsPerSide + 1 ),
                                       0 );
                D3DXMatrixMultiply( &mWorld, g_Camera.GetWorldMatrix(), &mWorld );
                D3DXMatrixMultiply( &mWorld, &g_matWorld, &mWorld );

                mWorldViewProjection = mWorld * mView * mProj;
                // Update the effect's variables.  Instead of using strings, it would 
                // be more efficient to cache a handle to the parameter by calling 
                // ID3DXEffect::GetParameterByName
                V( g_pEffect->SetMatrix( "g_mWorldViewProjection", &mWorldViewProjection ) );
                V( g_pEffect->SetMatrix( "g_mWorld", &mWorld ) );

                if( g_bShowVertexCacheOptimized )
                    DrawMeshData( pd3dDevice, g_pEffect, &g_MeshVertexCacheOptimized );
                else if( g_bShowStripReordered )
                    DrawMeshData( pd3dDevice, g_pEffect, &g_MeshStripReordered );
                else
                    DrawMeshData( pd3dDevice, g_pEffect, &g_MeshAttrSorted );
            }
        }

        DXUT_EndPerfEvent(); // end of drawing code

        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
            RenderText();
            V( g_HUD.OnRender( fElapsedTime ) );
            V( g_SampleUI.OnRender( fElapsedTime ) );
        }

        V( pd3dDevice->EndScene() );
    }
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
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    WCHAR* wszOptString;
    DWORD cTriangles = 0;
    // Calculate and show triangles per sec, a reasonable throughput number
    if( g_MeshAttrSorted.m_pMesh != NULL )
        cTriangles = g_MeshAttrSorted.m_pMesh->GetNumFaces() * g_cObjectsPerSide * g_cObjectsPerSide;
    else
        cTriangles = 0;

    float fTrisPerSec = DXUTGetFPS() * cTriangles;

    if( g_bShowVertexCacheOptimized )
        wszOptString = L"VCache Optimized";
    else if( g_bShowStripReordered )
        wszOptString = L"Strip Reordered";
    else
        wszOptString = L"Unoptimized";

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );
    txtHelper.DrawFormattedTextLine( L"%s, %ld tris per sec, %ld triangles",
                                     wszOptString, ( DWORD )fTrisPerSec, cTriangles );

    if( g_bShowSingleStrip && g_bCantDoSingleStrip )
        txtHelper.DrawTextLine( L"Couldn't draw to single strip -- too many primitives" );

    // Draw help
    if( g_bShowHelp )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 6 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls (F1 to hide):" );

        txtHelper.SetInsertionPos( 40, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Rotate mesh: Left click drag\n"
                                L"Zoom: mouse wheel\n"
                                L"Quit: ESC" );
    }
    else
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Press F1 for help" );
    }
    txtHelper.End();
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
        case IDC_MESHTYPE:
            switch( ( size_t )( ( CDXUTComboBox* )pControl )->GetSelectedData() )
            {
                case 0:
                    g_bShowVertexCacheOptimized = true;
                    g_bShowStripReordered = false;
                    break;
                case 1:
                    g_bShowVertexCacheOptimized = false;
                    g_bShowStripReordered = true;
                    break;
                case 2:
                    g_bShowVertexCacheOptimized = false;
                    g_bShowStripReordered = false;
                    break;
            }
            break;

        case IDC_PRIMTYPE:
            switch( ( size_t )( ( CDXUTComboBox* )pControl )->GetSelectedData() )
            {
                case 0:
                    g_bShowStrips = false;
                    g_bShowSingleStrip = false;
                    break;
                case 1:
                    g_bShowStrips = false;
                    g_bShowSingleStrip = true;
                    break;
                case 2:
                    g_bShowStrips = true;
                    g_bShowSingleStrip = false;
                    break;
            }
            break;

        case IDC_GRIDSIZE:
            g_cObjectsPerSide = ( int )( size_t )( ( CDXUTComboBox* )pControl )->GetSelectedData();
            break;
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
    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pEffect )
        g_pEffect->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );

    g_MeshAttrSorted.ReleaseLocalMeshes();
    g_MeshStripReordered.ReleaseLocalMeshes();
    g_MeshVertexCacheOptimized.ReleaseLocalMeshes();
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
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );

    for( UINT i = 0; i < g_dwNumMaterials; i++ )
        SAFE_RELEASE( g_ppMeshTextures[i] );
    SAFE_DELETE_ARRAY( g_ppMeshTextures );
    SAFE_DELETE_ARRAY( g_pMeshMaterials );
    SAFE_RELEASE( g_pDefaultTex );

    g_MeshAttrSorted.ReleaseAll();
    g_MeshStripReordered.ReleaseAll();
    g_MeshVertexCacheOptimized.ReleaseAll();

    g_dwNumMaterials = 0;
}



