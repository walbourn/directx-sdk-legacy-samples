//======================================================================
//
//      HIGH DYNAMIC RANGE RENDERING DEMONSTRATION
//      Written by Jack Hoxley, October 2005
//
//======================================================================

#include "DXUT.h"
#include "SDKmisc.h"
#include "HDRScene.h"
#include "PostProcess.h"
#include "Enumeration.h"

namespace PostProcess
{

//--------------------------------------------------------------------------------------
// Data Structure Definitions
//--------------------------------------------------------------------------------------
struct TLVertex
{
    D3DXVECTOR4 p;
    D3DXVECTOR2 t;
};

static const DWORD FVF_TLVERTEX = D3DFVF_XYZRHW | D3DFVF_TEX1;



//--------------------------------------------------------------------------------------
// Namespace-level variables
//--------------------------------------------------------------------------------------
LPDIRECT3DTEXTURE9 g_pBrightPassTex = NULL;             // The results of performing a bright pass on the original HDR imagery
LPDIRECT3DTEXTURE9 g_pDownSampledTex = NULL;             // The scaled down version of the bright pass results
LPDIRECT3DTEXTURE9 g_pBloomHorizontal = NULL;             // The horizontally blurred version of the downsampled texture
LPDIRECT3DTEXTURE9 g_pBloomVertical = NULL;             // The vertically blurred version of the horizontal blur

D3DFORMAT g_fmtHDR = D3DFMT_UNKNOWN;   // What HDR (128 or 64) format is being used

LPDIRECT3DPIXELSHADER9 g_pBrightPassPS = NULL;             // Represents the bright pass processing
LPD3DXCONSTANTTABLE g_pBrightPassConstants = NULL;

LPDIRECT3DPIXELSHADER9 g_pDownSamplePS = NULL;             // Represents the downsampling processing
LPD3DXCONSTANTTABLE g_pDownSampleConstants = NULL;

LPDIRECT3DPIXELSHADER9 g_pHBloomPS = NULL;             // Performs the first stage of the bloom rendering
LPD3DXCONSTANTTABLE g_pHBloomConstants = NULL;

LPDIRECT3DPIXELSHADER9 g_pVBloomPS = NULL;             // Performs the second stage of the bloom rendering
LPD3DXCONSTANTTABLE g_pVBloomConstants = NULL;

float g_BrightThreshold = 0.8f;             // A configurable parameter into the pixel shader
float g_GaussMultiplier = 0.4f;             // Default multiplier
float g_GaussMean = 0.0f;             // Default mean for gaussian distribution
float g_GaussStdDev = 0.8f;             // Default standard deviation for gaussian distribution


//--------------------------------------------------------------------------------------
// Function Prototypes
//--------------------------------------------------------------------------------------
HRESULT RenderToTexture( IDirect3DDevice9* pDev );
float ComputeGaussianValue( float x, float mean, float std_deviation );



//--------------------------------------------------------------------------------------
//  CreateResources( )
//
//      DESC:
//          This function creates all the necessary resources to produce the post-
//          processing effect for the HDR input. When this function completes successfully
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

    // [ 0 ] GATHER NECESSARY INFORMATION
    //-----------------------------------
    HRESULT hr = S_OK;
    LPD3DXBUFFER pCode = NULL;

    V( HDREnumeration::FindBestHDRFormat( &PostProcess::g_fmtHDR ) );
    if( FAILED( hr ) )
    {
        // High Dynamic Range Rendering is not supported on this device!
        OutputDebugString( L"PostProcess::CreateResources() - Current hardware does not allow HDR rendering!\n" );
        return hr;
    }



    // [ 1 ] CREATE BRIGHT PASS TEXTURE
    //---------------------------------
    // Bright pass texture is 1/2 the size of the original HDR render target.
    // Part of the pixel shader performs a 2x2 downsampling. The downsampling
    // is intended to reduce the number of pixels that need to be worked on - 
    // in general, HDR pipelines tend to be fill-rate heavy.
    V( pDevice->CreateTexture(
       pDisplayDesc->Width / 2, pDisplayDesc->Height / 2,
       1, D3DUSAGE_RENDERTARGET, PostProcess::g_fmtHDR,
       D3DPOOL_DEFAULT, &PostProcess::g_pBrightPassTex, NULL
       ) );
    if( FAILED( hr ) )
    {

        // We couldn't create the texture - lots of possible reasons for this...
        OutputDebugString(
            L"PostProcess::CreateResources() - Could not create bright-pass render target. Examine D3D Debug Output for details.\n" );
        return hr;

    }


    // [ 2 ] CREATE BRIGHT PASS PIXEL SHADER
    //--------------------------------------
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Shader Code\\PostProcessing.psh" ) );
    V( D3DXCompileShaderFromFile(
       str,
       NULL, NULL,
       "BrightPass",
       "ps_2_0",
       0,
       &pCode,
       NULL,
       &PostProcess::g_pBrightPassConstants
       ) );
    if( FAILED( hr ) )
    {

        // Couldn't compile the shader, use the 'compile_shaders.bat' script
        // in the 'Shader Code' folder to get a proper compile breakdown.
        OutputDebugString(
            L"PostProcess::CreateResources() - Compiling of 'BrightPass' from 'PostProcessing.psh' failed!\n" );
        return hr;

    }

