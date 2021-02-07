//--------------------------------------------------------------------------------------
// File: DetailTessellation11.cpp
//
// This sample demonstrates the use of detail tessellation for improving the 
// quality of material surfaces in real-time rendering applications.
//
// Developed by AMD Developer Relations team.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include <d3dx11.h>
#include "strsafe.h"
#include "resource.h"
#include "Grid_Creation11.h"

//--------------------------------------------------------------------------------------
// Macros
//--------------------------------------------------------------------------------------
#define DWORD_POSITIVE_RANDOM(x)    ((DWORD)(( ((x)*rand()) / RAND_MAX )))
#define FLOAT_POSITIVE_RANDOM(x)    ( ((x)*rand()) / RAND_MAX )
#define FLOAT_RANDOM(x)             ((((2.0f*rand())/RAND_MAX) - 1.0f)*(x))

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const float GRID_SIZE               = 200.0f;
const float MAX_TESSELLATION_FACTOR = 15.0f;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct CONSTANT_BUFFER_STRUCT
{
    D3DXMATRIXA16    mWorld;                         // World matrix
    D3DXMATRIXA16    mView;                          // View matrix
    D3DXMATRIXA16    mProjection;                    // Projection matrix
    D3DXMATRIXA16    mWorldViewProjection;           // WVP matrix
    D3DXMATRIXA16    mViewProjection;                // VP matrix
    D3DXMATRIXA16    mInvView;                       // Inverse of view matrix
    D3DXVECTOR4      vScreenResolution;              // Screen resolution
    D3DXVECTOR4      vMeshColor;                     // Mesh color
    D3DXVECTOR4      vTessellationFactor;            // Edge, inside, minimum tessellation factor and 1/desired triangle size
    D3DXVECTOR4      vDetailTessellationHeightScale; // Height scale for detail tessellation of grid surface
    D3DXVECTOR4      vGridSize;                      // Grid size
    D3DXVECTOR4      vDebugColorMultiply;            // Debug colors
    D3DXVECTOR4      vDebugColorAdd;                 // Debug colors
    D3DXVECTOR4      vFrustumPlaneEquation[4];       // View frustum plane equations
};                                                      
                                                        
struct MATERIAL_CB_STRUCT
{
    D3DXVECTOR4     g_materialAmbientColor;  // Material's ambient color
    D3DXVECTOR4     g_materialDiffuseColor;  // Material's diffuse color
    D3DXVECTOR4     g_materialSpecularColor; // Material's specular color
    D3DXVECTOR4     g_fSpecularExponent;     // Material's specular exponent

    D3DXVECTOR4     g_LightPosition;         // Light's position in world space
    D3DXVECTOR4     g_LightDiffuse;          // Light's diffuse color
    D3DXVECTOR4     g_LightAmbient;          // Light's ambient color

    D3DXVECTOR4     g_vEye;                  // Camera's location
    D3DXVECTOR4     g_fBaseTextureRepeat;    // The tiling factor for base and normal map textures
    D3DXVECTOR4     g_fPOMHeightMapScale;    // Describes the useful range of values for the height field

    D3DXVECTOR4     g_fShadowSoftening;      // Blurring factor for the soft shadows computation

    int             g_nMinSamples;           // The minimum number of samples for sampling the height field
    int             g_nMaxSamples;           // The maximum number of samples for sampling the height field
    int             uDummy1;
    int             uDummy2;
};

struct DETAIL_TESSELLATION_TEXTURE_STRUCT
{
    WCHAR* DiffuseMap;                  // Diffuse texture map
    WCHAR* NormalHeightMap;             // Normal and height map (normal in .xyz, height in .w)
    WCHAR* DisplayName;                 // Display name of texture
    float  fHeightScale;                // Height scale when rendering
    float  fBaseTextureRepeat;          // Base repeat of texture coordinates (1.0f for no repeat)
    float  fDensityScale;               // Density scale (used for density map generation)
    float  fMeaningfulDifference;       // Meaningful difference (used for density map generation)
};

//--------------------------------------------------------------------------------------
// Enums
//--------------------------------------------------------------------------------------
enum
{
    TESSELLATIONMETHOD_DISABLED,
    TESSELLATIONMETHOD_DISABLED_POM,
    TESSELLATIONMETHOD_DETAIL_TESSELLATION, 
} eTessellationMethod;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
// DXUT resources
CDXUTDialogResourceManager          g_DialogResourceManager;    // Manager for shared resources of dialogs
CFirstPersonCamera                  g_Camera;
CD3DSettingsDlg                     g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                         g_HUD;                      // Manages the 3D   
CDXUTDialog                         g_SampleUI;                 // Dialog for sample specific controls
CDXUTTextHelper*                    g_pTxtHelper = NULL;

// Shaders
ID3D11VertexShader*                 g_pPOMVS = NULL;
ID3D11PixelShader*                  g_pPOMPS = NULL;
ID3D11VertexShader*                 g_pDetailTessellationVS = NULL;
ID3D11VertexShader*                 g_pNoTessellationVS = NULL;
ID3D11HullShader*                   g_pDetailTessellationHS = NULL;
ID3D11DomainShader*                 g_pDetailTessellationDS = NULL;
ID3D11PixelShader*                  g_pBumpMapPS = NULL;
ID3D11VertexShader*                 g_pParticleVS = NULL;
ID3D11GeometryShader*               g_pParticleGS = NULL;
ID3D11PixelShader*                  g_pParticlePS = NULL;

// Textures
#define NUM_TEXTURES 7
DETAIL_TESSELLATION_TEXTURE_STRUCT DetailTessellationTextures[NUM_TEXTURES + 1] =
{
//    DiffuseMap              NormalHeightMap                 DisplayName,    fHeightScale fBaseTextureRepeat fDensityScale fMeaningfulDifference
    { L"Textures\\rocks.jpg",    L"Textures\\rocks_NM_height.dds",  L"Rocks",       10.0f,       1.0f,              25.0f,        2.0f/255.0f },
    { L"Textures\\stones.bmp",   L"Textures\\stones_NM_height.dds", L"Stones",      5.0f,        1.0f,              10.0f,        5.0f/255.0f },
    { L"Textures\\wall.jpg",     L"Textures\\wall_NM_height.dds",   L"Wall",        8.0f,        1.0f,              7.0f,         7.0f/255.0f },
    { L"Textures\\wood.jpg",     L"Textures\\four_NM_height.dds",   L"Four shapes", 30.0f,       1.0f,              35.0f,        2.0f/255.0f },
    { L"Textures\\concrete.bmp", L"Textures\\bump_NM_height.dds",   L"Bump",        10.0f,       4.0f,              50.0f,        2.0f/255.0f },
    { L"Textures\\concrete.bmp", L"Textures\\dent_NM_height.dds",   L"Dent",        10.0f,       4.0f,              50.0f,        2.0f/255.0f },
    { L"Textures\\wood.jpg",     L"Textures\\saint_NM_height.dds",  L"Saints" ,     20.0f,       1.0f,              40.0f,        2.0f/255.0f },
    { L"",                       L"",                               L"Custom" ,     5.0f,        1.0f,              10.0f,        2.0f/255.0f },
};
DWORD                               g_dwNumTextures = 0;
ID3D11ShaderResourceView*           g_pDetailTessellationBaseTextureRV[NUM_TEXTURES+1];
ID3D11ShaderResourceView*           g_pDetailTessellationHeightTextureRV[NUM_TEXTURES+1];
ID3D11ShaderResourceView*           g_pDetailTessellationDensityTextureRV[NUM_TEXTURES+1];
ID3D11ShaderResourceView*           g_pLightTextureRV = NULL;
WCHAR                               g_pszCustomTextureDiffuseFilename[MAX_PATH] = L"";
WCHAR                               g_pszCustomTextureNormalHeightFilename[MAX_PATH] = L"";
DWORD                               g_bRecreateCustomTextureDensityMap  = false;

// Geometry
ID3D11Buffer*                       g_pGridTangentSpaceVB = NULL;
ID3D11Buffer*                       g_pGridTangentSpaceIB = NULL;
ID3D11Buffer*                       g_pMainCB = NULL;
ID3D11Buffer*                       g_pMaterialCB = NULL;
ID3D11InputLayout*                  g_pTangentSpaceVertexLayout = NULL;
ID3D11InputLayout*                  g_pPositionOnlyVertexLayout = NULL;
ID3D11Buffer*                       g_pGridDensityVB[NUM_TEXTURES+1];
ID3D11ShaderResourceView*           g_pGridDensityVBSRV[NUM_TEXTURES+1];
ID3D11Buffer*                       g_pParticleVB = NULL;

// States
ID3D11RasterizerState*              g_pRasterizerStateSolid = NULL;
ID3D11RasterizerState*              g_pRasterizerStateWireframe = NULL;
ID3D11SamplerState*                 g_pSamplerStateLinear = NULL;
ID3D11BlendState*                   g_pBlendStateNoBlend = NULL;
ID3D11BlendState*                   g_pBlendStateAdditiveBlending = NULL;
ID3D11DepthStencilState*            g_pDepthStencilState = NULL;

// Camera and light parameters
D3DXVECTOR3                         g_vecEye(76.099495f, 43.191154f, -110.108971f);
D3DXVECTOR3                         g_vecAt (75.590889f, 42.676582f, -109.418678f);
D3DXVECTOR4                         g_LightPosition(100.0f, 30.0f, -50.0f, 1.0f);
CDXUTDirectionWidget                g_LightControl; 
D3DXVECTOR4							g_pWorldSpaceFrustumPlaneEquation[6];

// Render settings
int                                 g_nRenderHUD = 2;
DWORD                               g_dwGridTessellation = 50;
bool                                g_bEnableWireFrame = false;
float                               g_fTessellationFactorEdges = 7.0f;
float                               g_fTessellationFactorInside = 7.0f;
int                                 g_nTessellationMethod = TESSELLATIONMETHOD_DETAIL_TESSELLATION;
int                                 g_nCurrentTexture = 0;
bool                                g_bFrameBasedAnimation = FALSE;
bool                                g_bAnimatedCamera = FALSE;
bool                                g_bDisplacementEnabled = TRUE;
D3DXVECTOR3                         g_vDebugColorMultiply(1.0, 1.0, 1.0);
D3DXVECTOR3                         g_vDebugColorAdd(0.0, 0.0, 0.0);
bool                                g_bDensityBasedDetailTessellation = TRUE;
bool                                g_bDistanceAdaptiveDetailTessellation = TRUE;
bool                                g_bScreenSpaceAdaptiveDetailTessellation = FALSE;
bool                                g_bUseFrustumCullingOptimization = TRUE;
bool                                g_bDetailTessellationShadersNeedReloading = FALSE;
bool                                g_bShowFPS = TRUE;
bool                                g_bDrawLightSource = TRUE;
float                               g_fDesiredTriangleSize = 10.0f;

// Frame buffer readback ( 0 means never dump to disk (frame counter starts at 1) )
DWORD                               g_dwFrameNumberToDump = 0; 

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN                        1
#define IDC_TOGGLEREF                               3
#define IDC_CHANGEDEVICE                            4
#define IDC_STATIC_TECHNIQUE                       10
#define IDC_RADIOBUTTON_DISABLED                   11
#define IDC_RADIOBUTTON_POM                        12
#define IDC_RADIOBUTTON_DETAILTESSELLATION         13
#define IDC_STATIC_TEXTURE                         14
#define IDC_COMBOBOX_TEXTURE                       15
#define IDC_CHECKBOX_WIREFRAME                     16
#define IDC_CHECKBOX_DISPLACEMENT                  18
#define IDC_CHECKBOX_DENSITYBASED                  19    
#define IDC_STATIC_ADAPTIVE_SCHEME                 20
#define IDC_RADIOBUTTON_ADAPTIVESCHEME_OFF         21
#define IDC_RADIOBUTTON_ADAPTIVESCHEME_DISTANCE    22
#define IDC_RADIOBUTTON_ADAPTIVESCHEME_SCREENSPACE 23
#define IDC_STATIC_DESIRED_TRIANGLE_SIZE           24
#define IDC_SLIDER_DESIRED_TRIANGLE_SIZE           25
#define IDC_CHECKBOX_FRUSTUMCULLING                26
#define IDC_STATIC_TESS_FACTOR_EDGES               31
#define IDC_SLIDER_TESS_FACTOR_EDGES               32
#define IDC_STATIC_TESS_FACTOR_INSIDE              33
#define IDC_SLIDER_TESS_FACTOR_INSIDE              34
#define IDC_CHECKBOX_LINK_TESS_FACTORS             35
#define IDC_CHECKBOX_ROTATING_CAMERA               36

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, 
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, 
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain, 
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                                  double fTime, float fElapsedTime, void* pUserContext );

void ExtractPlanesFromFrustum( D3DXVECTOR4* pPlaneEquation, const D3DXMATRIX* pMatrix, bool bNormalize=TRUE );

