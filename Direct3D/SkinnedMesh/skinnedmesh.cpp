//--------------------------------------------------------------------------------------
// File: SkinnedMesh.cpp
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

#define MESHFILENAME L"tiny\\tiny.x"


WCHAR g_wszShaderSource[4][30] =
{
    L"skinmesh1.vsh",
    L"skinmesh2.vsh",
    L"skinmesh3.vsh",
    L"skinmesh4.vsh"
};


// enum for various skinning modes possible
enum METHOD
{
    D3DNONINDEXED,
    D3DINDEXED,
    SOFTWARE,
    D3DINDEXEDVS,
    D3DINDEXEDHLSLVS,
    NONE
};


//--------------------------------------------------------------------------------------
// Name: struct D3DXFRAME_DERIVED
// Desc: Structure derived from D3DXFRAME so we can add some app-specific
//       info that will be stored with each frame
//--------------------------------------------------------------------------------------
struct D3DXFRAME_DERIVED : public D3DXFRAME
{
    D3DXMATRIXA16 CombinedTransformationMatrix;
};


//--------------------------------------------------------------------------------------
// Name: struct D3DXMESHCONTAINER_DERIVED
// Desc: Structure derived from D3DXMESHCONTAINER so we can add some app-specific
//       info that will be stored with each mesh
//--------------------------------------------------------------------------------------
struct D3DXMESHCONTAINER_DERIVED : public D3DXMESHCONTAINER
{
    LPDIRECT3DTEXTURE9* ppTextures;       // array of textures, entries are NULL if no texture specified    

    // SkinMesh info             
    LPD3DXMESH pOrigMesh;
    LPD3DXATTRIBUTERANGE pAttributeTable;
    DWORD NumAttributeGroups;
    DWORD NumInfl;
    LPD3DXBUFFER pBoneCombinationBuf;
    D3DXMATRIX** ppBoneMatrixPtrs;
    D3DXMATRIX* pBoneOffsetMatrices;
    DWORD NumPaletteEntries;
    bool UseSoftwareVP;
    DWORD iAttributeSW;     // used to denote the split between SW and HW if necessary for non-indexed skinning
};


//--------------------------------------------------------------------------------------
// Name: class CAllocateHierarchy
// Desc: Custom version of ID3DXAllocateHierarchy with custom methods to create
//       frames and meshcontainers.
//--------------------------------------------------------------------------------------
class CAllocateHierarchy : public ID3DXAllocateHierarchy
{
public:
    STDMETHOD( CreateFrame )( THIS_ LPCSTR Name, LPD3DXFRAME *ppNewFrame );
    STDMETHOD( CreateMeshContainer )( THIS_
        LPCSTR Name,
        CONST D3DXMESHDATA *pMeshData,
        CONST D3DXMATERIAL *pMaterials,
        CONST D3DXEFFECTINSTANCE *pEffectInstances,
        DWORD NumMaterials,
        CONST DWORD *pAdjacency,
        LPD3DXSKININFO pSkinInfo,
        LPD3DXMESHCONTAINER *ppNewMeshContainer );
    STDMETHOD( DestroyFrame )( THIS_ LPD3DXFRAME pFrameToFree );
    STDMETHOD( DestroyMeshContainer )( THIS_ LPD3DXMESHCONTAINER pMeshContainerBase );

