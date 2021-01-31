//======================================================================
//
//      HIGH DYNAMIC RANGE RENDERING DEMONSTRATION
//      Written by Jack Hoxley, October 2005
//
//======================================================================

// Operating System Includes
#include "DXUT.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
// Project Includes
#include "HDRScene.h"
#include "Enumeration.h"

// As defined in the appropriate header, this translation unit is
// wrapped up in it's own C++ namespace:
namespace HDRScene
{

//--------------------------------------------------------------------------------------
// Data Structure Definitions
//--------------------------------------------------------------------------------------
struct LitVertex
{
    D3DXVECTOR3 p;
    DWORD c;
};

static const DWORD FVF_LITVERTEX = D3DFVF_XYZ | D3DFVF_DIFFUSE;

struct TLVertex
{
    D3DXVECTOR4 p;
    D3DXVECTOR2 t;
};

static const DWORD FVF_TLVERTEX = D3DFVF_XYZRHW | D3DFVF_TEX1;



//--------------------------------------------------------------------------------------
// Namespace-level variables
//--------------------------------------------------------------------------------------
LPD3DXMESH g_pCubeMesh = NULL;             // Mesh representing the HDR source in the middle of the scene
LPDIRECT3DPIXELSHADER9 g_pCubePS = NULL;             // The pixel shader for the cube
LPD3DXCONSTANTTABLE g_pCubePSConsts = NULL;             // Interface for setting parameters/constants for the above PS
LPDIRECT3DVERTEXSHADER9 g_pCubeVS = NULL;             // The vertex shader for the cube
LPDIRECT3DVERTEXDECLARATION9 g_pCubeVSDecl = NULL;             // The mapping from VB to VS
LPD3DXCONSTANTTABLE g_pCubeVSConsts = NULL;             // Interface for setting params for the cube rendering
D3DXMATRIXA16 g_mCubeMatrix;                              // The computed world*view*proj transform for the inner cube

LPDIRECT3DTEXTURE9 g_pTexScene = NULL;             // The main, floating point, render target
D3DFORMAT g_fmtHDR = D3DFMT_UNKNOWN;   // Enumerated and either set to 128 or 64 bit

LPD3DXMESH g_pOcclusionMesh = NULL;             // The occlusion mesh surrounding the HDR cube
LPDIRECT3DVERTEXDECLARATION9 g_pOcclusionVSDecl = NULL;             // The mapping for the ID3DXMesh
LPDIRECT3DVERTEXSHADER9 g_pOcclusionVS = NULL;             // The shader for drawing the occlusion mesh
LPD3DXCONSTANTTABLE g_pOcclusionVSConsts = NULL;             // Entry point for configuring above shader
D3DXMATRIXA16 g_mOcclusionMatrix;                         // The world*view*proj transform for transforming the POSITIONS
D3DXMATRIXA16 g_mOcclusionNormals;                        // The transpose(inverse(world)) matrix for transforming NORMALS



//--------------------------------------------------------------------------------------
// Function Prototypes
//--------------------------------------------------------------------------------------
HRESULT LoadMesh( WCHAR* strFileName, LPD3DXMESH* ppMesh );



//--------------------------------------------------------------------------------------
//  CreateResources( )
//
//      DESC:
//          This function creates all the necessary resources to render the HDR scene
//          to a render target for later use. When this function completes successfully
//          rendering can commence. A call to 'DestroyResources()' should be made when
//          the application closes.
//
//      PARAMS:
//          pDevice      : The current device that resources should be created with/from
//          pDisplayDesc : Describes the back-buffer currently in use, can be useful when
//                         creating GUI based resources.
//
//      NOTES:
//          n/a
//--------------------------------------------------------------------------------------
HRESULT CreateResources( IDirect3DDevice9* pDevice, const D3DSURFACE_DESC* pDisplayDesc )
{
    //[ 0 ] DECLARATIONS
    //------------------
    HRESULT hr = S_OK;
    LPD3DXBUFFER pCode;      // Container for the compiled HLSL code


    //[ 1 ] DETERMINE FP TEXTURE SUPPORT
    //----------------------------------
    V( HDREnumeration::FindBestHDRFormat( &HDRScene::g_fmtHDR ) );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"HDRScene::CreateResources() - Current hardware does not support HDR rendering!\n" );
        return hr;
    }


    //[ 2 ] CREATE HDR RENDER TARGET
    //------------------------------
    V( pDevice->CreateTexture(
       pDisplayDesc->Width, pDisplayDesc->Height,
       1, D3DUSAGE_RENDERTARGET, g_fmtHDR,
       D3DPOOL_DEFAULT, &HDRScene::g_pTexScene, NULL
       ) );
    if( FAILED( hr ) )
    {

        // We couldn't create the texture - lots of possible reasons for this. Best
        // check the D3D debug output for more details.
        OutputDebugString(
            L"HDRScene::CreateResources() - Could not create floating point render target. Examine D3D Debug Output for details.\n" );
        return hr;

    }


    //[ 3 ] CREATE HDR CUBE'S GEOMETRY
    //--------------------------------
    V( LoadMesh( L"misc\\Cube.x", &HDRScene::g_pCubeMesh ) );
    if( FAILED( hr ) )
    {

        // Couldn't load the mesh, could be a file system error...
        OutputDebugString( L"HDRScene::CreateResources() - Could not load 'Cube.x'.\n" );
        return hr;

    }



    //[ 4 ] CREATE HDR CUBE'S PIXEL SHADER
    //------------------------------------
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Shader Code\\HDRSource.psh" ) );
    V( D3DXCompileShaderFromFile(
       str,
       NULL, NULL,
       "main",                 // Entry Point found in 'HDRSource.psh'
       "ps_2_0",               // Profile to target
       0,
       &pCode,
       NULL,
       &HDRScene::g_pCubePSConsts
       ) );
    if( FAILED( hr ) )
    {

        // Couldn't compile the shader, use the 'compile_shaders.bat' script
        // in the 'Shader Code' folder to get a proper compile breakdown.
        OutputDebugString( L"HDRScene::CreateResources() - Compiling of 'HDRSource.psh' failed!\n" );
        return hr;

    }


    V( pDevice->CreatePixelShader( reinterpret_cast< DWORD* >( pCode->GetBufferPointer() ), &HDRScene::g_pCubePS ) );
    if( FAILED( hr ) )
    {

        // Couldn't turn the compiled shader into an actual, usable, pixel shader!
        OutputDebugString(
            L"HDRScene::CreateResources() : Couldn't create a pixel shader object from 'HDRSource.psh'.\n" );
        pCode->Release();
        return hr;

    }

    pCode->Release();

    // [ 5 ] CREATE THE CUBE'S VERTEX DECL
    //------------------------------------
    D3DVERTEXELEMENT9 cubeVertElems[MAX_FVF_DECL_SIZE];
    HDRScene::g_pCubeMesh->GetDeclaration( cubeVertElems );

    V( pDevice->CreateVertexDeclaration( cubeVertElems, &HDRScene::g_pCubeVSDecl ) );
    if( FAILED( hr ) )
    {

        // Couldn't create the declaration for the loaded mesh..
        OutputDebugString(
            L"HDRScene::CreateResources() - Couldn't create a vertex declaration for the HDR-Cube mesh.\n" );
        return hr;

    }



    // [ 6 ] CREATE THE CUBE'S VERTEX SHADER
    //--------------------------------------
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Shader Code\\HDRSource.vsh" ) );
    V( D3DXCompileShaderFromFile(
       str,
       NULL, NULL,
       "main",
       "vs_2_0",
       0,
       &pCode,
       NULL,
       &g_pCubeVSConsts
       ) );
    if( FAILED( hr ) )
    {

        // Couldn't compile the shader, use the 'compile_shaders.bat' script
        // in the 'Shader Code' folder to get a proper compile breakdown.
        OutputDebugString( L"HDRScene::CreateResources() - Compilation of 'HDRSource.vsh' Failed!\n" );
        return hr;

    }

    V( pDevice->CreateVertexShader( reinterpret_cast< DWORD* >( pCode->GetBufferPointer() ), &HDRScene::g_pCubeVS ) );
    if( FAILED( hr ) )
    {

        // Couldn't turn the compiled shader into an actual, usable, vertex shader!
        OutputDebugString(
            L"HDRScene::CreateResources() - Could not create a VS object from the compiled 'HDRSource.vsh' code.\n" );
        pCode->Release();
        return hr;

    }

    pCode->Release();

    //[ 5 ] LOAD THE OCCLUSION MESH
    //-----------------------------
    V( LoadMesh( L"misc\\OcclusionBox.x", &HDRScene::g_pOcclusionMesh ) );
    if( FAILED( hr ) )
    {

        // Couldn't load the mesh, could be a file system error...
        OutputDebugString( L"HDRScene::CreateResources() - Could not load 'OcclusionBox.x'.\n" );
        return hr;

    }



    //[ 6 ] CREATE THE MESH VERTEX DECLARATION
    //----------------------------------------
    D3DVERTEXELEMENT9 vertElems[MAX_FVF_DECL_SIZE];
    HDRScene::g_pOcclusionMesh->GetDeclaration( vertElems );

    V( pDevice->CreateVertexDeclaration( vertElems, &HDRScene::g_pOcclusionVSDecl ) );
    if( FAILED( hr ) )
    {

        // Couldn't create the declaration for the loaded mesh..
        OutputDebugString(
            L"HDRScene::CreateResources() - Couldn't create a vertex declaration for the occlusion mesh.\n" );
        return hr;

    }



    //[ 7 ] CREATE THE OCCLUSION VERTEX SHADER
    //----------------------------------------
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Shader Code\\OcclusionMesh.vsh" ) );
    V( D3DXCompileShaderFromFile(
       str,
       NULL, NULL,
       "main",
       "vs_2_0",
       0,
       &pCode,
       NULL,
       &HDRScene::g_pOcclusionVSConsts
       ) );
    if( FAILED( hr ) )
    {

        // Couldn't compile the shader, use the 'compile_shaders.bat' script
        // in the 'Shader Code' folder to get a proper compile breakdown.
        OutputDebugString( L"HDRScene::CreateResources() - Compilation of 'OcclusionMesh.vsh' Failed!\n" );
        return hr;

    }

    V( pDevice->CreateVertexShader( reinterpret_cast< DWORD* >( pCode->GetBufferPointer() ),
                                    &HDRScene::g_pOcclusionVS ) );
    if( FAILED( hr ) )
    {

        // Couldn't turn the compiled shader into an actual, usable, vertex shader!
        OutputDebugString(
            L"HDRScene::CreateResources() - Could not create a VS object from the compiled 'OcclusionMesh.vsh' code.\n"
            );
        pCode->Release();
        return hr;

    }

    pCode->Release();



    //[ 8 ] RETURN SUCCESS IF WE GOT THIS FAR
    //---------------------------------------
    return hr;

}