void InitApp();
void RenderText();
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg );
bool GetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag );
void CreateDensityMapFromHeightMap( ID3D11Device* pd3dDevice, ID3D11DeviceContext *pDeviceContext, ID3D11Texture2D* pHeightMap, 
                                    float fDensityScale, ID3D11Texture2D** ppDensityMap, ID3D11Texture2D** ppDensityMapStaging=NULL, 
                                    float fMeaningfulDifference=0.0f);
void CreateEdgeDensityVertexStream( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pDeviceContext, D3DXVECTOR2* pTexcoord, 
                                    DWORD dwVertexStride, void *pIndex, DWORD dwIndexSize, DWORD dwNumIndices, 
                                    ID3D11Texture2D* pDensityMap, ID3D11Buffer** ppEdgeDensityVertexStream, 
                                    float fBaseTextureRepeat);
void CreateStagingBufferFromBuffer( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pContext, ID3D11Buffer* pInputBuffer, 
                                    ID3D11Buffer **ppStagingBuffer);
HRESULT CreateShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                              LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                              ID3DX11ThreadPump* pPump, ID3D11DeviceChild** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                              BOOL bDumpShader = FALSE);
HRESULT CreateVertexShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11VertexShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                    BOOL bDumpShader = FALSE);
HRESULT CreateHullShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                  LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                  ID3DX11ThreadPump* pPump, ID3D11HullShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                  BOOL bDumpShader = FALSE);
HRESULT CreateDomainShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11DomainShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                    BOOL bDumpShader = FALSE);
HRESULT CreateGeometryShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                      LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                      ID3DX11ThreadPump* pPump, ID3D11GeometryShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                      BOOL bDumpShader = FALSE);
HRESULT CreatePixelShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                   LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                   ID3DX11ThreadPump* pPump, ID3D11PixelShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                   BOOL bDumpShader = FALSE);
HRESULT CreateComputeShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                     LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                     ID3DX11ThreadPump* pPump, ID3D11ComputeShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                     BOOL bDumpShader = FALSE);


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

    // DXUT will create and use the best device (either D3D10 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    DXUTSetCallbackKeyboard( OnKeyboard );

    // Perform application-dependant command line processing
    WCHAR* strCmdLine = GetCommandLine();
    WCHAR strFlag[MAX_PATH];
    int nNumArgs;
    WCHAR** pstrArgList = CommandLineToArgvW( strCmdLine, &nNumArgs );
    for( int iArg=1; iArg<nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            if( IsNextArg( strCmdLine, L"method" ) )
             {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   if( ( wcscmp( strFlag, L"disabled" )==0 ) )
                   {
                       g_nTessellationMethod = TESSELLATIONMETHOD_DISABLED;
                   }
                   if( ( wcscmp( strFlag, L"pom" )==0 ) )
                   {
                       g_nTessellationMethod = TESSELLATIONMETHOD_DISABLED_POM;
                   }
                   else if( ( wcscmp( strFlag, L"detail" )==0 ) )
                   {
                       g_nTessellationMethod = TESSELLATIONMETHOD_DETAIL_TESSELLATION;
                   }

                   continue;
                }
             }

            if( IsNextArg( strCmdLine, L"grid" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_dwGridTessellation = _wtoi( strFlag );
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"dumpframe" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_dwFrameNumberToDump = _wtoi( strFlag );
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"tessedges" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_fTessellationFactorEdges = (float)_wtof( strFlag );
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"tessinside" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_fTessellationFactorInside = (float)_wtof( strFlag );
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"wireframe" ) )
            {
                g_bEnableWireFrame = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"framebasedanimation" ) )
            {
                g_bFrameBasedAnimation = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"animatedcamera" ) )
            {
                g_bAnimatedCamera = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"hud" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_nRenderHUD = _wtoi(strFlag);
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"nofps" ) )
            {
                g_bShowFPS = false;
                continue;
            }

            if( IsNextArg( strCmdLine, L"customtexture_diffuse" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                    wcscpy_s( g_pszCustomTextureDiffuseFilename, 
                              sizeof(g_pszCustomTextureDiffuseFilename)/sizeof(WCHAR), strFlag );
                    DetailTessellationTextures[NUM_TEXTURES].DiffuseMap = g_pszCustomTextureDiffuseFilename;
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"customtexture_normalheight" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                    wcscpy_s( g_pszCustomTextureNormalHeightFilename, 
                              sizeof(g_pszCustomTextureNormalHeightFilename)/sizeof(WCHAR), strFlag );
                    DetailTessellationTextures[NUM_TEXTURES].NormalHeightMap = g_pszCustomTextureNormalHeightFilename;
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"customtexture_heightscale" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                    DetailTessellationTextures[NUM_TEXTURES].fHeightScale = (float)_wtof( strFlag );
                    g_bRecreateCustomTextureDensityMap = true;
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"customtexture_densityscale" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                    DetailTessellationTextures[NUM_TEXTURES].fDensityScale = (float)_wtof( strFlag );
                    g_bRecreateCustomTextureDensityMap = true;
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"customtexture_meaningfuldifference" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                    DetailTessellationTextures[NUM_TEXTURES].fMeaningfulDifference = (float)_wtof( strFlag );
                    g_bRecreateCustomTextureDensityMap = true;
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"customtexture_basetexturerepeat" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                    DetailTessellationTextures[NUM_TEXTURES].fBaseTextureRepeat = (float)_wtof( strFlag );
                    g_bRecreateCustomTextureDensityMap = true;
                }
                continue;
            }
        }
    }

    InitApp();
    DXUTInit( true, true );
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"DetailTessellation11 v1.14" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1024, 768);
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    WCHAR szTemp[256];
    
    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent );
    int iX = 0;
    int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    iY = 0;
    g_SampleUI.AddRadioButton( IDC_RADIOBUTTON_DISABLED, 0, L"BUMP MAPPING", iX, iY += 25, 
                               55, 24, g_nTessellationMethod==TESSELLATIONMETHOD_DISABLED ? true : false, '1');
    g_SampleUI.AddRadioButton( IDC_RADIOBUTTON_POM, 0, L"PARALLAX OCCLUSION MAPPING", iX, iY += 25, 
                               55, 24, g_nTessellationMethod==TESSELLATIONMETHOD_DISABLED_POM ? true : false, '2');
    g_SampleUI.AddRadioButton( IDC_RADIOBUTTON_DETAILTESSELLATION, 0, L"DETAIL TESSELLATION", iX, iY += 25, 
                               55, 24, g_nTessellationMethod==TESSELLATIONMETHOD_DETAIL_TESSELLATION ? true : false, '3');

    iX += 35;
    g_SampleUI.AddStatic( IDC_STATIC_TEXTURE, L"Te(X)ture:", iX, iY += 30, 55, 24 );
    CDXUTComboBox *pCombo;
    g_SampleUI.AddComboBox( IDC_COMBOBOX_TEXTURE, iX, iY += 25, 140, 24, 'X', false, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 65 );
        for (int i=0; i<NUM_TEXTURES; ++i)
        {
            pCombo->AddItem( DetailTessellationTextures[i].DisplayName, NULL );
        }
        // Also add custom texture if one is specified
        if (wcslen(g_pszCustomTextureDiffuseFilename)!=0 && wcslen(g_pszCustomTextureNormalHeightFilename)!=0)
        {
            pCombo->AddItem( DetailTessellationTextures[NUM_TEXTURES].DisplayName, NULL );
            g_nCurrentTexture = NUM_TEXTURES;
        }

        pCombo->SetSelectedByIndex( g_nCurrentTexture );
    }
    iY += 90;

    g_SampleUI.AddCheckBox( IDC_CHECKBOX_WIREFRAME, L"Wire(F)rame", iX, iY += 25, 140, 24, 
                            g_bEnableWireFrame, 'F');
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_DISPLACEMENT, L"Displace(M)ent", iX, iY += 25, 140, 24, 
                            g_bDisplacementEnabled, 'M' );
    g_SampleUI.AddStatic( IDC_STATIC_ADAPTIVE_SCHEME, L"Adaptive scheme:", iX+5, iY += 25, 108, 24 );
    g_SampleUI.AddRadioButton(IDC_RADIOBUTTON_ADAPTIVESCHEME_OFF, 1, L"(O)ff", iX+5, iY += 25, 120, 20, 
                            g_bDistanceAdaptiveDetailTessellation, 'O' );
    g_SampleUI.AddRadioButton(IDC_RADIOBUTTON_ADAPTIVESCHEME_DISTANCE, 1, L"D(I)stance", iX+5, iY += 25, 120, 20, 
                            g_bDistanceAdaptiveDetailTessellation, 'I' );
    g_SampleUI.AddRadioButton(IDC_RADIOBUTTON_ADAPTIVESCHEME_SCREENSPACE, 1, L"Sc(R)een Space", iX+5, iY += 25, 120, 20, 
                            g_bScreenSpaceAdaptiveDetailTessellation, 'R' );

    swprintf_s( szTemp, L"Desired Triangle Size %d", (int)g_fDesiredTriangleSize );
    g_SampleUI.AddStatic( IDC_STATIC_DESIRED_TRIANGLE_SIZE, szTemp, iX+5, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_DESIRED_TRIANGLE_SIZE, iX, iY += 25, 140, 24, (int)(4 * 10), 
                          (int)(20 * 10), (int)(g_fDesiredTriangleSize*10.0f), false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_DENSITYBASED, L"De(N)sity-based", iX, iY += 25, 140, 24, 
                            g_bDensityBasedDetailTessellation, 'N' );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_FRUSTUMCULLING, L"Frustum C(U)lling opt.", iX, iY += 25, 140, 24, 
                            g_bUseFrustumCullingOptimization, 'U' );
    
    swprintf_s( szTemp, L"Tess. Factor (edges) %.2f", g_fTessellationFactorEdges );
    g_SampleUI.AddStatic( IDC_STATIC_TESS_FACTOR_EDGES, szTemp, iX+5, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_TESS_FACTOR_EDGES, iX, iY += 25, 140, 24, 1, 
                          (int)(MAX_TESSELLATION_FACTOR * 10), (int)(g_fTessellationFactorEdges*10.0f), false );
    swprintf_s( szTemp, L"Tess. Factor (inside) %.2f", g_fTessellationFactorInside );
    g_SampleUI.AddStatic( IDC_STATIC_TESS_FACTOR_INSIDE, szTemp, iX+5, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_TESS_FACTOR_INSIDE, iX, iY += 25, 140, 24, 1, 
                          (int)(MAX_TESSELLATION_FACTOR * 10), (int)(g_fTessellationFactorInside*10.0f), false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_LINK_TESS_FACTORS, L"Link Tess. Factors", iX, iY += 25, 100, 18, true );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_ROTATING_CAMERA, L"Rotating (C)amera", iX, iY += 25, 100, 18, 
                            g_bAnimatedCamera, 'C' );

    // Set default enabled/disabled
    g_SampleUI.GetControl( IDC_STATIC_DESIRED_TRIANGLE_SIZE )->SetEnabled( !g_bDistanceAdaptiveDetailTessellation );
    g_SampleUI.GetControl( IDC_SLIDER_DESIRED_TRIANGLE_SIZE )->SetEnabled( !g_bDistanceAdaptiveDetailTessellation );
    g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_EDGES )->SetEnabled( g_bDistanceAdaptiveDetailTessellation );
    g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_EDGES )->SetEnabled( g_bDistanceAdaptiveDetailTessellation );
    g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_INSIDE )->SetEnabled( g_bDistanceAdaptiveDetailTessellation );
    g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_INSIDE )->SetEnabled( g_bDistanceAdaptiveDetailTessellation );
    g_SampleUI.GetControl( IDC_CHECKBOX_LINK_TESS_FACTORS )->SetEnabled(g_bDistanceAdaptiveDetailTessellation );

    g_SampleUI.SetCallback( OnGUIEvent ); 

    // Setup the camera's view parameters
    g_Camera.SetRotateButtons( true, false, false );
    g_Camera.SetEnablePositionMovement( true );
    g_Camera.SetViewParams( &g_vecEye, &g_vecAt );
    g_Camera.SetScalers( 0.005f, 100.0f );
    D3DXVECTOR3 vEyePt, vEyeTo;
    vEyePt.x = 76.099495f;
    vEyePt.y = 43.191154f;
    vEyePt.z = -110.108971f;

    vEyeTo.x = 75.590889f;
    vEyeTo.y = 42.676582f;
    vEyeTo.z = -109.418678f; 

    g_Camera.SetViewParams( &vEyePt, &vEyeTo );
    // Initialize the light control
    D3DXVECTOR3 vLightDirection;
    vLightDirection = -(D3DXVECTOR3)g_LightPosition;
    D3DXVec3Normalize( &vLightDirection, &vLightDirection );
    g_LightControl.SetLightDirection( vLightDirection );
    g_LightControl.SetRadius( GRID_SIZE/2.0f );

    // Load default scene
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }

        // Enable 4xMSAA by default
        DXGI_SAMPLE_DESC MSAA4xSampleDesc = { 4, 0 };
        pDeviceSettings->d3d11.sd.SampleDesc = MSAA4xSampleDesc;
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( g_bShowFPS && DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    
    g_pTxtHelper->End();
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

    switch( uMsg )
    {
        case WM_KEYDOWN:    // Prevent the camera class to use some prefefined keys that we're already using    
                            switch( (UINT)wParam )
                            {
                                case VK_CONTROL:    
                                case VK_LEFT:        
                                case VK_RIGHT:         
                                case VK_UP:            
                                case VK_DOWN:          
                                case VK_PRIOR:              
                                case VK_NEXT:             

                                case VK_NUMPAD4:     
                                case VK_NUMPAD6:     
                                case VK_NUMPAD8:     
                                case VK_NUMPAD2:     
                                case VK_NUMPAD9:           
                                case VK_NUMPAD3:         

                                case VK_HOME:       return 0;

                                case VK_RETURN:     
                                {
                                    // Reset camera position
                                    g_Camera.SetViewParams( &g_vecEye, &g_vecAt );

                                    // Reset light position
                                    g_LightPosition = D3DXVECTOR4(100.0f, 30.0f, -50.0f, 1.0f);
                                    D3DXVECTOR3 vLightDirection;
                                    vLightDirection = -(D3DXVECTOR3)g_LightPosition;
                                    D3DXVec3Normalize(&vLightDirection, &vLightDirection);
                                    g_LightControl.SetLightDirection(vLightDirection);
                                }
                                break;
                            }
    }

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    // Pass the message to be handled to the light control:
    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case 'V':           // Debug views
                                if (g_vDebugColorMultiply.x==1.0)
                                {
                                    g_vDebugColorMultiply.x=0.0;
                                    g_vDebugColorMultiply.y=0.0;
                                    g_vDebugColorMultiply.z=0.0;
                                    g_vDebugColorAdd.x=1.0;
                                    g_vDebugColorAdd.y=1.0;
                                    g_vDebugColorAdd.z=1.0;
                                }
                                else
                                {
                                    g_vDebugColorMultiply.x=1.0;
                                    g_vDebugColorMultiply.y=1.0;
                                    g_vDebugColorMultiply.z=1.0;
                                    g_vDebugColorAdd.x=0.0;
                                    g_vDebugColorAdd.y=0.0;
                                    g_vDebugColorAdd.z=0.0;
                                }
                                break;

            case 'H':
            case VK_F1:         g_nRenderHUD = ( g_nRenderHUD + 1 ) % 3; break;

        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR szTemp[256];

    switch( nControlID )
    {
        // Standard DXUT controls
        case IDC_TOGGLEFULLSCREEN:  
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:         
            DXUTToggleREF();        break;
        case IDC_CHANGEDEVICE:      
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;

        case IDC_RADIOBUTTON_DISABLED:              
        {
            g_nTessellationMethod = TESSELLATIONMETHOD_DISABLED; 
            // Disable UI elements for tessellation controls
            g_SampleUI.GetControl( IDC_CHECKBOX_DISPLACEMENT )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_CHECKBOX_DISPLACEMENT )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_STATIC_ADAPTIVE_SCHEME )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_OFF )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_OFF )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_DISTANCE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_DISTANCE )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_SCREENSPACE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_SCREENSPACE )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_STATIC_DESIRED_TRIANGLE_SIZE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_SLIDER_DESIRED_TRIANGLE_SIZE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_CHECKBOX_DENSITYBASED )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_CHECKBOX_DENSITYBASED )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_CHECKBOX_FRUSTUMCULLING )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_CHECKBOX_FRUSTUMCULLING )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_EDGES )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_EDGES )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_INSIDE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_INSIDE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_CHECKBOX_LINK_TESS_FACTORS )->SetEnabled (false );
        }
        break;

        case IDC_RADIOBUTTON_POM:                   
        {
            g_nTessellationMethod = TESSELLATIONMETHOD_DISABLED_POM; 
            // Disable UI elements for tessellation controls
            g_SampleUI.GetControl( IDC_CHECKBOX_DISPLACEMENT )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_CHECKBOX_DISPLACEMENT )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_STATIC_ADAPTIVE_SCHEME )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_OFF )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_OFF )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_DISTANCE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_DISTANCE )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_SCREENSPACE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_SCREENSPACE )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_STATIC_DESIRED_TRIANGLE_SIZE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_SLIDER_DESIRED_TRIANGLE_SIZE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_CHECKBOX_DENSITYBASED )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_CHECKBOX_DENSITYBASED )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_CHECKBOX_FRUSTUMCULLING )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_CHECKBOX_FRUSTUMCULLING )->SetHotkey( 0 );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_EDGES )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_EDGES )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_INSIDE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_INSIDE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_CHECKBOX_LINK_TESS_FACTORS )->SetEnabled( false );
        }
        break;
        
        case IDC_RADIOBUTTON_DETAILTESSELLATION:    
        {
            g_nTessellationMethod = TESSELLATIONMETHOD_DETAIL_TESSELLATION; 
            // Enable UI elements for tessellation controls
            g_SampleUI.GetControl( IDC_CHECKBOX_DISPLACEMENT )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_CHECKBOX_DISPLACEMENT )->SetHotkey( 'M' );
            g_SampleUI.GetControl( IDC_STATIC_ADAPTIVE_SCHEME )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_OFF )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_OFF )->SetHotkey( 'O' );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_DISTANCE )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_DISTANCE )->SetHotkey( 'I' );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_SCREENSPACE )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_RADIOBUTTON_ADAPTIVESCHEME_SCREENSPACE )->SetHotkey( 'R' );
            g_SampleUI.GetControl( IDC_STATIC_DESIRED_TRIANGLE_SIZE )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_SLIDER_DESIRED_TRIANGLE_SIZE )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_CHECKBOX_DENSITYBASED )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_CHECKBOX_DENSITYBASED )->SetHotkey( 'N' );
            g_SampleUI.GetControl( IDC_CHECKBOX_FRUSTUMCULLING )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_CHECKBOX_FRUSTUMCULLING )->SetHotkey( 'U' );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_EDGES )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_EDGES )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_INSIDE )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_INSIDE )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_CHECKBOX_LINK_TESS_FACTORS )->SetEnabled( true );
        }
        break;

        case IDC_COMBOBOX_TEXTURE:
        {
            g_nCurrentTexture = ((CDXUTComboBox*)pControl)->GetSelectedIndex(); 
        }
        break;

        case IDC_CHECKBOX_WIREFRAME:                
        {
            g_bEnableWireFrame = ((CDXUTCheckBox*)pControl)->GetChecked(); 
        }
        break;

        case IDC_CHECKBOX_DISPLACEMENT:          
        {
            g_bDisplacementEnabled = ((CDXUTCheckBox*)pControl)->GetChecked(); 
        }
        break;

        case IDC_CHECKBOX_DENSITYBASED:    
        {
            g_bDensityBasedDetailTessellation = ((CDXUTCheckBox*)pControl)->GetChecked(); 
            g_bDetailTessellationShadersNeedReloading = true;
        }
        break;

        case IDC_RADIOBUTTON_ADAPTIVESCHEME_OFF:
        {
            g_bDistanceAdaptiveDetailTessellation = false;
            g_bScreenSpaceAdaptiveDetailTessellation = false;
            g_bDetailTessellationShadersNeedReloading = true;
            g_SampleUI.GetControl( IDC_STATIC_DESIRED_TRIANGLE_SIZE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_SLIDER_DESIRED_TRIANGLE_SIZE )->SetEnabled( false );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_EDGES )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_EDGES )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_INSIDE )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_INSIDE )->SetEnabled( true );
            g_SampleUI.GetControl( IDC_CHECKBOX_LINK_TESS_FACTORS )->SetEnabled(true );
        }
        break;

        case IDC_RADIOBUTTON_ADAPTIVESCHEME_DISTANCE:
        {
            g_bDistanceAdaptiveDetailTessellation = ((CDXUTCheckBox*)pControl)->GetChecked(); 
            g_bScreenSpaceAdaptiveDetailTessellation = !g_bDistanceAdaptiveDetailTessellation;
            g_bDetailTessellationShadersNeedReloading = true;
            g_SampleUI.GetControl( IDC_STATIC_DESIRED_TRIANGLE_SIZE )->SetEnabled( !g_bDistanceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_SLIDER_DESIRED_TRIANGLE_SIZE )->SetEnabled( !g_bDistanceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_EDGES )->SetEnabled( g_bDistanceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_EDGES )->SetEnabled( g_bDistanceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_INSIDE )->SetEnabled( g_bDistanceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_INSIDE )->SetEnabled( g_bDistanceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_CHECKBOX_LINK_TESS_FACTORS )->SetEnabled(g_bDistanceAdaptiveDetailTessellation );
        }
        break;

        case IDC_RADIOBUTTON_ADAPTIVESCHEME_SCREENSPACE:
        {
            g_bScreenSpaceAdaptiveDetailTessellation = ((CDXUTCheckBox*)pControl)->GetChecked(); 
            g_bDistanceAdaptiveDetailTessellation = !g_bScreenSpaceAdaptiveDetailTessellation;
            g_bDetailTessellationShadersNeedReloading = true;
            g_SampleUI.GetControl( IDC_STATIC_DESIRED_TRIANGLE_SIZE )->SetEnabled( g_bScreenSpaceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_SLIDER_DESIRED_TRIANGLE_SIZE )->SetEnabled( g_bScreenSpaceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_EDGES )->SetEnabled( !g_bScreenSpaceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_EDGES )->SetEnabled( !g_bScreenSpaceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_STATIC_TESS_FACTOR_INSIDE )->SetEnabled( !g_bScreenSpaceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_SLIDER_TESS_FACTOR_INSIDE )->SetEnabled( !g_bScreenSpaceAdaptiveDetailTessellation );
            g_SampleUI.GetControl( IDC_CHECKBOX_LINK_TESS_FACTORS )->SetEnabled(!g_bScreenSpaceAdaptiveDetailTessellation );
        }
        break;

        case IDC_SLIDER_DESIRED_TRIANGLE_SIZE:          
        {
            g_fDesiredTriangleSize = ((CDXUTSlider*)pControl)->GetValue() / 10.0f;
            swprintf_s( szTemp, L"Desired Triangle Size %d", (int)g_fDesiredTriangleSize );
            g_SampleUI.GetStatic( IDC_STATIC_DESIRED_TRIANGLE_SIZE)->SetText( szTemp );
        }
        break;
        
        case IDC_CHECKBOX_FRUSTUMCULLING:
        {
            g_bUseFrustumCullingOptimization = ((CDXUTCheckBox*)pControl)->GetChecked(); 
            g_bDetailTessellationShadersNeedReloading = true;
        }
        break;

        case IDC_SLIDER_TESS_FACTOR_EDGES:          
        {
            g_fTessellationFactorEdges = ((CDXUTSlider*)pControl)->GetValue() / 10.0f;
            swprintf_s( szTemp, L"Tess. Factor (edges) %.2f", g_fTessellationFactorEdges );
            g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTOR_EDGES )->SetText( szTemp );
            if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_LINK_TESS_FACTORS)->GetChecked() )
            {
                g_fTessellationFactorInside = g_fTessellationFactorEdges;
                g_SampleUI.GetSlider(IDC_SLIDER_TESS_FACTOR_INSIDE)->SetValue( (int)(g_fTessellationFactorInside*10.0f) );
                swprintf_s( szTemp, L"Tess. Factor (inside) %.2f", g_fTessellationFactorInside );
                g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTOR_INSIDE )->SetText( szTemp );
            }
        }
        break;

        case IDC_SLIDER_TESS_FACTOR_INSIDE:
        {
            g_fTessellationFactorInside = ((CDXUTSlider*)pControl)->GetValue() / 10.0f;
            swprintf_s( szTemp, L"Tess. Factor (inside) %.2f", g_fTessellationFactorInside );
            g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTOR_INSIDE )->SetText( szTemp );
            if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_LINK_TESS_FACTORS)->GetChecked() )
            {
                g_fTessellationFactorEdges = g_fTessellationFactorInside;
                g_SampleUI.GetSlider(IDC_SLIDER_TESS_FACTOR_EDGES)->SetValue( (int)(g_fTessellationFactorEdges*10.0f) );
                swprintf_s( szTemp, L"Tess. Factor (edges) %.2f", g_fTessellationFactorEdges );
                g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTOR_EDGES )->SetText( szTemp );
            }
        }
        break;
        case IDC_CHECKBOX_ROTATING_CAMERA:          
            g_bAnimatedCamera = ((CDXUTCheckBox*)pControl)->GetChecked(); 
            break;
    }
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
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

    // Get device context
    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    // GUI creation
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Light control
    g_LightControl.StaticOnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext );

    //
    // Compile shaders
    //
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    // POM shaders
    WCHAR wcPath[256];
    DXUTFindDXSDKMediaFileCch(wcPath, 256, L"POM.hlsl");
    CreateVertexShaderFromFile(pd3dDevice, wcPath, NULL, NULL, "RenderSceneVS", "vs_5_0", dwShaderFlags, 0, NULL, &g_pPOMVS);
    CreatePixelShaderFromFile(pd3dDevice,  wcPath, NULL, NULL, "RenderScenePS", "ps_5_0", dwShaderFlags, 0, NULL, &g_pPOMPS);

    // Detail tessellation shaders
    D3D_SHADER_MACRO    pDetailTessellationDefines[] = 
        { "DENSITY_BASED_TESSELLATION", g_bDensityBasedDetailTessellation ? "1" : "0", 
          "DISTANCE_ADAPTIVE_TESSELLATION", g_bDistanceAdaptiveDetailTessellation ? "1" : "0", 
          NULL, NULL };
    ID3DBlob* pBlobVS_DetailTessellation = NULL;
    
    DXUTFindDXSDKMediaFileCch(wcPath, 256, L"DetailTessellation11.hlsl");
    CreateVertexShaderFromFile( pd3dDevice, wcPath, pDetailTessellationDefines, NULL, "VS_NoTessellation", 
                               "vs_5_0", dwShaderFlags, 0, NULL, &g_pNoTessellationVS );
    CreateVertexShaderFromFile( pd3dDevice, wcPath, pDetailTessellationDefines, NULL, "VS",                
                               "vs_5_0", dwShaderFlags, 0, NULL, &g_pDetailTessellationVS, &pBlobVS_DetailTessellation );
    CreateHullShaderFromFile( pd3dDevice,   wcPath, pDetailTessellationDefines, NULL, "HS",                
                               "hs_5_0", dwShaderFlags, 0, NULL, &g_pDetailTessellationHS );
    CreateDomainShaderFromFile( pd3dDevice, wcPath, pDetailTessellationDefines, NULL, "DS",                
                               "ds_5_0", dwShaderFlags, 0, NULL, &g_pDetailTessellationDS );
    CreatePixelShaderFromFile( pd3dDevice,  wcPath, pDetailTessellationDefines, NULL, "BumpMapPS",
                               "ps_5_0", dwShaderFlags, 0, NULL, &g_pBumpMapPS );

    // Particle rendering shader
    ID3DBlob* pBlobVS_Particle = NULL;

    DXUTFindDXSDKMediaFileCch(wcPath, 256, L"Particle.hlsl");
    CreateVertexShaderFromFile( pd3dDevice,   wcPath, NULL, NULL, "VSPassThrough",   "vs_5_0", dwShaderFlags, 0, NULL, 
                                &g_pParticleVS, &pBlobVS_Particle);
    CreateGeometryShaderFromFile( pd3dDevice, wcPath, NULL, NULL, "GSPointSprite",   "gs_5_0", dwShaderFlags, 0, NULL, 
                                &g_pParticleGS);
    CreatePixelShaderFromFile( pd3dDevice,    wcPath, NULL, NULL, "PSConstantColor", "ps_5_0", dwShaderFlags, 0, NULL, 
                                &g_pParticlePS);


    //
    // Create vertex layouts
    //
    
    // Tangent space vertex input layout
    const D3D11_INPUT_ELEMENT_DESC tangentspacevertexlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( pd3dDevice->CreateInputLayout( tangentspacevertexlayout, ARRAYSIZE( tangentspacevertexlayout ), 
                                             pBlobVS_DetailTessellation->GetBufferPointer(), 
                                             pBlobVS_DetailTessellation->GetBufferSize(), 
                                             &g_pTangentSpaceVertexLayout ) );
    DXUT_SetDebugName( g_pTangentSpaceVertexLayout, "Tangent Space" );

    // Release blobs
    SAFE_RELEASE( pBlobVS_DetailTessellation );

    // Position-only vertex input layout
    const D3D11_INPUT_ELEMENT_DESC positiononlyvertexlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( pd3dDevice->CreateInputLayout( positiononlyvertexlayout, ARRAYSIZE( positiononlyvertexlayout ), 
                                             pBlobVS_Particle->GetBufferPointer(), pBlobVS_Particle->GetBufferSize(), 
                                             &g_pPositionOnlyVertexLayout ) );
    DXUT_SetDebugName( g_pPositionOnlyVertexLayout, "Pos Only") ;

    // Release blobs
    SAFE_RELEASE( pBlobVS_Particle );
    
    
    // Create main constant buffer
    D3D11_BUFFER_DESC bd;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof( CONSTANT_BUFFER_STRUCT );
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    hr = pd3dDevice->CreateBuffer( &bd, NULL, &g_pMainCB );
    if( FAILED( hr ) )
    {
        OutputDebugString(L"Failed to create constant buffer.\n");
        return hr;
    }
    DXUT_SetDebugName( g_pMainCB, "CONSTANT_BUFFER_STRUCT" );

    // Create POM constant buffer
    bd.ByteWidth = sizeof( MATERIAL_CB_STRUCT );
    hr = pd3dDevice->CreateBuffer( &bd, NULL, &g_pMaterialCB );
    if( FAILED( hr ) )
    {
        OutputDebugString(L"Failed to create POM constant buffer.\n");
        return hr;
    }
    DXUT_SetDebugName( g_pMaterialCB, "MATERIAL_CB_STRUCT" );

    // Create grid geometry
    FillGrid_Indexed_WithTangentSpace( pd3dDevice, g_dwGridTessellation, g_dwGridTessellation, GRID_SIZE, GRID_SIZE, 
                                       &g_pGridTangentSpaceVB, &g_pGridTangentSpaceIB );

    // Create particle VB
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof( D3DXVECTOR3 );
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    hr = pd3dDevice->CreateBuffer( &bd, NULL, &g_pParticleVB );
    if( FAILED( hr ) )
    {
        OutputDebugString(L"Failed to create particle vertex buffer.\n");
        return hr;
    }
    DXUT_SetDebugName( g_pParticleVB, "ParticleVB" );

    //
    // Load textures
    //
    
    // Determine how many textures to load
    g_dwNumTextures = NUM_TEXTURES;

    // Is a custom texture specified?
    if ( ( wcslen( DetailTessellationTextures[NUM_TEXTURES].DiffuseMap )!=0 ) && 
         ( wcslen( DetailTessellationTextures[NUM_TEXTURES].NormalHeightMap )!=0 ) )
    {
        // Yes, add one to number of textures and update array
        g_dwNumTextures += 1;
    }

    // Loop through all textures and load them
    WCHAR str[256];
    for ( int i=0; i<(int)g_dwNumTextures; ++i )
    {
        // Load detail tessellation base texture
        DXUTFindDXSDKMediaFileCch( str, 256, DetailTessellationTextures[i].DiffuseMap );
        hr = D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, 
                                                     NULL, NULL, &g_pDetailTessellationBaseTextureRV[i], NULL );
        if( FAILED( hr ) )
            return hr;