    CAllocateHierarchy()
    {
    }
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                  g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                g_pTextSprite = NULL;   // Sprite for batching draw text calls
ID3DXEffect*                g_pEffect = NULL;       // D3DX effect interface
CD3DArcBall                 g_ArcBall;              // Arcball for model control
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls
LPD3DXFRAME                 g_pFrameRoot = NULL;
ID3DXAnimationController*   g_pAnimController = NULL;
D3DXVECTOR3                 g_vObjectCenter;        // Center of bounding sphere of object
FLOAT                       g_fObjectRadius;        // Radius of bounding sphere of object
METHOD                      g_SkinningMethod = D3DNONINDEXED; // Current skinning method
D3DXMATRIXA16*              g_pBoneMatrices = NULL;
UINT                        g_NumBoneMatricesMax = 0;
IDirect3DVertexShader9*     g_pIndexedVertexShader[4];
D3DXMATRIXA16               g_matView;              // View matrix
D3DXMATRIXA16               g_matProj;              // Projection matrix
D3DXMATRIXA16               g_matProjT;             // Transpose of projection matrix (for asm shader)
DWORD                       g_dwBehaviorFlags;      // Behavior flags of the 3D device
bool                        g_bUseSoftwareVP;       // Flag to indicate whether software vp is
// required due to lack of hardware


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_METHOD              5



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
void DrawMeshContainer( IDirect3DDevice9* pd3dDevice, LPD3DXMESHCONTAINER pMeshContainerBase, LPD3DXFRAME pFrameBase );
void DrawFrame( IDirect3DDevice9* pd3dDevice, LPD3DXFRAME pFrame );
HRESULT SetupBoneMatrixPointersOnMesh( LPD3DXMESHCONTAINER pMeshContainer );
HRESULT SetupBoneMatrixPointers( LPD3DXFRAME pFrame );
void UpdateFrameMatrices( LPD3DXFRAME pFrameBase, LPD3DXMATRIX pParentMatrix );
void UpdateSkinningMethod( LPD3DXFRAME pFrameBase );
HRESULT GenerateSkinnedMesh( IDirect3DDevice9* pd3dDevice, D3DXMESHCONTAINER_DERIVED* pMeshContainer );
void ReleaseAttributeTable( LPD3DXFRAME pFrameBase );


//--------------------------------------------------------------------------------------
// Name: AllocateName()
// Desc: Allocates memory for a string to hold the name of a frame or mesh
//--------------------------------------------------------------------------------------
HRESULT AllocateName( LPCSTR Name, LPSTR* pNewName )
{
    UINT cbLength;

    if( Name != NULL )
    {
        cbLength = ( UINT )strlen( Name ) + 1;
        *pNewName = new CHAR[cbLength];
        if( *pNewName == NULL )
            return E_OUTOFMEMORY;
        memcpy( *pNewName, Name, cbLength * sizeof( CHAR ) );
    }
    else
    {
        *pNewName = NULL;
    }

    return S_OK;
}




//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::CreateFrame()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::CreateFrame( LPCSTR Name, LPD3DXFRAME* ppNewFrame )
{
    HRESULT hr = S_OK;
    D3DXFRAME_DERIVED* pFrame;

    *ppNewFrame = NULL;

    pFrame = new D3DXFRAME_DERIVED;
    if( pFrame == NULL )
    {
        hr = E_OUTOFMEMORY;
        goto e_Exit;
    }

    hr = AllocateName( Name, &pFrame->Name );
    if( FAILED( hr ) )
        goto e_Exit;

    // initialize other data members of the frame
    D3DXMatrixIdentity( &pFrame->TransformationMatrix );
    D3DXMatrixIdentity( &pFrame->CombinedTransformationMatrix );

    pFrame->pMeshContainer = NULL;
    pFrame->pFrameSibling = NULL;
    pFrame->pFrameFirstChild = NULL;

    *ppNewFrame = pFrame;
    pFrame = NULL;

e_Exit:
    delete pFrame;
    return hr;
}




//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::CreateMeshContainer()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::CreateMeshContainer(
    LPCSTR Name,
    CONST D3DXMESHDATA *pMeshData,
    CONST D3DXMATERIAL *pMaterials,
    CONST D3DXEFFECTINSTANCE *pEffectInstances,
    DWORD NumMaterials,
    CONST DWORD *pAdjacency,
    LPD3DXSKININFO pSkinInfo,
    LPD3DXMESHCONTAINER *ppNewMeshContainer )
 {
    HRESULT hr;
    D3DXMESHCONTAINER_DERIVED *pMeshContainer = NULL;
    UINT NumFaces;
    UINT iMaterial;
    UINT iBone, cBones;
    LPDIRECT3DDEVICE9 pd3dDevice = NULL;

    LPD3DXMESH pMesh = NULL;

    *ppNewMeshContainer = NULL;

    // this sample does not handle patch meshes, so fail when one is found
    if( pMeshData->Type != D3DXMESHTYPE_MESH )
 {
        hr = E_FAIL;
        goto e_Exit;
    }

    // get the pMesh interface pointer out of the mesh data structure
    pMesh = pMeshData->pMesh;

    // this sample does not FVF compatible meshes, so fail when one is found
    if( pMesh->GetFVF() == 0 )
 {
        hr = E_FAIL;
        goto e_Exit;
    }

    // allocate the overloaded structure to return as a D3DXMESHCONTAINER
    pMeshContainer = new D3DXMESHCONTAINER_DERIVED;
    if( pMeshContainer == NULL )
 {
        hr = E_OUTOFMEMORY;
        goto e_Exit;
    }
    memset( pMeshContainer, 0, sizeof( D3DXMESHCONTAINER_DERIVED ) );

    // make sure and copy the name.  All memory as input belongs to caller, interfaces can be addref'd though
    hr = AllocateName( Name, &pMeshContainer->Name );
    if( FAILED( hr ) )
        goto e_Exit;

    pMesh->GetDevice( &pd3dDevice );
    NumFaces = pMesh->GetNumFaces();

    // if no normals are in the mesh, add them
    if( !( pMesh->GetFVF() & D3DFVF_NORMAL ) )
 {
        pMeshContainer->MeshData.Type = D3DXMESHTYPE_MESH;

    // clone the mesh to make room for the normals
        hr = pMesh->CloneMeshFVF( pMesh->GetOptions(),
                                    pMesh->GetFVF() | D3DFVF_NORMAL,
                                    pd3dDevice, &pMeshContainer->MeshData.pMesh );
        if( FAILED( hr ) )
            goto e_Exit;

    // get the new pMesh pointer back out of the mesh container to use
    // NOTE: we do not release pMesh because we do not have a reference to it yet
        pMesh = pMeshContainer->MeshData.pMesh;

    // now generate the normals for the pmesh
        D3DXComputeNormals( pMesh, NULL );
    }
    else  // if no normals, just add a reference to the mesh for the mesh container
 {
        pMeshContainer->MeshData.pMesh = pMesh;
        pMeshContainer->MeshData.Type = D3DXMESHTYPE_MESH;

        pMesh->AddRef();
    }

    // allocate memory to contain the material information.  This sample uses
    //   the D3D9 materials and texture names instead of the EffectInstance style materials
    pMeshContainer->NumMaterials = max( 1, NumMaterials );
    pMeshContainer->pMaterials = new D3DXMATERIAL[pMeshContainer->NumMaterials];
    pMeshContainer->ppTextures = new LPDIRECT3DTEXTURE9[pMeshContainer->NumMaterials];
    pMeshContainer->pAdjacency = new DWORD[NumFaces*3];
    if( ( pMeshContainer->pAdjacency == NULL ) || ( pMeshContainer->pMaterials == NULL ) )
 {
        hr = E_OUTOFMEMORY;
        goto e_Exit;
    }

    memcpy( pMeshContainer->pAdjacency, pAdjacency, sizeof( DWORD ) * NumFaces*3 );
    memset( pMeshContainer->ppTextures, 0, sizeof( LPDIRECT3DTEXTURE9 ) * pMeshContainer->NumMaterials );

    // if materials provided, copy them
    if( NumMaterials > 0 )
 {
        memcpy( pMeshContainer->pMaterials, pMaterials, sizeof( D3DXMATERIAL ) * NumMaterials );

        for( iMaterial = 0; iMaterial < NumMaterials; iMaterial++ )
 {
            if( pMeshContainer->pMaterials[iMaterial].pTextureFilename != NULL )
 {
                WCHAR strTexturePath[MAX_PATH];
                WCHAR wszBuf[MAX_PATH];
                MultiByteToWideChar( CP_ACP, 0, pMeshContainer->pMaterials[iMaterial].pTextureFilename, -1, wszBuf, MAX_PATH );
                wszBuf[MAX_PATH - 1] = L'\0';
                DXUTFindDXSDKMediaFileCch( strTexturePath, MAX_PATH, wszBuf );
                if( FAILED( D3DXCreateTextureFromFile( pd3dDevice, strTexturePath,
                                                        &pMeshContainer->ppTextures[iMaterial] ) ) )
                    pMeshContainer->ppTextures[iMaterial] = NULL;

    // don't remember a pointer into the dynamic memory, just forget the name after loading
                pMeshContainer->pMaterials[iMaterial].pTextureFilename = NULL;
            }
        }
    }
    else // if no materials provided, use a default one
 {
        pMeshContainer->pMaterials[0].pTextureFilename = NULL;
        memset( &pMeshContainer->pMaterials[0].MatD3D, 0, sizeof( D3DMATERIAL9 ) );
        pMeshContainer->pMaterials[0].MatD3D.Diffuse.r = 0.5f;
        pMeshContainer->pMaterials[0].MatD3D.Diffuse.g = 0.5f;
        pMeshContainer->pMaterials[0].MatD3D.Diffuse.b = 0.5f;
        pMeshContainer->pMaterials[0].MatD3D.Specular = pMeshContainer->pMaterials[0].MatD3D.Diffuse;
    }

    // if there is skinning information, save off the required data and then setup for HW skinning
    if( pSkinInfo != NULL )
 {
    // first save off the SkinInfo and original mesh data
        pMeshContainer->pSkinInfo = pSkinInfo;
        pSkinInfo->AddRef();

        pMeshContainer->pOrigMesh = pMesh;
        pMesh->AddRef();

    // Will need an array of offset matrices to move the vertices from the figure space to the bone's space
        cBones = pSkinInfo->GetNumBones();
        pMeshContainer->pBoneOffsetMatrices = new D3DXMATRIX[cBones];
        if( pMeshContainer->pBoneOffsetMatrices == NULL )
 {
            hr = E_OUTOFMEMORY;
            goto e_Exit;
        }

    // get each of the bone offset matrices so that we don't need to get them later
        for( iBone = 0; iBone < cBones; iBone++ )
 {
            pMeshContainer->pBoneOffsetMatrices[iBone] = *( pMeshContainer->pSkinInfo->GetBoneOffsetMatrix( iBone ) );
        }

    // GenerateSkinnedMesh will take the general skinning information and transform it to a HW friendly version
        hr = GenerateSkinnedMesh( pd3dDevice, pMeshContainer );
        if( FAILED( hr ) )
            goto e_Exit;
    }

    *ppNewMeshContainer = pMeshContainer;
    pMeshContainer = NULL;

e_Exit:
    SAFE_RELEASE( pd3dDevice );

    // call Destroy function to properly clean up the memory allocated 
    if( pMeshContainer != NULL )
 {
        DestroyMeshContainer( pMeshContainer );
    }

    return hr;
}




//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::DestroyFrame()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::DestroyFrame( LPD3DXFRAME pFrameToFree )
{
    SAFE_DELETE_ARRAY( pFrameToFree->Name );
    SAFE_DELETE( pFrameToFree );
    return S_OK;
}