//--------------------------------------------------------------------------------------
//  DestroyResources( )
//
//      DESC:
//          Makes sure that the resources acquired in CreateResources() are cleanly
//          destroyed to avoid any errors and/or memory leaks.
//
//--------------------------------------------------------------------------------------
HRESULT DestroyResources()
{

    SAFE_RELEASE( HDRScene::g_pCubeMesh );
    SAFE_RELEASE( HDRScene::g_pCubePS );
    SAFE_RELEASE( HDRScene::g_pCubePSConsts );
    SAFE_RELEASE( HDRScene::g_pTexScene );
    SAFE_RELEASE( HDRScene::g_pCubeVS );
    SAFE_RELEASE( HDRScene::g_pCubeVSConsts );
    SAFE_RELEASE( HDRScene::g_pCubeVSDecl );
    SAFE_RELEASE( HDRScene::g_pOcclusionMesh );
    SAFE_RELEASE( HDRScene::g_pOcclusionVSDecl );
    SAFE_RELEASE( HDRScene::g_pOcclusionVS );
    SAFE_RELEASE( HDRScene::g_pOcclusionVSConsts );

    return S_OK;

}



//--------------------------------------------------------------------------------------
//  CalculateResourceUsage( )
//
//      DESC:
//          Based on the known resources this function attempts to make an accurate
//          measurement of how much VRAM is being used by this part of the application.
//
//      NOTES:
//          Whilst the return value should be pretty accurate, it shouldn't be relied
//          on due to the way drivers/hardware can allocate memory.
//
//          Only the first level of the render target is checked as there should, by
//          definition, be no mip levels.
//
//--------------------------------------------------------------------------------------
DWORD CalculateResourceUsage()
{

    // [ 0 ] DECLARATIONS
    //-------------------
    DWORD usage = 0;

    // [ 1 ] RENDER TARGET SIZE
    //-------------------------
    D3DSURFACE_DESC texDesc;
    HDRScene::g_pTexScene->GetLevelDesc( 0, &texDesc );
    usage += ( ( texDesc.Width * texDesc.Height ) * ( HDRScene::g_fmtHDR == D3DFMT_A16B16G16R16F ? 8 : 16 ) );

    // [ 2 ] OCCLUSION MESH SIZE
    //--------------------------
    usage += ( HDRScene::g_pOcclusionMesh->GetNumBytesPerVertex() * HDRScene::g_pOcclusionMesh->GetNumVertices() );
    int index_size = ( ( HDRScene::g_pOcclusionMesh->GetOptions() & D3DXMESH_32BIT ) != 0 ) ? 4 : 2;
    usage += ( index_size * 3 * HDRScene::g_pOcclusionMesh->GetNumFaces() );

    return usage;

}