#if defined(DEBUG) || defined(PROFILE)
        CHAR strFileA[MAX_PATH];
        WideCharToMultiByte( CP_ACP, 0, DetailTessellationTextures[i].DiffuseMap, -1, strFileA, MAX_PATH, NULL, FALSE );
        CHAR* pstrName = strrchr( strFileA, '\\' );
        if( pstrName == NULL )
            pstrName = strFileA;
        else
            pstrName++;
        DXUT_SetDebugName( g_pDetailTessellationBaseTextureRV[i], pstrName );
#endif

        // Load detail tessellation normal+height texture
        DXUTFindDXSDKMediaFileCch( str, 256, DetailTessellationTextures[i].NormalHeightMap );
        hr = D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, 
                                                     NULL, NULL, &g_pDetailTessellationHeightTextureRV[i], NULL );
        if( FAILED( hr ) )
            return hr;

#if defined(DEBUG) || defined(PROFILE)
        WideCharToMultiByte( CP_ACP, 0, DetailTessellationTextures[i].NormalHeightMap, -1, strFileA, MAX_PATH, NULL, FALSE );
        pstrName = strrchr( strFileA, '\\' );
        if( pstrName == NULL )
            pstrName = strFileA;
        else
            pstrName++;
        DXUT_SetDebugName( g_pDetailTessellationHeightTextureRV[i], pstrName );