    V( pDevice->CreatePixelShader( reinterpret_cast< DWORD* >( pCode->GetBufferPointer() ),
                                   &PostProcess::g_pBrightPassPS ) );
    if( FAILED( hr ) )
    {

        // Couldn't turn the compiled shader into an actual, usable, pixel shader!
        OutputDebugString(
            L"PostProcess::CreateResources() - Could not create a pixel shader object for 'BrightPass'.\n" );
        pCode->Release();
        return hr;

    }

    pCode->Release();



    // [ 3 ] CREATE DOWNSAMPLED TEXTURE
    //---------------------------------
    // This render target is 1/8th the size of the original HDR image (or, more
    // importantly, 1/4 the size of the bright-pass). The downsampling pixel
    // shader performs a 4x4 downsample in order to reduce the number of pixels
    // that are sent to the horizontal/vertical blurring stages.
    V( pDevice->CreateTexture(
       pDisplayDesc->Width / 8, pDisplayDesc->Height / 8,
       1, D3DUSAGE_RENDERTARGET, PostProcess::g_fmtHDR,
       D3DPOOL_DEFAULT, &PostProcess::g_pDownSampledTex, NULL
       ) );
    if( FAILED( hr ) )
    {

        // We couldn't create the texture - lots of possible reasons for this...
        OutputDebugString(
            L"PostProcess::CreateResources() - Could not create downsampling render target. Examine D3D Debug Output for details.\n" );
        return hr;

    }


    // [ 3 ] CREATE DOWNSAMPLING PIXEL SHADER
    //---------------------------------------
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Shader Code\\PostProcessing.psh" ) );
    V( D3DXCompileShaderFromFile(
       str,
       NULL, NULL,
       "DownSample",
       "ps_2_0",
       0,
       &pCode,
       NULL,
       &PostProcess::g_pDownSampleConstants
       ) );
    if( FAILED( hr ) )
    {

        // Couldn't compile the shader, use the 'compile_shaders.bat' script
        // in the 'Shader Code' folder to get a proper compile breakdown.
        OutputDebugString(
            L"PostProcess::CreateResources() - Compiling of 'DownSample' from 'PostProcessing.psh' failed!\n" );
        return hr;

    }

    V( pDevice->CreatePixelShader( reinterpret_cast< DWORD* >( pCode->GetBufferPointer() ),
                                   &PostProcess::g_pDownSamplePS ) );
    if( FAILED( hr ) )
    {

        // Couldn't turn the compiled shader into an actual, usable, pixel shader!
        OutputDebugString(
            L"PostProcess::CreateResources() - Could not create a pixel shader object for 'DownSample'.\n" );
        pCode->Release();
        return hr;

    }

    pCode->Release();



    // [ 4 ] CREATE HORIZONTAL BLOOM TEXTURE
    //--------------------------------------
    // The horizontal bloom texture is the same dimension as the down sample
    // render target. Combining a 4x4 downsample operation as well as a
    // horizontal blur leads to a prohibitively high number of texture reads.
    V( pDevice->CreateTexture(
       pDisplayDesc->Width / 8, pDisplayDesc->Height / 8,
       1, D3DUSAGE_RENDERTARGET, PostProcess::g_fmtHDR,
       D3DPOOL_DEFAULT, &PostProcess::g_pBloomHorizontal, NULL
       ) );
    if( FAILED( hr ) )
    {

        // We couldn't create the texture - lots of possible reasons for this...
        OutputDebugString(
            L"PostProcess::CreateResources() - Could not create horizontal bloom render target. Examine D3D Debug Output for details.\n" );
        return hr;

    }

    // [ 5 ] CREATE HORIZONTAL BLOOM PIXEL SHADER
    //-------------------------------------------
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Shader Code\\PostProcessing.psh" ) );
    V( D3DXCompileShaderFromFile(
       str,
       NULL, NULL,
       "HorizontalBlur",
       "ps_2_0",
       0,
       &pCode,
       NULL,
       &PostProcess::g_pHBloomConstants
       ) );
    if( FAILED( hr ) )
    {

        // Couldn't compile the shader, use the 'compile_shaders.bat' script
        // in the 'Shader Code' folder to get a proper compile breakdown.
        OutputDebugString(
            L"PostProcess::CreateResources() - Compiling of 'HorizontalBlur' from 'PostProcessing.psh' failed!\n" );
        return hr;

    }

    V( pDevice->CreatePixelShader( reinterpret_cast< DWORD* >( pCode->GetBufferPointer() ),
                                   &PostProcess::g_pHBloomPS ) );
    if( FAILED( hr ) )
    {

        // Couldn't turn the compiled shader into an actual, usable, pixel shader!
        OutputDebugString(
            L"PostProcess::CreateResources() - Could not create a pixel shader object for 'HorizontalBlur'.\n" );
        pCode->Release();
        return hr;

    }

    pCode->Release();