//--------------------------------------------------------------------------------------
//  RenderScene( )
//
//      DESC:
//          This is the core function for this unit - when it succesfully completes the
//          render target (obtainable via GetOutputTexture) will be a completed scene
//          that, crucially, contains values outside the LDR (0..1) range ready to be
//          fed into the various stages of the HDR post-processing pipeline.
//
//      PARAMS:
//          pDevice     : The device that is currently being used for rendering
//
//      NOTES:
//          For future modifications, this is the entry point that should be used if
//          you require a different image/scene to be displayed on the screen.
//
//          This function assumes that the device is already in a ready-to-render
//          state (e.g. BeginScene() has been called).
//
//--------------------------------------------------------------------------------------
HRESULT RenderScene( IDirect3DDevice9* pDevice )
{


    // [ 0 ] CONFIGURE GEOMETRY INPUTS
    //--------------------------------
    pDevice->SetVertexShader( HDRScene::g_pCubeVS );
    pDevice->SetVertexDeclaration( HDRScene::g_pCubeVSDecl );
    HDRScene::g_pCubeVSConsts->SetMatrix( pDevice, "matWorldViewProj", &HDRScene::g_mCubeMatrix );



    // [ 1 ] PIXEL SHADER ( + PARAMS )
    //--------------------------------
    pDevice->SetPixelShader( HDRScene::g_pCubePS );
    HDRScene::g_pCubePSConsts->SetFloat( pDevice, "HDRScalar", 5.0f );
    pDevice->SetTexture( 0, NULL );



    // [ 2 ] GET PREVIOUS RENDER TARGET
    //---------------------------------
    LPDIRECT3DSURFACE9 pPrevSurf = NULL;
    if( FAILED( pDevice->GetRenderTarget( 0, &pPrevSurf ) ) )
    {

        // Couldn't retrieve the current render target (for restoration later on)
        OutputDebugString( L"HDRScene::RenderScene() - Could not retrieve a reference to the previous render target.\n"
                           );
        return E_FAIL;

    }



    // [ 3 ] SET NEW RENDER TARGET
    //----------------------------
    LPDIRECT3DSURFACE9 pRTSurf = NULL;
    if( FAILED( HDRScene::g_pTexScene->GetSurfaceLevel( 0, &pRTSurf ) ) )
    {

        // Bad news! couldn't get a reference to the HDR render target. Most
        // Likely due to a failed/corrupt resource creation stage.
        OutputDebugString( L"HDRScene::RenderScene() - Could not get the top level surface for the HDR render target\n"
                           );
        return E_FAIL;

    }

    if( FAILED( pDevice->SetRenderTarget( 0, pRTSurf ) ) )
    {

        // For whatever reason we can't set the HDR texture as the
        // the render target...
        OutputDebugString( L"HDRScene::RenderScene() - Could not set the HDR texture as a new render target.\n" );
        return E_FAIL;

    }

    // It is worth noting that the colour used to clear the render target will
    // be considered for the luminance measurement stage.
    pDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 64, 64, 192 ), 1.0f, 0 );



    // [ 4 ] RENDER THE HDR CUBE
    //--------------------------
    HDRScene::g_pCubeMesh->DrawSubset( 0 );



    // [ 5 ] DRAW THE OCCLUSION CUBE
    //------------------------------
    pDevice->SetPixelShader( NULL );
    pDevice->SetVertexDeclaration( HDRScene::g_pOcclusionVSDecl );
    pDevice->SetVertexShader( HDRScene::g_pOcclusionVS );
    HDRScene::g_pOcclusionVSConsts->SetMatrix( pDevice, "matWorldViewProj", &HDRScene::g_mOcclusionMatrix );
    HDRScene::g_pOcclusionVSConsts->SetMatrix( pDevice, "matInvTPoseWorld", &HDRScene::g_mOcclusionNormals );

    // Due to the way that the mesh was authored, there is
    // only (and always) going to be 1 subset/group to render.
    HDRScene::g_pOcclusionMesh->DrawSubset( 0 );


    // [ 6 ] RESTORE PREVIOUS RENDER TARGET
    //-------------------------------------
    pDevice->SetRenderTarget( 0, pPrevSurf );



    // [ 7 ] RELEASE TEMPORARY REFERENCES
    //-----------------------------------
    SAFE_RELEASE( pRTSurf );
    SAFE_RELEASE( pPrevSurf );



    return S_OK;

}