#endif

        // Compute density map filename
        WCHAR pszDensityMapFilename[256];
        WCHAR *pExtensionPointer;

        // Copy normal_height filename
        DXUTFindDXSDKMediaFileCch(pszDensityMapFilename, 256, DetailTessellationTextures[i].NormalHeightMap );
        pExtensionPointer = wcsrchr(pszDensityMapFilename, '.');
        swprintf_s(pExtensionPointer, pExtensionPointer-pszDensityMapFilename, L"_density.dds");
        
        {
            // Density file for this texture doesn't exist, create it

            // Get description of source texture
            ID3D11Texture2D* pHeightMap;
            ID3D11Texture2D* pDensityMap;
            g_pDetailTessellationHeightTextureRV[i]->GetResource( (ID3D11Resource**)&pHeightMap );
            DXUT_SetDebugName( pHeightMap, "Height Map" );
            
            // Create density map from height map
            CreateDensityMapFromHeightMap( pd3dDevice, pd3dImmediateContext, pHeightMap, 
                                           DetailTessellationTextures[i].fDensityScale, &pDensityMap, NULL, 
                                           DetailTessellationTextures[i].fMeaningfulDifference );

            // Save density map to file
            D3DX11SaveTextureToFile( pd3dImmediateContext, pDensityMap, D3DX11_IFF_DDS, pszDensityMapFilename );
            D3D11_TEXTURE2D_DESC d2ddsc;
            pDensityMap->GetDesc(&d2ddsc); 
            D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd;
            dsrvd.Format = d2ddsc.Format;
            dsrvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            dsrvd.Texture2D.MipLevels = d2ddsc.MipLevels;
            dsrvd.Texture2D.MostDetailedMip = 0;

            hr = pd3dDevice->CreateShaderResourceView( pDensityMap, &dsrvd, &g_pDetailTessellationDensityTextureRV[i] );
            DXUT_SetDebugName( g_pDetailTessellationDensityTextureRV[i], "Density SRV" );
            
            if( FAILED( hr ) )
                return hr;

            SAFE_RELEASE(pDensityMap);
            SAFE_RELEASE(pHeightMap);
        }

        // Create density vertex stream for grid

        // Create STAGING versions of VB and IB
        ID3D11Buffer* pGridVBSTAGING = NULL;
        CreateStagingBufferFromBuffer( pd3dDevice, pd3dImmediateContext, g_pGridTangentSpaceVB, &pGridVBSTAGING );
        DXUT_SetDebugName( pGridVBSTAGING, "Grid VB" );
        
        ID3D11Buffer* pGridIBSTAGING = NULL;
        CreateStagingBufferFromBuffer( pd3dDevice, pd3dImmediateContext, g_pGridTangentSpaceIB, &pGridIBSTAGING );
        DXUT_SetDebugName( pGridIBSTAGING, "Grid IB" );

        ID3D11Texture2D* pDensityMap = NULL;
        g_pDetailTessellationDensityTextureRV[i]->GetResource( (ID3D11Resource **)&pDensityMap );

        // Map those buffers for reading
        D3D11_MAPPED_SUBRESOURCE VBRead;
        D3D11_MAPPED_SUBRESOURCE IBRead;
        pd3dImmediateContext->Map( pGridVBSTAGING, 0, D3D11_MAP_READ, 0, &VBRead );
        pd3dImmediateContext->Map( pGridIBSTAGING, 0, D3D11_MAP_READ, 0, &IBRead );

        // Set up a pointer to texture coordinates
        D3DXVECTOR2* pTexcoord = (D3DXVECTOR2 *)( &( (float *)VBRead.pData )[3] );

        // Create vertex buffer containing edge density data
        CreateEdgeDensityVertexStream( pd3dDevice, pd3dImmediateContext, pTexcoord, sizeof(TANGENTSPACEVERTEX), 
                                       IBRead.pData, sizeof(WORD), 3*2*g_dwGridTessellation*g_dwGridTessellation, 
                                       pDensityMap, &g_pGridDensityVB[i], DetailTessellationTextures[i].fBaseTextureRepeat );

        pDensityMap->Release();

        pd3dImmediateContext->Unmap( pGridIBSTAGING, 0 );
        pd3dImmediateContext->Unmap( pGridVBSTAGING, 0 );

        SAFE_RELEASE( pGridIBSTAGING );
        SAFE_RELEASE( pGridVBSTAGING );

        // Create SRV for density vertex buffer
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVBufferDesc;
        SRVBufferDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVBufferDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        SRVBufferDesc.Buffer.ElementOffset = 0;
        SRVBufferDesc.Buffer.ElementWidth = 2*g_dwGridTessellation*g_dwGridTessellation;
        hr = pd3dDevice->CreateShaderResourceView(g_pGridDensityVB[i], &SRVBufferDesc, &g_pGridDensityVBSRV[i]);
        DXUT_SetDebugName( g_pGridDensityVBSRV[i], "Grid Density SRV" );
    }

    // Load light texture
    WCHAR path[256];

    DXUTFindDXSDKMediaFileCch(path, 256, L"media\\Particle.dds" );
    hr = D3DX11CreateShaderResourceViewFromFile( pd3dDevice, path, 
                                                 NULL, NULL, &g_pLightTextureRV, NULL );
    DXUT_SetDebugName( g_pLightTextureRV, "Particles.dds" );
    
    //
    // Create state objects
    //

    // Create solid and wireframe rasterizer state objects
    D3D11_RASTERIZER_DESC RasterDesc;
    ZeroMemory( &RasterDesc, sizeof( D3D11_RASTERIZER_DESC ) );
    RasterDesc.FillMode = D3D11_FILL_SOLID;
    RasterDesc.CullMode = D3D11_CULL_BACK;
    RasterDesc.DepthClipEnable = TRUE;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateSolid ) );
    DXUT_SetDebugName( g_pRasterizerStateSolid, "Solid" );

    RasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateWireframe ) );
    DXUT_SetDebugName( g_pRasterizerStateWireframe, "Wireframe" );

    // Create sampler state for heightmap and normal map
    D3D11_SAMPLER_DESC SSDesc;
    ZeroMemory( &SSDesc, sizeof( D3D11_SAMPLER_DESC ) );
    SSDesc.Filter =         D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SSDesc.AddressU =       D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressV =       D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressW =       D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SSDesc.MaxAnisotropy =  16;
    SSDesc.MinLOD =         0;
    SSDesc.MaxLOD =         D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SSDesc, &g_pSamplerStateLinear) );
    DXUT_SetDebugName( g_pSamplerStateLinear, "Linear" );

    // Create a blend state to disable alpha blending
    D3D11_BLEND_DESC BlendState;
    ZeroMemory(&BlendState, sizeof(D3D11_BLEND_DESC));
    BlendState.IndependentBlendEnable =                 FALSE;
    BlendState.RenderTarget[0].BlendEnable =            FALSE;
    BlendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = pd3dDevice->CreateBlendState( &BlendState, &g_pBlendStateNoBlend );
    DXUT_SetDebugName( g_pBlendStateNoBlend, "No Blending" );

    BlendState.RenderTarget[0].BlendEnable =            TRUE;
    BlendState.RenderTarget[0].BlendOp =                D3D11_BLEND_OP_ADD;
    BlendState.RenderTarget[0].SrcBlend =               D3D11_BLEND_ONE;
    BlendState.RenderTarget[0].DestBlend =              D3D11_BLEND_ONE;
    BlendState.RenderTarget[0].RenderTargetWriteMask =  D3D11_COLOR_WRITE_ENABLE_ALL;
    BlendState.RenderTarget[0].BlendOpAlpha =           D3D11_BLEND_OP_ADD;
    BlendState.RenderTarget[0].SrcBlendAlpha =          D3D11_BLEND_ZERO;
    BlendState.RenderTarget[0].DestBlendAlpha =         D3D11_BLEND_ZERO;
    hr = pd3dDevice->CreateBlendState(&BlendState, &g_pBlendStateAdditiveBlending);
    DXUT_SetDebugName( g_pBlendStateAdditiveBlending, "Additive Blending") ;
    
    // Create a depthstencil state
    D3D11_DEPTH_STENCIL_DESC DSDesc;
    DSDesc.DepthEnable =        TRUE;
    DSDesc.DepthFunc =          D3D11_COMPARISON_LESS_EQUAL;
    DSDesc.DepthWriteMask =     D3D11_DEPTH_WRITE_MASK_ALL;
    DSDesc.StencilEnable =      FALSE;
    hr = pd3dDevice->CreateDepthStencilState( &DSDesc, &g_pDepthStencilState );
    DXUT_SetDebugName( g_pDepthStencilState, "DepthStencil" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI/4, fAspectRatio, 2.0f, 100000.0f );

    // Set GUI size and locations
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 245, pBackBufferSurfaceDesc->Height - 660 );
    g_SampleUI.SetSize( 245, 660 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                                  double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT                     hr;
    static DWORD                s_dwFrameNumber = 1;
    ID3D11Buffer*               pBuffers[2];
    ID3D11ShaderResourceView*   pSRV[4];
    ID3D11SamplerState*         pSS[1];
    D3DXVECTOR3                 vFrom;

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Check if detail tessellation shaders need reloading
    if ( g_bDetailTessellationShadersNeedReloading )
    {
        // Yes, releasing existing detail tessellation shaders
        SAFE_RELEASE(g_pBumpMapPS);
        SAFE_RELEASE(g_pDetailTessellationDS);
        SAFE_RELEASE(g_pDetailTessellationHS);
        SAFE_RELEASE(g_pDetailTessellationVS);
        SAFE_RELEASE(g_pNoTessellationVS);

        // ... and reload them
        D3D_SHADER_MACRO    pDetailTessellationDefines[] = 
            { "DENSITY_BASED_TESSELLATION", g_bDensityBasedDetailTessellation ? "1" : "0", 
              "DISTANCE_ADAPTIVE_TESSELLATION", g_bDistanceAdaptiveDetailTessellation ? "1" : "0", 
              "SCREEN_SPACE_ADAPTIVE_TESSELLATION", g_bScreenSpaceAdaptiveDetailTessellation ? "1" : "0", 
              "FRUSTUM_CULLING_OPTIMIZATION", g_bUseFrustumCullingOptimization ? "1" : "0", 
              NULL, NULL };
        WCHAR wcPath[256];
        DXUTFindDXSDKMediaFileCch( wcPath, 256, L"DetailTessellation11.hlsl" );
        CreateVertexShaderFromFile(pd3dDevice, wcPath, pDetailTessellationDefines, NULL, 
                "VS_NoTessellation", "vs_5_0", 0, 0, NULL, &g_pNoTessellationVS);
        CreateVertexShaderFromFile(pd3dDevice, wcPath, pDetailTessellationDefines, NULL, 
                "VS","vs_5_0", 0, 0, NULL, &g_pDetailTessellationVS);
        CreateHullShaderFromFile(pd3dDevice,   wcPath, pDetailTessellationDefines, NULL, 
                "HS", "hs_5_0", 0, 0, NULL, &g_pDetailTessellationHS);
        CreateDomainShaderFromFile(pd3dDevice, wcPath, pDetailTessellationDefines, NULL, 
                "DS", "ds_5_0", 0, 0, NULL, &g_pDetailTessellationDS);
        CreatePixelShaderFromFile(pd3dDevice,  wcPath, pDetailTessellationDefines, NULL, 
                "BumpMapPS", "ps_5_0", 0, 0, NULL, &g_pBumpMapPS);
        
        g_bDetailTessellationShadersNeedReloading = false;
    }

    // Projection matrix
    D3DXMATRIX mProj = *g_Camera.GetProjMatrix();

    // View matrix
    D3DXMATRIX mView;
    if ( g_bAnimatedCamera )
    {
        float fRPS = 0.1f;
        float fAngleRAD;
        if ( g_bFrameBasedAnimation )
        {
            static float s_fTick = 0.0f;
            s_fTick += 1.0f/60.0f;
            fTime = s_fTick;
        }
        fAngleRAD = (float)( fRPS * fTime * 2.0f * D3DX_PI );
        float fRadius = (3.0f/4.0f) * GRID_SIZE;
        vFrom.x = fRadius * cos( fAngleRAD );
        vFrom.y = 80.0f;
        vFrom.z = fRadius * sin( fAngleRAD );
        const D3DXVECTOR3 vAt( 0, 0, 0 );
        const D3DXVECTOR3 vUp(0, 1, 0);
        D3DXMatrixLookAtLH( &mView, &vFrom, &vAt, &vUp );
    }
    else
    {
        vFrom = *g_Camera.GetEyePt();
        mView = *g_Camera.GetViewMatrix();
    }

    // World matrix: identity
    D3DXMATRIX mWorld;
    D3DXMatrixIdentity( &mWorld );

    // Update combined matrices
    D3DXMATRIX mWorldViewProjection = mWorld * mView * mProj;    
    D3DXMATRIX mViewProjection = mView * mProj;    
    D3DXMATRIX mInvView;
    D3DXMatrixInverse( &mInvView, NULL, &mView );

    // Calculate plane equations of frustum in world space
    ExtractPlanesFromFrustum( g_pWorldSpaceFrustumPlaneEquation, &mViewProjection );

    // Transpose matrices before passing to shader stages
    D3DXMatrixTranspose( &mProj, &mProj );
    D3DXMatrixTranspose( &mView, &mView );
    D3DXMatrixTranspose( &mWorld, &mWorld );                    
    D3DXMatrixTranspose( &mWorldViewProjection, &mWorldViewProjection ); 
    D3DXMatrixTranspose( &mViewProjection, &mViewProjection );
    D3DXMatrixTranspose( &mInvView, &mInvView );

    // Update light position from light direction control
    g_LightPosition.x =  -(GRID_SIZE/2.0f) * g_LightControl.GetLightDirection().x;
    g_LightPosition.y =  -(GRID_SIZE/2.0f) * g_LightControl.GetLightDirection().y;
    g_LightPosition.z =  -(GRID_SIZE/2.0f) * g_LightControl.GetLightDirection().z;

    // Update main constant buffer
    D3DXVECTOR4 vWhite( 1.0f, 1.0f, 1.0f, 1.0f );
    D3D11_MAPPED_SUBRESOURCE MappedSubResource;
    hr = pd3dImmediateContext->Map( g_pMainCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubResource );
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->mWorld =               mWorld;
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->mView =                mView;
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->mProjection =          mProj;
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->mWorldViewProjection = mWorldViewProjection;
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->mViewProjection =      mViewProjection;
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->mInvView =             mInvView;
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vScreenResolution =    
        D3DXVECTOR4((float)DXUTGetDXGIBackBufferSurfaceDesc()->Width, (float)DXUTGetDXGIBackBufferSurfaceDesc()->Height, 
                    1.0f/(float)DXUTGetDXGIBackBufferSurfaceDesc()->Width, 1.0f/(float)DXUTGetDXGIBackBufferSurfaceDesc()->Height);
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vMeshColor =           vWhite;
    // Min tessellation factor is half the selected edge tessellation factor
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vTessellationFactor =             
        D3DXVECTOR4( g_fTessellationFactorEdges, g_fTessellationFactorInside, g_fTessellationFactorEdges / 2.0f, 1.0f/g_fDesiredTriangleSize );  
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vDetailTessellationHeightScale =  
        g_bDisplacementEnabled ? D3DXVECTOR4( DetailTessellationTextures[g_nCurrentTexture].fHeightScale, 0.0f, 0.0f, 0.0f ) : vWhite;
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vGridSize =                       
        D3DXVECTOR4( GRID_SIZE, GRID_SIZE, 1.0f / GRID_SIZE, 1.0f / GRID_SIZE );
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vDebugColorMultiply=   D3DXVECTOR4( g_vDebugColorMultiply, 1.0f );
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vDebugColorAdd =       D3DXVECTOR4( g_vDebugColorAdd, 0.0f );
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vFrustumPlaneEquation[0] = g_pWorldSpaceFrustumPlaneEquation[0];
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vFrustumPlaneEquation[1] = g_pWorldSpaceFrustumPlaneEquation[1];
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vFrustumPlaneEquation[2] = g_pWorldSpaceFrustumPlaneEquation[2];
    ((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vFrustumPlaneEquation[3] = g_pWorldSpaceFrustumPlaneEquation[3];
    // Not using front clip plane for culling
    //((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vFrustumPlaneEquation[4] = g_pWorldSpaceFrustumPlaneEquation[4]; 
    // Not using far clip plane for culling
    //((CONSTANT_BUFFER_STRUCT *)MappedSubResource.pData)->vFrustumPlaneEquation[5] = g_pWorldSpaceFrustumPlaneEquation[5]; 
    pd3dImmediateContext->Unmap( g_pMainCB, 0 );

    // Update material constant buffer
    D3DXVECTOR4 g_colorMtrlAmbient( 0.35f, 0.35f, 0.35f, 0 );
    D3DXVECTOR4 g_colorMtrlDiffuse( 1.0f, 1.0f, 1.0f, 1.0f );
    D3DXVECTOR4 g_colorMtrlSpecular( 1.0f, 1.0f, 1.0f, 1.0f );
    float       g_fSpecularExponent( 60.0f );
    hr = pd3dImmediateContext->Map( g_pMaterialCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubResource );
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_materialAmbientColor =    g_colorMtrlAmbient;
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_materialDiffuseColor =    g_colorMtrlDiffuse;
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_materialSpecularColor =   g_colorMtrlSpecular;
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_fSpecularExponent =       D3DXVECTOR4( g_fSpecularExponent, 0.0f, 0.0f, 0.0f );
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_LightPosition =           g_LightPosition;
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_LightDiffuse =            vWhite;
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_LightAmbient =            vWhite;
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_vEye =                    D3DXVECTOR4( vFrom, 0.0f );
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_fBaseTextureRepeat =      
        D3DXVECTOR4( DetailTessellationTextures[g_nCurrentTexture].fBaseTextureRepeat, 0.0f, 0.0f, 0.0f );
    // POM height scale is in texcoord per world space unit thus: (heightscale * basetexturerepeat) / (texture size in world space units)
    float fPomHeightScale = 
        ( DetailTessellationTextures[g_nCurrentTexture].fHeightScale*DetailTessellationTextures[g_nCurrentTexture].fBaseTextureRepeat ) / GRID_SIZE;
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_fPOMHeightMapScale =      D3DXVECTOR4(fPomHeightScale , 0.0f, 0.0f, 0.0f );
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_fShadowSoftening =        D3DXVECTOR4( 0.58f, 0.0f, 0.0f, 0.0f );
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_nMinSamples =             8;
    ((MATERIAL_CB_STRUCT *)MappedSubResource.pData)->g_nMaxSamples =             50;
    pd3dImmediateContext->Unmap(g_pMaterialCB, 0);

    // Bind the constant buffers to the device for all stages
    pBuffers[0] = g_pMainCB;
    pBuffers[1] = g_pMaterialCB;
    pd3dImmediateContext->VSSetConstantBuffers( 0, 2, pBuffers );
    pd3dImmediateContext->GSSetConstantBuffers( 0, 2, pBuffers );
    pd3dImmediateContext->HSSetConstantBuffers( 0, 2, pBuffers );
    pd3dImmediateContext->DSSetConstantBuffers( 0, 2, pBuffers );
    pd3dImmediateContext->PSSetConstantBuffers( 0, 2, pBuffers );

    // Set sampler states
    pSS[0] = g_pSamplerStateLinear;
    pd3dImmediateContext->VSSetSamplers(0, 1, pSS);
    pd3dImmediateContext->HSSetSamplers(0, 1, pSS);
    pd3dImmediateContext->DSSetSamplers(0, 1, pSS);
    pd3dImmediateContext->PSSetSamplers(0, 1, pSS);

    // Set states
    pd3dImmediateContext->OMSetBlendState( g_pBlendStateNoBlend, 0, 0xffffffff );
    pd3dImmediateContext->RSSetState( g_bEnableWireFrame ? g_pRasterizerStateWireframe : g_pRasterizerStateSolid );
    pd3dImmediateContext->OMSetDepthStencilState( g_pDepthStencilState, 0 );

    // Clear the render target and depth stencil
    float ClearColor[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0 );

    // Set vertex buffer
    UINT stride = sizeof(TANGENTSPACEVERTEX);
    UINT offset = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &g_pGridTangentSpaceVB, &stride, &offset );

    // Set index buffer
    pd3dImmediateContext->IASetIndexBuffer( g_pGridTangentSpaceIB, DXGI_FORMAT_R16_UINT, 0 );

    // Set input layout
    pd3dImmediateContext->IASetInputLayout( g_pTangentSpaceVertexLayout );

    // Set shaders and geometries
    switch (g_nTessellationMethod)
    {
        case TESSELLATIONMETHOD_DISABLED:
        {
            // Render grid with simple bump mapping applied            
            pd3dImmediateContext->VSSetShader( g_pNoTessellationVS, NULL, 0 );
            pd3dImmediateContext->HSSetShader( NULL, NULL, 0);
            pd3dImmediateContext->DSSetShader( NULL, NULL, 0);
            pd3dImmediateContext->GSSetShader( NULL, NULL, 0 );
            pd3dImmediateContext->PSSetShader( g_pBumpMapPS, NULL, 0 ); 

            // Set primitive topology
            pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

            // Set shader resources
            pSRV[0] = g_pDetailTessellationBaseTextureRV[g_nCurrentTexture];
            pSRV[1] = g_pDetailTessellationHeightTextureRV[g_nCurrentTexture];
            pd3dImmediateContext->VSSetShaderResources( 0, 2, pSRV );
            pd3dImmediateContext->PSSetShaderResources( 0, 2, pSRV );
        }
        break;

        case TESSELLATIONMETHOD_DISABLED_POM:
        {
            // Render grid with POM applied                
            pd3dImmediateContext->VSSetShader( g_pPOMVS, NULL, 0 );
            pd3dImmediateContext->HSSetShader( NULL, NULL, 0);
            pd3dImmediateContext->DSSetShader( NULL, NULL, 0);
            pd3dImmediateContext->GSSetShader( NULL, NULL, 0 );
            pd3dImmediateContext->PSSetShader( g_pPOMPS, NULL, 0 ); 

            // Set primitive topology
            pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

            // Set shader resources
            pSRV[0] = g_pDetailTessellationBaseTextureRV[g_nCurrentTexture];
            pSRV[1] = g_pDetailTessellationHeightTextureRV[g_nCurrentTexture];
            pd3dImmediateContext->PSSetShaderResources( 0, 2, pSRV );
        }
        break;

        case TESSELLATIONMETHOD_DETAIL_TESSELLATION:
        {
            // Render grid with detail tessellation
            pd3dImmediateContext->VSSetShader( g_pDetailTessellationVS, NULL, 0 );
            pd3dImmediateContext->HSSetShader( g_pDetailTessellationHS, NULL, 0);
            pd3dImmediateContext->DSSetShader( g_pDetailTessellationDS, NULL, 0);
            pd3dImmediateContext->GSSetShader( NULL, NULL, 0 );
            pd3dImmediateContext->PSSetShader( g_pBumpMapPS, NULL, 0 ); 

            // Set primitive topology
            pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST );

            // Set shader resources
            pSRV[0] = g_pDetailTessellationBaseTextureRV[g_nCurrentTexture];
            pSRV[1] = g_pDetailTessellationHeightTextureRV[g_nCurrentTexture];
            pSRV[2] = g_pDetailTessellationDensityTextureRV[g_nCurrentTexture];
            pSRV[3] = g_pGridDensityVBSRV[g_nCurrentTexture];
            pd3dImmediateContext->PSSetShaderResources( 0, 3, pSRV );
            pd3dImmediateContext->DSSetShaderResources( 0, 3, pSRV );
            pd3dImmediateContext->VSSetShaderResources( 0, 4, pSRV );

            pSRV[0] = g_pGridDensityVBSRV[g_nCurrentTexture];
            pd3dImmediateContext->HSSetShaderResources( 0, 1, pSRV );
        }
        break;
    }

    // Draw grid
    pd3dImmediateContext->DrawIndexed( 3*2*g_dwGridTessellation*g_dwGridTessellation, 0, 0 );

    // Draw light source if requested
    if ( g_bDrawLightSource )
    {
        // Set shaders
        pd3dImmediateContext->VSSetShader( g_pParticleVS, NULL, 0 );
        pd3dImmediateContext->HSSetShader( NULL, NULL, 0);
        pd3dImmediateContext->DSSetShader( NULL, NULL, 0);
        pd3dImmediateContext->GSSetShader( g_pParticleGS, NULL, 0 );
        pd3dImmediateContext->PSSetShader( g_pParticlePS, NULL, 0 ); 

        // Set primitive topology
        pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );

        // Set shader resources
        pSRV[0] = g_pLightTextureRV;
        pd3dImmediateContext->PSSetShaderResources( 0, 1, pSRV );

        // Store new light position into particle's VB
        D3D11_MAPPED_SUBRESOURCE MappedSubresource;
        pd3dImmediateContext->Map( g_pParticleVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource );
        *(D3DXVECTOR3*)MappedSubresource.pData = (D3DXVECTOR3)g_LightPosition;
        pd3dImmediateContext->Unmap( g_pParticleVB, 0 );

        // Set vertex buffer
        UINT stride = sizeof(D3DXVECTOR3);
        UINT offset = 0;
        pd3dImmediateContext->IASetVertexBuffers( 0, 1, &g_pParticleVB, &stride, &offset );

        // Set input layout
        pd3dImmediateContext->IASetInputLayout( g_pPositionOnlyVertexLayout );

        // Additive blending
        pd3dImmediateContext->OMSetBlendState( g_pBlendStateAdditiveBlending, 0, 0xffffffff );

        // Solid rendering (not affected by global wireframe toggle)
        pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );

        // Draw light
        pd3dImmediateContext->Draw( 1, 0 );
    }

    // Render the HUD
    if ( g_nRenderHUD > 0 )
    {
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
        if ( g_nRenderHUD > 1 )
        {
            g_HUD.OnRender( fElapsedTime );
            g_SampleUI.OnRender( fElapsedTime );
        }
        RenderText();
        DXUT_EndPerfEvent();
    }

    // Check if current frame needs to be dumped to disk
    if ( s_dwFrameNumber == g_dwFrameNumberToDump )
    {
        // Retrieve resource for current render target
        ID3D11Resource *pRTResource;
        DXUTGetD3D11RenderTargetView()->GetResource( &pRTResource );

        // Retrieve a Texture2D interface from resource
        ID3D11Texture2D* RTTexture;
        hr = pRTResource->QueryInterface( __uuidof( ID3D11Texture2D ), ( LPVOID* )&RTTexture );

        // Check if RT is multisampled or not
        D3D11_TEXTURE2D_DESC TexDesc;
        RTTexture->GetDesc( &TexDesc );
        if ( TexDesc.SampleDesc.Count > 1 )
        {
            // RT is multisampled: need resolving before dumping to disk

            // Create single-sample RT of the same type and dimensions
            DXGI_SAMPLE_DESC SingleSample = { 1, 0 };
            TexDesc.CPUAccessFlags =    D3D11_CPU_ACCESS_READ;
            TexDesc.MipLevels =         1;
            TexDesc.Usage =             D3D11_USAGE_DEFAULT;
            TexDesc.CPUAccessFlags =    0;
            TexDesc.BindFlags =         0;
            TexDesc.SampleDesc =        SingleSample;

            // Create single-sample texture
            ID3D11Texture2D *pSingleSampleTexture;
            hr = pd3dDevice->CreateTexture2D( &TexDesc, NULL, &pSingleSampleTexture );
            DXUT_SetDebugName( pSingleSampleTexture, "Single Sample" );

            // Resolve multisampled render target into single-sample texture
            pd3dImmediateContext->ResolveSubresource( pSingleSampleTexture, 0, RTTexture, 0, TexDesc.Format );

            // Save texture to disk
            hr = D3DX11SaveTextureToFile( pd3dImmediateContext, pSingleSampleTexture, D3DX11_IFF_BMP, L"RTOutput.bmp" );

            // Release single-sample version of texture
            SAFE_RELEASE(pSingleSampleTexture);
            
        }
        else
        {
            // Single sample case
            hr = D3DX11SaveTextureToFile( pd3dImmediateContext, RTTexture, D3DX11_IFF_BMP, L"RTOutput.bmp" );
        }

        // Release texture and resource
        SAFE_RELEASE(RTTexture);
        SAFE_RELEASE(pRTResource);
    }

    // Increase frame number
    s_dwFrameNumber++;
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    g_LightControl.StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    SAFE_RELEASE( g_pLightTextureRV );
    for ( int i=0; i<(int)g_dwNumTextures; ++i )
    {
        SAFE_RELEASE( g_pGridDensityVBSRV[i] );
        SAFE_RELEASE( g_pGridDensityVB[i] );
        SAFE_RELEASE( g_pDetailTessellationDensityTextureRV[i] );
        SAFE_RELEASE( g_pDetailTessellationHeightTextureRV[i] );
        SAFE_RELEASE( g_pDetailTessellationBaseTextureRV[i] );
    }
    SAFE_RELEASE( g_pParticleVB );
    SAFE_RELEASE( g_pGridTangentSpaceIB )
    SAFE_RELEASE( g_pGridTangentSpaceVB );
    
    SAFE_RELEASE( g_pPositionOnlyVertexLayout );
    SAFE_RELEASE( g_pTangentSpaceVertexLayout );

    SAFE_RELEASE( g_pParticlePS );
    SAFE_RELEASE( g_pParticleGS );
    SAFE_RELEASE( g_pParticleVS );
    SAFE_RELEASE( g_pBumpMapPS );
    SAFE_RELEASE( g_pDetailTessellationDS );
    SAFE_RELEASE( g_pDetailTessellationHS );
    SAFE_RELEASE( g_pDetailTessellationVS );
    SAFE_RELEASE( g_pNoTessellationVS );

    SAFE_RELEASE( g_pPOMPS );
    SAFE_RELEASE( g_pPOMVS );

    SAFE_RELEASE( g_pMaterialCB );
    SAFE_RELEASE( g_pMainCB );

    SAFE_RELEASE( g_pDepthStencilState );
    SAFE_RELEASE( g_pBlendStateAdditiveBlending );
    SAFE_RELEASE( g_pBlendStateNoBlend );
    SAFE_RELEASE( g_pRasterizerStateSolid );
    SAFE_RELEASE( g_pRasterizerStateWireframe );
    SAFE_RELEASE( g_pSamplerStateLinear );
}


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval
//--------------------------------------------------------------------------------------
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg )
{
   int nArgLen = (int)wcslen( strArg );
   int nCmdLen = (int)wcslen( strCmdLine );

   if( ( nCmdLen >= nArgLen ) && 
       ( _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 ) && 
       ( (strCmdLine[nArgLen] == 0 || strCmdLine[nArgLen] == L':' || strCmdLine[nArgLen] == L'=' ) ) )
   {
      strCmdLine += nArgLen;
      return true;
   }

   return false;
}


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval.  Updates strCmdLine and strFlag 
//      Example: if strCmdLine=="-width:1024 -forceref"
// then after: strCmdLine==" -forceref" and strFlag=="1024"
//--------------------------------------------------------------------------------------
bool GetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag )
{
   if( *strCmdLine == L':' || *strCmdLine == L'=' )
   {       
      strCmdLine++; // Skip ':'

      // Place NULL terminator in strFlag after current token
      wcscpy_s( strFlag, 256, strCmdLine );
      WCHAR* strSpace = strFlag;
      while ( *strSpace && (*strSpace > L' ') )
         strSpace++;
      *strSpace = 0;

      // Update strCmdLine
      strCmdLine += wcslen( strFlag );
      return true;
   }
   else
   {
      strFlag[0] = 0;
      return false;
   }
}