    // [ 6 ] CREATE VERTICAL BLOOM TEXTURE
    //------------------------------------
    // The vertical blur texture must be the same size as the horizontal blur texture
    // so as to get a correct 2D distribution pattern.
    V( pDevice->CreateTexture(
       pDisplayDesc->Width / 8, pDisplayDesc->Height / 8,
       1, D3DUSAGE_RENDERTARGET, PostProcess::g_fmtHDR,
       D3DPOOL_DEFAULT, &PostProcess::g_pBloomVertical, NULL
       ) );
    if( FAILED( hr ) )
    {

        // We couldn't create the texture - lots of possible reasons for this...
        OutputDebugString(
            L"PostProcess::CreateResources() - Could not create vertical bloom render target. Examine D3D Debug Output for details.\n" );
        return hr;

    }


    // [ 7 ] CREATE VERTICAL BLOOM PIXEL SHADER
    //-----------------------------------------
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Shader Code\\PostProcessing.psh" ) );
    V( D3DXCompileShaderFromFile(
       str,
       NULL, NULL,
       "VerticalBlur",
       "ps_2_0",
       0,
       &pCode,
       NULL,
       &PostProcess::g_pVBloomConstants
       ) );
    if( FAILED( hr ) )
    {

        // Couldn't compile the shader, use the 'compile_shaders.bat' script
        // in the 'Shader Code' folder to get a proper compile breakdown.
        OutputDebugString(
            L"PostProcess::CreateResources() - Compiling of 'VerticalBlur' from 'PostProcessing.psh' failed!\n" );
        return hr;

    }

    V( pDevice->CreatePixelShader( reinterpret_cast< DWORD* >( pCode->GetBufferPointer() ),
                                   &PostProcess::g_pVBloomPS ) );
    if( FAILED( hr ) )
    {

        // Couldn't turn the compiled shader into an actual, usable, pixel shader!
        OutputDebugString(
            L"PostProcess::CreateResources() - Could not create a pixel shader object for 'VerticalBlur'.\n" );
        pCode->Release();
        return hr;

    }

    pCode->Release();

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

    SAFE_RELEASE( g_pBrightPassTex );
    SAFE_RELEASE( g_pDownSampledTex );
    SAFE_RELEASE( g_pBloomHorizontal );
    SAFE_RELEASE( g_pBloomVertical );

    SAFE_RELEASE( g_pBrightPassPS );
    SAFE_RELEASE( g_pBrightPassConstants );

    SAFE_RELEASE( g_pDownSamplePS );
    SAFE_RELEASE( g_pDownSampleConstants );

    SAFE_RELEASE( g_pHBloomPS );
    SAFE_RELEASE( g_pHBloomConstants );

    SAFE_RELEASE( g_pVBloomPS );
    SAFE_RELEASE( g_pVBloomConstants );

    return S_OK;

}