//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::DestroyMeshContainer()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::DestroyMeshContainer( LPD3DXMESHCONTAINER pMeshContainerBase )
{
    UINT iMaterial;
    D3DXMESHCONTAINER_DERIVED* pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainerBase;

    SAFE_DELETE_ARRAY( pMeshContainer->Name );
    SAFE_DELETE_ARRAY( pMeshContainer->pAdjacency );
    SAFE_DELETE_ARRAY( pMeshContainer->pMaterials );
    SAFE_DELETE_ARRAY( pMeshContainer->pBoneOffsetMatrices );

    // release all the allocated textures
    if( pMeshContainer->ppTextures != NULL )
    {
        for( iMaterial = 0; iMaterial < pMeshContainer->NumMaterials; iMaterial++ )
        {
            SAFE_RELEASE( pMeshContainer->ppTextures[iMaterial] );
        }
    }

    SAFE_DELETE_ARRAY( pMeshContainer->ppTextures );
    SAFE_DELETE_ARRAY( pMeshContainer->ppBoneMatrixPtrs );
    SAFE_RELEASE( pMeshContainer->pBoneCombinationBuf );
    SAFE_RELEASE( pMeshContainer->MeshData.pMesh );
    SAFE_RELEASE( pMeshContainer->pSkinInfo );
    SAFE_RELEASE( pMeshContainer->pOrigMesh );
    SAFE_DELETE( pMeshContainer );
    return S_OK;
}


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
    DXUTSetCallbackD3D9DeviceCreated( OnCreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnFrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnLostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTCreateWindow( L"Skinned Mesh" );

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );

    InitApp();

    // Initialize DXUT and create the desired Win32 window and Direct3D 
    // device for the application. Calling each of these functions is optional, but they
    // allow you to set several options which control the behavior of the framework.
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the defaul hotkeys

    // Supports all types of vertex processing, including mixed.
    DXUTGetD3D9Enumeration()->SetPossibleVertexProcessingList( true, true, true, true );

    DXUTCreateDevice( true, 640, 480 );

    // Pass control to DXUT for handling the message pump and 
    // dispatching render calls. DXUT will call your FrameMove 
    // and FrameRender callback when there is idle time between handling window messages.
    DXUTMainLoop();

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.
    ReleaseAttributeTable( g_pFrameRoot );
    delete[] g_pBoneMatrices;

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

    // Add mixed vp to the available vp choices in device settings dialog.
    DXUTGetD3D9Enumeration()->SetPossibleVertexProcessingList( true, false, false, true );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.AddComboBox( IDC_METHOD, 0, iY, 230, 24, L'S' );
    g_SampleUI.GetComboBox( IDC_METHOD )->AddItem( L"Fixed function non-indexed (s)kinning", ( void* )D3DNONINDEXED );
    g_SampleUI.GetComboBox( IDC_METHOD )->AddItem( L"Fixed function indexed (s)kinning", ( void* )D3DINDEXED );
    g_SampleUI.GetComboBox( IDC_METHOD )->AddItem( L"Software (s)kinning", ( void* )SOFTWARE );
    g_SampleUI.GetComboBox( IDC_METHOD )->AddItem( L"ASM shader indexed (s)kinning", ( void* )D3DINDEXEDVS );
    g_SampleUI.GetComboBox( IDC_METHOD )->AddItem( L"HLSL shader indexed (s)kinning", ( void* )D3DINDEXEDHLSLVS );
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

    // Turn vsync off
    pDeviceSettings->d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

    // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
    // then switch to SWVP.
    if( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
        caps.VertexShaderVersion < D3DVS_VERSION( 1, 1 ) )
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // If the hardware cannot do vertex blending, use software vertex processing.
    if( caps.MaxVertexBlendMatrices < 2 )
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    // If using hardware vertex processing, change to mixed vertex processing
    // so there is a fallback.
    if( pDeviceSettings->d3d9.BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_MIXED_VERTEXPROCESSING;

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
// Called either by CreateMeshContainer when loading a skin mesh, or when 
// changing methods.  This function uses the pSkinInfo of the mesh 
// container to generate the desired drawable mesh and bone combination 
// table.
//--------------------------------------------------------------------------------------
HRESULT GenerateSkinnedMesh( IDirect3DDevice9* pd3dDevice, D3DXMESHCONTAINER_DERIVED* pMeshContainer )
{
    HRESULT hr = S_OK;
    D3DCAPS9 d3dCaps;
    pd3dDevice->GetDeviceCaps( &d3dCaps );

    if( pMeshContainer->pSkinInfo == NULL )
        return hr;

    g_bUseSoftwareVP = false;

    SAFE_RELEASE( pMeshContainer->MeshData.pMesh );
    SAFE_RELEASE( pMeshContainer->pBoneCombinationBuf );

    // if non-indexed skinning mode selected, use ConvertToBlendedMesh to generate drawable mesh
    if( g_SkinningMethod == D3DNONINDEXED )
    {

        hr = pMeshContainer->pSkinInfo->ConvertToBlendedMesh
            (
            pMeshContainer->pOrigMesh,
            D3DXMESH_MANAGED | D3DXMESHOPT_VERTEXCACHE,
            pMeshContainer->pAdjacency,
            NULL, NULL, NULL,
            &pMeshContainer->NumInfl,
            &pMeshContainer->NumAttributeGroups,
            &pMeshContainer->pBoneCombinationBuf,
            &pMeshContainer->MeshData.pMesh
            );
        if( FAILED( hr ) )
            goto e_Exit;


        // If the device can only do 2 matrix blends, ConvertToBlendedMesh cannot approximate all meshes to it
        // Thus we split the mesh in two parts: The part that uses at most 2 matrices and the rest. The first is
        // drawn using the device's HW vertex processing and the rest is drawn using SW vertex processing.
        LPD3DXBONECOMBINATION rgBoneCombinations = reinterpret_cast<LPD3DXBONECOMBINATION>(
            pMeshContainer->pBoneCombinationBuf->GetBufferPointer() );

        // look for any set of bone combinations that do not fit the caps
        for( pMeshContainer->iAttributeSW = 0; pMeshContainer->iAttributeSW < pMeshContainer->NumAttributeGroups;
             pMeshContainer->iAttributeSW++ )
        {
            DWORD cInfl = 0;

            for( DWORD iInfl = 0; iInfl < pMeshContainer->NumInfl; iInfl++ )
            {
                if( rgBoneCombinations[pMeshContainer->iAttributeSW].BoneId[iInfl] != UINT_MAX )
                {
                    ++cInfl;
                }
            }

            if( cInfl > d3dCaps.MaxVertexBlendMatrices )
            {
                break;
            }
        }

        // if there is both HW and SW, add the Software Processing flag
        if( pMeshContainer->iAttributeSW < pMeshContainer->NumAttributeGroups )
        {
            LPD3DXMESH pMeshTmp;

            hr = pMeshContainer->MeshData.pMesh->CloneMeshFVF( D3DXMESH_SOFTWAREPROCESSING |
                                                               pMeshContainer->MeshData.pMesh->GetOptions(),
                                                               pMeshContainer->MeshData.pMesh->GetFVF(),
                                                               pd3dDevice, &pMeshTmp );
            if( FAILED( hr ) )
            {
                goto e_Exit;
            }

            pMeshContainer->MeshData.pMesh->Release();
            pMeshContainer->MeshData.pMesh = pMeshTmp;
            pMeshTmp = NULL;
        }
    }
        // if indexed skinning mode selected, use ConvertToIndexedsBlendedMesh to generate drawable mesh
    else if( g_SkinningMethod == D3DINDEXED )
    {
        DWORD NumMaxFaceInfl;
        DWORD Flags = D3DXMESHOPT_VERTEXCACHE;

        LPDIRECT3DINDEXBUFFER9 pIB;
        hr = pMeshContainer->pOrigMesh->GetIndexBuffer( &pIB );
        if( FAILED( hr ) )
            goto e_Exit;

        hr = pMeshContainer->pSkinInfo->GetMaxFaceInfluences( pIB,
                                                              pMeshContainer->pOrigMesh->GetNumFaces(),
                                                              &NumMaxFaceInfl );
        pIB->Release();
        if( FAILED( hr ) )
            goto e_Exit;

        // 12 entry palette guarantees that any triangle (4 independent influences per vertex of a tri)
        // can be handled
        NumMaxFaceInfl = min( NumMaxFaceInfl, 12 );

        if( d3dCaps.MaxVertexBlendMatrixIndex + 1 < NumMaxFaceInfl )
        {
            // HW does not support indexed vertex blending. Use SW instead
            pMeshContainer->NumPaletteEntries = min( 256, pMeshContainer->pSkinInfo->GetNumBones() );
            pMeshContainer->UseSoftwareVP = true;
            g_bUseSoftwareVP = true;
            Flags |= D3DXMESH_SYSTEMMEM;
        }
        else
        {
            // using hardware - determine palette size from caps and number of bones
            // If normals are present in the vertex data that needs to be blended for lighting, then 
            // the number of matrices is half the number specified by MaxVertexBlendMatrixIndex.
            pMeshContainer->NumPaletteEntries = min( ( d3dCaps.MaxVertexBlendMatrixIndex + 1 ) / 2,
                                                     pMeshContainer->pSkinInfo->GetNumBones() );
            pMeshContainer->UseSoftwareVP = false;
            Flags |= D3DXMESH_MANAGED;
        }

        hr = pMeshContainer->pSkinInfo->ConvertToIndexedBlendedMesh
            (
            pMeshContainer->pOrigMesh,
            Flags,
            pMeshContainer->NumPaletteEntries,
            pMeshContainer->pAdjacency,
            NULL, NULL, NULL,
            &pMeshContainer->NumInfl,
            &pMeshContainer->NumAttributeGroups,
            &pMeshContainer->pBoneCombinationBuf,
            &pMeshContainer->MeshData.pMesh );
        if( FAILED( hr ) )
            goto e_Exit;
    }
        // if vertex shader indexed skinning mode selected, use ConvertToIndexedsBlendedMesh to generate drawable mesh
    else if( ( g_SkinningMethod == D3DINDEXEDVS ) || ( g_SkinningMethod == D3DINDEXEDHLSLVS ) )
    {
        // Get palette size
        // First 9 constants are used for other data.  Each 4x3 matrix takes up 3 constants.
        // (96 - 9) /3 i.e. Maximum constant count - used constants 
        UINT MaxMatrices = 26;
        pMeshContainer->NumPaletteEntries = min( MaxMatrices, pMeshContainer->pSkinInfo->GetNumBones() );

        DWORD Flags = D3DXMESHOPT_VERTEXCACHE;
        if( d3dCaps.VertexShaderVersion >= D3DVS_VERSION( 1, 1 ) )
        {
            pMeshContainer->UseSoftwareVP = false;
            Flags |= D3DXMESH_MANAGED;
        }
        else
        {
            pMeshContainer->UseSoftwareVP = true;
            g_bUseSoftwareVP = true;
            Flags |= D3DXMESH_SYSTEMMEM;
        }

        SAFE_RELEASE( pMeshContainer->MeshData.pMesh );

        hr = pMeshContainer->pSkinInfo->ConvertToIndexedBlendedMesh
            (
            pMeshContainer->pOrigMesh,
            Flags,
            pMeshContainer->NumPaletteEntries,
            pMeshContainer->pAdjacency,
            NULL, NULL, NULL,
            &pMeshContainer->NumInfl,
            &pMeshContainer->NumAttributeGroups,
            &pMeshContainer->pBoneCombinationBuf,
            &pMeshContainer->MeshData.pMesh );
        if( FAILED( hr ) )
            goto e_Exit;


        // FVF has to match our declarator. Vertex shaders are not as forgiving as FF pipeline
        DWORD NewFVF = ( pMeshContainer->MeshData.pMesh->GetFVF() & D3DFVF_POSITION_MASK ) | D3DFVF_NORMAL |
            D3DFVF_TEX1 | D3DFVF_LASTBETA_UBYTE4;
        if( NewFVF != pMeshContainer->MeshData.pMesh->GetFVF() )
        {
            LPD3DXMESH pMesh;
            hr = pMeshContainer->MeshData.pMesh->CloneMeshFVF( pMeshContainer->MeshData.pMesh->GetOptions(), NewFVF,
                                                               pd3dDevice, &pMesh );
            if( !FAILED( hr ) )
            {
                pMeshContainer->MeshData.pMesh->Release();
                pMeshContainer->MeshData.pMesh = pMesh;
                pMesh = NULL;
            }
        }

        D3DVERTEXELEMENT9 pDecl[MAX_FVF_DECL_SIZE];
        LPD3DVERTEXELEMENT9 pDeclCur;
        hr = pMeshContainer->MeshData.pMesh->GetDeclaration( pDecl );
        if( FAILED( hr ) )
            goto e_Exit;

        // the vertex shader is expecting to interpret the UBYTE4 as a D3DCOLOR, so update the type 
        //   NOTE: this cannot be done with CloneMesh, that would convert the UBYTE4 data to float and then to D3DCOLOR
        //          this is more of a "cast" operation
        pDeclCur = pDecl;
        while( pDeclCur->Stream != 0xff )
        {
            if( ( pDeclCur->Usage == D3DDECLUSAGE_BLENDINDICES ) && ( pDeclCur->UsageIndex == 0 ) )
                pDeclCur->Type = D3DDECLTYPE_D3DCOLOR;
            pDeclCur++;
        }

        hr = pMeshContainer->MeshData.pMesh->UpdateSemantics( pDecl );
        if( FAILED( hr ) )
            goto e_Exit;

        // allocate a buffer for bone matrices, but only if another mesh has not allocated one of the same size or larger
        if( g_NumBoneMatricesMax < pMeshContainer->pSkinInfo->GetNumBones() )
        {
            g_NumBoneMatricesMax = pMeshContainer->pSkinInfo->GetNumBones();

            // Allocate space for blend matrices
            delete[] g_pBoneMatrices;
            g_pBoneMatrices = new D3DXMATRIXA16[g_NumBoneMatricesMax];
            if( g_pBoneMatrices == NULL )
            {
                hr = E_OUTOFMEMORY;
                goto e_Exit;
            }
        }

    }
        // if software skinning selected, use GenerateSkinnedMesh to create a mesh that can be used with UpdateSkinnedMesh
    else if( g_SkinningMethod == SOFTWARE )
    {
        hr = pMeshContainer->pOrigMesh->CloneMeshFVF( D3DXMESH_MANAGED, pMeshContainer->pOrigMesh->GetFVF(),
                                                      pd3dDevice, &pMeshContainer->MeshData.pMesh );
        if( FAILED( hr ) )
            goto e_Exit;

        hr = pMeshContainer->MeshData.pMesh->GetAttributeTable( NULL, &pMeshContainer->NumAttributeGroups );
        if( FAILED( hr ) )
            goto e_Exit;

        delete[] pMeshContainer->pAttributeTable;
        pMeshContainer->pAttributeTable = new D3DXATTRIBUTERANGE[pMeshContainer->NumAttributeGroups];
        if( pMeshContainer->pAttributeTable == NULL )
        {
            hr = E_OUTOFMEMORY;
            goto e_Exit;
        }

        hr = pMeshContainer->MeshData.pMesh->GetAttributeTable( pMeshContainer->pAttributeTable, NULL );
        if( FAILED( hr ) )
            goto e_Exit;

        // allocate a buffer for bone matrices, but only if another mesh has not allocated one of the same size or larger
        if( g_NumBoneMatricesMax < pMeshContainer->pSkinInfo->GetNumBones() )
        {
            g_NumBoneMatricesMax = pMeshContainer->pSkinInfo->GetNumBones();

            // Allocate space for blend matrices
            delete[] g_pBoneMatrices;
            g_pBoneMatrices = new D3DXMATRIXA16[g_NumBoneMatricesMax];
            if( g_pBoneMatrices == NULL )
            {
                hr = E_OUTOFMEMORY;
                goto e_Exit;
            }
        }
    }
    else  // invalid g_SkinningMethod value
    {
        // return failure due to invalid skinning method value
        hr = E_INVALIDARG;
        goto e_Exit;
    }

e_Exit:
    return hr;
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
    CAllocateHierarchy Alloc;

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET,
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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SkinnedMesh.fx" ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );

    // Load the mesh
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, MESHFILENAME ) );
    WCHAR strPath[MAX_PATH];
    wcscpy_s( strPath, MAX_PATH, str );
    WCHAR* pLastSlash = wcsrchr( strPath, L'\\' );
    if( pLastSlash )
    {
        *pLastSlash = 0;
        ++pLastSlash;
    }
    else
    {
        wcscpy_s( strPath, MAX_PATH, L"." );
        pLastSlash = str;
    }
    WCHAR strCWD[MAX_PATH];
    GetCurrentDirectory( MAX_PATH, strCWD );
    SetCurrentDirectory( strPath );
    V_RETURN( D3DXLoadMeshHierarchyFromX( pLastSlash, D3DXMESH_MANAGED, pd3dDevice,
                                          &Alloc, NULL, &g_pFrameRoot, &g_pAnimController ) );
    V_RETURN( SetupBoneMatrixPointers( g_pFrameRoot ) );
    V_RETURN( D3DXFrameCalculateBoundingSphere( g_pFrameRoot, &g_vObjectCenter, &g_fObjectRadius ) );
    SetCurrentDirectory( strCWD );

    // Obtain the behavior flags
    D3DDEVICE_CREATION_PARAMETERS cp;
    pd3dDevice->GetCreationParameters( &cp );
    g_dwBehaviorFlags = cp.BehaviorFlags;

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

    // Setup render state
    pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x33333333 );
    pd3dDevice->SetRenderState( D3DRS_NORMALIZENORMALS, TRUE );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );

    // load the indexed vertex shaders
    DWORD dwShaderFlags = 0;