//--------------------------------------------------------------------------------------
// Create a density texture map from a height map
//--------------------------------------------------------------------------------------
void CreateDensityMapFromHeightMap( ID3D11Device* pd3dDevice, ID3D11DeviceContext *pDeviceContext, ID3D11Texture2D* pHeightMap, 
                                    float fDensityScale, ID3D11Texture2D** ppDensityMap, ID3D11Texture2D** ppDensityMapStaging, 
                                    float fMeaningfulDifference )
{
#define ReadPixelFromMappedSubResource(x, y)       ( *( (RGBQUAD *)((BYTE *)MappedSubResourceRead.pData + (y)*MappedSubResourceRead.RowPitch + (x)*sizeof(DWORD)) ) )
#define WritePixelToMappedSubResource(x, y, value) ( ( *( (DWORD *)((BYTE *)MappedSubResourceWrite.pData + (y)*MappedSubResourceWrite.RowPitch + (x)*sizeof(DWORD)) ) ) = (DWORD)(value) )

    // Get description of source texture
    D3D11_TEXTURE2D_DESC Desc;
    pHeightMap->GetDesc( &Desc );    

    // Create density map with the same properties
    pd3dDevice->CreateTexture2D( &Desc, NULL, ppDensityMap );
    DXUT_SetDebugName( *ppDensityMap, "Density Map" );

    // Create STAGING height map texture
    ID3D11Texture2D* pHeightMapStaging;
    Desc.CPUAccessFlags =   D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    Desc.Usage =            D3D11_USAGE_STAGING;
    Desc.BindFlags =        0;
    pd3dDevice->CreateTexture2D( &Desc, NULL, &pHeightMapStaging );
    DXUT_SetDebugName( pHeightMapStaging, "Height Map" );

    // Create STAGING density map
    ID3D11Texture2D* pDensityMapStaging;
    pd3dDevice->CreateTexture2D( &Desc, NULL, &pDensityMapStaging );
    DXUT_SetDebugName( pDensityMapStaging, "Density Map" );

    // Copy contents of height map into staging version
    pDeviceContext->CopyResource( pHeightMapStaging, pHeightMap );

    // Compute density map and store into staging version
    D3D11_MAPPED_SUBRESOURCE MappedSubResourceRead, MappedSubResourceWrite;
    pDeviceContext->Map( pHeightMapStaging, 0, D3D11_MAP_READ, 0, &MappedSubResourceRead );

    pDeviceContext->Map( pDensityMapStaging, 0, D3D11_MAP_WRITE, 0, &MappedSubResourceWrite );

    // Loop map and compute derivatives
    for ( int j=0; j <= (int)Desc.Height-1; ++j )
    {
        for ( int i=0; i <= (int)Desc.Width-1; ++i )
        {
            // Edges are set to the minimum value
            if ( ( j == 0 ) || 
                 ( i == 0 ) || 
                 ( j == ( (int)Desc.Height-1 ) ) || 
                 ( i == ((int)Desc.Width-1 ) ) )
            {
                WritePixelToMappedSubResource( i, j, (DWORD)1 );
                continue;
            }

            // Get current pixel
            RGBQUAD CurrentPixel =     ReadPixelFromMappedSubResource( i, j );

            // Get left pixel
            RGBQUAD LeftPixel =        ReadPixelFromMappedSubResource( i-1, j );

            // Get right pixel
            RGBQUAD RightPixel =       ReadPixelFromMappedSubResource( i+1, j );

            // Get top pixel
            RGBQUAD TopPixel =         ReadPixelFromMappedSubResource( i, j-1 );

            // Get bottom pixel
            RGBQUAD BottomPixel =      ReadPixelFromMappedSubResource( i, j+1 );

            // Get top-right pixel
            RGBQUAD TopRightPixel =    ReadPixelFromMappedSubResource( i+1, j-1 );

            // Get bottom-right pixel
            RGBQUAD BottomRightPixel = ReadPixelFromMappedSubResource( i+1, j+1 );
            
            // Get top-left pixel
            RGBQUAD TopLeftPixel =     ReadPixelFromMappedSubResource( i-1, j-1 );
            
            // Get bottom-left pixel
            RGBQUAD BottomLeft =       ReadPixelFromMappedSubResource( i-1, j+1 );

            float fCurrentPixelHeight =            ( CurrentPixel.rgbReserved     / 255.0f );
            float fCurrentPixelLeftHeight =        ( LeftPixel.rgbReserved        / 255.0f );
            float fCurrentPixelRightHeight =       ( RightPixel.rgbReserved       / 255.0f );
            float fCurrentPixelTopHeight =         ( TopPixel.rgbReserved         / 255.0f );
            float fCurrentPixelBottomHeight =      ( BottomPixel.rgbReserved      / 255.0f );
            float fCurrentPixelTopRightHeight =    ( TopRightPixel.rgbReserved    / 255.0f );
            float fCurrentPixelBottomRightHeight = ( BottomRightPixel.rgbReserved / 255.0f );
            float fCurrentPixelTopLeftHeight =     ( TopLeftPixel.rgbReserved     / 255.0f );
            float fCurrentPixelBottomLeftHeight =  ( BottomLeft.rgbReserved       / 255.0f );

            float fDensity = 0.0f;
            float fHorizontalVariation = fabs( ( fCurrentPixelRightHeight - fCurrentPixelHeight ) - 
                                               ( fCurrentPixelHeight - fCurrentPixelLeftHeight ) );
            float fVerticalVariation = fabs( ( fCurrentPixelBottomHeight - fCurrentPixelHeight ) - 
                                             ( fCurrentPixelHeight - fCurrentPixelTopHeight ) );
            float fFirstDiagonalVariation = fabs( ( fCurrentPixelTopRightHeight - fCurrentPixelHeight ) - 
                                                  ( fCurrentPixelHeight - fCurrentPixelBottomLeftHeight ) );
            float fSecondDiagonalVariation = fabs( ( fCurrentPixelBottomRightHeight - fCurrentPixelHeight ) - 
                                                   ( fCurrentPixelHeight - fCurrentPixelTopLeftHeight ) );
            if ( fHorizontalVariation     >= fMeaningfulDifference) fDensity += 0.293f * fHorizontalVariation;
            if ( fVerticalVariation       >= fMeaningfulDifference) fDensity += 0.293f * fVerticalVariation;
            if ( fFirstDiagonalVariation  >= fMeaningfulDifference) fDensity += 0.207f * fFirstDiagonalVariation;
            if ( fSecondDiagonalVariation >= fMeaningfulDifference) fDensity += 0.207f * fSecondDiagonalVariation;
                
            // Scale density with supplied scale                
            fDensity *= fDensityScale;

            // Clamp density
            fDensity = max( min( fDensity, 1.0f ), 1.0f/255.0f );

            // Write density into density map
            WritePixelToMappedSubResource( i, j, (DWORD)( fDensity * 255.0f ) );
        }
    }

    pDeviceContext->Unmap( pDensityMapStaging, 0 );

    pDeviceContext->Unmap( pHeightMapStaging, 0 );
    SAFE_RELEASE( pHeightMapStaging );

    // Copy contents of density map into DEFAULT density version
    pDeviceContext->CopyResource( *ppDensityMap, pDensityMapStaging );
    
    // If a pointer to a staging density map was provided then return it with staging density map
    if ( ppDensityMapStaging )
    {
        *ppDensityMapStaging = pDensityMapStaging;
    }
    else
    {
        SAFE_RELEASE( pDensityMapStaging );
    }
}