//--------------------------------------------------------------------------------------
//  PerformPostProcessing( )
//
//      DESC:
//          This is the core function for this module - it takes the raw HDR image
//          generated by the 'HDRScene' component and puts it through 4 post
//          processing stages - to end up with a bloom effect on the over-exposed
//          (HDR) parts of the image.
//
//      PARAMS:
//          pDevice : The device that will be rendered to
//
//      NOTES:
//          n/a
//
//--------------------------------------------------------------------------------------
HRESULT PerformPostProcessing( IDirect3DDevice9* pDevice )
{

    // [ 0 ] BRIGHT PASS
    //------------------
    LPDIRECT3DTEXTURE9 pHDRSource = NULL;
    if( FAILED( HDRScene::GetOutputTexture( &pHDRSource ) ) )
    {
        // Couldn't get the input - means that none of the subsequent
        // work is worth attempting!
        OutputDebugString( L"PostProcess::PerformPostProcessing() - Unable to retrieve source HDR information!\n" );
        return E_FAIL;

    }

    LPDIRECT3DSURFACE9 pBrightPassSurf = NULL;
    if( FAILED( PostProcess::g_pBrightPassTex->GetSurfaceLevel( 0, &pBrightPassSurf ) ) )
    {

        // Can't get the render target. Not good news!
        OutputDebugString(
            L"PostProcess::PerformPostProcessing() - Couldn't retrieve top level surface for bright pass render target.\n" );
        return E_FAIL;

    }

    pDevice->SetRenderTarget( 0, pBrightPassSurf );         // Configure the output of this stage
    pDevice->SetTexture( 0, pHDRSource );                   // Configure the input..
    pDevice->SetPixelShader( PostProcess::g_pBrightPassPS );
    PostProcess::g_pBrightPassConstants->SetFloat( pDevice, "fBrightPassThreshold", PostProcess::g_BrightThreshold );

    // We need to compute the sampling offsets used for this pass.
    // A 2x2 sampling pattern is used, so we need to generate 4 offsets
    D3DXVECTOR4 offsets[4];

    // Find the dimensions for the source data
    D3DSURFACE_DESC srcDesc;
    pHDRSource->GetLevelDesc( 0, &srcDesc );

    // Because the source and destination are NOT the same sizes, we
    // need to provide offsets to correctly map between them.
    float sU = ( 1.0f / static_cast< float >( srcDesc.Width ) );
    float sV = ( 1.0f / static_cast< float >( srcDesc.Height ) );

    // The last two components (z,w) are unused. This makes for simpler code, but if
    // constant-storage is limited then it is possible to pack 4 offsets into 2 float4's
    offsets[0] = D3DXVECTOR4( -0.5f * sU, 0.5f * sV, 0.0f, 0.0f );
    offsets[1] = D3DXVECTOR4( 0.5f * sU, 0.5f * sV, 0.0f, 0.0f );
    offsets[2] = D3DXVECTOR4( -0.5f * sU, -0.5f * sV, 0.0f, 0.0f );
    offsets[3] = D3DXVECTOR4( 0.5f * sU, -0.5f * sV, 0.0f, 0.0f );

    PostProcess::g_pBrightPassConstants->SetVectorArray( pDevice, "tcDownSampleOffsets", offsets, 4 );

    RenderToTexture( pDevice );



    // [ 1 ] DOWN SAMPLE
    //------------------
    LPDIRECT3DSURFACE9 pDownSampleSurf = NULL;
    if( FAILED( PostProcess::g_pDownSampledTex->GetSurfaceLevel( 0, &pDownSampleSurf ) ) )
    {

        // Can't get the render target. Not good news!
        OutputDebugString(
            L"PostProcess::PerformPostProcessing() - Couldn't retrieve top level surface for down sample render target.\n" );
        return E_FAIL;

    }

    pDevice->SetRenderTarget( 0, pDownSampleSurf );
    pDevice->SetTexture( 0, PostProcess::g_pBrightPassTex );
    pDevice->SetPixelShader( PostProcess::g_pDownSamplePS );

    // We need to compute the sampling offsets used for this pass.
    // A 4x4 sampling pattern is used, so we need to generate 16 offsets

    // Find the dimensions for the source data
    PostProcess::g_pBrightPassTex->GetLevelDesc( 0, &srcDesc );

    // Find the dimensions for the destination data
    D3DSURFACE_DESC destDesc;
    pDownSampleSurf->GetDesc( &destDesc );

    // Compute the offsets required for down-sampling. If constant-storage space
    // is important then this code could be packed into 8xFloat4's. The code here
    // is intentionally less efficient to aid readability...
    D3DXVECTOR4 dsOffsets[16];
    int idx = 0;
    for( int i = -2; i < 2; i++ )
    {
        for( int j = -2; j < 2; j++ )
        {
            dsOffsets[idx++] = D3DXVECTOR4(
                ( static_cast< float >( i ) + 0.5f ) * ( 1.0f / static_cast< float >( destDesc.Width ) ),
                ( static_cast< float >( j ) + 0.5f ) * ( 1.0f / static_cast< float >( destDesc.Height ) ),
                0.0f, // unused 
                0.0f  // unused
                );
        }
    }

    PostProcess::g_pDownSampleConstants->SetVectorArray( pDevice, "tcDownSampleOffsets", dsOffsets, 16 );

    RenderToTexture( pDevice );



    // [ 2 ] BLUR HORIZONTALLY
    //------------------------
    LPDIRECT3DSURFACE9 pHBloomSurf = NULL;
    if( FAILED( PostProcess::g_pBloomHorizontal->GetSurfaceLevel( 0, &pHBloomSurf ) ) )
    {

        // Can't get the render target. Not good news!
        OutputDebugString(
            L"PostProcess::PerformPostProcessing() - Couldn't retrieve top level surface for horizontal bloom render target.\n" );
        return E_FAIL;

    }

    pDevice->SetRenderTarget( 0, pHBloomSurf );
    pDevice->SetTexture( 0, PostProcess::g_pDownSampledTex );
    pDevice->SetPixelShader( PostProcess::g_pHBloomPS );

    // Configure the sampling offsets and their weights
    float HBloomWeights[9];
    float HBloomOffsets[9];

    for( int i = 0; i < 9; i++ )
    {
        // Compute the offsets. We take 9 samples - 4 either side and one in the middle:
        //     i =  0,  1,  2,  3, 4,  5,  6,  7,  8
        //Offset = -4, -3, -2, -1, 0, +1, +2, +3, +4
        HBloomOffsets[i] = ( static_cast< float >( i ) - 4.0f ) * ( 1.0f / static_cast< float >( destDesc.Width ) );

        // 'x' is just a simple alias to map the [0,8] range down to a [-1,+1]
        float x = ( static_cast< float >( i ) - 4.0f ) / 4.0f;

        // Use a gaussian distribution. Changing the standard-deviation
        // (second parameter) as well as the amplitude (multiplier) gives
        // distinctly different results.
        HBloomWeights[i] = g_GaussMultiplier * ComputeGaussianValue( x, g_GaussMean, g_GaussStdDev );
    }

    // Commit both arrays to the device:
    PostProcess::g_pHBloomConstants->SetFloatArray( pDevice, "HBloomWeights", HBloomWeights, 9 );
    PostProcess::g_pHBloomConstants->SetFloatArray( pDevice, "HBloomOffsets", HBloomOffsets, 9 );

    RenderToTexture( pDevice );



    // [ 3 ] BLUR VERTICALLY
    //----------------------
    LPDIRECT3DSURFACE9 pVBloomSurf = NULL;
    if( FAILED( PostProcess::g_pBloomVertical->GetSurfaceLevel( 0, &pVBloomSurf ) ) )
    {

        // Can't get the render target. Not good news!
        OutputDebugString(
            L"PostProcess::PerformPostProcessing() - Couldn't retrieve top level surface for vertical bloom render target.\n" );
        return E_FAIL;

    }

    pDevice->SetRenderTarget( 0, pVBloomSurf );
    pDevice->SetTexture( 0, PostProcess::g_pBloomHorizontal );
    pDevice->SetPixelShader( PostProcess::g_pVBloomPS );

    // Configure the sampling offsets and their weights

    // It is worth noting that although this code is almost identical to the
    // previous section ('H' weights, above) there is an important difference: destDesc.Height.
    // The bloom render targets are *not* square, such that you can't re-use the same offsets in
    // both directions.
    float VBloomWeights[9];
    float VBloomOffsets[9];

    for( int i = 0; i < 9; i++ )
    {
        // Compute the offsets. We take 9 samples - 4 either side and one in the middle:
        //     i =  0,  1,  2,  3, 4,  5,  6,  7,  8
        //Offset = -4, -3, -2, -1, 0, +1, +2, +3, +4
        VBloomOffsets[i] = ( static_cast< float >( i ) - 4.0f ) * ( 1.0f / static_cast< float >( destDesc.Height ) );

        // 'x' is just a simple alias to map the [0,8] range down to a [-1,+1]
        float x = ( static_cast< float >( i ) - 4.0f ) / 4.0f;

        // Use a gaussian distribution. Changing the standard-deviation
        // (second parameter) as well as the amplitude (multiplier) gives
        // distinctly different results.
        VBloomWeights[i] = g_GaussMultiplier * ComputeGaussianValue( x, g_GaussMean, g_GaussStdDev );
    }

    // Commit both arrays to the device:
    PostProcess::g_pVBloomConstants->SetFloatArray( pDevice, "VBloomWeights", VBloomWeights, 9 );
    PostProcess::g_pVBloomConstants->SetFloatArray( pDevice, "VBloomOffsets", VBloomOffsets, 9 );

    RenderToTexture( pDevice );



    // [ 4 ] CLEAN UP
    //---------------
    SAFE_RELEASE( pHDRSource );
    SAFE_RELEASE( pBrightPassSurf );
    SAFE_RELEASE( pDownSampleSurf );
    SAFE_RELEASE( pHBloomSurf );
    SAFE_RELEASE( pVBloomSurf );

    return S_OK;

}