#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DXSHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DXSHADER_DEBUG;
    #endif

#if defined(DEBUG_VS) || defined(DEBUG_PS)
        dwShaderFlags |= D3DXSHADER_DEBUG|D3DXSHADER_SKIPVALIDATION;
    #endif
    for( DWORD iInfl = 0; iInfl < 4; ++iInfl )
        {
            LPD3DXBUFFER pCode;

            // Assemble the vertex shader file
            WCHAR str[MAX_PATH];
            DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_wszShaderSource[iInfl] );
            if( FAILED( hr = D3DXAssembleShaderFromFile( str, NULL, NULL, dwShaderFlags, &pCode, NULL ) ) )
                return hr;

            // Create the vertex shader
            if( FAILED( hr = pd3dDevice->CreateVertexShader( ( DWORD* )pCode->GetBufferPointer(),
                                                             &g_pIndexedVertexShader[iInfl] ) ) )
            {
                return hr;
            }

            pCode->Release();
        }

    // Setup the projection matrix
    float fAspect = ( float )pBackBufferSurfaceDesc->Width / ( float )pBackBufferSurfaceDesc->Height;
    D3DXMatrixPerspectiveFovLH( &g_matProj, D3DX_PI / 4, fAspect,
                                g_fObjectRadius / 64.0f, g_fObjectRadius * 200.0f );
    pd3dDevice->SetTransform( D3DTS_PROJECTION, &g_matProj );
    D3DXMatrixTranspose( &g_matProjT, &g_matProj );

    // Setup the arcball parameters
    g_ArcBall.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, 0.85f );
    g_ArcBall.SetTranslationRadius( g_fObjectRadius );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( 3, 45 );
    g_SampleUI.SetSize( 240, 70 );

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
    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

    // Setup world matrix
    D3DXMATRIXA16 matWorld;
    D3DXMatrixTranslation( &matWorld, -g_vObjectCenter.x,
                           -g_vObjectCenter.y,
                           -g_vObjectCenter.z );
    D3DXMatrixMultiply( &matWorld, &matWorld, g_ArcBall.GetRotationMatrix() );
    D3DXMatrixMultiply( &matWorld, &matWorld, g_ArcBall.GetTranslationMatrix() );
    pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    D3DXVECTOR3 vEye( 0, 0, -2 * g_fObjectRadius );
    D3DXVECTOR3 vAt( 0, 0, 0 );
    D3DXVECTOR3 vUp( 0, 1, 0 );
    D3DXMatrixLookAtLH( &g_matView, &vEye, &vAt, &vUp );

    pd3dDevice->SetTransform( D3DTS_VIEW, &g_matView );

    if( g_pAnimController != NULL )
        g_pAnimController->AdvanceTime( fElapsedTime, NULL );

    UpdateFrameMatrices( g_pFrameRoot, &matWorld );
}