//--------------------------------------------------------------------------------------
//  UpdateScene( )
//
//      DESC:
//          An entry point for updating various parameters and internal data on a
//          per-frame basis.
//
//      PARAMS:
//          pDevice     : The currently active device
//          fTime       : The number of milliseconds elapsed since the start of execution
//          pCamera     : The arcball based camera that the end-user controls
//
//      NOTES:
//          n/a
//
//--------------------------------------------------------------------------------------
HRESULT UpdateScene( IDirect3DDevice9* pDevice, float fTime, CModelViewerCamera* pCamera )
{

    // The HDR cube in the middle of the scene never changes position in world
    // space, but must respond to view changes.
    HDRScene::g_mCubeMatrix = ( *pCamera->GetViewMatrix() ) * ( *pCamera->GetProjMatrix() );

    D3DXMATRIXA16 matWorld;
    D3DXMATRIXA16 matTemp;

    // The occlusion cube must be slightly larger than the inner HDR cube, so
    // a scaling constant is applied to the world matrix.
    D3DXMatrixIdentity( &matTemp );
    D3DXMatrixScaling( &matTemp, 2.5f, 2.5f, 2.5f );
    D3DXMatrixMultiply( &matWorld, &matTemp, pCamera->GetWorldMatrix() ); //&matWorld );

    // The occlusion cube contains lighting normals, so for the shader to operate
    // on them correctly, the inverse transpose of the world matrix is needed.
    D3DXMatrixIdentity( &matTemp );
    D3DXMatrixInverse( &matTemp, NULL, &matWorld );
    D3DXMatrixTranspose( &HDRScene::g_mOcclusionNormals, &matTemp );

    HDRScene::g_mOcclusionMatrix = matWorld * ( *pCamera->GetViewMatrix() ) * ( *pCamera->GetProjMatrix() );

    return S_OK;

}