//--------------------------------------------------------------------------------------
// Sample a 32-bit texture at the specified texture coordinate (point sampling only)
//--------------------------------------------------------------------------------------
__inline RGBQUAD SampleTexture2D_32bit( DWORD *pTexture2D, DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 fTexcoord, DWORD dwStride )
{
#define FROUND(x)    ( (int)( (x) + 0.5 ) )

    // Convert texture coordinates to texel coordinates using round-to-nearest
    int nU = FROUND( fTexcoord.x * ( dwWidth-1 ) );
    int nV = FROUND( fTexcoord.y * ( dwHeight-1 ) );

    // Clamp texcoord between 0 and [width-1, height-1]
    nU = nU % dwWidth;
    nV = nV % dwHeight;

    // Return value at this texel coordinate
    return *(RGBQUAD *)( ( (BYTE *)pTexture2D) + ( nV*dwStride ) + ( nU*sizeof(DWORD) ) );
}


//--------------------------------------------------------------------------------------
// Sample density map along specified edge and return maximum value
//--------------------------------------------------------------------------------------
float GetMaximumDensityAlongEdge( DWORD *pTexture2D, DWORD dwStride, DWORD dwWidth, DWORD dwHeight, 
                                  D3DXVECTOR2 fTexcoord0, D3DXVECTOR2 fTexcoord1 )
{
#define GETTEXEL(x, y)       ( *(RGBQUAD *)( ( (BYTE *)pTexture2D ) + ( (y)*dwStride ) + ( (x)*sizeof(DWORD) ) ) )
#define LERP(x, y, a)        ( (x)*(1.0f-(a)) + (y)*(a) )

    // Convert texture coordinates to texel coordinates using round-to-nearest
    int nU0 = FROUND( fTexcoord0.x * ( dwWidth  - 1 )  );
    int nV0 = FROUND( fTexcoord0.y * ( dwHeight - 1 ) );
    int nU1 = FROUND( fTexcoord1.x * ( dwWidth  - 1 )  );
    int nV1 = FROUND( fTexcoord1.y * ( dwHeight - 1 ) );

    // Wrap texture coordinates
    nU0 = nU0 % dwWidth;
    nV0 = nV0 % dwHeight;
    nU1 = nU1 % dwWidth;
    nV1 = nV1 % dwHeight;

    // Find how many texels on edge
    int nNumTexelsOnEdge = max( abs( nU1 - nU0 ), abs( nV1 - nV0 ) ) + 1;

    float fU, fV;
    float fMaxDensity = 0.0f;
    for ( int i=0; i<nNumTexelsOnEdge; ++i )
    {
        // Calculate current texel coordinates
        fU = LERP( (float)nU0, (float)nU1, ( (float)i ) / ( nNumTexelsOnEdge - 1 ) );
        fV = LERP( (float)nV0, (float)nV1, ( (float)i ) / ( nNumTexelsOnEdge - 1 ) );

        // Round texel texture coordinates to nearest
        int nCurrentU = FROUND( fU );
        int nCurrentV = FROUND( fV );

        // Update max density along edge
        fMaxDensity = max( fMaxDensity, GETTEXEL( nCurrentU, nCurrentV ).rgbBlue / 255.0f );
    }
        
    return fMaxDensity;
}