//--------------------------------------------------------------------------------------
// Called to render a mesh in the hierarchy
//--------------------------------------------------------------------------------------
void DrawMeshContainer( IDirect3DDevice9* pd3dDevice, LPD3DXMESHCONTAINER pMeshContainerBase, LPD3DXFRAME pFrameBase )
{
    HRESULT hr;
    D3DXMESHCONTAINER_DERIVED* pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainerBase;
    D3DXFRAME_DERIVED* pFrame = ( D3DXFRAME_DERIVED* )pFrameBase;
    UINT iMaterial;
    UINT NumBlend;
    UINT iAttrib;
    DWORD AttribIdPrev;
    LPD3DXBONECOMBINATION pBoneComb;

    UINT iMatrixIndex;
    UINT iPaletteEntry;
    D3DXMATRIXA16 matTemp;
    D3DCAPS9 d3dCaps;
    pd3dDevice->GetDeviceCaps( &d3dCaps );

    // first check for skinning
    if( pMeshContainer->pSkinInfo != NULL )
    {
        if( g_SkinningMethod == D3DNONINDEXED )
        {
            AttribIdPrev = UNUSED32;
            pBoneComb = reinterpret_cast<LPD3DXBONECOMBINATION>( pMeshContainer->pBoneCombinationBuf->GetBufferPointer
                                                                 () );

            // Draw using default vtx processing of the device (typically HW)
            for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
            {
                NumBlend = 0;
                for( DWORD i = 0; i < pMeshContainer->NumInfl; ++i )
                {
                    if( pBoneComb[iAttrib].BoneId[i] != UINT_MAX )
                    {
                        NumBlend = i;
                    }
                }

                if( d3dCaps.MaxVertexBlendMatrices >= NumBlend + 1 )
                {
                    // first calculate the world matrices for the current set of blend weights and get the accurate count of the number of blends
                    for( DWORD i = 0; i < pMeshContainer->NumInfl; ++i )
                    {
                        iMatrixIndex = pBoneComb[iAttrib].BoneId[i];
                        if( iMatrixIndex != UINT_MAX )
                        {
                            D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
                                                pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
                            V( pd3dDevice->SetTransform( D3DTS_WORLDMATRIX( i ), &matTemp ) );
                        }
                    }

                    V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, NumBlend ) );

                    // lookup the material used for this subset of faces
                    if( ( AttribIdPrev != pBoneComb[iAttrib].AttribId ) || ( AttribIdPrev == UNUSED32 ) )
                    {
                        V( pd3dDevice->SetMaterial( &pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D )
                           );
                        V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );
                        AttribIdPrev = pBoneComb[iAttrib].AttribId;
                    }

                    // draw the subset now that the correct material and matrices are loaded
                    V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );
                }
            }

            // If necessary, draw parts that HW could not handle using SW
            if( pMeshContainer->iAttributeSW < pMeshContainer->NumAttributeGroups )
            {
                AttribIdPrev = UNUSED32;
                V( pd3dDevice->SetSoftwareVertexProcessing( TRUE ) );
                for( iAttrib = pMeshContainer->iAttributeSW; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
                {
                    NumBlend = 0;
                    for( DWORD i = 0; i < pMeshContainer->NumInfl; ++i )
                    {
                        if( pBoneComb[iAttrib].BoneId[i] != UINT_MAX )
                        {
                            NumBlend = i;
                        }
                    }

                    if( d3dCaps.MaxVertexBlendMatrices < NumBlend + 1 )
                    {
                        // first calculate the world matrices for the current set of blend weights and get the accurate count of the number of blends
                        for( DWORD i = 0; i < pMeshContainer->NumInfl; ++i )
                        {
                            iMatrixIndex = pBoneComb[iAttrib].BoneId[i];
                            if( iMatrixIndex != UINT_MAX )
                            {
                                D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
                                                    pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
                                V( pd3dDevice->SetTransform( D3DTS_WORLDMATRIX( i ), &matTemp ) );
                            }
                        }

                        V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, NumBlend ) );

                        // lookup the material used for this subset of faces
                        if( ( AttribIdPrev != pBoneComb[iAttrib].AttribId ) || ( AttribIdPrev == UNUSED32 ) )
                        {
                            V( pd3dDevice->SetMaterial( &pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D
                                                        ) );
                            V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );
                            AttribIdPrev = pBoneComb[iAttrib].AttribId;
                        }

                        // draw the subset now that the correct material and matrices are loaded
                        V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );
                    }
                }
                V( pd3dDevice->SetSoftwareVertexProcessing( FALSE ) );
            }

            V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, 0 ) );
        }
        else if( g_SkinningMethod == D3DINDEXED )
        {
            // if hw doesn't support indexed vertex processing, switch to software vertex processing
            if( pMeshContainer->UseSoftwareVP )
            {
                // If hw or pure hw vertex processing is forced, we can't render the
                // mesh, so just exit out.  Typical applications should create
                // a device with appropriate vertex processing capability for this
                // skinning method.
                if( g_dwBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
                    return;

                V( pd3dDevice->SetSoftwareVertexProcessing( TRUE ) );
            }

            // set the number of vertex blend indices to be blended
            if( pMeshContainer->NumInfl == 1 )
            {
                V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, D3DVBF_0WEIGHTS ) );
            }
            else
            {
                V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, pMeshContainer->NumInfl - 1 ) );
            }

            if( pMeshContainer->NumInfl )
                V( pd3dDevice->SetRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, TRUE ) );

            // for each attribute group in the mesh, calculate the set of matrices in the palette and then draw the mesh subset
            pBoneComb = reinterpret_cast<LPD3DXBONECOMBINATION>( pMeshContainer->pBoneCombinationBuf->GetBufferPointer
                                                                 () );
            for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
            {
                // first calculate all the world matrices
                for( iPaletteEntry = 0; iPaletteEntry < pMeshContainer->NumPaletteEntries; ++iPaletteEntry )
                {
                    iMatrixIndex = pBoneComb[iAttrib].BoneId[iPaletteEntry];
                    if( iMatrixIndex != UINT_MAX )
                    {
                        D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
                                            pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
                        V( pd3dDevice->SetTransform( D3DTS_WORLDMATRIX( iPaletteEntry ), &matTemp ) );
                    }
                }

                // setup the material of the mesh subset - REMEMBER to use the original pre-skinning attribute id to get the correct material id
                V( pd3dDevice->SetMaterial( &pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D ) );
                V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );

                // finally draw the subset with the current world matrix palette and material state
                V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );
            }

            // reset blending state
            V( pd3dDevice->SetRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE ) );
            V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, 0 ) );

            // remember to reset back to hw vertex processing if software was required
            if( pMeshContainer->UseSoftwareVP )
            {
                V( pd3dDevice->SetSoftwareVertexProcessing( FALSE ) );
            }
        }
        else if( g_SkinningMethod == D3DINDEXEDVS )
        {
            // Use COLOR instead of UBYTE4 since Geforce3 does not support it
            // vConst.w should be 3, but due to COLOR/UBYTE4 issue, mul by 255 and add epsilon
            D3DXVECTOR4 vConst( 1.0f, 0.0f, 0.0f, 765.01f );

            if( pMeshContainer->UseSoftwareVP )
            {
                // If hw or pure hw vertex processing is forced, we can't render the
                // mesh, so just exit out.  Typical applications should create
                // a device with appropriate vertex processing capability for this
                // skinning method.
                if( g_dwBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
                    return;

                V( pd3dDevice->SetSoftwareVertexProcessing( TRUE ) );
            }

            V( pd3dDevice->SetVertexShader( g_pIndexedVertexShader[pMeshContainer->NumInfl - 1] ) );

            pBoneComb = reinterpret_cast<LPD3DXBONECOMBINATION>( pMeshContainer->pBoneCombinationBuf->GetBufferPointer
                                                                 () );
            for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
            {
                // first calculate all the world matrices
                for( iPaletteEntry = 0; iPaletteEntry < pMeshContainer->NumPaletteEntries; ++iPaletteEntry )
                {
                    iMatrixIndex = pBoneComb[iAttrib].BoneId[iPaletteEntry];
                    if( iMatrixIndex != UINT_MAX )
                    {
                        D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
                                            pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
                        D3DXMatrixMultiplyTranspose( &matTemp, &matTemp, &g_matView );
                        V( pd3dDevice->SetVertexShaderConstantF( iPaletteEntry * 3 + 9, ( float* )&matTemp, 3 ) );
                    }
                }

                // Sum of all ambient and emissive contribution
                D3DXCOLOR color1( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Ambient );
                D3DXCOLOR color2( .25, .25, .25, 1.0 );
                D3DXCOLOR ambEmm;
                D3DXColorModulate( &ambEmm, &color1, &color2 );
                ambEmm += D3DXCOLOR( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Emissive );

                // set material color properties 
                V( pd3dDevice->SetVertexShaderConstantF( 8,
                                                         ( float* )&(
                                                         pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Diffuse ), 1 ) );
                V( pd3dDevice->SetVertexShaderConstantF( 7, ( float* )&ambEmm, 1 ) );
                vConst.y = pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Power;
                V( pd3dDevice->SetVertexShaderConstantF( 0, ( float* )&vConst, 1 ) );

                V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );

                // finally draw the subset with the current world matrix palette and material state
                V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );
            }

            // remember to reset back to hw vertex processing if software was required
            if( pMeshContainer->UseSoftwareVP )
            {
                V( pd3dDevice->SetSoftwareVertexProcessing( FALSE ) );
            }
            V( pd3dDevice->SetVertexShader( NULL ) );
        }
        else if( g_SkinningMethod == D3DINDEXEDHLSLVS )
        {
            if( pMeshContainer->UseSoftwareVP )
            {
                // If hw or pure hw vertex processing is forced, we can't render the
                // mesh, so just exit out.  Typical applications should create
                // a device with appropriate vertex processing capability for this
                // skinning method.
                if( g_dwBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
                    return;

                V( pd3dDevice->SetSoftwareVertexProcessing( TRUE ) );
            }

            pBoneComb = reinterpret_cast<LPD3DXBONECOMBINATION>( pMeshContainer->pBoneCombinationBuf->GetBufferPointer
                                                                 () );
            for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
            {
                // first calculate all the world matrices
                for( iPaletteEntry = 0; iPaletteEntry < pMeshContainer->NumPaletteEntries; ++iPaletteEntry )
                {
                    iMatrixIndex = pBoneComb[iAttrib].BoneId[iPaletteEntry];
                    if( iMatrixIndex != UINT_MAX )
                    {
                        D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
                                            pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
                        D3DXMatrixMultiply( &g_pBoneMatrices[iPaletteEntry], &matTemp, &g_matView );
                    }
                }
                V( g_pEffect->SetMatrixArray( "mWorldMatrixArray", g_pBoneMatrices,
                                              pMeshContainer->NumPaletteEntries ) );

                // Sum of all ambient and emissive contribution
                D3DXCOLOR color1( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Ambient );
                D3DXCOLOR color2( .25, .25, .25, 1.0 );
                D3DXCOLOR ambEmm;
                D3DXColorModulate( &ambEmm, &color1, &color2 );
                ambEmm += D3DXCOLOR( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Emissive );

                // set material color properties 
                V( g_pEffect->SetVector( "MaterialDiffuse",
                                         ( D3DXVECTOR4* )&(
                                         pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Diffuse ) ) );
                V( g_pEffect->SetVector( "MaterialAmbient", ( D3DXVECTOR4* )&ambEmm ) );

                // setup the material of the mesh subset - REMEMBER to use the original pre-skinning attribute id to get the correct material id
                V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );

                // Set CurNumBones to select the correct vertex shader for the number of bones
                V( g_pEffect->SetInt( "CurNumBones", pMeshContainer->NumInfl - 1 ) );

                // Start the effect now all parameters have been updated
                UINT numPasses;
                V( g_pEffect->Begin( &numPasses, D3DXFX_DONOTSAVESTATE ) );
                for( UINT iPass = 0; iPass < numPasses; iPass++ )
                {
                    V( g_pEffect->BeginPass( iPass ) );

                    // draw the subset with the current world matrix palette and material state
                    V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );

                    V( g_pEffect->EndPass() );
                }

                V( g_pEffect->End() );

                V( pd3dDevice->SetVertexShader( NULL ) );
            }

            // remember to reset back to hw vertex processing if software was required
            if( pMeshContainer->UseSoftwareVP )
            {
                V( pd3dDevice->SetSoftwareVertexProcessing( FALSE ) );
            }
        }
        else if( g_SkinningMethod == SOFTWARE )
        {
            D3DXMATRIX Identity;
            DWORD cBones = pMeshContainer->pSkinInfo->GetNumBones();
            DWORD iBone;
            PBYTE pbVerticesSrc;
            PBYTE pbVerticesDest;

            // set up bone transforms
            for( iBone = 0; iBone < cBones; ++iBone )
            {
                D3DXMatrixMultiply
                    (
                    &g_pBoneMatrices[iBone],                 // output
                    &pMeshContainer->pBoneOffsetMatrices[iBone],
                    pMeshContainer->ppBoneMatrixPtrs[iBone]
                    );
            }

            // set world transform
            D3DXMatrixIdentity( &Identity );
            V( pd3dDevice->SetTransform( D3DTS_WORLD, &Identity ) );

            V( pMeshContainer->pOrigMesh->LockVertexBuffer( D3DLOCK_READONLY, ( LPVOID* )&pbVerticesSrc ) );
            V( pMeshContainer->MeshData.pMesh->LockVertexBuffer( 0, ( LPVOID* )&pbVerticesDest ) );

            // generate skinned mesh
            pMeshContainer->pSkinInfo->UpdateSkinnedMesh( g_pBoneMatrices, NULL, pbVerticesSrc, pbVerticesDest );

            V( pMeshContainer->pOrigMesh->UnlockVertexBuffer() );
            V( pMeshContainer->MeshData.pMesh->UnlockVertexBuffer() );

            for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
            {
                V( pd3dDevice->SetMaterial( &(
                                            pMeshContainer->pMaterials[pMeshContainer->pAttributeTable[iAttrib].AttribId].MatD3D ) ) );
                V( pd3dDevice->SetTexture( 0,
                                           pMeshContainer->ppTextures[pMeshContainer->pAttributeTable[iAttrib].AttribId] ) );
                V( pMeshContainer->MeshData.pMesh->DrawSubset( pMeshContainer->pAttributeTable[iAttrib].AttribId ) );
            }
        }
        else // bug out as unsupported mode
        {
            return;
        }
    }
    else  // standard mesh, just draw it after setting material properties
    {
        V( pd3dDevice->SetTransform( D3DTS_WORLD, &pFrame->CombinedTransformationMatrix ) );

        for( iMaterial = 0; iMaterial < pMeshContainer->NumMaterials; iMaterial++ )
        {
            V( pd3dDevice->SetMaterial( &pMeshContainer->pMaterials[iMaterial].MatD3D ) );
            V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[iMaterial] ) );
            V( pMeshContainer->MeshData.pMesh->DrawSubset( iMaterial ) );
        }
    }
}