//--------------------------------------------------------------------------------------
//  GetTexture( )
//
//      DESC:
//          The results generated by PerformPostProcessing() are required as an input
//          into the final image composition. Because that final stage is located
//          in another code module, it must be able to safely access the correct
//          texture stored internally within this module.
//
//      PARAMS:
//          pTexture    : Should be NULL on entry, will be a valid reference on exit
//
//      NOTES:
//          The code that requests the reference is responsible for releasing their
//          copy of the texture as soon as they are finished using it.
//
//--------------------------------------------------------------------------------------
HRESULT GetTexture( IDirect3DTexture9** pTexture )
{

    // [ 0 ] ERASE ANY DATA IN THE INPUT
    //----------------------------------
    SAFE_RELEASE( *pTexture );

    // [ 1 ] COPY THE PRIVATE REFERENCE
    //---------------------------------
    *pTexture = PostProcess::g_pBloomVertical;

    // [ 2 ] INCREMENT THE REFERENCE COUNT..
    //--------------------------------------
    ( *pTexture )->AddRef();

    return S_OK;

}



//--------------------------------------------------------------------------------------
//  GetBrightPassThreshold( )
//
//      DESC:
//          Returns the current bright pass threshold, as used by the PostProcessing.psh
//          pixel shader.
//
//--------------------------------------------------------------------------------------
float GetBrightPassThreshold()
{

    return PostProcess::g_BrightThreshold;

}



//--------------------------------------------------------------------------------------
//  SetBrightPassThreshold( )
//
//      DESC:
//          Allows another module to configure the threshold at which a pixel is
//          considered to be "bright" and thus get fed into the post-processing
//          part of the pipeline.
//
//      PARAMS:
//          threshold   : The new value to be used
//
//--------------------------------------------------------------------------------------
void SetBrightPassThreshold( const float& threshold )
{

    PostProcess::g_BrightThreshold = threshold;

}



//--------------------------------------------------------------------------------------
//  GetGaussianMultiplier( )
//
//      DESC:
//          Returns the multiplier used to scale the gaussian distribution.
//
//--------------------------------------------------------------------------------------
float GetGaussianMultiplier()
{

    return PostProcess::g_GaussMultiplier;

}



//--------------------------------------------------------------------------------------
//  SetGaussianMultiplier( )
//
//      DESC:
//          Allows another module to configure the general multiplier. Not strictly
//          part of the gaussian distribution, but a useful factor to help control the
//          intensity of the bloom.
//
//      PARAMS:
//          multiplier   : The new value to be used
//
//--------------------------------------------------------------------------------------
void SetGaussianMultiplier( const float& multiplier )
{

    PostProcess::g_GaussMultiplier = multiplier;

}



