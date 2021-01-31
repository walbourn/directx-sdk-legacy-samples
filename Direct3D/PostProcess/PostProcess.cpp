//--------------------------------------------------------------------------------------
// File: PostProcess.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"
#include <stdio.h>


//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


// PPCOUNT is the number of postprocess effects supported
#define PPCOUNT (sizeof(g_aszFxFile) / sizeof(g_aszFxFile[0]))


// NUM_PARAMS is the maximum number of changeable parameters supported
// in an effect.
#define NUM_PARAMS 2


// RT_COUNT is the number of simultaneous render targets used in the sample.
#define RT_COUNT 3


// Name of the postprocess .fx files
LPCWSTR g_aszFxFile[] =
{
    L"PP_ColorMonochrome.fx",
    L"PP_ColorInverse.fx",
    L"PP_ColorGBlurH.fx",
    L"PP_ColorGBlurV.fx",
    L"PP_ColorBloomH.fx",
    L"PP_ColorBloomV.fx",
    L"PP_ColorBrightPass.fx",
    L"PP_ColorToneMap.fx",
    L"PP_ColorEdgeDetect.fx",
    L"PP_ColorDownFilter4.fx",
    L"PP_ColorUpFilter4.fx",
    L"PP_ColorCombine.fx",
    L"PP_ColorCombine4.fx",
    L"PP_NormalEdgeDetect.fx",
    L"PP_DofCombine.fx",
    L"PP_NormalMap.fx",
    L"PP_PositionMap.fx",
};


// Description of each postprocess supported
LPCWSTR g_aszPpDesc[] =
{
    L"[Color] Monochrome",
    L"[Color] Inversion",
    L"[Color] Gaussian Blur Horizontal",
    L"[Color] Gaussian Blur Vertical",
    L"[Color] Bloom Horizontal",
    L"[Color] Bloom Vertical",
    L"[Color] Bright Pass",
    L"[Color] Tone Mapping",
    L"[Color] Edge Detection",
    L"[Color] Down Filter 4x",
    L"[Color] Up Filter 4x",
    L"[Color] Combine",
    L"[Color] Combine 4x",
    L"[Normal] Edge Detection",
    L"DOF Combine",
    L"Normal Map",
    L"Position Map",
};


//--------------------------------------------------------------------------------------
// This is the vertex format used for the meshes.
struct MESHVERT
{
    float x, y, z;      // Position
    float nx, ny, nz;   // Normal
    float tu, tv;       // Texcoord

    const static D3DVERTEXELEMENT9 Decl[4];
};