//--------------------------------------------------------------------------------------
// Called to render a frame in the hierarchy
//--------------------------------------------------------------------------------------
void DrawFrame( IDirect3DDevice9* pd3dDevice, LPD3DXFRAME pFrame )
{
    LPD3DXMESHCONTAINER pMeshContainer;

    pMeshContainer = pFrame->pMeshContainer;
    while( pMeshContainer != NULL )
    {
        DrawMeshContainer( pd3dDevice, pMeshContainer, pFrame );

        pMeshContainer = pMeshContainer->pNextMeshContainer;
    }

    if( pFrame->pFrameSibling != NULL )
    {
        DrawFrame( pd3dDevice, pFrame->pFrameSibling );
    }

    if( pFrame->pFrameFirstChild != NULL )
    {
        DrawFrame( pd3dDevice, pFrame->pFrameFirstChild );
    }
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

    // Clear the backbuffer
    pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                       D3DCOLOR_ARGB( 0, 66, 75, 121 ), 1.0f, 0L );

    // Setup the light
    D3DLIGHT9 light;
    D3DXVECTOR3 vecLightDirUnnormalized( 0.0f, -1.0f, 1.0f );
    ZeroMemory( &light, sizeof( D3DLIGHT9 ) );
    light.Type = D3DLIGHT_DIRECTIONAL;
    light.Diffuse.r = 1.0f;
    light.Diffuse.g = 1.0f;
    light.Diffuse.b = 1.0f;
    D3DXVec3Normalize( ( D3DXVECTOR3* )&light.Direction, &vecLightDirUnnormalized );
    light.Position.x = 0.0f;
    light.Position.y = -1.0f;
    light.Position.z = 1.0f;
    light.Range = 1000.0f;
    V( pd3dDevice->SetLight( 0, &light ) );
    V( pd3dDevice->LightEnable( 0, TRUE ) );

    // Set the projection matrix for the vertex shader based skinning method
    if( g_SkinningMethod == D3DINDEXEDVS )
    {
        V( pd3dDevice->SetVertexShaderConstantF( 2, ( float* )&g_matProjT, 4 ) );
    }
    else if( g_SkinningMethod == D3DINDEXEDHLSLVS )
    {
        V( g_pEffect->SetMatrix( "mViewProj", &g_matProj ) );
    }

    // Set Light for vertex shader
    D3DXVECTOR4 vLightDir( 0.0f, 1.0f, -1.0f, 0.0f );
    D3DXVec4Normalize( &vLightDir, &vLightDir );
    V( pd3dDevice->SetVertexShaderConstantF( 1, ( float* )&vLightDir, 1 ) );
    V( g_pEffect->SetVector( "lhtDir", &vLightDir ) );

    // Begin the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        DrawFrame( pd3dDevice, g_pFrameRoot );

        RenderText();
        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_SampleUI.OnRender( fElapsedTime ) );

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
    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) ); // Show FPS
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    // Output statistics
    switch( g_SkinningMethod )
    {
        case D3DNONINDEXED:
            txtHelper.DrawTextLine( L"Using fixed-function non-indexed skinning\n" );
            break;
        case D3DINDEXED:
            txtHelper.DrawTextLine( L"Using fixed-function indexed skinning\n" );
            break;
        case SOFTWARE:
            txtHelper.DrawTextLine( L"Using software skinning\n" );
            break;
        case D3DINDEXEDVS:
            txtHelper.DrawTextLine( L"Using assembly vertex shader indexed skinning\n" );
            break;
        case D3DINDEXEDHLSLVS:
            txtHelper.DrawTextLine( L"Using HLSL vertex shader indexed skinning\n" );
            break;
        default:
            txtHelper.DrawTextLine( L"No skinning\n" );
    }

    // Draw help
    if( g_bShowHelp )
    {
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 6 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls (F1 to hide):" );

        txtHelper.SetInsertionPos( 40, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Rotate model: Left click drag\n"
                                L"Zoom: Middle click drag\n"
                                L"Pane: Right click drag\n"
                                L"Quit: ESC" );
    }
    else
    {
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 2 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Press F1 for help" );
    }

    // If software vp is required and we are using a hwvp device,
    // the mesh is not being displayed and we output an error message here.
    if( g_bUseSoftwareVP && ( g_dwBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING ) )
    {
        txtHelper.SetInsertionPos( 5, 85 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"The HWVP device does not support this skinning method.\n"
                                L"Select another skinning method or switch to mixed or software VP." );
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

    // Pass all remaining windows messages to arcball so it can respond to user input
    g_ArcBall.HandleMessages( hWnd, uMsg, wParam, lParam );

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
        case IDC_METHOD:
        {
            METHOD NewSkinningMethod = ( METHOD )( size_t )( ( CDXUTComboBox* )pControl )->GetSelectedData();

            // If the selected skinning method is different than the current one
            if( g_SkinningMethod != NewSkinningMethod )
            {
                g_SkinningMethod = NewSkinningMethod;

                // update the meshes to the new skinning method
                UpdateSkinningMethod( g_pFrameRoot );
            }
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
    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pEffect )
        g_pEffect->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );

    // Release the vertex shaders
    for( DWORD iInfl = 0; iInfl < 4; ++iInfl )
        SAFE_RELEASE( g_pIndexedVertexShader[iInfl] );
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

    CAllocateHierarchy Alloc;
    D3DXFrameDestroy( g_pFrameRoot, &Alloc );
    SAFE_RELEASE( g_pAnimController );
}