//--------------------------------------------------------------------------------------
// Calculate the maximum density along a triangle edge and
// store it in the resulting vertex stream.
//--------------------------------------------------------------------------------------
void CreateEdgeDensityVertexStream( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pDeviceContext, D3DXVECTOR2* pTexcoord, 
                                    DWORD dwVertexStride, void *pIndex, DWORD dwIndexSize, DWORD dwNumIndices, 
                                    ID3D11Texture2D* pDensityMap, ID3D11Buffer** ppEdgeDensityVertexStream,
                                    float fBaseTextureRepeat )
{
    HRESULT                hr;
    D3D11_SUBRESOURCE_DATA InitData;
    ID3D11Texture2D*       pDensityMapReadFrom;
    DWORD                  dwNumTriangles = dwNumIndices / 3;

    // Create host memory buffer
    D3DXVECTOR4 *pEdgeDensityBuffer = new D3DXVECTOR4[dwNumTriangles];

    // Retrieve density map description
    D3D11_TEXTURE2D_DESC Tex2DDesc;
    pDensityMap->GetDesc( &Tex2DDesc );
 
    // Check if provided texture can be Mapped() for reading directly
    BOOL bCanBeRead = Tex2DDesc.CPUAccessFlags & D3D11_CPU_ACCESS_READ;
    if ( !bCanBeRead )
    {
        // Texture cannot be read directly, need to create STAGING texture and copy contents into it
        Tex2DDesc.CPUAccessFlags =   D3D11_CPU_ACCESS_READ;
        Tex2DDesc.Usage =            D3D11_USAGE_STAGING;
        Tex2DDesc.BindFlags =        0;
        pd3dDevice->CreateTexture2D( &Tex2DDesc, NULL, &pDensityMapReadFrom );
        DXUT_SetDebugName( pDensityMapReadFrom, "DensityMap Read" );

        // Copy contents of height map into staging version
        pDeviceContext->CopyResource( pDensityMapReadFrom, pDensityMap );
    }
    else
    {
        pDensityMapReadFrom = pDensityMap;
    }

    // Map density map for reading
    D3D11_MAPPED_SUBRESOURCE MappedSubResource;
    pDeviceContext->Map( pDensityMapReadFrom, 0, D3D11_MAP_READ, 0, &MappedSubResource );

    // Loop through all triangles
    for ( DWORD i=0; i<dwNumTriangles; ++i )
    {
        DWORD wIndex0, wIndex1, wIndex2;

        // Retrieve indices of current triangle
        if ( dwIndexSize == sizeof(WORD) )
        {
            wIndex0 = (DWORD)( (WORD *)pIndex )[3*i+0];
            wIndex1 = (DWORD)( (WORD *)pIndex )[3*i+1];
            wIndex2 = (DWORD)( (WORD *)pIndex )[3*i+2];
        }
        else
        {
            wIndex0 = ( (DWORD *)pIndex )[3*i+0];
            wIndex1 = ( (DWORD *)pIndex )[3*i+1];
            wIndex2 = ( (DWORD *)pIndex )[3*i+2];
        }

        // Retrieve texture coordinates of each vertex making up current triangle
        D3DXVECTOR2    vTexcoord0 = *(D3DXVECTOR2 *)( (BYTE *)pTexcoord + wIndex0 * dwVertexStride );
        D3DXVECTOR2    vTexcoord1 = *(D3DXVECTOR2 *)( (BYTE *)pTexcoord + wIndex1 * dwVertexStride );
        D3DXVECTOR2    vTexcoord2 = *(D3DXVECTOR2 *)( (BYTE *)pTexcoord + wIndex2 * dwVertexStride );

        // Adjust texture coordinates with texture repeat
        vTexcoord0 *= fBaseTextureRepeat;
        vTexcoord1 *= fBaseTextureRepeat;
        vTexcoord2 *= fBaseTextureRepeat;

        // Sample density map at vertex location
        float fEdgeDensity0 = GetMaximumDensityAlongEdge( (DWORD *)MappedSubResource.pData, MappedSubResource.RowPitch, 
                                                          Tex2DDesc.Width, Tex2DDesc.Height, vTexcoord0, vTexcoord1 );
        float fEdgeDensity1 = GetMaximumDensityAlongEdge( (DWORD *)MappedSubResource.pData, MappedSubResource.RowPitch, 
                                                          Tex2DDesc.Width, Tex2DDesc.Height, vTexcoord1, vTexcoord2 );
        float fEdgeDensity2 = GetMaximumDensityAlongEdge( (DWORD *)MappedSubResource.pData, MappedSubResource.RowPitch, 
                                                          Tex2DDesc.Width, Tex2DDesc.Height, vTexcoord2, vTexcoord0 );

        // Store edge density in x,y,z and store maximum density in .w
        pEdgeDensityBuffer[i] = D3DXVECTOR4( fEdgeDensity0, fEdgeDensity1, fEdgeDensity2, 
                                             max( max( fEdgeDensity0, fEdgeDensity1 ), fEdgeDensity2 ) );
    }

    // Unmap density map
    pDeviceContext->Unmap( pDensityMapReadFrom, 0 );

    // Release staging density map if we had to create it
    if ( !bCanBeRead )
    {
        SAFE_RELEASE( pDensityMapReadFrom );
    }
    
    // Set source buffer for initialization data
    InitData.pSysMem = (void *)pEdgeDensityBuffer;

    // Create density vertex stream buffer: 1 float per entry
    D3D11_BUFFER_DESC bd;
    bd.Usage =            D3D11_USAGE_DEFAULT;
    bd.ByteWidth =        dwNumTriangles * sizeof( D3DXVECTOR4 );
    bd.BindFlags =        D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags =   0;
    bd.MiscFlags =        0;
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, ppEdgeDensityVertexStream );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"Failed to create vertex buffer.\n" );
    }
    DXUT_SetDebugName( *ppEdgeDensityVertexStream, "Edge Density" );

    // Free host memory buffer
    delete [] pEdgeDensityBuffer;
}