//--------------------------------------------------------------------------------------
//  GetOutputTexture( )
//
//      DESC:
//          The results of this modules rendering are used as inputs to several
//          other parts of the rendering pipeline. As such it is necessary to obtain
//          a reference to the internally managed texture.
//
//      PARAMS:
//          pTexture    : Should be NULL on entry, will be a valid reference on exit
//
//      NOTES:
//          The code that requests the reference is responsible for releasing their
//          copy of the texture as soon as they are finished using it.
//
//--------------------------------------------------------------------------------------
HRESULT GetOutputTexture( IDirect3DTexture9** pTexture )
{

    // [ 0 ] ERASE ANY DATA IN THE INPUT
    //----------------------------------
    SAFE_RELEASE( *pTexture );

    // [ 1 ] COPY THE PRIVATE REFERENCE
    //---------------------------------
    *pTexture = HDRScene::g_pTexScene;

    // [ 2 ] INCREMENT THE REFERENCE COUNT..
    //--------------------------------------
    ( *pTexture )->AddRef();

    return S_OK;

}



//--------------------------------------------------------------------------------------
//  DrawToScreen( )
//
//      DESC:
//          Part of the GUI in this application displays the "raw" HDR data as part
//          of the process. This function places the texture, created by this
//          module, in the correct place on the screen.
//
//      PARAMS:
//          pDevice     : The device to be drawn to.
//          pFont       : The font to use when annotating the display
//          pTextSprite : Used with the font for more efficient rendering
//          pArrowTex   : Stores the 4 (up/down/left/right) icons used in the GUI
//
//      NOTES:
//          n/a
//
//--------------------------------------------------------------------------------------
HRESULT DrawToScreen( IDirect3DDevice9* pDevice, ID3DXFont* pFont, ID3DXSprite* pTextSprite,
                      IDirect3DTexture9* pArrowTex )
{

    // [ 0 ] GATHER NECESSARY INFORMATION
    //-----------------------------------
    LPDIRECT3DSURFACE9 pSurf = NULL;
    D3DSURFACE_DESC d;
    if( FAILED( pDevice->GetRenderTarget( 0, &pSurf ) ) )
    {

        // Couldn't get the current render target!
        OutputDebugString( L"HDRScene::DrawToScreen() - Could not get current render target to extract dimensions.\n"
                           );
        return E_FAIL;

    }

    pSurf->GetDesc( &d );
    SAFE_RELEASE( pSurf );

    // Cache the dimensions as floats for later use
    float fCellWidth = ( static_cast< float >( d.Width ) - 48.0f ) / 4.0f;
    float fCellHeight = ( static_cast< float >( d.Height ) - 36.0f ) / 4.0f;

    CDXUTTextHelper txtHelper( pFont, pTextSprite, 12 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.5f, 0.0f, 1.0f ) );

    // [ 1 ] CREATE TILE GEOMETRY
    //---------------------------
    HDRScene::TLVertex v[4];

    v[0].p = D3DXVECTOR4( 0.0f, fCellHeight + 16.0f, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( fCellWidth, fCellHeight + 16.0f, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( 0.0f, ( 2.0f * fCellHeight ) + 16.0f, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( fCellWidth, ( 2.0f * fCellHeight ) + 16.0f, 0.0f, 1.0f );

    v[0].t = D3DXVECTOR2( 0.0f, 0.0f );
    v[1].t = D3DXVECTOR2( 1.0f, 0.0f );
    v[2].t = D3DXVECTOR2( 0.0f, 1.0f );
    v[3].t = D3DXVECTOR2( 1.0f, 1.0f );

    // [ 2 ] DISPLAY TILE ON SCREEN
    //-----------------------------
    pDevice->SetVertexShader( NULL );
    pDevice->SetFVF( HDRScene::FVF_TLVERTEX );
    pDevice->SetTexture( 0, HDRScene::g_pTexScene );
    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( HDRScene::TLVertex ) );

    // [ 3 ] RENDER CONNECTING ARROWS
    //-------------------------------
    pDevice->SetTexture( 0, pArrowTex );

    v[0].p = D3DXVECTOR4( ( fCellWidth / 2.0f ) - 8.0f, fCellHeight, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( ( fCellWidth / 2.0f ) + 8.0f, fCellHeight, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( ( fCellWidth / 2.0f ) - 8.0f, fCellHeight + 16.0f, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( ( fCellWidth / 2.0f ) + 8.0f, fCellHeight + 16.0f, 0.0f, 1.0f );

    v[0].t = D3DXVECTOR2( 0.0f, 0.0f );
    v[1].t = D3DXVECTOR2( 0.25f, 0.0f );
    v[2].t = D3DXVECTOR2( 0.0f, 1.0f );
    v[3].t = D3DXVECTOR2( 0.25f, 1.0f );

    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( HDRScene::TLVertex ) );

    v[0].p = D3DXVECTOR4( fCellWidth, fCellHeight + 8.0f + ( fCellHeight / 2.0f ), 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( fCellWidth + 16.0f, fCellHeight + 8.0f + ( fCellHeight / 2.0f ), 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( fCellWidth, fCellHeight + 24.0f + ( fCellHeight / 2.0f ), 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( fCellWidth + 16.0f, fCellHeight + 24.0f + ( fCellHeight / 2.0f ), 0.0f, 1.0f );

    v[0].t = D3DXVECTOR2( 0.25f, 0.0f );
    v[1].t = D3DXVECTOR2( 0.50f, 0.0f );
    v[2].t = D3DXVECTOR2( 0.25f, 1.0f );
    v[3].t = D3DXVECTOR2( 0.50f, 1.0f );

    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( HDRScene::TLVertex ) );

    float fLumCellSize = ( ( static_cast< float >( d.Height ) - ( ( 2.0f * fCellHeight ) + 32.0f ) ) - 32.0f ) / 3.0f;
    float fLumStartX = ( fCellWidth + 16.0f ) - ( ( 2.0f * fLumCellSize ) + 32.0f );

    v[0].p = D3DXVECTOR4( fLumStartX + ( fLumCellSize / 2.0f ) - 8.0f, ( 2.0f * fCellHeight ) + 16.0f, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( fLumStartX + ( fLumCellSize / 2.0f ) + 8.0f, ( 2.0f * fCellHeight ) + 16.0f, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( fLumStartX + ( fLumCellSize / 2.0f ) - 8.0f, ( 2.0f * fCellHeight ) + 32.0f, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( fLumStartX + ( fLumCellSize / 2.0f ) + 8.0f, ( 2.0f * fCellHeight ) + 32.0f, 0.0f, 1.0f );

    v[0].t = D3DXVECTOR2( 0.50f, 0.0f );
    v[1].t = D3DXVECTOR2( 0.75f, 0.0f );
    v[2].t = D3DXVECTOR2( 0.50f, 1.0f );
    v[3].t = D3DXVECTOR2( 0.75f, 1.0f );

    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( HDRScene::TLVertex ) );

    // [ 4 ] ANNOTATE CELL
    //--------------------
    txtHelper.Begin();
    {
        txtHelper.SetInsertionPos( 5, static_cast< int >( ( 2.0f * fCellHeight ) + 16.0f - 25.0f ) );
        txtHelper.DrawTextLine( L"Source HDR Frame" );

        D3DSURFACE_DESC d2;
        HDRScene::g_pTexScene->GetLevelDesc( 0, &d2 );

        WCHAR str[100];
        swprintf_s( str, 100, L"%dx%d", d2.Width, d2.Height );
        txtHelper.DrawTextLine( str );
    }
    txtHelper.End();

    return S_OK;

}



//--------------------------------------------------------------------------------------
//  LoadMesh( )
//
//      DESC:
//          A utility method borrowed from the DXSDK samples. Loads a .X mesh into
//          an ID3DXMesh object for rendering.
//
//--------------------------------------------------------------------------------------
HRESULT LoadMesh( WCHAR* strFileName, LPD3DXMESH* ppMesh )
{

    LPD3DXMESH pMesh = NULL;
    WCHAR str[MAX_PATH];
    HRESULT hr = S_OK;

    if( ppMesh == NULL )
        return E_INVALIDARG;

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFileName ) );
    hr = D3DXLoadMeshFromX( str, D3DXMESH_MANAGED,
                            DXUTGetD3D9Device(), NULL, NULL, NULL, NULL, &pMesh );
    if( FAILED( hr ) || ( pMesh == NULL ) )
        return hr;

    DWORD* rgdwAdjacency = NULL;

    // Make sure there are normals which are required for lighting
    if( !( pMesh->GetFVF() & D3DFVF_NORMAL ) )
    {
        LPD3DXMESH pTempMesh;
        hr = pMesh->CloneMeshFVF( pMesh->GetOptions(),
                                  pMesh->GetFVF() | D3DFVF_NORMAL,
                                  DXUTGetD3D9Device(), &pTempMesh );
        if( FAILED( hr ) )
            return hr;

        D3DXComputeNormals( pTempMesh, NULL );

        SAFE_RELEASE( pMesh );
        pMesh = pTempMesh;
    }

    // Optimize the mesh to make it fast for the user's graphics card
    rgdwAdjacency = new DWORD[pMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
        return E_OUTOFMEMORY;
    V( pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE, rgdwAdjacency, NULL, NULL, NULL );
    delete []rgdwAdjacency;

    *ppMesh = pMesh;

    return hr;

}

}
;