//--------------------------------------------------------------------------------------
//  GetGaussianMean( )
//
//      DESC:
//          Returns the mean used to compute the gaussian distribution for the bloom
//
//--------------------------------------------------------------------------------------
float GetGaussianMean()
{

    return PostProcess::g_GaussMean;

}



//--------------------------------------------------------------------------------------
//  SetGaussianMean( )
//
//      DESC:
//          Allows another module to specify where the peak will be in the gaussian
//          distribution. Values should be between -1 and +1; a value of 0 will be
//          best.
//
//      PARAMS:
//          mean   : The new value to be used
//
//--------------------------------------------------------------------------------------
void SetGaussianMean( const float& mean )
{

    PostProcess::g_GaussMean = mean;

}



//--------------------------------------------------------------------------------------
//  GetGaussianStandardDeviation( )
//
//      DESC:
//          Returns the standard deviation used to construct the gaussian distribution
//          used by the horizontal/vertical bloom processing.
//
//--------------------------------------------------------------------------------------
float GetGaussianStandardDeviation()
{

    return PostProcess::g_GaussStdDev;

}



//--------------------------------------------------------------------------------------
//  SetGaussianStandardDeviation( )
//
//      DESC:
//          Allows another module to configure the standard deviation
//          used to generate a gaussian distribution. Should be between
//          0.0 and 1.0 for valid results.
//
//      PARAMS:
//          sd    : The new value to be used
//
//--------------------------------------------------------------------------------------
void SetGaussianStandardDeviation( const float& sd )
{

    PostProcess::g_GaussStdDev = sd;

}