//--------------------------------------------------------------------------------------
// Helper function to create a staging buffer from a buffer resource
//--------------------------------------------------------------------------------------
void CreateStagingBufferFromBuffer( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pContext, 
                                   ID3D11Buffer* pInputBuffer, ID3D11Buffer **ppStagingBuffer )
{
    D3D11_BUFFER_DESC BufferDesc;

    // Get buffer description
    pInputBuffer->GetDesc( &BufferDesc );

    // Modify description to create STAGING buffer
    BufferDesc.BindFlags =          0;
    BufferDesc.CPUAccessFlags =     D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
    BufferDesc.Usage =              D3D11_USAGE_STAGING;

    // Create staging buffer
    pd3dDevice->CreateBuffer( &BufferDesc, NULL, ppStagingBuffer );

    // Copy buffer into STAGING buffer
    pContext->CopyResource( *ppStagingBuffer, pInputBuffer );
}


//--------------------------------------------------------------------------------------
// Helper function to create a shader from the specified filename
// This function is called by the shader-specific versions of this
// function located after the body of this function.
//--------------------------------------------------------------------------------------
HRESULT CreateShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                              LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                              ID3DX11ThreadPump* pPump, ID3D11DeviceChild** ppShader, ID3DBlob** ppShaderBlob, 
                              BOOL bDumpShader)
{
    HRESULT   hr = D3D_OK;
    ID3DBlob* pShaderBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    WCHAR     wcFullPath[256];
    
    DXUTFindDXSDKMediaFileCch( wcFullPath, 256, pSrcFile );
    // Compile shader into binary blob
    hr = D3DX11CompileFromFile( wcFullPath, pDefines, pInclude, pFunctionName, pProfile, 
                                Flags1, Flags2, pPump, &pShaderBlob, &pErrorBlob, NULL );
    if( FAILED( hr ) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    
    // Create shader from binary blob
    if ( ppShader )
    {
        hr = E_FAIL;
        if ( strstr( pProfile, "vs" ) )
        {
            hr = pd3dDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11VertexShader**)ppShader );
        }
        else if ( strstr( pProfile, "hs" ) )
        {
            hr = pd3dDevice->CreateHullShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11HullShader**)ppShader ); 
        }
        else if ( strstr( pProfile, "ds" ) )
        {
            hr = pd3dDevice->CreateDomainShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11DomainShader**)ppShader );
        }
        else if ( strstr( pProfile, "gs" ) )
        {
            hr = pd3dDevice->CreateGeometryShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11GeometryShader**)ppShader ); 
        }
        else if ( strstr( pProfile, "ps" ) )
        {
            hr = pd3dDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11PixelShader**)ppShader ); 
        }
        else if ( strstr( pProfile, "cs" ) )
        {
            hr = pd3dDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11ComputeShader**)ppShader );
        }
        if ( FAILED( hr ) )
        {
            OutputDebugString( L"Shader creation failed\n" );
            SAFE_RELEASE( pErrorBlob );
            SAFE_RELEASE( pShaderBlob );
            return hr;
        }
    }

    DXUT_SetDebugName( *ppShader, pFunctionName );

    // If blob was requested then pass it otherwise release it
    if ( ppShaderBlob )
    {
        *ppShaderBlob = pShaderBlob;
    }
    else
    {
        pShaderBlob->Release();
    }

    // Return error code
    return hr;
}


//--------------------------------------------------------------------------------------
// Create a vertex shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateVertexShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11VertexShader** ppShader, ID3DBlob** ppShaderBlob, 
                                    BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a hull shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateHullShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                  LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                  ID3DX11ThreadPump* pPump, ID3D11HullShader** ppShader, ID3DBlob** ppShaderBlob, 
                                  BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}
//--------------------------------------------------------------------------------------
// Create a domain shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateDomainShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11DomainShader** ppShader, ID3DBlob** ppShaderBlob, 
                                    BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a geometry shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateGeometryShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                      LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                      ID3DX11ThreadPump* pPump, ID3D11GeometryShader** ppShader, ID3DBlob** ppShaderBlob, 
                                      BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a pixel shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreatePixelShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                   LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                   ID3DX11ThreadPump* pPump, ID3D11PixelShader** ppShader, ID3DBlob** ppShaderBlob, 
                                   BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a compute shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateComputeShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                     LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                     ID3DX11ThreadPump* pPump, ID3D11ComputeShader** ppShader, ID3DBlob** ppShaderBlob, 
                                     BOOL bDumpShader)
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Helper function to normalize a plane
//--------------------------------------------------------------------------------------
void NormalizePlane( D3DXVECTOR4* pPlaneEquation )
{
    float mag;
    
    mag = sqrt( pPlaneEquation->x * pPlaneEquation->x + 
                pPlaneEquation->y * pPlaneEquation->y + 
                pPlaneEquation->z * pPlaneEquation->z );
    
    pPlaneEquation->x = pPlaneEquation->x / mag;
    pPlaneEquation->y = pPlaneEquation->y / mag;
    pPlaneEquation->z = pPlaneEquation->z / mag;
    pPlaneEquation->w = pPlaneEquation->w / mag;
}


//--------------------------------------------------------------------------------------
// Extract all 6 plane equations from frustum denoted by supplied matrix
//--------------------------------------------------------------------------------------
void ExtractPlanesFromFrustum( D3DXVECTOR4* pPlaneEquation, const D3DXMATRIX* pMatrix, bool bNormalize )
{
    // Left clipping plane
    pPlaneEquation[0].x = pMatrix->_14 + pMatrix->_11;
    pPlaneEquation[0].y = pMatrix->_24 + pMatrix->_21;
    pPlaneEquation[0].z = pMatrix->_34 + pMatrix->_31;
    pPlaneEquation[0].w = pMatrix->_44 + pMatrix->_41;
    
    // Right clipping plane
    pPlaneEquation[1].x = pMatrix->_14 - pMatrix->_11;
    pPlaneEquation[1].y = pMatrix->_24 - pMatrix->_21;
    pPlaneEquation[1].z = pMatrix->_34 - pMatrix->_31;
    pPlaneEquation[1].w = pMatrix->_44 - pMatrix->_41;
    
    // Top clipping plane
    pPlaneEquation[2].x = pMatrix->_14 - pMatrix->_12;
    pPlaneEquation[2].y = pMatrix->_24 - pMatrix->_22;
    pPlaneEquation[2].z = pMatrix->_34 - pMatrix->_32;
    pPlaneEquation[2].w = pMatrix->_44 - pMatrix->_42;
    
    // Bottom clipping plane
    pPlaneEquation[3].x = pMatrix->_14 + pMatrix->_12;
    pPlaneEquation[3].y = pMatrix->_24 + pMatrix->_22;
    pPlaneEquation[3].z = pMatrix->_34 + pMatrix->_32;
    pPlaneEquation[3].w = pMatrix->_44 + pMatrix->_42;
    
    // Near clipping plane
    pPlaneEquation[4].x = pMatrix->_13;
    pPlaneEquation[4].y = pMatrix->_23;
    pPlaneEquation[4].z = pMatrix->_33;
    pPlaneEquation[4].w = pMatrix->_43;
    
    // Far clipping plane
    pPlaneEquation[5].x = pMatrix->_14 - pMatrix->_13;
    pPlaneEquation[5].y = pMatrix->_24 - pMatrix->_23;
    pPlaneEquation[5].z = pMatrix->_34 - pMatrix->_33;
    pPlaneEquation[5].w = pMatrix->_44 - pMatrix->_43;
    
    // Normalize the plane equations, if requested
    if ( bNormalize )
    {
        NormalizePlane( &pPlaneEquation[0] );
        NormalizePlane( &pPlaneEquation[1] );
        NormalizePlane( &pPlaneEquation[2] );
        NormalizePlane( &pPlaneEquation[3] );
        NormalizePlane( &pPlaneEquation[4] );
        NormalizePlane( &pPlaneEquation[5] );
    }
}