const D3DVERTEXELEMENT9 MESHVERT::Decl[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
// This is the vertex format used for the skybox.
struct SKYBOXVERT
{
    float x, y, z;      // Position
    D3DXVECTOR3 tex;    // Texcoord

    const static D3DVERTEXELEMENT9 Decl[3];
};

const D3DVERTEXELEMENT9 SKYBOXVERT::Decl[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
// This is the vertex format used with the quad during post-process.
struct PPVERT
{
    float x, y, z, rhw;
    float tu, tv;       // Texcoord for post-process source
    float tu2, tv2;     // Texcoord for the original scene

    const static D3DVERTEXELEMENT9 Decl[4];
};

// Vertex declaration for post-processing
const D3DVERTEXELEMENT9 PPVERT::Decl[4] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  1 },
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
// struct CPostProcess
// A struct that encapsulates aspects of a render target postprocess
// technique.
//--------------------------------------------------------------------------------------
struct CPostProcess
{
    LPD3DXEFFECT m_pEffect;              // Effect object for this technique
    D3DXHANDLE m_hTPostProcess;        // PostProcess technique handle
    int m_nRenderTarget;        // Render target channel this PP outputs
    D3DXHANDLE  m_hTexSource[4];        // Handle to the post-process source textures
    D3DXHANDLE  m_hTexScene[4];         // Handle to the saved scene texture
    bool        m_bWrite[4];            // Indicates whether the post-process technique
    //   outputs data for this render target.
    WCHAR       m_awszParamName
        [NUM_PARAMS][MAX_PATH]; // Names of changeable parameters
    WCHAR       m_awszParamDesc
        [NUM_PARAMS][MAX_PATH]; // Description of parameters
    D3DXHANDLE  m_ahParam[NUM_PARAMS];  // Handles to the changeable parameters
    int         m_anParamSize[NUM_PARAMS];// Size of the parameter. Indicates
    // how many components of float4
    // are used.
    D3DXVECTOR4 m_avParamDef[NUM_PARAMS]; // Parameter default

public:
                CPostProcess() : m_pEffect( NULL ),
                                 m_hTPostProcess( NULL ),
                                 m_nRenderTarget( 0 )
                {
                    ZeroMemory( m_hTexSource, sizeof( m_hTexSource ) );
                    ZeroMemory( m_hTexScene, sizeof( m_hTexScene ) );
                    ZeroMemory( m_bWrite, sizeof( m_bWrite ) );
                    ZeroMemory( m_ahParam, sizeof( m_ahParam ) );
                    ZeroMemory( m_awszParamName, sizeof( m_awszParamName ) );
                    ZeroMemory( m_awszParamDesc, sizeof( m_awszParamDesc ) );
                    ZeroMemory( m_anParamSize, sizeof( m_anParamSize ) );
                    ZeroMemory( m_avParamDef, sizeof( m_avParamDef ) );
                }
                ~CPostProcess()
                {
                    Cleanup();
                }
    HRESULT     Init( LPDIRECT3DDEVICE9 pDev, DWORD dwShaderFlags, LPCWSTR wszName )
    {
        HRESULT hr;
        WCHAR wszPath[MAX_PATH];

        if( FAILED( hr = DXUTFindDXSDKMediaFileCch( wszPath, MAX_PATH, wszName ) ) )
            return hr;
        hr = D3DXCreateEffectFromFile( pDev,
                                       wszPath,
                                       NULL,
                                       NULL,
                                       dwShaderFlags,
                                       NULL,
                                       &m_pEffect,
                                       NULL );
        if( FAILED( hr ) )
            return hr;

        // Get the PostProcess technique handle
        m_hTPostProcess = m_pEffect->GetTechniqueByName( "PostProcess" );

        // Obtain the handles to all texture objects in the effect
        m_hTexScene[0] = m_pEffect->GetParameterByName( NULL, "g_txSceneColor" );
        m_hTexScene[1] = m_pEffect->GetParameterByName( NULL, "g_txSceneNormal" );
        m_hTexScene[2] = m_pEffect->GetParameterByName( NULL, "g_txScenePosition" );
        m_hTexScene[3] = m_pEffect->GetParameterByName( NULL, "g_txSceneVelocity" );
        m_hTexSource[0] = m_pEffect->GetParameterByName( NULL, "g_txSrcColor" );
        m_hTexSource[1] = m_pEffect->GetParameterByName( NULL, "g_txSrcNormal" );
        m_hTexSource[2] = m_pEffect->GetParameterByName( NULL, "g_txSrcPosition" );
        m_hTexSource[3] = m_pEffect->GetParameterByName( NULL, "g_txSrcVelocity" );

        // Find out what render targets the technique writes to.
        D3DXTECHNIQUE_DESC techdesc;
        if( FAILED( m_pEffect->GetTechniqueDesc( m_hTPostProcess, &techdesc ) ) )
            return D3DERR_INVALIDCALL;

        for( DWORD i = 0; i < techdesc.Passes; ++i )
        {
            D3DXPASS_DESC passdesc;
            if( SUCCEEDED( m_pEffect->GetPassDesc( m_pEffect->GetPass( m_hTPostProcess, i ), &passdesc ) ) )
            {
                D3DXSEMANTIC aSem[MAXD3DDECLLENGTH];
                UINT uCount;
                if( SUCCEEDED( D3DXGetShaderOutputSemantics( passdesc.pPixelShaderFunction, aSem, &uCount ) ) )
                {
                    // Semantics received. Now examine the content and
                    // find out which render target this technique
                    // writes to.
                    while( uCount-- )
                    {
                        if( D3DDECLUSAGE_COLOR == aSem[uCount].Usage &&
                            RT_COUNT > aSem[uCount].UsageIndex )
                            m_bWrite[uCount] = true;
                    }
                }
            }
        }

        // Obtain the render target channel
        D3DXHANDLE hAnno;
        hAnno = m_pEffect->GetAnnotationByName( m_hTPostProcess, "nRenderTarget" );
        if( hAnno )
            m_pEffect->GetInt( hAnno, &m_nRenderTarget );

        // Obtain the handles to the changeable parameters, if any.
        for( int i = 0; i < NUM_PARAMS; ++i )
        {
            char szName[32];

            sprintf_s( szName, 32, "Parameter%d", i );
            hAnno = m_pEffect->GetAnnotationByName( m_hTPostProcess, szName );
            LPCSTR szParamName;
            if( hAnno &&
                SUCCEEDED( m_pEffect->GetString( hAnno, &szParamName ) ) )
            {
                m_ahParam[i] = m_pEffect->GetParameterByName( NULL, szParamName );
                MultiByteToWideChar( CP_ACP, 0, szParamName, -1, m_awszParamName[i], MAX_PATH );
            }

            // Get the parameter description
            sprintf_s( szName, 32, "Parameter%dDesc", i );
            hAnno = m_pEffect->GetAnnotationByName( m_hTPostProcess, szName );
            if( hAnno &&
                SUCCEEDED( m_pEffect->GetString( hAnno, &szParamName ) ) )
            {
                MultiByteToWideChar( CP_ACP, 0, szParamName, -1, m_awszParamDesc[i], MAX_PATH );
            }

            // Get the parameter size
            sprintf_s( szName, 32, "Parameter%dSize", i );
            hAnno = m_pEffect->GetAnnotationByName( m_hTPostProcess, szName );
            if( hAnno )
                m_pEffect->GetInt( hAnno, &m_anParamSize[i] );

            // Get the parameter default
            sprintf_s( szName, 32, "Parameter%dDef", i );
            hAnno = m_pEffect->GetAnnotationByName( m_hTPostProcess, szName );
            if( hAnno )
                m_pEffect->GetVector( hAnno, &m_avParamDef[i] );
        }

        return S_OK;
    }
    void        Cleanup()
    {
        SAFE_RELEASE( m_pEffect );
    }
    HRESULT     OnLostDevice()
    {
        assert( m_pEffect );
        m_pEffect->OnLostDevice();
        return S_OK;
    }
    HRESULT     OnResetDevice( DWORD dwWidth, DWORD dwHeight )
    {
        assert( m_pEffect );
        m_pEffect->OnResetDevice();

        // If one or more kernel exists, convert kernel from
        // pixel space to texel space.

        // First check for kernels.  Kernels are identified by
        // having a string annotation of name "ConvertPixelsToTexels"
        D3DXHANDLE hParamToConvert;
        D3DXHANDLE hAnnotation;
        UINT uParamIndex = 0;
        // If a top-level parameter has the "ConvertPixelsToTexels" annotation,
        // do the conversion.
        while( NULL != ( hParamToConvert = m_pEffect->GetParameter( NULL, uParamIndex++ ) ) )
        {
            if( NULL != ( hAnnotation = m_pEffect->GetAnnotationByName( hParamToConvert, "ConvertPixelsToTexels" ) ) )
            {
                LPCSTR szSource;
                m_pEffect->GetString( hAnnotation, &szSource );
                D3DXHANDLE hConvertSource = m_pEffect->GetParameterByName( NULL, szSource );

                if( hConvertSource )
                {
                    // Kernel source exists. Proceed.
                    // Retrieve the kernel size
                    D3DXPARAMETER_DESC desc;
                    m_pEffect->GetParameterDesc( hConvertSource, &desc );
                    // Each element has 2 floats
                    DWORD cKernel = desc.Bytes / ( 2 * sizeof( float ) );
                    D3DXVECTOR4* pvKernel = new D3DXVECTOR4[cKernel];
                    if( !pvKernel )
                        return E_OUTOFMEMORY;
                    m_pEffect->GetVectorArray( hConvertSource, pvKernel, cKernel );
                    // Convert
                    for( DWORD i = 0; i < cKernel; ++i )
                    {
                        pvKernel[i].x = pvKernel[i].x / dwWidth;
                        pvKernel[i].y = pvKernel[i].y / dwHeight;
                    }
                    // Copy back
                    m_pEffect->SetVectorArray( hParamToConvert, pvKernel, cKernel );

                    delete[] pvKernel;
                }
            }
        }

        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// struct CPProcInstance
// A class that represents an instance of a post-process to be applied
// to the scene.
//--------------------------------------------------------------------------------------
struct CPProcInstance
{
    D3DXVECTOR4 m_avParam[NUM_PARAMS];
    int m_nFxIndex;

public:
                CPProcInstance() : m_nFxIndex( -1 )
                {
                    ZeroMemory( m_avParam, sizeof( m_avParam ) );
                }
};


struct CRenderTargetChain
{
    int m_nNext;
    bool m_bFirstRender;
    LPDIRECT3DTEXTURE9  m_pRenderTarget[2];

public:
                        CRenderTargetChain() : m_nNext( 0 ),
                                               m_bFirstRender( true )
                        {
                            ZeroMemory( m_pRenderTarget, sizeof( m_pRenderTarget ) );
                        }

                        ~CRenderTargetChain()
                        {
                            Cleanup();
                        }

    void                Init( LPDIRECT3DTEXTURE9* pRT )
    {
        for( int i = 0; i < 2; ++i )
        {
            m_pRenderTarget[i] = pRT[i];
            m_pRenderTarget[i]->AddRef();
        }
    }

    void                Cleanup()
    {
        SAFE_RELEASE( m_pRenderTarget[0] );
        SAFE_RELEASE( m_pRenderTarget[1] );
    }

    void                Flip()
    {
        m_nNext = 1 - m_nNext;
    };

    LPDIRECT3DTEXTURE9  GetPrevTarget()
    {
        return m_pRenderTarget[1 - m_nNext];
    }
    LPDIRECT3DTEXTURE9  GetPrevSource()
    {
        return m_pRenderTarget[m_nNext];
    }
    LPDIRECT3DTEXTURE9  GetNextTarget()
    {
        return m_pRenderTarget[m_nNext];
    }
    LPDIRECT3DTEXTURE9  GetNextSource()
    {
        return m_pRenderTarget[1 - m_nNext];
    }
};


// An CRenderTargetSet object dictates what render targets
// to use in a pass of scene rendering.
struct CRenderTargetSet
{
    IDirect3DSurface9* pRT[RT_COUNT];
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                      g_pFont = NULL;          // Font for drawing text
ID3DXSprite*                    g_pTextSprite = NULL;    // Sprite for batching draw text calls
ID3DXEffect*                    g_pEffect = NULL;        // D3DX effect interface
CModelViewerCamera              g_Camera;                // A model viewing camera
bool                            g_bShowHelp = true;      // If true, it renders the UI control text
CDXUTDialogResourceManager      g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                 g_SettingsDlg;          // Device settings dialog
CDXUTDialog                     g_HUD;                   // dialog for standard controls
CDXUTDialog                     g_SampleUI;              // dialog for sample specific controls
IDirect3DCubeTexture9*          g_pEnvTex;               // Texture for environment mapping
IDirect3DVertexDeclaration9*    g_pVertDecl = NULL; // Vertex decl for scene rendering
IDirect3DVertexDeclaration9*    g_pSkyBoxDecl = NULL; // Vertex decl for Skybox rendering
IDirect3DVertexDeclaration9*    g_pVertDeclPP = NULL; // Vertex decl for post-processing
D3DXMATRIXA16                   g_mMeshWorld;            // World matrix (xlate and scale) for the mesh
int                             g_nScene = 0;            // Indicates the scene # to render
CPostProcess g_aPostProcess[PPCOUNT]; // Effect object for postprocesses
D3DXHANDLE                      g_hTRenderScene;         // Handle to RenderScene technique
D3DXHANDLE                      g_hTRenderEnvMapScene;   // Handle to RenderEnvMapScene technique
D3DXHANDLE                      g_hTRenderNoLight;       // Handle to RenderNoLight technique
D3DXHANDLE                      g_hTRenderSkyBox;        // Handle to RenderSkyBox technique
CDXUTXFileMesh g_SceneMesh[2];          // Mesh objects in the scene
CDXUTXFileMesh                  g_Skybox;                // Skybox mesh
D3DFORMAT                       g_TexFormat;             // Render target texture format
IDirect3DTexture9*              g_pSceneSave[RT_COUNT];  // To save original scene image before postprocess
CRenderTargetChain g_RTChain[RT_COUNT];     // Render target chain (4 used in sample)
bool                            g_bEnablePostProc = true;// Whether or not to enable post-processing

int                             g_nPasses = 0;           // Number of passes required to render scene
int                             g_nRtUsed = 0;           // Number of simultaneous RT used to render scene
CRenderTargetSet g_aRtTable[RT_COUNT];    // Table of which RT to use for all passes


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_SELECTSCENE         5
#define IDC_AVAILABLELIST       6
#define IDC_ACTIVELIST          7
#define IDC_AVAILABLELISTLABEL  8
#define IDC_ACTIVELISTLABEL     9
#define IDC_PARAM0NAME          10
#define IDC_PARAM1NAME          11
#define IDC_PARAM0              12
#define IDC_PARAM1              13
#define IDC_MOVEUP              14
#define IDC_MOVEDOWN            15
#define IDC_CLEAR               16
#define IDC_ENABLEPP            17
#define IDC_PRESETLABEL         18
#define IDC_PREBLUR             19
#define IDC_PREBLOOM            20
#define IDC_PREDOF              21
#define IDC_PREEDGE             22


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
HRESULT PerformSinglePostProcess( CPostProcess& PP, CPProcInstance& Inst, LPDIRECT3DVERTEXBUFFER9 pVB, PPVERT* aQuad,
                                  float& fExtentX, float& fExtentY );
HRESULT PerformPostProcess( IDirect3DDevice9* pd3dDevice );
HRESULT ComputeMeshWorldMatrix( LPD3DXMESH pMesh );


//--------------------------------------------------------------------------------------
// Empty the active effect list except the last (blank) item.
void ClearActiveList()
{
    // Clear all items in the active list except the last one.

    CDXUTListBox* pListBox = g_SampleUI.GetListBox( IDC_ACTIVELIST );
    if( !pListBox )
        return;

    int i = ( int )pListBox->GetSize() - 1;
    while( --i >= 0 )
    {
        DXUTListBoxItem* pItem = pListBox->GetItem( 0 );
        delete pItem->pData;
        pListBox->RemoveItem( 0 );
    }
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

    DXUTCreateWindow( L"PostProcess" );


    // This sample does not do multisampling.
    CGrowableArray <D3DMULTISAMPLE_TYPE>* pMSType = DXUTGetD3D9Enumeration()->GetPossibleMultisampleTypeList();
    pMSType->RemoveAll();
    pMSType->Add( D3DMULTISAMPLE_NONE );

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );

    InitApp();

    // Initialize DXUT and create the desired Win32 window and Direct3D 
    // device for the application. Calling each of these functions is optional, but they
    // allow you to set several options which control the behavior of the framework.
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the defaul hotkeys
    DXUTCreateDevice( true, 640, 480 );

    // Pass control to DXUT for handling the message pump and 
    // dispatching render calls. DXUT will call your FrameMove 
    // and FrameRender callback when there is idle time between handling window messages.
    DXUTMainLoop();

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.
    ClearActiveList();

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

    g_HUD.SetCallback( OnGUIEvent ); int iY = 0;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 5;
    g_SampleUI.EnableCaption( true );
    g_SampleUI.SetCaptionText( L"Effect Manager" );
    g_SampleUI.SetBackgroundColors( D3DCOLOR_ARGB( 100, 255, 255, 255 ) );

    // Initialize sample-specific UI controls
    g_SampleUI.AddStatic( IDC_AVAILABLELISTLABEL, L"Available effects (Dbl click inserts effect):", 10, iY, 210, 16 );
    g_SampleUI.GetStatic( IDC_AVAILABLELISTLABEL )->GetElement( 0 )->dwTextFormat = DT_LEFT | DT_TOP;
    CDXUTListBox* pListBox;
    g_SampleUI.AddListBox( IDC_AVAILABLELIST, 10, iY += 18, 200, 82, 0, &pListBox );
    if( pListBox )
    {
        // Populate ListBox items
        for( int i = 0; i < PPCOUNT; ++i )
            pListBox->AddItem( g_aszPpDesc[i], ( LPVOID )( size_t )i );
    }
    g_SampleUI.AddStatic( IDC_ACTIVELISTLABEL, L"Active effects (Dbl click removes effect):", 10, iY += 87, 210, 16 );
    g_SampleUI.GetStatic( IDC_ACTIVELISTLABEL )->GetElement( 0 )->dwTextFormat = DT_LEFT | DT_TOP;
    g_SampleUI.AddListBox( IDC_ACTIVELIST, 10, iY += 18, 200, 82, 0, &pListBox );
    if( pListBox )
    {
        // Add a blank entry for users to add effect to the end of list.
        pListBox->AddItem( L"", NULL );
    }
    g_SampleUI.AddButton( IDC_MOVEUP, L"Move Up", 0, iY += 92, 70, 22 );
    g_SampleUI.AddButton( IDC_MOVEDOWN, L"Move Down", 72, iY, 75, 22 );
    g_SampleUI.AddButton( IDC_CLEAR, L"Clear All", 149, iY, 65, 22 );
    g_SampleUI.AddStatic( IDC_PARAM0NAME, L"Select an active effect to set its parameter.", 5, iY += 24, 215, 15 );
    g_SampleUI.GetStatic( IDC_PARAM0NAME )->GetElement( 0 )->dwTextFormat = DT_LEFT | DT_TOP;
    CDXUTEditBox* pEditBox;
    g_SampleUI.AddEditBox( IDC_PARAM0, L"", 5, iY += 15, 210, 20, false, &pEditBox );
    if( pEditBox )
    {
        pEditBox->SetBorderWidth( 1 );
        pEditBox->SetSpacing( 2 );
    }
    g_SampleUI.AddStatic( IDC_PARAM1NAME, L"Select an active effect to set its parameter.", 5, iY += 20, 215, 15 );
    g_SampleUI.GetStatic( IDC_PARAM1NAME )->GetElement( 0 )->dwTextFormat = DT_LEFT | DT_TOP;
    g_SampleUI.AddEditBox( IDC_PARAM1, L"", 5, iY += 15, 210, 20, false, &pEditBox );
    if( pEditBox )
    {
        pEditBox->SetBorderWidth( 1 );
        pEditBox->SetSpacing( 2 );
    }

    // Disable the edit boxes and 2nd static control by default.
    g_SampleUI.GetControl( IDC_PARAM0 )->SetEnabled( false );
    g_SampleUI.GetControl( IDC_PARAM0 )->SetVisible( false );
    g_SampleUI.GetControl( IDC_PARAM1 )->SetEnabled( false );
    g_SampleUI.GetControl( IDC_PARAM1 )->SetVisible( false );
    g_SampleUI.GetControl( IDC_PARAM1NAME )->SetEnabled( false );
    g_SampleUI.GetControl( IDC_PARAM1NAME )->SetVisible( false );

    g_SampleUI.AddCheckBox( IDC_ENABLEPP, L"(E)nable post-processing", 5, iY += 25, 200, 24, true, L'E' );

    CDXUTComboBox* pComboBox;
    g_SampleUI.AddComboBox( IDC_SELECTSCENE, 5, iY += 25, 210, 24, L'C', false, &pComboBox );
    if( pComboBox )
    {
        pComboBox->AddItem( L"(C)urrent Mesh: Dwarf", ( LPVOID )0 );
        pComboBox->AddItem( L"(C)urrent Mesh: Skull", ( LPVOID )1 );
    }

    g_SampleUI.AddStatic( IDC_PRESETLABEL, L"Predefined post-process combinations:", 5, iY += 28, 210, 22 );
    g_SampleUI.GetControl( IDC_PRESETLABEL )->GetElement( 0 )->dwTextFormat = DT_LEFT | DT_TOP;
    g_SampleUI.AddButton( IDC_PREBLUR, L"Blur", 5, iY += 22, 100, 22 );
    g_SampleUI.AddButton( IDC_PREBLOOM, L"Bloom", 115, iY, 100, 22 );
    g_SampleUI.AddButton( IDC_PREDOF, L"Depth of Field", 5, iY += 24, 100, 22 );
    g_SampleUI.AddButton( IDC_PREEDGE, L"Edge Glow", 115, iY, 100, 22 );

    // Initialize camera parameters
    g_Camera.SetModelCenter( D3DXVECTOR3( 0.0f, 0.0f, 0.0f ) );
    g_Camera.SetRadius( 2.8f );
    g_Camera.SetEnablePositionMovement( false );
}


//--------------------------------------------------------------------------------------
// Compute the translate and scale transform for the current mesh.
HRESULT ComputeMeshWorldMatrix( LPD3DXMESH pMesh )
{
    LPDIRECT3DVERTEXBUFFER9 pVB;
    if( FAILED( pMesh->GetVertexBuffer( &pVB ) ) )
        return E_FAIL;

    LPVOID pVBData;
    if( SUCCEEDED( pVB->Lock( 0, 0, &pVBData, D3DLOCK_READONLY ) ) )
    {
        D3DXVECTOR3 vCtr;
        float fRadius;
        D3DXComputeBoundingSphere( ( D3DXVECTOR3* )pVBData, pMesh->GetNumVertices(),
                                   D3DXGetFVFVertexSize( pMesh->GetFVF() ),
                                   &vCtr, &fRadius );

        D3DXMatrixTranslation( &g_mMeshWorld, -vCtr.x, -vCtr.y, -vCtr.z );
        D3DXMATRIXA16 m;
        D3DXMatrixScaling( &m, 1 / fRadius, 1 / fRadius, 1 / fRadius );
        D3DXMatrixMultiply( &g_mMeshWorld, &g_mMeshWorld, &m );

        pVB->Unlock();
    }

    SAFE_RELEASE( pVB );

    return S_OK;
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

    // Check 32 bit integer format support
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_CUBETEXTURE, D3DFMT_A8R8G8B8 ) ) )
        return false;

    // Must support pixel shader 2.0
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

    // Turn vsync off
    pDeviceSettings->d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

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
    // Query multiple RT setting and set the num of passes required
    D3DCAPS9 Caps;
    pd3dDevice->GetDeviceCaps( &Caps );
    if( Caps.NumSimultaneousRTs > 2 )
    {
        // One pass of 3 RTs
        g_nPasses = 1;
        g_nRtUsed = 3;
    }
    else if( Caps.NumSimultaneousRTs > 1 )
    {
        // Two passes of 2 RTs. The 2nd pass uses only one.
        g_nPasses = 2;
        g_nRtUsed = 2;
    }
    else
    {
        // Three passes of single RT.
        g_nPasses = 3;
        g_nRtUsed = 1;
    }

    // Determine which of D3DFMT_A16B16G16R16F or D3DFMT_A8R8G8B8
    // to use for scene-rendering RTs.
    IDirect3D9* pD3D;
    pd3dDevice->GetDirect3D( &pD3D );
    D3DDISPLAYMODE DisplayMode;
    pd3dDevice->GetDisplayMode( 0, &DisplayMode );

    if( FAILED( pD3D->CheckDeviceFormat( Caps.AdapterOrdinal, Caps.DeviceType,
                                         DisplayMode.Format, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) ) )
        g_TexFormat = D3DFMT_A8R8G8B8;
    else
        g_TexFormat = D3DFMT_A16B16G16R16F;

    SAFE_RELEASE( pD3D );

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
    DWORD dwShaderFlags = 0;

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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Scene.fx" ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );

    // Initialize the postprocess objects
    for( int i = 0; i < PPCOUNT; ++i )
    {
        hr = g_aPostProcess[i].Init( pd3dDevice, dwShaderFlags, g_aszFxFile[i] );
        if( FAILED( hr ) )
            return hr;
    }

    // Obtain the technique handles
    switch( g_nPasses )
    {
        case 1:
            g_hTRenderScene = g_pEffect->GetTechniqueByName( "RenderScene" );
            g_hTRenderEnvMapScene = g_pEffect->GetTechniqueByName( "RenderEnvMapScene" );
            g_hTRenderSkyBox = g_pEffect->GetTechniqueByName( "RenderSkyBox" );
            break;
        case 2:
            g_hTRenderScene = g_pEffect->GetTechniqueByName( "RenderSceneTwoPasses" );
            g_hTRenderEnvMapScene = g_pEffect->GetTechniqueByName( "RenderEnvMapSceneTwoPasses" );
            g_hTRenderSkyBox = g_pEffect->GetTechniqueByName( "RenderSkyBoxTwoPasses" );
            break;
        case 3:
            g_hTRenderScene = g_pEffect->GetTechniqueByName( "RenderSceneThreePasses" );
            g_hTRenderEnvMapScene = g_pEffect->GetTechniqueByName( "RenderEnvMapSceneThreePasses" );
            g_hTRenderSkyBox = g_pEffect->GetTechniqueByName( "RenderSkyBoxThreePasses" );
            break;
    }
    g_hTRenderNoLight = g_pEffect->GetTechniqueByName( "RenderNoLight" );

    // Create vertex declaration for scene
    if( FAILED( hr = pd3dDevice->CreateVertexDeclaration( MESHVERT::Decl, &g_pVertDecl ) ) )
    {
        return hr;
    }

    // Create vertex declaration for skybox
    if( FAILED( hr = pd3dDevice->CreateVertexDeclaration( SKYBOXVERT::Decl, &g_pSkyBoxDecl ) ) )
    {
        return hr;
    }

    // Create vertex declaration for post-process
    if( FAILED( hr = pd3dDevice->CreateVertexDeclaration( PPVERT::Decl, &g_pVertDeclPP ) ) )
    {
        return hr;
    }

    // Load the meshes
    if( FAILED( g_SceneMesh[0].Create( pd3dDevice, L"dwarf\\dwarf.x" ) ) )
        return DXUTERR_MEDIANOTFOUND;
    g_SceneMesh[0].SetVertexDecl( pd3dDevice, MESHVERT::Decl );
    if( FAILED( g_SceneMesh[1].Create( pd3dDevice, L"misc\\skullocc.x" ) ) )
        return DXUTERR_MEDIANOTFOUND;
    g_SceneMesh[1].SetVertexDecl( pd3dDevice, MESHVERT::Decl );
    if( FAILED( g_Skybox.Create( pd3dDevice, L"alley_skybox.x" ) ) )
        return DXUTERR_MEDIANOTFOUND;
    g_Skybox.SetVertexDecl( pd3dDevice, SKYBOXVERT::Decl );

    // Initialize the mesh's world matrix
    ComputeMeshWorldMatrix( g_SceneMesh[g_nScene].GetMesh() );

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

    for( int p = 0; p < PPCOUNT; ++p )
        V_RETURN( g_aPostProcess[p].OnResetDevice( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height ) );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_pEffect->SetMatrix( "g_mProj", g_Camera.GetProjMatrix() );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_SceneMesh[0].RestoreDeviceObjects( pd3dDevice );
    g_SceneMesh[1].RestoreDeviceObjects( pd3dDevice );
    g_Skybox.RestoreDeviceObjects( pd3dDevice );

    // Create scene save texture
    for( int i = 0; i < RT_COUNT; ++i )
    {
        V_RETURN( pd3dDevice->CreateTexture( pBackBufferSurfaceDesc->Width,
                                             pBackBufferSurfaceDesc->Height,
                                             1,
                                             D3DUSAGE_RENDERTARGET,
                                             g_TexFormat,
                                             D3DPOOL_DEFAULT,
                                             &g_pSceneSave[i],
                                             NULL ) );

        // Create the textures for this render target chains
        IDirect3DTexture9* pRT[2];
        ZeroMemory( pRT, sizeof( pRT ) );
        for( int t = 0; t < 2; ++t )
        {
            V_RETURN( pd3dDevice->CreateTexture( pBackBufferSurfaceDesc->Width,
                                                 pBackBufferSurfaceDesc->Height,
                                                 1,
                                                 D3DUSAGE_RENDERTARGET,
                                                 D3DFMT_A8R8G8B8,
                                                 D3DPOOL_DEFAULT,
                                                 &pRT[t],
                                                 NULL ) );
        }
        g_RTChain[i].Init( pRT );
        SAFE_RELEASE( pRT[0] );
        SAFE_RELEASE( pRT[1] );
    }

    // Create the environment mapping texture
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Light Probes\\uffizi_cross.dds" ) );
    if( FAILED( D3DXCreateCubeTextureFromFile( pd3dDevice, str, &g_pEnvTex ) ) )
        return DXUTERR_MEDIANOTFOUND;

    // Initialize the render target table based on how many simultaneous RTs
    // the card can support.
    IDirect3DSurface9* pSurf;
    switch( g_nPasses )
    {
        case 1:
            g_pSceneSave[0]->GetSurfaceLevel( 0, &pSurf );
            g_aRtTable[0].pRT[0] = pSurf;
            g_pSceneSave[1]->GetSurfaceLevel( 0, &pSurf );
            g_aRtTable[0].pRT[1] = pSurf;
            g_pSceneSave[2]->GetSurfaceLevel( 0, &pSurf );
            g_aRtTable[0].pRT[2] = pSurf;
            // Passes 1 and 2 are not used
            g_aRtTable[1].pRT[0] = NULL;
            g_aRtTable[1].pRT[1] = NULL;
            g_aRtTable[1].pRT[2] = NULL;
            g_aRtTable[2].pRT[0] = NULL;
            g_aRtTable[2].pRT[1] = NULL;
            g_aRtTable[2].pRT[2] = NULL;
            break;
        case 2:
            g_pSceneSave[0]->GetSurfaceLevel( 0, &pSurf );
            g_aRtTable[0].pRT[0] = pSurf;
            g_pSceneSave[1]->GetSurfaceLevel( 0, &pSurf );
            g_aRtTable[0].pRT[1] = pSurf;
            g_aRtTable[0].pRT[2] = NULL;  // RT 2 of pass 0 not used
            g_pSceneSave[2]->GetSurfaceLevel( 0, &pSurf );
            g_aRtTable[1].pRT[0] = pSurf;
            // RT 1 & 2 of pass 1 not used
            g_aRtTable[1].pRT[1] = NULL;
            g_aRtTable[1].pRT[2] = NULL;
            // Pass 2 not used
            g_aRtTable[2].pRT[0] = NULL;
            g_aRtTable[2].pRT[1] = NULL;
            g_aRtTable[2].pRT[2] = NULL;
            break;
        case 3:
            g_pSceneSave[0]->GetSurfaceLevel( 0, &pSurf );
            g_aRtTable[0].pRT[0] = pSurf;
            // RT 1 & 2 of pass 0 not used
            g_aRtTable[0].pRT[1] = NULL;
            g_aRtTable[0].pRT[2] = NULL;
            g_pSceneSave[1]->GetSurfaceLevel( 0, &pSurf );
            g_aRtTable[1].pRT[0] = pSurf;
            // RT 1 & 2 of pass 1 not used
            g_aRtTable[1].pRT[1] = NULL;
            g_aRtTable[1].pRT[2] = NULL;
            g_pSceneSave[2]->GetSurfaceLevel( 0, &pSurf );
            g_aRtTable[2].pRT[0] = pSurf;
            // RT 1 & 2 of pass 2 not used
            g_aRtTable[2].pRT[1] = NULL;
            g_aRtTable[2].pRT[2] = NULL;
            break;
    }

    g_HUD.SetLocation( 0, 100 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 225, 5 );
    g_SampleUI.SetSize( 220, 470 );

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
// Name: PerformSinglePostProcess()
// Desc: Perform post-process by setting the previous render target as a
//       source texture and rendering a quad with the post-process technique
//       set.
//       This method changes render target without saving any. The caller
//       should ensure that the default render target is saved before calling
//       this.
//       When this method is invoked, m_dwNextTarget is the index of the
//       rendertarget of this post-process.  1 - m_dwNextTarget is the index
//       of the source of this post-process.
HRESULT PerformSinglePostProcess( IDirect3DDevice9* pd3dDevice,
                                  CPostProcess& PP,
                                  CPProcInstance& Inst,
                                  IDirect3DVertexBuffer9* pVB,
                                  PPVERT* aQuad,
                                  float& fExtentX,
                                  float& fExtentY )
{
    HRESULT hr;

    //
    // The post-process effect may require that a copy of the
    // originally rendered scene be available for use, so
    // we initialize them here.
    //

    for( int i = 0; i < RT_COUNT; ++i )
        PP.m_pEffect->SetTexture( PP.m_hTexScene[i], g_pSceneSave[i] );

    //
    // If there are any parameters, initialize them here.
    //

    for( int i = 0; i < NUM_PARAMS; ++i )
        if( PP.m_ahParam[i] )
            PP.m_pEffect->SetVector( PP.m_ahParam[i], &Inst.m_avParam[i] );

    // Render the quad
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        PP.m_pEffect->SetTechnique( "PostProcess" );

        // Set the vertex declaration
        pd3dDevice->SetVertexDeclaration( g_pVertDeclPP );

        // Draw the quad
        UINT cPasses, p;
        PP.m_pEffect->Begin( &cPasses, 0 );
        for( p = 0; p < cPasses; ++p )
        {
            bool bUpdateVB = false;  // Inidicates whether the vertex buffer
            // needs update for this pass.

            //
            // If the extents has been modified, the texture coordinates
            // in the quad need to be updated.
            //

            if( aQuad[1].tu != fExtentX )
            {
                aQuad[1].tu = aQuad[3].tu = fExtentX;
                bUpdateVB = true;
            }
            if( aQuad[2].tv != fExtentY )
            {
                aQuad[2].tv = aQuad[3].tv = fExtentY;
                bUpdateVB = true;
            }

            //
            // Check if the pass has annotation for extent info.  Update
            // fScaleX and fScaleY if it does.  Otherwise, default to 1.0f.
            //

            float fScaleX = 1.0f, fScaleY = 1.0f;
            D3DXHANDLE hPass = PP.m_pEffect->GetPass( PP.m_hTPostProcess, p );
            D3DXHANDLE hExtentScaleX = PP.m_pEffect->GetAnnotationByName( hPass, "fScaleX" );
            if( hExtentScaleX )
                PP.m_pEffect->GetFloat( hExtentScaleX, &fScaleX );
            D3DXHANDLE hExtentScaleY = PP.m_pEffect->GetAnnotationByName( hPass, "fScaleY" );
            if( hExtentScaleY )
                PP.m_pEffect->GetFloat( hExtentScaleY, &fScaleY );

            //
            // Now modify the quad according to the scaling values specified for
            // this pass
            //
            if( fScaleX != 1.0f )
            {
                aQuad[1].x = ( aQuad[1].x + 0.5f ) * fScaleX - 0.5f;
                aQuad[3].x = ( aQuad[3].x + 0.5f ) * fScaleX - 0.5f;
                bUpdateVB = true;
            }
            if( fScaleY != 1.0f )
            {
                aQuad[2].y = ( aQuad[2].y + 0.5f ) * fScaleY - 0.5f;
                aQuad[3].y = ( aQuad[3].y + 0.5f ) * fScaleY - 0.5f;
                bUpdateVB = true;
            }

            if( bUpdateVB )
            {
                LPVOID pVBData;
                // Scaling requires updating the vertex buffer.
                if( SUCCEEDED( pVB->Lock( 0, 0, &pVBData, D3DLOCK_DISCARD ) ) )
                {
                    CopyMemory( pVBData, aQuad, 4 * sizeof( PPVERT ) );
                    pVB->Unlock();
                }
            }
            fExtentX *= fScaleX;
            fExtentY *= fScaleY;

            // Set up the textures and the render target
            //
            for( int i = 0; i < RT_COUNT; ++i )
            {
                // If this is the very first post-process rendering,
                // obtain the source textures from the scene.
                // Otherwise, initialize the post-process source texture to
                // the previous render target.
                //
                if( g_RTChain[i].m_bFirstRender )
                    PP.m_pEffect->SetTexture( PP.m_hTexSource[i], g_pSceneSave[i] );
                else
                    PP.m_pEffect->SetTexture( PP.m_hTexSource[i], g_RTChain[i].GetNextSource() );
            }

            //
            // Set up the new render target
            //
            IDirect3DTexture9* pTarget = g_RTChain[PP.m_nRenderTarget].GetNextTarget();
            IDirect3DSurface9* pTexSurf;
            hr = pTarget->GetSurfaceLevel( 0, &pTexSurf );
            if( FAILED( hr ) )
                return DXUT_ERR( L"GetSurfaceLevel", hr );
            pd3dDevice->SetRenderTarget( 0, pTexSurf );
            pTexSurf->Release();
            // We have output to this render target. Flag it.
            g_RTChain[PP.m_nRenderTarget].m_bFirstRender = false;

            //
            // Clear the render target
            //
            pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET,
                               0x00000000, 1.0f, 0L );
            //
            // Render
            //
            PP.m_pEffect->BeginPass( p );
            pd3dDevice->SetStreamSource( 0, pVB, 0, sizeof( PPVERT ) );
            pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
            PP.m_pEffect->EndPass();

            // Update next rendertarget index
            g_RTChain[PP.m_nRenderTarget].Flip();
        }
        PP.m_pEffect->End();

        // End scene
        pd3dDevice->EndScene();
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// PerformPostProcess()
// Perform all active post-processes in order.
HRESULT PerformPostProcess( IDirect3DDevice9* pd3dDevice )
{
    HRESULT hr;

    //
    // Extents are used to control how much of the rendertarget is rendered
    // during postprocess. For example, with the extent of 0.5 and 0.5, only
    // the upper left quarter of the rendertarget will be rendered during
    // postprocess.
    //
    float fExtentX = 1.0f, fExtentY = 1.0f;
    const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();

    //
    // Set up our quad
    //
    PPVERT Quad[4] =
    {
        { -0.5f,                        -0.5f,                         1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { pd3dsdBackBuffer->Width - 0.5f, -0.5,                          1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f },
        { -0.5,                         pd3dsdBackBuffer->Height - 0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        { pd3dsdBackBuffer->Width - 0.5f, pd3dsdBackBuffer->Height - 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f }
    };

    //
    // Create a vertex buffer out of the quad
    //
    IDirect3DVertexBuffer9* pVB;
    hr = pd3dDevice->CreateVertexBuffer( sizeof( PPVERT ) * 4,
                                         D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
                                         0,
                                         D3DPOOL_DEFAULT,
                                         &pVB,
                                         NULL );
    if( FAILED( hr ) )
        return DXUT_ERR( L"CreateVertexBuffer", hr );

    // Fill in the vertex buffer
    LPVOID pVBData;
    if( SUCCEEDED( pVB->Lock( 0, 0, &pVBData, D3DLOCK_DISCARD ) ) )
    {
        CopyMemory( pVBData, Quad, sizeof( Quad ) );
        pVB->Unlock();
    }

    // Clear first-time render flags
    for( int i = 0; i < RT_COUNT; ++i )
        g_RTChain[i].m_bFirstRender = true;

    // Perform post processing
    CDXUTListBox* pListBox = g_SampleUI.GetListBox( IDC_ACTIVELIST );
    if( pListBox )
    {
        // The last (blank) item has special purpose so do not process it.
        for( int nEffIndex = 0; nEffIndex != pListBox->GetSize() - 1; ++nEffIndex )
        {
            DXUTListBoxItem* pItem = pListBox->GetItem( nEffIndex );
            if( pItem )
            {
                CPProcInstance* pInstance = ( CPProcInstance* )pItem->pData;
                PerformSinglePostProcess( pd3dDevice,
                                          g_aPostProcess[pInstance->m_nFxIndex],
                                          *pInstance,
                                          pVB,
                                          Quad,
                                          fExtentX,
                                          fExtentY );
            }
        }
    }

    // Release the vertex buffer
    pVB->Release();

    return S_OK;
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
    UINT cPass, p;
    LPD3DXMESH pMeshObj;
    D3DXMATRIXA16 mWorldView;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

    // Save render target 0 so we can restore it later
    IDirect3DSurface9* pOldRT;
    pd3dDevice->GetRenderTarget( 0, &pOldRT );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Set the vertex declaration
        V( pd3dDevice->SetVertexDeclaration( g_pVertDecl ) );

        // Render the mesh
        D3DXMatrixMultiply( &mWorldView, &g_mMeshWorld, g_Camera.GetWorldMatrix() );
        D3DXMatrixMultiply( &mWorldView, &mWorldView, g_Camera.GetViewMatrix() );
        V( g_pEffect->SetMatrix( "g_mWorldView", &mWorldView ) );
        V( g_pEffect->SetTexture( "g_txEnvMap", g_pEnvTex ) );
        D3DXMATRIX mRevView = *g_Camera.GetViewMatrix();
        mRevView._41 = mRevView._42 = mRevView._43 = 0.0f;
        D3DXMatrixInverse( &mRevView, NULL, &mRevView );
        V( g_pEffect->SetMatrix( "g_mRevView", &mRevView ) );

        switch( g_nScene )
        {
            case 0:
                V( g_pEffect->SetTechnique( g_hTRenderScene ) );
                break;
            case 1:
                V( g_pEffect->SetTechnique( g_hTRenderEnvMapScene ) );
                break;
        }

        pMeshObj = g_SceneMesh[g_nScene].GetMesh();
        V( g_pEffect->Begin( &cPass, 0 ) );
        for( p = 0; p < cPass; ++p )
        {
            // Set the render target(s) for this pass
            for( int rt = 0; rt < g_nRtUsed; ++rt )
                V( pd3dDevice->SetRenderTarget( rt, g_aRtTable[p].pRT[rt] ) );

            V( g_pEffect->BeginPass( p ) );

            // Iterate through each subset and render with its texture
            for( DWORD m = 0; m < g_SceneMesh[g_nScene].m_dwNumMaterials; ++m )
            {
                V( g_pEffect->SetTexture( "g_txScene", g_SceneMesh[g_nScene].m_pTextures[m] ) );
                V( g_pEffect->CommitChanges() );
                V( pMeshObj->DrawSubset( m ) );
            }

            V( g_pEffect->EndPass() );
        }
        V( g_pEffect->End() );

        // Render the skybox as if the camera is at center
        V( g_pEffect->SetTechnique( g_hTRenderSkyBox ) );
        V( pd3dDevice->SetVertexDeclaration( g_pSkyBoxDecl ) );

        D3DXMATRIXA16 mView = *g_Camera.GetViewMatrix();
        mView._41 = mView._42 = mView._43 = 0.0f;

        D3DXMatrixScaling( &mWorldView, 100.0f, 100.0f, 100.0f );
        D3DXMatrixMultiply( &mWorldView, &mWorldView, &mView );
        V( g_pEffect->SetMatrix( "g_mWorldView", &mWorldView ) );

        pMeshObj = g_Skybox.GetMesh();
        V( g_pEffect->Begin( &cPass, 0 ) );
        for( p = 0; p < cPass; ++p )
        {
            // Set the render target(s) for this pass
            for( int rt = 0; rt < g_nRtUsed; ++rt )
                V( pd3dDevice->SetRenderTarget( rt, g_aRtTable[p].pRT[rt] ) );

            V( g_pEffect->BeginPass( p ) );

            // Iterate through each subset and render with its texture
            for( DWORD m = 0; m < g_Skybox.m_dwNumMaterials; ++m )
            {
                V( g_pEffect->SetTexture( "g_txScene", g_Skybox.m_pTextures[m] ) );
                V( g_pEffect->CommitChanges() );
                V( pMeshObj->DrawSubset( m ) );
            }

            V( g_pEffect->EndPass() );
        }
        V( g_pEffect->End() );

        V( pd3dDevice->EndScene() );
    }

    //
    // Swap the chains
    //
    int i;
    for( i = 0; i < RT_COUNT; ++i )
        g_RTChain[i].Flip();

    // Reset all render targets used besides RT 0
    for( i = 1; i < g_nRtUsed; ++i )
        V( pd3dDevice->SetRenderTarget( i, NULL ) );

    //
    // Perform post-processes
    //

    CDXUTListBox* pListBox = g_SampleUI.GetListBox( IDC_ACTIVELIST );
    bool bPerformPostProcess = g_bEnablePostProc && pListBox && ( pListBox->GetSize() > 1 );
    if( bPerformPostProcess )
        PerformPostProcess( pd3dDevice );

    // Restore old render target 0 (back buffer)
    V( pd3dDevice->SetRenderTarget( 0, pOldRT ) );
    SAFE_RELEASE( pOldRT );

    //
    // Get the final result image onto the backbuffer
    //

    const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Render a screen-sized quad
        PPVERT quad[4] =
        {
            { -0.5f,                          -0.5f,                           0.5f, 1.0f, 0.0f, 0.0f, 0.0f,
                0.0f },
            { pd3dsdBackBuffer->Width - 0.5f, -0.5f,                           0.5f, 1.0f, 1.0f, 0.0f, 0.0f,
                0.0f },
            { -0.5f,                          pd3dsdBackBuffer->Height - 0.5f, 0.5f, 1.0f, 0.0f, 1.0f, 0.0f,
                0.0f },
            { pd3dsdBackBuffer->Width - 0.5f, pd3dsdBackBuffer->Height - 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f }
        };
        IDirect3DTexture9* pPrevTarget;
        pPrevTarget = ( bPerformPostProcess ) ? g_RTChain[0].GetPrevTarget() : g_pSceneSave[0];

        V( pd3dDevice->SetVertexDeclaration( g_pVertDeclPP ) );
        V( g_pEffect->SetTechnique( g_hTRenderNoLight ) );
        V( g_pEffect->SetTexture( "g_txScene", pPrevTarget ) );
        UINT cPasses;
        V( g_pEffect->Begin( &cPasses, 0 ) );
        for( p = 0; p < cPasses; ++p )
        {
            V( g_pEffect->BeginPass( p ) );
            V( pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, quad, sizeof( PPVERT ) ) );
            V( g_pEffect->EndPass() );
        }
        V( g_pEffect->End() );

        // Render text
        RenderText();

        // Render dialogs
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
    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );
    const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) ); // Show FPS
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    // If floating point rendertarget is not supported, display a warning
    // message to the user that some effects may not work correctly.
    if( D3DFMT_A16B16G16R16F != g_TexFormat )
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
        txtHelper.SetInsertionPos( 5, 45 );
        txtHelper.DrawTextLine( L"Floating-point render target not supported\n"
                                L"by the 3D device.  Some post-process effects\n"
                                L"may not render correctly." );
    }

    // Draw help
    if( g_bShowHelp )
    {
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 4 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls (F1 to hide):" );

        txtHelper.SetInsertionPos( 40, pd3dsdBackBuffer->Height - 15 * 3 );
        txtHelper.DrawTextLine( L"Mesh Movement: Left drag mouse\n"
                                L"Camera Movement: Right drag mouse\n" );
    }
    else
    {
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 2 );
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