//--------------------------------------------------------------------------------------
//  DisplaySteps( )
//
//      DESC:
//          Part of the GUI in this application displays the four stages that make up
//          the post-processing stage of the HDR rendering pipeline. This function
//          is responsible for making sure that each stage is drawn as expected.
//
//      PARAMS:
//          pDevice     : The device to be drawn to.
//          pFont       : The font used to annotate the display
//          pTextSprite : Used to improve the performance of text rendering
//          pArrowTex   : Stores the 4 (up/down/left/right) icons used in the GUI
//
//      NOTES:
//          n/a
//
//--------------------------------------------------------------------------------------
HRESULT DisplaySteps( IDirect3DDevice9* pDevice, ID3DXFont* pFont, ID3DXSprite* pTextSprite,
                      IDirect3DTexture9* pArrowTex )
{

    // [ 0 ] COMMON INITIALIZATION
    //----------------------------

    LPDIRECT3DSURFACE9 pSurf = NULL;
    D3DSURFACE_DESC d;
    if( FAILED( pDevice->GetRenderTarget( 0, &pSurf ) ) )
    {

        // Couldn't get the current render target!
        OutputDebugString(
            L"PostProcess::DisplaySteps() - Could not get current render target to extract dimensions.\n" );
        return E_FAIL;

    }

    pSurf->GetDesc( &d );
    SAFE_RELEASE( pSurf );

    // Cache the dimensions as floats for later use
    float fW = static_cast< float >( d.Width );
    float fH = static_cast< float >( d.Height );

    float fCellW = ( fW - 48.0f ) / 4.0f;
    float fCellH = ( fH - 36.0f ) / 4.0f;

    // Fill out the basic TLQuad information - this
    // stuff doesn't change for each stage
    PostProcess::TLVertex v[4];
    v[0].t = D3DXVECTOR2( 0.0f, 0.0f );
    v[1].t = D3DXVECTOR2( 1.0f, 0.0f );
    v[2].t = D3DXVECTOR2( 0.0f, 1.0f );
    v[3].t = D3DXVECTOR2( 1.0f, 1.0f );

    // Configure the device for it's basic states
    pDevice->SetVertexShader( NULL );
    pDevice->SetFVF( FVF_TLVERTEX );
    pDevice->SetPixelShader( NULL );

    CDXUTTextHelper txtHelper( pFont, pTextSprite, 12 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.5f, 0.0f, 1.0f ) );


    // [ 1 ] RENDER BRIGHT PASS STAGE
    //-------------------------------
    v[0].p = D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( fCellW, 0.0f, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( 0.0f, fCellH, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( fCellW, fCellH, 0.0f, 1.0f );

    pDevice->SetTexture( 0, PostProcess::g_pBrightPassTex );
    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( PostProcess::TLVertex ) );

    txtHelper.Begin();
    {
        txtHelper.SetInsertionPos( 5, static_cast< int >( fCellH - 25.0f ) );
        txtHelper.DrawTextLine( L"Bright-Pass" );

        D3DSURFACE_DESC d2;
        PostProcess::g_pBrightPassTex->GetLevelDesc( 0, &d2 );

        WCHAR str[100];
        swprintf_s( str, 100, L"%dx%d", d2.Width, d2.Height );
        txtHelper.DrawTextLine( str );
    }
    txtHelper.End();

    // [ 2 ] RENDER DOWNSAMPLED STAGE
    //-------------------------------
    v[0].p = D3DXVECTOR4( fCellW + 16.0f, 0.0f, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( ( 2.0f * fCellW ) + 16.0f, 0.0f, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( fCellW + 16.0f, fCellH, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( ( 2.0f * fCellW ) + 16.0f, fCellH, 0.0f, 1.0f );

    pDevice->SetTexture( 0, PostProcess::g_pDownSampledTex );
    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( PostProcess::TLVertex ) );

    txtHelper.Begin();
    {
        txtHelper.SetInsertionPos( static_cast< int >( fCellW + 16.0f ) + 5, static_cast< int >( fCellH - 25.0f ) );
        txtHelper.DrawTextLine( L"Down-Sampled" );

        D3DSURFACE_DESC d;
        PostProcess::g_pDownSampledTex->GetLevelDesc( 0, &d );

        WCHAR str[100];
        swprintf_s( str, 100, L"%dx%d", d.Width, d.Height );
        txtHelper.DrawTextLine( str );
    }
    txtHelper.End();

    // [ 3 ] RENDER HORIZONTAL BLUR STAGE
    //-----------------------------------
    v[0].p = D3DXVECTOR4( ( 2.0f * fCellW ) + 32.0f, 0.0f, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( ( 3.0f * fCellW ) + 32.0f, 0.0f, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( ( 2.0f * fCellW ) + 32.0f, fCellH, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( ( 3.0f * fCellW ) + 32.0f, fCellH, 0.0f, 1.0f );

    pDevice->SetTexture( 0, PostProcess::g_pBloomHorizontal );
    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( PostProcess::TLVertex ) );

    txtHelper.Begin();
    {
        txtHelper.SetInsertionPos( static_cast< int >( 2.0f * fCellW + 32.0f ) + 5,
                                   static_cast< int >( fCellH - 25.0f ) );
        txtHelper.DrawTextLine( L"Horizontal Blur" );

        D3DSURFACE_DESC d;
        PostProcess::g_pBloomHorizontal->GetLevelDesc( 0, &d );

        WCHAR str[100];
        swprintf_s( str, 100, L"%dx%d", d.Width, d.Height );
        txtHelper.DrawTextLine( str );
    }
    txtHelper.End();

    // [ 4 ] RENDER VERTICAL BLUR STAGE
    //---------------------------------
    v[0].p = D3DXVECTOR4( ( 3.0f * fCellW ) + 48.0f, 0.0f, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( ( 4.0f * fCellW ) + 48.0f, 0.0f, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( ( 3.0f * fCellW ) + 48.0f, fCellH, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( ( 4.0f * fCellW ) + 48.0f, fCellH, 0.0f, 1.0f );

    pDevice->SetTexture( 0, PostProcess::g_pBloomVertical );
    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( PostProcess::TLVertex ) );

    txtHelper.Begin();
    {
        txtHelper.SetInsertionPos( static_cast< int >( 3.0f * fCellW + 48.0f ) + 5,
                                   static_cast< int >( fCellH - 25.0f ) );
        txtHelper.DrawTextLine( L"Vertical Blur" );

        D3DSURFACE_DESC d;
        PostProcess::g_pDownSampledTex->GetLevelDesc( 0, &d );

        WCHAR str[100];
        swprintf_s( str, 100, L"%dx%d", d.Width, d.Height );
        txtHelper.DrawTextLine( str );
    }
    txtHelper.End();


    // [ 5 ] RENDER ARROWS
    //--------------------
    pDevice->SetTexture( 0, pArrowTex );

    // Locate the "Left" Arrow:

    v[0].t = D3DXVECTOR2( 0.25f, 0.0f );
    v[1].t = D3DXVECTOR2( 0.50f, 0.0f );
    v[2].t = D3DXVECTOR2( 0.25f, 1.0f );
    v[3].t = D3DXVECTOR2( 0.50f, 1.0f );

    // Bright-Pass -> Down Sampled:

    v[0].p = D3DXVECTOR4( fCellW, ( fCellH / 2.0f ) - 8.0f, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( fCellW + 16.0f, ( fCellH / 2.0f ) - 8.0f, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( fCellW, ( fCellH / 2.0f ) + 8.0f, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( fCellW + 16.0f, ( fCellH / 2.0f ) + 8.0f, 0.0f, 1.0f );

    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( PostProcess::TLVertex ) );

    // Down Sampled -> Horizontal Blur:

    v[0].p = D3DXVECTOR4( ( 2.0f * fCellW ) + 16.0f, ( fCellH / 2.0f ) - 8.0f, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( ( 2.0f * fCellW ) + 32.0f, ( fCellH / 2.0f ) - 8.0f, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( ( 2.0f * fCellW ) + 16.0f, ( fCellH / 2.0f ) + 8.0f, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( ( 2.0f * fCellW ) + 32.0f, ( fCellH / 2.0f ) + 8.0f, 0.0f, 1.0f );

    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( PostProcess::TLVertex ) );

    // Horizontal Blur -> Vertical Blur:

    v[0].p = D3DXVECTOR4( ( 3.0f * fCellW ) + 32.0f, ( fCellH / 2.0f ) - 8.0f, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( ( 3.0f * fCellW ) + 48.0f, ( fCellH / 2.0f ) - 8.0f, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( ( 3.0f * fCellW ) + 32.0f, ( fCellH / 2.0f ) + 8.0f, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( ( 3.0f * fCellW ) + 48.0f, ( fCellH / 2.0f ) + 8.0f, 0.0f, 1.0f );

    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( PostProcess::TLVertex ) );

    // Locate the "Down" arrow:

    v[0].t = D3DXVECTOR2( 0.50f, 0.0f );
    v[1].t = D3DXVECTOR2( 0.75f, 0.0f );
    v[2].t = D3DXVECTOR2( 0.50f, 1.0f );
    v[3].t = D3DXVECTOR2( 0.75f, 1.0f );

    // Vertical Blur -> Final Image Composition:

    v[0].p = D3DXVECTOR4( ( 3.5f * fCellW ) + 40.0f, fCellH, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( ( 3.5f * fCellW ) + 56.0f, fCellH, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4( ( 3.5f * fCellW ) + 40.0f, fCellH + 16.0f, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4( ( 3.5f * fCellW ) + 56.0f, fCellH + 16.0f, 0.0f, 1.0f );

    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( PostProcess::TLVertex ) );

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

    DWORD usage = 0;
    D3DSURFACE_DESC d;

    if( SUCCEEDED( g_pBrightPassTex->GetLevelDesc( 0, &d ) ) )
        usage += ( ( d.Width * d.Height ) * ( PostProcess::g_fmtHDR == D3DFMT_A32B32G32R32F ? 16 : 8 ) );

    if( SUCCEEDED( g_pDownSampledTex->GetLevelDesc( 0, &d ) ) )
        usage += ( ( d.Width * d.Height ) * ( PostProcess::g_fmtHDR == D3DFMT_A32B32G32R32F ? 16 : 8 ) );

    if( SUCCEEDED( g_pBloomHorizontal->GetLevelDesc( 0, &d ) ) )
        usage += ( ( d.Width * d.Height ) * ( PostProcess::g_fmtHDR == D3DFMT_A32B32G32R32F ? 16 : 8 ) );

    if( SUCCEEDED( g_pBloomVertical->GetLevelDesc( 0, &d ) ) )
        usage += ( ( d.Width * d.Height ) * ( PostProcess::g_fmtHDR == D3DFMT_A32B32G32R32F ? 16 : 8 ) );

    return usage;

}



//--------------------------------------------------------------------------------------
//  RenderToTexture( )
//
//      DESC:
//          A simple utility function that draws, as a TL Quad, one texture to another
//          such that a pixel shader (configured before this function is called) can
//          operate on the texture. Used by MeasureLuminance() to perform the
//          downsampling and filtering.
//
//      PARAMS:
//          pDevice : The currently active device
//
//      NOTES:
//          n/a
//
//--------------------------------------------------------------------------------------
HRESULT RenderToTexture( IDirect3DDevice9* pDev )
{

    D3DSURFACE_DESC desc;
    LPDIRECT3DSURFACE9 pSurfRT;

    pDev->GetRenderTarget( 0, &pSurfRT );
    pSurfRT->GetDesc( &desc );
    pSurfRT->Release();

    // To correctly map from texels->pixels we offset the coordinates
    // by -0.5f:
    float fWidth = static_cast< float >( desc.Width ) - 0.5f;
    float fHeight = static_cast< float >( desc.Height ) - 0.5f;

    // Now we can actually assemble the screen-space geometry
    PostProcess::TLVertex v[4];

    v[0].p = D3DXVECTOR4( -0.5f, -0.5f, 0.0f, 1.0f );
    v[0].t = D3DXVECTOR2( 0.0f, 0.0f );

    v[1].p = D3DXVECTOR4( fWidth, -0.5f, 0.0f, 1.0f );
    v[1].t = D3DXVECTOR2( 1.0f, 0.0f );

    v[2].p = D3DXVECTOR4( -0.5f, fHeight, 0.0f, 1.0f );
    v[2].t = D3DXVECTOR2( 0.0f, 1.0f );

    v[3].p = D3DXVECTOR4( fWidth, fHeight, 0.0f, 1.0f );
    v[3].t = D3DXVECTOR2( 1.0f, 1.0f );

    // Configure the device and render..
    pDev->SetVertexShader( NULL );
    pDev->SetFVF( PostProcess::FVF_TLVERTEX );
    pDev->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( PostProcess::TLVertex ) );

    return S_OK;

}


float ComputeGaussianValue( float x, float mean, float std_deviation )
{
    // The gaussian equation is defined as such:
    /*    
      -(x - mean)^2
      -------------
      1.0               2*std_dev^2
      f(x,mean,std_dev) = -------------------- * e^
      sqrt(2*pi*std_dev^2)
      
     */

    return ( 1.0f / sqrt( 2.0f * D3DX_PI * std_deviation * std_deviation ) )
        * expf( ( -( ( x - mean ) * ( x - mean ) ) ) / ( 2.0f * std_deviation * std_deviation ) );
}

}
;