//--------------------------------------------------------------------------------------
// Called to setup the pointers for a given bone to its transformation matrix
//--------------------------------------------------------------------------------------
HRESULT SetupBoneMatrixPointersOnMesh( LPD3DXMESHCONTAINER pMeshContainerBase )
{
    UINT iBone, cBones;
    D3DXFRAME_DERIVED* pFrame;

    D3DXMESHCONTAINER_DERIVED* pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainerBase;

    // if there is a skinmesh, then setup the bone matrices
    if( pMeshContainer->pSkinInfo != NULL )
    {
        cBones = pMeshContainer->pSkinInfo->GetNumBones();

        pMeshContainer->ppBoneMatrixPtrs = new D3DXMATRIX*[cBones];
        if( pMeshContainer->ppBoneMatrixPtrs == NULL )
            return E_OUTOFMEMORY;

        for( iBone = 0; iBone < cBones; iBone++ )
        {
            pFrame = ( D3DXFRAME_DERIVED* )D3DXFrameFind( g_pFrameRoot,
                                                          pMeshContainer->pSkinInfo->GetBoneName( iBone ) );
            if( pFrame == NULL )
                return E_FAIL;

            pMeshContainer->ppBoneMatrixPtrs[iBone] = &pFrame->CombinedTransformationMatrix;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Called to setup the pointers for a given bone to its transformation matrix
//--------------------------------------------------------------------------------------
HRESULT SetupBoneMatrixPointers( LPD3DXFRAME pFrame )
{
    HRESULT hr;

    if( pFrame->pMeshContainer != NULL )
    {
        hr = SetupBoneMatrixPointersOnMesh( pFrame->pMeshContainer );
        if( FAILED( hr ) )
            return hr;
    }

    if( pFrame->pFrameSibling != NULL )
    {
        hr = SetupBoneMatrixPointers( pFrame->pFrameSibling );
        if( FAILED( hr ) )
            return hr;
    }

    if( pFrame->pFrameFirstChild != NULL )
    {
        hr = SetupBoneMatrixPointers( pFrame->pFrameFirstChild );
        if( FAILED( hr ) )
            return hr;
    }

    return S_OK;
}




//--------------------------------------------------------------------------------------
// update the frame matrices
//--------------------------------------------------------------------------------------
void UpdateFrameMatrices( LPD3DXFRAME pFrameBase, LPD3DXMATRIX pParentMatrix )
{
    D3DXFRAME_DERIVED* pFrame = ( D3DXFRAME_DERIVED* )pFrameBase;

    if( pParentMatrix != NULL )
        D3DXMatrixMultiply( &pFrame->CombinedTransformationMatrix, &pFrame->TransformationMatrix, pParentMatrix );
    else
        pFrame->CombinedTransformationMatrix = pFrame->TransformationMatrix;

    if( pFrame->pFrameSibling != NULL )
    {
        UpdateFrameMatrices( pFrame->pFrameSibling, pParentMatrix );
    }

    if( pFrame->pFrameFirstChild != NULL )
    {
        UpdateFrameMatrices( pFrame->pFrameFirstChild, &pFrame->CombinedTransformationMatrix );
    }
}


//--------------------------------------------------------------------------------------
// update the skinning method
//--------------------------------------------------------------------------------------
void UpdateSkinningMethod( LPD3DXFRAME pFrameBase )
{
    D3DXFRAME_DERIVED* pFrame = ( D3DXFRAME_DERIVED* )pFrameBase;
    D3DXMESHCONTAINER_DERIVED* pMeshContainer;

    pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pFrame->pMeshContainer;

    while( pMeshContainer != NULL )
    {
        GenerateSkinnedMesh( DXUTGetD3D9Device(), pMeshContainer );

        pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainer->pNextMeshContainer;
    }

    if( pFrame->pFrameSibling != NULL )
    {
        UpdateSkinningMethod( pFrame->pFrameSibling );
    }

    if( pFrame->pFrameFirstChild != NULL )
    {
        UpdateSkinningMethod( pFrame->pFrameFirstChild );
    }
}


void ReleaseAttributeTable( LPD3DXFRAME pFrameBase )
{
    D3DXFRAME_DERIVED* pFrame = ( D3DXFRAME_DERIVED* )pFrameBase;
    D3DXMESHCONTAINER_DERIVED* pMeshContainer;

    pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pFrame->pMeshContainer;

    while( pMeshContainer != NULL )
    {
        delete[] pMeshContainer->pAttributeTable;

        pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainer->pNextMeshContainer;
    }

    if( pFrame->pFrameSibling != NULL )
    {
        ReleaseAttributeTable( pFrame->pFrameSibling );
    }

    if( pFrame->pFrameFirstChild != NULL )
    {
        ReleaseAttributeTable( pFrame->pFrameFirstChild );
    }
}