#define IN_FLOAT_CHARSET( c ) \
    ( (c) == L'-' || (c) == L'.' || ( (c) >= L'0' && (c) <= L'9' ) )

//--------------------------------------------------------------------------------------
// Parse a string that contains a list of floats separated by space, and store
// them in the array of float pointed to by pBuffer.
// Parses up to 4 floats.
void ParseFloatList( const WCHAR* pwszText, float* pBuffer )
{
    int nWritten = 0;  // Number of floats written
    const WCHAR* pToken, *pEnd;
    WCHAR wszToken[30];

    pToken = pwszText;
    while( nWritten < 4 && *pToken != L'\0' )
    {
        // Skip leading spaces
        while( *pToken == L' ' )
            ++pToken;

        // Locate the end of number
        pEnd = pToken;
        while( IN_FLOAT_CHARSET( *pEnd ) )
            ++pEnd;

        // Copy the token to our buffer
        int nTokenLen = min( sizeof( wszToken ) / sizeof( wszToken[0] ), int( pEnd - pToken ) + 1 );
        wcscpy_s( wszToken, nTokenLen, pToken );
        *pBuffer = ( float )_wtof( wszToken );
        ++nWritten;
        ++pBuffer;
        pToken = pEnd;
    }
}


//--------------------------------------------------------------------------------------
// Inserts the postprocess effect identified by the index nEffectIndex into the
// active list.
void InsertEffect( int nEffectIndex )
{
    int nInsertPosition = g_SampleUI.GetListBox( IDC_ACTIVELIST )->GetSelectedIndex();
    if( nInsertPosition == -1 )
        nInsertPosition = g_SampleUI.GetListBox( IDC_ACTIVELIST )->GetSize() - 1;

    // Create a new CPProcInstance object and set it as the data field of the
    // newly inserted item.
    CPProcInstance* pNewInst = new CPProcInstance;

    if( pNewInst )
    {
        pNewInst->m_nFxIndex = nEffectIndex;
        ZeroMemory( pNewInst->m_avParam, sizeof( pNewInst->m_avParam ) );
        for( int p = 0; p < NUM_PARAMS; ++p )
            pNewInst->m_avParam[p] = g_aPostProcess[pNewInst->m_nFxIndex].m_avParamDef[p];

        g_SampleUI.GetListBox( IDC_ACTIVELIST )->InsertItem( nInsertPosition, g_aszPpDesc[nEffectIndex], pNewInst );

        // Set selection to the item after the inserted one.
        int nSelected = g_SampleUI.GetListBox( IDC_ACTIVELIST )->GetSelectedIndex();
        if( nSelected >= 0 )
            g_SampleUI.GetListBox( IDC_ACTIVELIST )->SelectItem( nSelected + 1 );
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
        case IDC_SELECTSCENE:
        {
            DXUTComboBoxItem* pItem = ( ( CDXUTComboBox* )pControl )->GetSelectedItem();
            if( pItem )
            {
                g_nScene = ( int )( size_t )pItem->pData;

                // Scene mesh has chanaged. Re-initialize the world matrix
                // for the mesh.
                ComputeMeshWorldMatrix( g_SceneMesh[g_nScene].GetMesh() );
            }
            break;
        }
        case IDC_AVAILABLELIST:
        {
            switch( nEvent )
            {
                case EVENT_LISTBOX_ITEM_DBLCLK:
                {
                    DXUTListBoxItem* pItem = ( ( CDXUTListBox* )pControl )->GetSelectedItem();
                    if( pItem )
                    {
                        // Inserts the selected effect in available list to the active list
                        // before its selected item.

                        InsertEffect( ( int )( size_t )pItem->pData );
                    }
                    break;
                }
            }
            break;
        }

        case IDC_ACTIVELIST:
        {
            switch( nEvent )
            {
                case EVENT_LISTBOX_ITEM_DBLCLK:
                {
                    int nSelected = ( ( CDXUTListBox* )pControl )->GetSelectedIndex();

                    // Do not remove the last (blank) item
                    if( nSelected != ( ( CDXUTListBox* )pControl )->GetSize() - 1 )
                    {
                        DXUTListBoxItem* pItem = ( ( CDXUTListBox* )pControl )->GetSelectedItem();
                        if( pItem )
                            delete pItem->pData;
                        ( ( CDXUTListBox* )pControl )->RemoveItem( nSelected );
                    }
                    break;
                }
                case EVENT_LISTBOX_SELECTION:
                {
                    // Selection changed in the active list.  Update the parameter
                    // controls.

                    int nSelected = ( ( CDXUTListBox* )pControl )->GetSelectedIndex();

                    if( nSelected >= 0 && nSelected < ( int )( ( CDXUTListBox* )pControl )->GetSize() - 1 )
                    {
                        DXUTListBoxItem* pItem = ( ( CDXUTListBox* )pControl )->GetSelectedItem();
                        CPProcInstance* pInstance = ( CPProcInstance* )pItem->pData;
                        CPostProcess& PP = g_aPostProcess[pInstance->m_nFxIndex];

                        if( pInstance && PP.m_awszParamName[0][0] != L'\0' )
                        {
                            g_SampleUI.GetStatic( IDC_PARAM0NAME )->SetText( PP.m_awszParamName[0] );

                            // Fill the editboxes with the parameter values
                            for( int p = 0; p < NUM_PARAMS; ++p )
                            {
                                if( PP.m_awszParamName[p][0] != L'\0' )
                                {
                                    // Enable the label and editbox for this parameter
                                    g_SampleUI.GetControl( IDC_PARAM0 + p )->SetEnabled( true );
                                    g_SampleUI.GetControl( IDC_PARAM0 + p )->SetVisible( true );
                                    g_SampleUI.GetControl( IDC_PARAM0NAME + p )->SetEnabled( true );
                                    g_SampleUI.GetControl( IDC_PARAM0NAME + p )->SetVisible( true );

                                    WCHAR wszParamText[512] = L"";

                                    for( int i = 0; i < PP.m_anParamSize[p]; ++i )
                                    {
                                        WCHAR wsz[512];
                                        swprintf_s( wsz, 512, L"%.5f ", pInstance->m_avParam[p][i] );
                                        wcscat_s( wszParamText, 512, wsz );
                                    }

                                    // Remove trailing space
                                    if( wszParamText[lstrlenW( wszParamText ) - 1] == L' ' )
                                        wszParamText[lstrlenW( wszParamText ) - 1] = L'\0';
                                    g_SampleUI.GetEditBox( IDC_PARAM0 + p )->SetText( wszParamText );
                                }
                            }
                        }
                        else
                        {
                            g_SampleUI.GetStatic( IDC_PARAM0NAME )->SetText( L"Selected effect has no parameters." );

                            // Disable the edit boxes and 2nd parameter static
                            g_SampleUI.GetControl( IDC_PARAM0 )->SetEnabled( false );
                            g_SampleUI.GetControl( IDC_PARAM0 )->SetVisible( false );
                            g_SampleUI.GetControl( IDC_PARAM1 )->SetEnabled( false );
                            g_SampleUI.GetControl( IDC_PARAM1 )->SetVisible( false );
                            g_SampleUI.GetControl( IDC_PARAM1NAME )->SetEnabled( false );
                            g_SampleUI.GetControl( IDC_PARAM1NAME )->SetVisible( false );
                        }
                    }
                    else
                    {
                        g_SampleUI.GetStatic( IDC_PARAM0NAME )->SetText(
                            L"Select an active effect to set its parameter." );

                        // Disable the edit boxes and 2nd parameter static
                        g_SampleUI.GetControl( IDC_PARAM0 )->SetEnabled( false );
                        g_SampleUI.GetControl( IDC_PARAM0 )->SetVisible( false );
                        g_SampleUI.GetControl( IDC_PARAM1 )->SetEnabled( false );
                        g_SampleUI.GetControl( IDC_PARAM1 )->SetVisible( false );
                        g_SampleUI.GetControl( IDC_PARAM1NAME )->SetEnabled( false );
                        g_SampleUI.GetControl( IDC_PARAM1NAME )->SetVisible( false );
                    }

                    break;
                }
            }
            break;
        }

        case IDC_CLEAR:
        {
            ClearActiveList();
            break;
        }

        case IDC_MOVEUP:
        {
            CDXUTListBox* pListBox = g_SampleUI.GetListBox( IDC_ACTIVELIST );
            if( !pListBox )
                break;

            int nSelected = pListBox->GetSelectedIndex();
            if( nSelected < 0 )
                break;

            // Cannot move up the first item or the last (blank) item.
            if( nSelected != 0 && nSelected != pListBox->GetSize() - 1 )
            {
                DXUTListBoxItem* pPrevItem = pListBox->GetItem( nSelected - 1 );
                DXUTListBoxItem* pItem = pListBox->GetItem( nSelected );
                DXUTListBoxItem Temp;
                // Swap
                Temp = *pItem;
                *pItem = *pPrevItem;
                *pPrevItem = Temp;

                pListBox->SelectItem( nSelected - 1 );
            }
            break;
        }

        case IDC_MOVEDOWN:
        {
            CDXUTListBox* pListBox = g_SampleUI.GetListBox( IDC_ACTIVELIST );
            if( !pListBox )
                break;

            int nSelected = pListBox->GetSelectedIndex();
            if( nSelected < 0 )
                break;

            // Cannot move down either of the last two item.
            if( nSelected < pListBox->GetSize() - 2 )
            {
                DXUTListBoxItem* pNextItem = pListBox->GetItem( nSelected + 1 );
                DXUTListBoxItem* pItem = pListBox->GetItem( nSelected );
                DXUTListBoxItem Temp;

                // Swap
                if( pItem && pNextItem )
                {
                    Temp = *pItem;
                    *pItem = *pNextItem;
                    *pNextItem = Temp;
                }

                pListBox->SelectItem( nSelected + 1 );
            }
            break;
        }

        case IDC_PARAM0:
        case IDC_PARAM1:
            {
                if( nEvent == EVENT_EDITBOX_CHANGE )
                {
                    int nParamIndex;
                    switch( nControlID )
                    {
                        case IDC_PARAM0:
                            nParamIndex = 0; break;
                        case IDC_PARAM1:
                            nParamIndex = 1; break;
                        default:
                            return;
                    }

                    DXUTListBoxItem* pItem = g_SampleUI.GetListBox( IDC_ACTIVELIST )->GetSelectedItem();
                    CPProcInstance* pInstance = NULL;
                    if( pItem )
                        pInstance = ( CPProcInstance* )pItem->pData;

                    if( pInstance )
                    {
                        D3DXVECTOR4 v;
                        ZeroMemory( &v, sizeof( v ) );
                        ParseFloatList( ( ( CDXUTEditBox* )pControl )->GetText(), ( float* )&v );

                        // We parsed the values. Now save them in the instance data.
                        pInstance->m_avParam[nParamIndex] = v;
                    }
                }
                break;
            }

        case IDC_ENABLEPP:
        {
            g_bEnablePostProc = ( ( CDXUTCheckBox* )pControl )->GetChecked();
            break;
        }

        case IDC_PREBLUR:
        {
            // Clear the list
            ClearActiveList();

            // Insert effects
            InsertEffect( 9 );
            InsertEffect( 2 );
            InsertEffect( 3 );
            InsertEffect( 2 );
            InsertEffect( 3 );
            InsertEffect( 10 );
            break;
        }

        case IDC_PREBLOOM:
        {
            // Clear the list
            ClearActiveList();

            // Insert effects
            InsertEffect( 9 );
            InsertEffect( 9 );
            InsertEffect( 6 );
            InsertEffect( 4 );
            InsertEffect( 5 );
            InsertEffect( 4 );
            InsertEffect( 5 );
            InsertEffect( 10 );
            InsertEffect( 12 );
            break;
        }

        case IDC_PREDOF:
        {
            // Clear the list
            ClearActiveList();

            // Insert effects
            InsertEffect( 9 );
            InsertEffect( 2 );
            InsertEffect( 3 );
            InsertEffect( 2 );
            InsertEffect( 3 );
            InsertEffect( 10 );
            InsertEffect( 14 );
            break;
        }

        case IDC_PREEDGE:
        {
            // Clear the list
            ClearActiveList();

            // Insert effects
            InsertEffect( 13 );
            InsertEffect( 9 );
            InsertEffect( 4 );
            InsertEffect( 5 );
            InsertEffect( 12 );
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

    for( int p = 0; p < PPCOUNT; ++p )
        g_aPostProcess[p].OnLostDevice();

    // Release the scene save and render target textures
    for( int i = 0; i < RT_COUNT; ++i )
    {
        SAFE_RELEASE( g_pSceneSave[i] );
        g_RTChain[i].Cleanup();
    }

    g_SceneMesh[0].InvalidateDeviceObjects();
    g_SceneMesh[1].InvalidateDeviceObjects();
    g_Skybox.InvalidateDeviceObjects();

    // Release the RT table's references
    for( int p = 0; p < RT_COUNT; ++p )
        for( int rt = 0; rt < RT_COUNT; ++rt )
            SAFE_RELEASE( g_aRtTable[p].pRT[rt] );

    SAFE_RELEASE( g_pEnvTex );
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
    SAFE_RELEASE( g_pVertDecl );
    SAFE_RELEASE( g_pSkyBoxDecl );
    SAFE_RELEASE( g_pVertDeclPP );

    g_SceneMesh[0].Destroy();
    g_SceneMesh[1].Destroy();
    g_Skybox.Destroy();

    for( int p = 0; p < PPCOUNT; ++p )
        g_aPostProcess[p].Cleanup();
}
