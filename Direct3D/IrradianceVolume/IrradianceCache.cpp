//--------------------------------------------------------------------------------------
// File: IrradianceCache.cpp
// Copyright ATI Research, Inc. 
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "IrradianceCache.h"
#include "float.h"

#define IRRADIANCECACHE_FILE_VERSION_STRING (L"ATI Irradiance Cache File v1.2")

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WFILE__ WIDEN(__FILE__)

#define OUTPUT_ERROR_MESSAGE DXUTOutputDebugString(L"%s(%d) : ",  __WFILE__, __LINE__); OutputDebugString

//========================================================================//
//=== CIrradianceCacheGenerator:: Generates/Fills an irradiance cache. ===//
//========================================================================//

//============//
// Contructor //
//============//
CIrradianceCacheGenerator::CIrradianceCacheGenerator( void )
{
    m_dwMinOctreeSubdivision = 0;
    m_dwMaxOctreeSubdivision = 0;
    m_bAdaptiveOctreeSubdivision = false;
    m_fHMDepthSubdivThreshold = 1.0f;

    m_vBoundingBoxMin = D3DXVECTOR3( FLT_MAX, FLT_MAX, FLT_MAX );
    m_vBoundingBoxMax = D3DXVECTOR3( -FLT_MAX, -FLT_MAX, -FLT_MAX );

    m_pD3DDevice = NULL;
    m_pRenderToEnvMap = NULL;
    m_pCubeTexture = NULL;

    return;
}

//============//
// Destructor //
//============//
CIrradianceCacheGenerator::~CIrradianceCacheGenerator( void )
{
    ReleaseGPUResources();
    m_pSceneMeshes.RemoveAll();
    m_bAdaptiveTest.RemoveAll();

    return;
}

//========================================================================================//
// Add a scene mesh to the list of meshes from which radiance/irradiance will be captured //
//========================================================================================//
bool CIrradianceCacheGenerator::AddSceneMesh( CSceneMesh* pSceneMesh, bool bUseForAdaptiveTest )
{
    //=================================//
    // Sanity check the mesh's pointer //
    //=================================//
    if( NULL == pSceneMesh )
    {
        OUTPUT_ERROR_MESSAGE( L"Received a NULL pointer!\n" );
        return false;
    }

    //==============================//
    // Get this mesh's bounding box //
    //==============================//
    D3DXVECTOR3 vMin( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vMax( 0.0f, 0.0f, 0.0f );

    if( !( pSceneMesh->GetBoundingBox( &vMin, &vMax ) ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Unabled to get scene's bounding box!\n" );
        return false;
    }

    //================================================================================//
    // Add this mesh to the list.  If the caller sends the same scene mesh more than  //
    // once, then that's the callers fault.  No checks are implemented to prevent it. //
    //================================================================================//
    HRESULT hResult = 0x0;
    hResult = m_pSceneMeshes.Add( pSceneMesh );
    if( FAILED( hResult ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Unable to add mesh to scene!\n" );
        return false;
    }

    hResult = m_bAdaptiveTest.Add( bUseForAdaptiveTest );
    if( FAILED( hResult ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Unable to add bUseAdaptiveTest to array!\n" );
        return false;
    }

    //=================================//
    // Update the scene's bounding box //
    //=================================//
    D3DXVec3Maximize( &m_vBoundingBoxMax, &m_vBoundingBoxMax, &vMax );
    D3DXVec3Minimize( &m_vBoundingBoxMin, &m_vBoundingBoxMin, &vMin );

    return true;
}

//=========================================================//
// Set how the scene's radiance/irradiance will be sampled //
//=========================================================//
bool CIrradianceCacheGenerator::SetSamplingInfo( DWORD dwMaxSubdivision, bool bAdaptiveSubdivision,
                                                 DWORD dwMinSubdivision, float fHMDepthSubdivThreshold )
{
    if( ( m_bAdaptiveOctreeSubdivision ) && ( m_dwMaxOctreeSubdivision <= dwMinSubdivision ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Either adaptive subdivision is not enabled or your arguments are incorrect!\n" );
        return false;
    }

    if( fHMDepthSubdivThreshold < 0.0f )
    {
        OUTPUT_ERROR_MESSAGE( L"fHMDepthSubdivThreshold out of range [0, +inf]!\n" );
        return false;
    }

    m_dwMaxOctreeSubdivision = dwMaxSubdivision;
    m_dwMinOctreeSubdivision = dwMinSubdivision;
    m_bAdaptiveOctreeSubdivision = bAdaptiveSubdivision;
    m_fHMDepthSubdivThreshold = fHMDepthSubdivThreshold;

    return true;
}

//======================================================================================================//
// Pass pointers to the GPU resources that should be used for capturing the scene's radiance/irradiance //
// These pointers must remain valid until FillCache() finishes!  This function will increment the ref   //
// count.  Ref count is decremented by ReleaseGPUResources() or ~CIrradianceCacheGenerator() which ever //
// happens first.  The cubemap must be D3DFMT_A32B32G32R32F.                                            //
//======================================================================================================//
bool CIrradianceCacheGenerator::SetGPUResources( IDirect3DDevice9* pD3DDevice, ID3DXRenderToEnvMap* pRenderToEnvMap,
                                                 IDirect3DCubeTexture9* pCubeTexture )
{
    if( ( NULL == pD3DDevice ) || ( NULL == pRenderToEnvMap ) || ( NULL == pCubeTexture ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received a NULL pointer!\n" );
        return false;
    }

    //===============================================================================================//
    // If we already have these pointers, release them (decrement ref count) before caching new ones //
    //===============================================================================================//
    ReleaseGPUResources();

    //==============================================//
    // Make sure the cubemap is of the right format //
    //==============================================//
    HRESULT hResult = 0x0;
    D3DXRTE_DESC rtDesc;

    hResult = pRenderToEnvMap->GetDesc( &rtDesc );
    if( FAILED( hResult ) )
    {
        DXUT_ERR( L"GetDesc() failed!\n", hResult );
        return false;
    }

    if( ( D3DFMT_A32B32G32R32F != rtDesc.Format ) && ( D3DFMT_A16B16G16R16F != rtDesc.Format ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Render target format must be D3DFMT_A32B32G32R32F or D3DFMT_A16B16G16R16F\n" );
        return false;
    }

    D3DSURFACE_DESC surfaceDesc;

    hResult = pCubeTexture->GetLevelDesc( 0, &surfaceDesc );
    if( FAILED( hResult ) )
    {
        DXUT_ERR( L"GetLevelDesc() failed!\n", hResult );
        return false;
    }

    if( ( D3DFMT_A32B32G32R32F != surfaceDesc.Format ) && ( D3DFMT_A16B16G16R16F != surfaceDesc.Format ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Surface format must be D3DFMT_A32B32G32R32F or D3DFMT_A16B16G16R16F\n" );
        return false;
    }

    m_pD3DDevice = pD3DDevice;
    m_pRenderToEnvMap = pRenderToEnvMap;
    m_pCubeTexture = pCubeTexture;

    m_pD3DDevice->AddRef();
    m_pRenderToEnvMap->AddRef();
    m_pCubeTexture->AddRef();

    return true;
}

//====================================================================================================================//
// Decrement ref count of GPU resources and set internal pointers to these resources to NULL.  If you don't call this //
// then it will happen during destruction (if necessary).                                                             //
//====================================================================================================================//
bool CIrradianceCacheGenerator::ReleaseGPUResources( void )
{
    SAFE_RELEASE( m_pD3DDevice );
    SAFE_RELEASE( m_pRenderToEnvMap );
    SAFE_RELEASE( m_pCubeTexture );

    return true;
}

//===========================================================================================================//
// Calculate the bounding box for all meshes in the scene:                                                   //
// pMin: Pointer to a D3DXVECTOR3 structure, describing the returned lower-left corner of the bounding box.  //
// pMax: Pointer to a D3DXVECTOR3 structure, describing the returned upper-right corner of the bounding box. //
//===========================================================================================================//
bool CIrradianceCacheGenerator::GetSceneBounds( D3DXVECTOR3* pMin, D3DXVECTOR3* pMax )
{
    if( ( NULL == pMin ) || ( NULL == pMax ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointers!\n" );
        return false;
    }

    *pMin = m_vBoundingBoxMin;
    *pMax = m_vBoundingBoxMax;

    return true;
}

//==============================================================================================================//
// Creates a cache and optionally fills it.  While the cache is being filled, you may not use the GPU resources //
// that were passed to SetGPUResources.  This function may take a long time to complete.  If you need to use    //
// the GPU resources to continue updating a window's UI or if you wish to display progress information to the   //
// user, set bFillCache to false and use the ProgressiveCacheFill to fill the cache.                            //
//==============================================================================================================//
bool CIrradianceCacheGenerator::CreateCache( CIrradianceCache** ppCache, bool bFillCache )
{
    //============================//
    // Sanity check some pointers //
    //============================//
    if( ( NULL == ppCache ) || ( NULL == m_pD3DDevice ) || ( NULL == m_pRenderToEnvMap ) ||
        ( NULL == m_pCubeTexture ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointers!\n" );
        return false;
    }

    //=====================================//
    // Make sure we have a scene to sample //
    //=====================================//
    if( 0 >= m_pSceneMeshes.GetSize() )
    {
        OUTPUT_ERROR_MESSAGE( L"No scene meshes to preprocess!\n" );
        return false;
    }

    //===============================//
    // Pull the MIN and MAX in a bit //
    //===============================//
    //m_vBoundingBoxMax = m_vBoundingBoxMax - D3DXVECTOR3(1.0f, 1.0f, 1.0f);
    //m_vBoundingBoxMin = m_vBoundingBoxMin + D3DXVECTOR3(1.0f, 1.0f, 1.0f);

    //====================//
    // Allocate the cache //
    //====================//
    *ppCache = new CIrradianceCache( &m_vBoundingBoxMin, &m_vBoundingBoxMax );
    if( NULL == *ppCache )
    {
        OUTPUT_ERROR_MESSAGE( L"Ran out of memory!\n" );
        return false;
    }

    //===========================//
    // Start with an empty cache //
    //===========================//
    ( *ppCache )->ClearCache();

    //===================================//
    // Fill cache right now, if asked to //
    //===================================//
    if( bFillCache )
    {
        bool bDone = false;
        while( !( bDone ) )
        {
            bool bResult = ProgressiveCacheFill( *ppCache, &bDone, NULL );
            if( !( bResult ) )
            {
                OUTPUT_ERROR_MESSAGE( L"Cache fill failed!\n" );
                SAFE_DELETE( *ppCache );
                return false;
            }
        }
    }

    return true;
}

//============================//
// Create a cache from a file //
//============================//
bool CIrradianceCacheGenerator::CreateCache( WCHAR* strFileName, CIrradianceCache** ppCache )
{
    if( ( NULL == strFileName ) || ( NULL == ppCache ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointers!\n" );
        return false;
    }

    //====================//
    // Allocate the cache //
    //====================//
    D3DXVECTOR3 vMin( -1.0f, -1.0f, -1.0f );
    D3DXVECTOR3 vMax( 1.0f, 1.0f, 1.0f );
    *ppCache = new CIrradianceCache( &vMin, &vMax );
    if( NULL == *ppCache )
    {
        OUTPUT_ERROR_MESSAGE( L"Ran out of memory!\n" );
        return false;
    }

    if( !( ( *ppCache )->LoadCache( strFileName ) ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Unable to load cache file!\n" );
        SAFE_DELETE( *ppCache );
        return false;
    }

    CIrradianceCacheOctree::OctreeNode* pRootNode = ( *ppCache )->m_pOctree->GetRootNode();
    if( NULL == pRootNode )
    {
        SAFE_DELETE( *ppCache );
        return false;
    }

    m_vBoundingBoxMin = pRootNode->vPosition[0];
    m_vBoundingBoxMax = pRootNode->vPosition[7];

    return true;
}

//======================================================================================================//
// Progressively fill cache.  This function should be called iteratively until *pDone is true (or until //
// the function returns an error.  This allows you to continue using the GPU resources between calls to //
// this function (to continue updating a window's UI for example or to display progress information).   //
//======================================================================================================//
bool CIrradianceCacheGenerator::ProgressiveCacheFill( CIrradianceCache* pCache, bool* pDone, float* pPercent )
{
    if( ( NULL == pCache ) || ( NULL == pDone ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointers!\n" );
        return false;
    }

    if( NULL == pCache->m_pOctree )
    {
        OUTPUT_ERROR_MESSAGE( L"No octree?!\n" );
        return false;
    }

    //========================================================//
    // Find the first unsampled node in a depth first search. //
    //========================================================//
    DWORD dwDepth = 0;
    CIrradianceCacheOctree::OctreeNode* pNode = pCache->m_pOctree->FindUnsampledNode( pCache->m_pOctree->GetRootNode(),
                                                                                      &dwDepth );

    if( NULL == pNode )
    {
        *pDone = true;
        if( NULL != pPercent )
        {
            *pPercent = 100.0f;
        }

        return true;
    }

    //===============================//
    // Fill in samples for this node //
    //===============================//
    for( int sampleIndex = 0; sampleIndex < 8; sampleIndex++ )
    {
        if( pNode->bSampleInCache[sampleIndex] )
        {
            continue;
        }

        //=======================================================================================//
        // Search cache to see if we have a usable sample for this location in the cache already //
        //=======================================================================================//
        bool bFoundInCache = false;
        int cacheSize = pCache->m_pCache.GetSize();

        //=================================================================================================//
        // Search backwards since recently added nodes will be more likely to share a vertex with this one //
        // because subdivion happens in a depth first manner.                                              //
        //=================================================================================================//
        for( int cacheIndex = cacheSize - 1; 0 <= cacheIndex; cacheIndex-- )
        {
            D3DXVECTOR3 vTemp = pCache->m_pCache[cacheIndex]->vPosition - pNode->vPosition[sampleIndex];

            if( 0.0001f >= D3DXVec3LengthSq( &vTemp ) )
            {
                pNode->dwSampleIndex[sampleIndex] = ( DWORD )cacheIndex;
                pNode->bSampleInCache[sampleIndex] = true;
                bFoundInCache = true;

                pCache->m_pCache[cacheIndex]->dwRefCount++;

                break;
            }
        }

        //=========================================================================//
        // If we found a usable sample in the cache, then don't generate a new one //
        //=========================================================================//
        if( bFoundInCache )
        {
            continue;
        }

        CIrradianceCache::IrradianceSample* pSample = new CIrradianceCache::IrradianceSample;
        if( NULL == pSample )
        {
            OUTPUT_ERROR_MESSAGE( L"Ran out of memory!\n" );
            return false;
        }

        pSample->vPosition = pNode->vPosition[sampleIndex];

        if( !( SampleIncidentRadiance( &pSample->vPosition, pSample->pRedCoefs, pSample->pGreenCoefs,
                                       pSample->pBlueCoefs ) ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Unable to sample scene!\n" );
            SAFE_DELETE( pSample );
            return false;
        }

        if( !( SampleDepth( &( pSample->vPosition ), &( pSample->fHMDepth ), NULL, NULL ) ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Unable to sample scene depth!\n" );
            SAFE_DELETE( pSample );
            return false;
        }

        pCache->m_pCache.Add( pSample );
        pNode->dwSampleIndex[sampleIndex] = pCache->m_pCache.IndexOf( pSample );
        pNode->bSampleInCache[sampleIndex] = true;

    }

    if( m_bAdaptiveOctreeSubdivision )
    {
        if( dwDepth < m_dwMinOctreeSubdivision )
        {
            if( !( pCache->m_pOctree->Subdivide( pNode, 1 ) ) )
            {
                OUTPUT_ERROR_MESSAGE( L"Octree subdivision failed!\n" );
                return false;
            }
        }
        else if( dwDepth < m_dwMaxOctreeSubdivision )
        {
            D3DXVECTOR3 vMax = pNode->vPosition[0] - pNode->vPosition[7];
            float fMax = fabsf( vMax.x ) > fabsf( vMax.y ) ? fabsf( vMax.x ) : fabsf( vMax.y );
            fMax = fMax > fabsf( vMax.z ) ? fMax : fabsf( vMax.z );

            bool bSubdivide = false;
            float fHMDepth = 0.0f;
            float fMaxDepth = 0.0f;
            float fMinDepth = 0.0f;

            D3DXVECTOR3 vMid = ( pNode->vPosition[0] + pNode->vPosition[7] ) / 2.0f;

            if( !( SampleDepth( &vMid, &fHMDepth, &fMinDepth, &fMaxDepth ) ) )
            {
                OUTPUT_ERROR_MESSAGE( L"Unable to sample scene depth!\n" );
                return false;
            }

            // Subdivide voxels who's extent is greater than the harmonic mean of scene depth
            if( fHMDepth * m_fHMDepthSubdivThreshold <= ( fMax / 2.0f ) )
            {
                bSubdivide = true;
            }

            // Subdivide voxels that contain geometry
            if( fMinDepth <= ( fMax / 2.0f ) )
            {
                bSubdivide = true;
            }

            if( bSubdivide )
            {
                if( !( pCache->m_pOctree->Subdivide( pNode, 1 ) ) )
                {
                    OUTPUT_ERROR_MESSAGE( L"Octree subdivision failed!\n" );
                    return false;
                }
            }
            else
            {
                DWORD dwEffectiveDepth = ( m_dwMaxOctreeSubdivision - dwDepth );
                float fS = powf( 2.0f, ( float )dwEffectiveDepth + 1 );
                float fS3 = fS * fS * fS;
                float fAdd = ( fS3 - 1.0f ) / 7.0f;

                pCache->m_dwNumSkippedSamples += ( DWORD )( fAdd - 1 );
            }
        }
    }
    else
    {
        if( dwDepth < m_dwMaxOctreeSubdivision )
        {
            if( !( pCache->m_pOctree->Subdivide( pNode, 1 ) ) )
            {
                OUTPUT_ERROR_MESSAGE( L"Octree subdivision failed!\n" );
                return false;
            }
        }
    }

    *pDone = false;
    if( NULL != pPercent )
    {
        if( !( m_bAdaptiveOctreeSubdivision ) )
        {
            float fS = powf( 2.0f, ( float )m_dwMaxOctreeSubdivision + 1 );
            float fS3 = fS * fS * fS;
            *pPercent = 100.0f * ( ( float )pCache->GetNodeCount() ) / ( ( fS3 - 1.0f ) / 7.0f );
        }
        else
        {
            float fS = powf( 2.0f, ( float )m_dwMaxOctreeSubdivision + 1 );
            float fS3 = fS * fS * fS;
            *pPercent = 100.0f * ( ( float )( pCache->GetNodeCount() + pCache->m_dwNumSkippedSamples ) ) /
                ( ( fS3 - 1.0f ) / 7.0f );
        }

    }

    return true;
}

//=========================================================================//
// Free all dynamic resources.  Call this between calls to CreateCache().  //
// This function calls ReleaseGPUResources().                              //
//=========================================================================//
void CIrradianceCacheGenerator::Reset( void )
{
    ReleaseGPUResources();

    m_dwMinOctreeSubdivision = 0;
    m_dwMaxOctreeSubdivision = 0;
    m_bAdaptiveOctreeSubdivision = false;
    m_fHMDepthSubdivThreshold = 1.0f;

    m_vBoundingBoxMin = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    m_vBoundingBoxMax = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );

    m_pD3DDevice = NULL;
    m_pRenderToEnvMap = NULL;
    m_pCubeTexture = NULL;

    m_pSceneMeshes.RemoveAll();

    return;
}

//===============================================================================================//
// Samples incident radiance at a point and projects this into Spherical Harmonics.  Optionally, //
// gradients may be found (using a finite differencing method).                                  //
//===============================================================================================//
bool CIrradianceCacheGenerator::SampleIncidentRadiance( D3DXVECTOR3* pPosition, float* pRed, float* pGreen,
                                                        float* pBlue )
{
    if( ( NULL == pPosition ) || ( NULL == pRed ) || ( NULL == pGreen ) || ( NULL == pBlue ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointers!\n" );
        return false;
    }

    if( ( NULL == m_pD3DDevice ) || ( NULL == m_pRenderToEnvMap ) || ( NULL == m_pCubeTexture ) )
    {
        OUTPUT_ERROR_MESSAGE( L"GPU Resources have not been allocated!\n" );
        return false;
    }

    HRESULT hResult = 0x0;


    hResult = m_pRenderToEnvMap->BeginCube( m_pCubeTexture );
    if( FAILED( hResult ) )
    {
        DXUT_ERR( L"BeginCube() failed!\n", hResult );
        return false;
    }

    for( DWORD faceIndex = 0; faceIndex < 6; faceIndex++ )
    {
        D3DXMATRIX mProj;
        D3DXMatrixPerspectiveFovLH( &mProj, D3DX_PI / 2, 1.0f, 0.001f, 1000.0f );

        // Standard view that will be overridden below
        D3DXVECTOR3 vEnvEyePt = *pPosition;
        D3DXVECTOR3 vLookatPt, vUpVec;

        switch( faceIndex )
        {
            case D3DCUBEMAP_FACE_POSITIVE_X:
                vLookatPt = D3DXVECTOR3( 1.0f, 0.0f, 0.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
                break;
            case D3DCUBEMAP_FACE_NEGATIVE_X:
                vLookatPt = D3DXVECTOR3( -1.0f, 0.0f, 0.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
                break;
            case D3DCUBEMAP_FACE_POSITIVE_Y:
                vLookatPt = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
                break;
            case D3DCUBEMAP_FACE_NEGATIVE_Y:
                vLookatPt = D3DXVECTOR3( 0.0f, -1.0f, 0.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 0.0f, 1.0f );
                break;
            case D3DCUBEMAP_FACE_POSITIVE_Z:
                vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, 1.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
                break;
            case D3DCUBEMAP_FACE_NEGATIVE_Z:
                vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
                break;
        }

        D3DXMATRIX mView;
        D3DXVECTOR3 vDir = ( vEnvEyePt + vLookatPt );
        D3DXMatrixLookAtLH( &mView, &vEnvEyePt, &vDir, &vUpVec );
        D3DXMATRIX mWVP = mView * mProj;

        hResult = m_pRenderToEnvMap->Face( ( D3DCUBEMAP_FACES )faceIndex, D3DX_DEFAULT );
        if( FAILED( hResult ) )
        {
            DXUT_ERR( L"Face() failed!\n", hResult );
            return false;
        }


        hResult = m_pD3DDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f,
                                       0 );
        if( FAILED( hResult ) )
        {
            DXUT_ERR( L"Clear() failed!\n", hResult );
            return false;
        }

        DWORD numMeshes = m_pSceneMeshes.GetSize();
        for( DWORD meshIndex = 0; meshIndex < numMeshes; meshIndex++ )
        {
            m_pSceneMeshes[meshIndex]->RenderRadiance( m_pD3DDevice, &mWVP );
        }

    }

    hResult = m_pRenderToEnvMap->End( D3DX_FILTER_NONE );
    if( FAILED( hResult ) )
    {
        DXUT_ERR( L"End() failed!\n", hResult );
        return false;
    }

    hResult = D3DXSHProjectCubeMap( IRRADIANCE_CACHE_MAX_SH_ORDER, m_pCubeTexture, pRed, pGreen, pBlue );
    if( FAILED( hResult ) )
    {
        DXUT_ERR( L"D3DXSHProjectCubeMap() failed!\n", hResult );
        return false;
    }

    return true;
}

//===================================================================//
// Computes the harmonic mean of scene depth at a point in the scene //
//===================================================================//
bool CIrradianceCacheGenerator::SampleDepth( D3DXVECTOR3* pPosition, float* pHMDepth, float* pMinDepth,
                                             float* pMaxDepth )
{
    if( ( NULL == pPosition ) || ( NULL == pHMDepth ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointers!\n" );
        return false;
    }

    if( ( NULL == m_pD3DDevice ) || ( NULL == m_pRenderToEnvMap ) || ( NULL == m_pCubeTexture ) )
    {
        OUTPUT_ERROR_MESSAGE( L"GPU Resources haven't been set!\n" );
        return false;
    }

    HRESULT hResult = 0x0;

    hResult = m_pRenderToEnvMap->BeginCube( m_pCubeTexture );
    if( FAILED( hResult ) )
    {
        DXUT_ERR( L"BeginCube() failed!\n", hResult );
        return false;
    }

    for( DWORD faceIndex = 0; faceIndex < 6; faceIndex++ )
    {
        D3DXMATRIX mProj;
        D3DXMatrixPerspectiveFovLH( &mProj, D3DX_PI / 2, 1.0f, 0.5f, 1000.0f );

        // Standard view that will be overridden below
        D3DXVECTOR3 vEnvEyePt = *pPosition;
        D3DXVECTOR3 vLookatPt, vUpVec;

        switch( faceIndex )
        {
            case D3DCUBEMAP_FACE_POSITIVE_X:
                vLookatPt = D3DXVECTOR3( 1.0f, 0.0f, 0.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
                break;
            case D3DCUBEMAP_FACE_NEGATIVE_X:
                vLookatPt = D3DXVECTOR3( -1.0f, 0.0f, 0.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
                break;
            case D3DCUBEMAP_FACE_POSITIVE_Y:
                vLookatPt = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
                break;
            case D3DCUBEMAP_FACE_NEGATIVE_Y:
                vLookatPt = D3DXVECTOR3( 0.0f, -1.0f, 0.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 0.0f, 1.0f );
                break;
            case D3DCUBEMAP_FACE_POSITIVE_Z:
                vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, 1.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
                break;
            case D3DCUBEMAP_FACE_NEGATIVE_Z:
                vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
                vUpVec = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
                break;
        }

        D3DXMATRIX mView;
        D3DXVECTOR3 vDir = ( vEnvEyePt + vLookatPt );
        D3DXMatrixLookAtLH( &mView, &vEnvEyePt, &vDir, &vUpVec );
        D3DXMATRIX mWVP = mView * mProj;

        hResult = m_pRenderToEnvMap->Face( ( D3DCUBEMAP_FACES )faceIndex, D3DX_DEFAULT );
        if( FAILED( hResult ) )
        {
            DXUT_ERR( L"Face() failed!\n", hResult );
            return false;
        }


        hResult = m_pD3DDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f,
                                       0 );
        if( FAILED( hResult ) )
        {
            DXUT_ERR( L"Clear() failed!\n", hResult );
            return false;
        }

        DWORD numMeshes = m_pSceneMeshes.GetSize();
        for( DWORD meshIndex = 0; meshIndex < numMeshes; meshIndex++ )
        {
            if( m_bAdaptiveTest[meshIndex] )
            {
                m_pSceneMeshes[meshIndex]->RenderDepth( m_pD3DDevice, &mView, &mWVP );
            }
        }

    }

    hResult = m_pRenderToEnvMap->End( D3DX_FILTER_NONE );
    if( FAILED( hResult ) )
    {
        DXUT_ERR( L"End() failed!\n", hResult );
        return false;
    }

    D3DXRTE_DESC rtDesc;
    hResult = m_pRenderToEnvMap->GetDesc( &rtDesc );
    if( FAILED( hResult ) )
    {
        DXUT_ERR( L"GetDesc() failed!\n", hResult );
        return false;
    }

    float fInverseDepthSum = 0.0f;
    float fMinDepth = FLT_MAX;
    float fMaxDepth = 0.0f;
    float fSolidAngleSum = 0.0f;


    float* pfScanLine = NULL;

    pfScanLine = new float[rtDesc.Size * 4];

    if( !pfScanLine )
    {
        DXUT_ERR( L"Out of memory\n", E_OUTOFMEMORY );
        return false;
    }

    //=============================================================================//
    // Compute the harmonic mean (ignoring the texel's solid angle for the moment) //
    //=============================================================================//
    for( DWORD faceIndex = 0; faceIndex < 6; faceIndex++ )
    {
        IDirect3DSurface9* pSurface = NULL;

        hResult = m_pCubeTexture->GetCubeMapSurface( ( D3DCUBEMAP_FACES )faceIndex, 0, &pSurface );
        if( FAILED( hResult ) )
        {
            DXUT_ERR( L"GetCubeMapSurface() failed!\n", hResult );
            delete[] pfScanLine;
            return false;
        }

        D3DLOCKED_RECT lockedRect;
        hResult = pSurface->LockRect( &lockedRect, NULL, D3DLOCK_READONLY );
        if( FAILED( hResult ) )
        {
            DXUT_ERR( L"LockRect() failed!\n", hResult );
            SAFE_RELEASE( pSurface );
            delete[] pfScanLine;
            return false;
        }

        //===========//
        // Find mean //
        //===========//

        const float fPixSize = 2.0f / rtDesc.Size; // size of a pixel

        for( DWORD y = 0; y < rtDesc.Size; y++ )
        {
            float fY = -1.0f + y * fPixSize + 0.5f * fPixSize;

            // blit a row of texels

            if( rtDesc.Format == D3DFMT_A16B16G16R16F ) // 16 bit
            {
                D3DXFloat16To32Array( pfScanLine, ( ( D3DXFLOAT16* )lockedRect.pBits ) +
                                      ( lockedRect.Pitch / sizeof( D3DXFLOAT16 ) ) * y, rtDesc.Size * 4 );
            }
            else // 32 bit, just do a memcopy
            {
                memcpy( pfScanLine, ( ( char* )lockedRect.pBits ) + lockedRect.Pitch * y, rtDesc.Size * 4 * 4 );
            }

            for( DWORD x = 0; x < rtDesc.Size; x++ )
            {
                float fX = -1.0f + x * fPixSize + 0.5f * fPixSize;

                float fDiffSolid = 2.0f / ( ( fX * fX + fY * fY + 1.0f ) * sqrtf( fX * fX + fY * fY + 1.0f ) );
                fSolidAngleSum += fDiffSolid;

                //========================================//
                // Texture stores 1/depth in all channels //
                //========================================//
                float fInvDepth = pfScanLine[4 * x];
                float fDepth = fInvDepth <= FLT_MIN ? FLT_MAX : 1.0f / fInvDepth;

                fMinDepth = fMinDepth > fDepth ? fDepth : fMinDepth;
                fMaxDepth = fMaxDepth < fDepth ? fDepth : fMaxDepth;

                fInverseDepthSum += fInvDepth * fDiffSolid;
            }
        }

        hResult = pSurface->UnlockRect();
        if( FAILED( hResult ) )
        {
            DXUT_ERR( L"UnlockRect() failed!\n", hResult );
            SAFE_RELEASE( pSurface );
            delete[] pfScanLine;
            return false;
        }

        SAFE_RELEASE( pSurface );
    }

    delete[] pfScanLine;

    ( *pHMDepth ) = fSolidAngleSum / fInverseDepthSum;

    if( NULL != pMinDepth )
    {
        ( *pMinDepth ) = fMinDepth;
    }

    if( NULL != pMaxDepth )
    {
        ( *pMaxDepth ) = fMaxDepth;
    }

    return true;
}





//================================================================================================//
//=== CIrradianceCache:: Irradiance sample cache.  Uses an octree to partition cached samples. ===//
//================================================================================================//




//=============//
// Constructor //
//=============//
CIrradianceCache::CIrradianceCache( D3DXVECTOR3* pBoundBoxMin, D3DXVECTOR3* pBoundBoxMax )
{
    //=============================================================//
    // Create an octree for spatial partitioning of cached samples //
    //=============================================================//
    m_pOctree = new CIrradianceCacheOctree( pBoundBoxMin, pBoundBoxMax );
    assert( m_pOctree );

    m_dwNumSkippedSamples = 0;

    return;
}

//============//
// Destructor //
//============//
CIrradianceCache::~CIrradianceCache( void )
{
    ClearCache();
    SAFE_DELETE( m_pOctree );
    return;
}

//==================================================//
// Clear all the cached samples (flushes the cache) //
//==================================================//
void CIrradianceCache::ClearCache( void )
{
    //===============================//
    // Delete all the cached samples //
    //===============================//
    for( int i = 0; i < m_pCache.GetSize(); i++ )
    {
        SAFE_DELETE( m_pCache[i] );
    }

    //=================//
    // Flush the cache //
    //=================//
    m_pCache.RemoveAll();

    //==================//
    // Empty the Octree //
    //==================//
    CIrradianceCacheOctree::OctreeNode* pRoot = m_pOctree->GetRootNode();
    if( NULL == pRoot )
    {
        return;
    }

    m_pOctree->DeleteChildren( pRoot );

    for( int i = 0; i < 8; i++ )
    {
        pRoot->bSampleInCache[i] = false;
        pRoot->dwSampleIndex[i] = 0;
    }

    m_dwNumSkippedSamples = 0;

    return;
}

//=========================================================================================//
// Samples volume with trilinear filtering.  Returns false if you try to sample outside of //
// the volume's bounds or if the volume hasn't been filled yet.                            //
//=========================================================================================//
bool CIrradianceCache::SampleTrilinear( D3DXVECTOR3* pPosition, CIrradianceCache::IrradianceSample* pSample,
                                        D3DXVECTOR3 pBox[8] )
{
    if( ( NULL == pPosition ) || ( NULL == pSample ) || ( NULL == m_pOctree ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointers!\n" );
        return false;
    }

    CIrradianceCacheOctree::OctreeNode* pNode = m_pOctree->FindEnclosingNode( pPosition, m_pOctree->GetRootNode() );
    if( NULL == pNode )
    {
        return false;
    }

    float xWeight = ( pPosition->x - pNode->vPosition[0].x ) / ( pNode->vPosition[7].x - pNode->vPosition[0].x );
    float yWeight = ( pPosition->y - pNode->vPosition[0].y ) / ( pNode->vPosition[7].y - pNode->vPosition[0].y );
    float zWeight = ( pPosition->z - pNode->vPosition[0].z ) / ( pNode->vPosition[7].z - pNode->vPosition[0].z );

    CIrradianceCache::IrradianceSample* pSampleArray[8] = { NULL };
    for( int i = 0; i < 8; i++ )
    {
        pBox[i] = pNode->vPosition[i];

        pSampleArray[i] = m_pCache[pNode->dwSampleIndex[i]];
        if( NULL == pSampleArray[i] )
        {
            OUTPUT_ERROR_MESSAGE( L"Sample node is NULL!\n" );
            return false;
        }
    }


    for( int i = 0; i < IRRADIANCE_CACHE_MAX_SH_COEF; i++ )
    {
        pSample->pRedCoefs[i] = pSampleArray[0]->pRedCoefs[i] * ( 1.0f - xWeight ) * ( 1.0f - yWeight ) *
            ( 1.0f - zWeight );
        pSample->pRedCoefs[i] += pSampleArray[1]->pRedCoefs[i] * ( 1.0f - xWeight ) * ( 1.0f - yWeight ) * ( zWeight );
        pSample->pRedCoefs[i] += pSampleArray[2]->pRedCoefs[i] * ( 1.0f - xWeight ) * ( yWeight )*( 1.0f - zWeight );
        pSample->pRedCoefs[i] += pSampleArray[3]->pRedCoefs[i] * ( 1.0f - xWeight ) * ( yWeight )*( zWeight );
        pSample->pRedCoefs[i] += pSampleArray[4]->pRedCoefs[i] * ( xWeight )*( 1.0f - yWeight ) * ( 1.0f - zWeight );
        pSample->pRedCoefs[i] += pSampleArray[5]->pRedCoefs[i] * ( xWeight )*( 1.0f - yWeight ) * ( zWeight );
        pSample->pRedCoefs[i] += pSampleArray[6]->pRedCoefs[i] * ( xWeight )*( yWeight )*( 1.0f - zWeight );
        pSample->pRedCoefs[i] += pSampleArray[7]->pRedCoefs[i] * ( xWeight )*( yWeight )*( zWeight );

        pSample->pGreenCoefs[i] = pSampleArray[0]->pGreenCoefs[i] * ( 1.0f - xWeight ) * ( 1.0f - yWeight ) *
            ( 1.0f - zWeight );
        pSample->pGreenCoefs[i] += pSampleArray[1]->pGreenCoefs[i] * ( 1.0f - xWeight ) * ( 1.0f - yWeight ) *
            ( zWeight );
        pSample->pGreenCoefs[i] += pSampleArray[2]->pGreenCoefs[i] *
            ( 1.0f - xWeight ) * ( yWeight )*( 1.0f - zWeight );
        pSample->pGreenCoefs[i] += pSampleArray[3]->pGreenCoefs[i] * ( 1.0f - xWeight ) * ( yWeight )*( zWeight );
        pSample->pGreenCoefs[i] += pSampleArray[4]->pGreenCoefs[i] * ( xWeight )*( 1.0f - yWeight ) *
            ( 1.0f - zWeight );
        pSample->pGreenCoefs[i] += pSampleArray[5]->pGreenCoefs[i] * ( xWeight )*( 1.0f - yWeight ) * ( zWeight );
        pSample->pGreenCoefs[i] += pSampleArray[6]->pGreenCoefs[i] * ( xWeight )*( yWeight )*( 1.0f - zWeight );
        pSample->pGreenCoefs[i] += pSampleArray[7]->pGreenCoefs[i] * ( xWeight )*( yWeight )*( zWeight );

        pSample->pBlueCoefs[i] = pSampleArray[0]->pBlueCoefs[i] * ( 1.0f - xWeight ) * ( 1.0f - yWeight ) *
            ( 1.0f - zWeight );
        pSample->pBlueCoefs[i] += pSampleArray[1]->pBlueCoefs[i] * ( 1.0f - xWeight ) * ( 1.0f - yWeight ) *
            ( zWeight );
        pSample->pBlueCoefs[i] += pSampleArray[2]->pBlueCoefs[i] * ( 1.0f - xWeight ) * ( yWeight )*( 1.0f - zWeight );
        pSample->pBlueCoefs[i] += pSampleArray[3]->pBlueCoefs[i] * ( 1.0f - xWeight ) * ( yWeight )*( zWeight );
        pSample->pBlueCoefs[i] += pSampleArray[4]->pBlueCoefs[i] * ( xWeight )*( 1.0f - yWeight ) * ( 1.0f - zWeight );
        pSample->pBlueCoefs[i] += pSampleArray[5]->pBlueCoefs[i] * ( xWeight )*( 1.0f - yWeight ) * ( zWeight );
        pSample->pBlueCoefs[i] += pSampleArray[6]->pBlueCoefs[i] * ( xWeight )*( yWeight )*( 1.0f - zWeight );
        pSample->pBlueCoefs[i] += pSampleArray[7]->pBlueCoefs[i] * ( xWeight )*( yWeight )*( zWeight );

    }

    return true;
}

//=========================================================================//
// Returns the number of nodes or voxels in the octree.  This can be used  //
// to create a correctly sized array for passing to CreateOctreeLineList() //
//=========================================================================//
DWORD CIrradianceCache::GetNodeCount( void )
{
    if( NULL == m_pOctree )
    {
        return 0;
    }

    return m_pOctree->GetNodeCount( NULL );
}

//===========================================================================================//
// Fills an array of D3DXVECTOR3 with line primative vertices.  This array should be         //
// pre-allocated with NumNodes*24 elements.  NumNodes can be found by calling GetNodeCount() //
// This array may be used for visualizing the Octree hierarchy by passing the array to       //
// ID3DXLine::DrawTransform().  This draw function should be called NumNodes times.  Each    //
// call should advance the pointer by a stride of 24 elements since each cube is drawn with  //
// with a 24 point line list (this is inefficient but that's the problem with drawing cubes  //
// using line lists).                                                                        //
//===========================================================================================//
bool CIrradianceCache::CreateOctreeLineList( D3DXVECTOR3* pVertexList )
{
    if( NULL == pVertexList )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return false;
    }

    //================================//
    // Get the number of octree nodes //
    //================================//
    DWORD numBoxes = GetNodeCount();
    if( numBoxes == 0 )
        return false;

    //=======================================//
    // Get the positions for all these nodes //
    //=======================================//
    D3DXVECTOR3* pPositions = new D3DXVECTOR3 [numBoxes * 8];
    if( NULL == pPositions )
    {
        OUTPUT_ERROR_MESSAGE( L"Ran out of memory!\n" );
        return false;
    }

    if( !( m_pOctree->GetNodePositions( m_pOctree->GetRootNode(), pPositions ) ) )
    {
        OUTPUT_ERROR_MESSAGE( L"GetNodePositions() failed!\n" );
        SAFE_DELETE_ARRAY( pPositions );
        return false;
    }


    for( DWORD i = 0; i < numBoxes; i++ )
    {
        DWORD destOffset = i * 24;

        pVertexList[destOffset++] = pPositions[i * 8 + 0];
        pVertexList[destOffset++] = pPositions[i * 8 + 4];
        pVertexList[destOffset++] = pPositions[i * 8 + 6];
        pVertexList[destOffset++] = pPositions[i * 8 + 2];
        pVertexList[destOffset++] = pPositions[i * 8 + 0];
        pVertexList[destOffset++] = pPositions[i * 8 + 1];
        pVertexList[destOffset++] = pPositions[i * 8 + 3];
        pVertexList[destOffset++] = pPositions[i * 8 + 2];
        pVertexList[destOffset++] = pPositions[i * 8 + 6];
        pVertexList[destOffset++] = pPositions[i * 8 + 7];
        pVertexList[destOffset++] = pPositions[i * 8 + 5];
        pVertexList[destOffset++] = pPositions[i * 8 + 1];
        pVertexList[destOffset++] = pPositions[i * 8 + 3];
        pVertexList[destOffset++] = pPositions[i * 8 + 7];
        pVertexList[destOffset++] = pPositions[i * 8 + 5];
        pVertexList[destOffset++] = pPositions[i * 8 + 4];

        pVertexList[destOffset++] = pPositions[i * 8 + 0];
        pVertexList[destOffset++] = pPositions[i * 8 + 2];
        pVertexList[destOffset++] = pPositions[i * 8 + 4];
        pVertexList[destOffset++] = pPositions[i * 8 + 6];
        pVertexList[destOffset++] = pPositions[i * 8 + 5];
        pVertexList[destOffset++] = pPositions[i * 8 + 7];
        pVertexList[destOffset++] = pPositions[i * 8 + 1];
        pVertexList[destOffset++] = pPositions[i * 8 + 3];
    }

    SAFE_DELETE_ARRAY( pPositions );

    return true;
}

//====================//
// Save cache to disk //
//====================//
bool RecursiveOctreeWrite( HANDLE pFile, CIrradianceCacheOctree::OctreeNode* pNode )
{
    if( ( NULL == pFile ) || ( NULL == pNode ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointers!\n" );
        return false;
    }

    DWORD dwWritten;
    if( 0 == WriteFile( pFile, &( pNode->bHasChildren ), sizeof( bool ) * 1, &dwWritten, NULL ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
        return false;
    }

    if( 0 == WriteFile( pFile, pNode->bSampleInCache, sizeof( bool ) * 8, &dwWritten, NULL ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
        return false;
    }

    if( 0 == WriteFile( pFile, pNode->dwSampleIndex, sizeof( DWORD ) * 8, &dwWritten, NULL ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
        return false;
    }

    for( DWORD index = 0; index < 8; index++ )
    {
        if( 0 == WriteFile( pFile, ( float* )( pNode->vPosition[index] ), sizeof( float ) * 3, &dwWritten, NULL ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
            return false;
        }
    }

    if( pNode->bHasChildren )
    {
        for( DWORD index = 0; index < 8; index++ )
        {
            if( !( RecursiveOctreeWrite( pFile, pNode->pChildren[index] ) ) )
            {
                OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
                return false;
            }
        }
    }

    return true;
}

bool CIrradianceCache::SaveCache( WCHAR* strFileName )
{
    if( NULL == strFileName )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return false;
    }

    //=======================//
    // Open file for writing //
    //=======================//
    DWORD dwWritten;
    HANDLE pFile = CreateFile( strFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( INVALID_HANDLE_VALUE == pFile )
    {
        OUTPUT_ERROR_MESSAGE( L"Unable to open file for saving!\n" );
        return false;
    }

    //===========================//
    // Write file format version //
    //===========================//
    if( 0 == WriteFile( pFile, IRRADIANCECACHE_FILE_VERSION_STRING, sizeof( IRRADIANCECACHE_FILE_VERSION_STRING ),
                        &dwWritten, NULL ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
        CloseHandle( pFile );
        return false;
    }

    //=================//
    // Write the cache //
    //=================//
    DWORD dwCacheSize = ( DWORD )m_pCache.GetSize();
    if( 0 == WriteFile( pFile, &dwCacheSize, sizeof( DWORD ), &dwWritten, NULL ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
        CloseHandle( pFile );
        return false;
    }

    for( DWORD index = 0; index < dwCacheSize; index++ )
    {
        if( 0 == WriteFile( pFile, ( float* )( m_pCache[index]->vPosition ), sizeof( float ) * 3, &dwWritten, NULL ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
            CloseHandle( pFile );
            return false;
        }

        if( 0 == WriteFile( pFile, m_pCache[index]->pRedCoefs, sizeof( float ) * IRRADIANCE_CACHE_MAX_SH_COEF,
                            &dwWritten, NULL ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
            CloseHandle( pFile );
            return false;
        }

        if( 0 == WriteFile( pFile, m_pCache[index]->pGreenCoefs, sizeof( float ) * IRRADIANCE_CACHE_MAX_SH_COEF,
                            &dwWritten, NULL ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
            CloseHandle( pFile );
            return false;
        }

        if( 0 == WriteFile( pFile, m_pCache[index]->pBlueCoefs, sizeof( float ) * IRRADIANCE_CACHE_MAX_SH_COEF,
                            &dwWritten, NULL ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
            CloseHandle( pFile );
            return false;
        }
    }

    if( !( RecursiveOctreeWrite( pFile, m_pOctree->GetRootNode() ) ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Write failed!\n" );
        CloseHandle( pFile );
        return false;
    }

    CloseHandle( pFile );
    return true;
}

//======================//
// Load cache from disk //
//======================//
bool RecursiveOctreeRead( HANDLE pFile, CIrradianceCacheOctree* pOctree, CIrradianceCacheOctree::OctreeNode* pNode )
{
    if( ( NULL == pFile ) || ( NULL == pNode ) || ( NULL == pOctree ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return false;
    }

    DWORD dwRead;
    if( 0 == ReadFile( pFile, &( pNode->bHasChildren ), sizeof( bool ) * 1, &dwRead, NULL ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
        return false;
    }

    if( 0 == ReadFile( pFile, pNode->bSampleInCache, sizeof( bool ) * 8, &dwRead, NULL ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
        return false;
    }

    if( 0 == ReadFile( pFile, pNode->dwSampleIndex, sizeof( DWORD ) * 8, &dwRead, NULL ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
        return false;
    }

    for( DWORD index = 0; index < 8; index++ )
    {
        if( 0 == ReadFile( pFile, ( float* )( pNode->vPosition[index] ), sizeof( float ) * 3, &dwRead, NULL ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
            return false;
        }
    }

    if( pNode->bHasChildren )
    {
        //=======================================================//
        // We have to do this so that AddChildNodes doesn't fail //
        //=======================================================//
        pNode->bHasChildren = false;

        if( !( pOctree->AddChildNodes( pNode ) ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Unable to create child nodes!\n" );
            return false;
        }

        for( DWORD index = 0; index < 8; index++ )
        {
            if( !( RecursiveOctreeRead( pFile, pOctree, pNode->pChildren[index] ) ) )
            {
                OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
                return false;
            }
        }
    }

    return true;
}
bool CIrradianceCache::LoadCache( WCHAR* strFileName )
{
    if( NULL == strFileName )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return false;
    }

    ClearCache();

    //=======================//
    // Open file for reading //
    //=======================//
    HANDLE pFile = CreateFile( strFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    if( INVALID_HANDLE_VALUE == pFile )
    {
        OUTPUT_ERROR_MESSAGE( L"Unable to open file for reading!\n" );
        return false;
    }

    //=========================//
    // Read the version string //
    //=========================//
    WCHAR strFileString[] = {IRRADIANCECACHE_FILE_VERSION_STRING};

    DWORD dwRead;
    if( 0 == ReadFile( pFile, strFileString, sizeof( IRRADIANCECACHE_FILE_VERSION_STRING ) * 1, &dwRead, NULL ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
        CloseHandle( pFile );
        return false;
    }

    //=============================//
    // Compare the version strings //
    //=============================//
    if( 0 != wcsncmp( strFileString,
                      IRRADIANCECACHE_FILE_VERSION_STRING, sizeof( IRRADIANCECACHE_FILE_VERSION_STRING ) ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Version doesn't match!\n" );
        CloseHandle( pFile );
        return false;
    }

    //================//
    // Read the cache //
    //================//
    DWORD dwCacheSize = 0;
    if( 0 == ReadFile( pFile, &dwCacheSize, sizeof( DWORD ) * 1, &dwRead, NULL ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
        CloseHandle( pFile );
        return false;
    }

    for( DWORD index = 0; index < dwCacheSize; index++ )
    {
        CIrradianceCache::IrradianceSample* pSample = new CIrradianceCache::IrradianceSample;
        if( NULL == pSample )
        {
            OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
            CloseHandle( pFile );
            return false;
        }

        if( 0 == ReadFile( pFile, ( float* )( pSample->vPosition ), sizeof( float ) * 3, &dwRead, NULL ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
            CloseHandle( pFile );
            return false;
        }

        if( 0 == ReadFile( pFile, pSample->pRedCoefs, sizeof( float ) * IRRADIANCE_CACHE_MAX_SH_COEF, &dwRead, NULL ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
            CloseHandle( pFile );
            return false;
        }

        if( 0 == ReadFile( pFile, pSample->pGreenCoefs, sizeof( float ) * IRRADIANCE_CACHE_MAX_SH_COEF, &dwRead,
                           NULL ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
            CloseHandle( pFile );
            return false;
        }

        if( 0 == ReadFile( pFile, pSample->pBlueCoefs, sizeof( float ) * IRRADIANCE_CACHE_MAX_SH_COEF, &dwRead,
                           NULL ) )
        {
            OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
            CloseHandle( pFile );
            return false;
        }

        HRESULT hResult = m_pCache.Add( pSample );
        if( FAILED( hResult ) )
        {
            DXUT_ERR( L"Unable to add sample to cache array!\n", hResult );
            return false;
        }
    }

    if( !( RecursiveOctreeRead( pFile, m_pOctree, m_pOctree->GetRootNode() ) ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Read failed!\n" );
        CloseHandle( pFile );
        return false;
    }

    CloseHandle( pFile );

    return true;
}





//==============================================================================================//
//=== CIrradianceCacheOctree:: Octree for spatial partitioning of cached irradiance samples. ===//
//==============================================================================================//




//=============//
// Constructor //
//=============//
CIrradianceCacheOctree::CIrradianceCacheOctree( D3DXVECTOR3* pBoundBoxMin, D3DXVECTOR3* pBoundBoxMax )
{
    //==========================//
    // Initialize the root node //
    //==========================//
    for( int i = 0; i < 8; i++ )
    {
        m_root.pChildren[i] = NULL;
        m_root.bSampleInCache[i] = false;
        m_root.dwSampleIndex[i] = 0;
    }

    SetNodeBounds( &m_root, pBoundBoxMin, pBoundBoxMax );

    m_dwNodeCount = 1;

    return;
}

//============//
// Destructor //
//============//
CIrradianceCacheOctree::~CIrradianceCacheOctree( void )
{
    DeleteChildren( &m_root );
    return;
}

//===========================================================//
// Return the Octree node that encloses the specified point. //
//===========================================================//
CIrradianceCacheOctree::OctreeNode* CIrradianceCacheOctree::FindEnclosingNode( D3DXVECTOR3* pPosition,
                                                                               CIrradianceCacheOctree::OctreeNode*
                                                                               pNode )
{
    if( ( NULL == pPosition ) || ( NULL == pNode ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return NULL;
    }

    //======================================//
    // See if the point is inside this node //
    //======================================//
    if( ( ( pPosition->x >= pNode->vPosition[0].x ) && ( pPosition->y >= pNode->vPosition[0].y ) &&
          ( pPosition->z >= pNode->vPosition[0].z ) ) &&
        ( ( pPosition->x <= pNode->vPosition[7].x ) && ( pPosition->y <= pNode->vPosition[7].y ) &&
          ( pPosition->z <= pNode->vPosition[7].z ) ) )
    {
        if( pNode->bHasChildren )
        {
            if( pPosition->z >= ( ( pNode->vPosition[0].z + pNode->vPosition[7].z ) / 2.0f ) )
            {
                // octant is: 1, 3, 5, or 7

                if( pPosition->y >= ( ( pNode->vPosition[0].y + pNode->vPosition[7].y ) / 2.0f ) )
                {
                    // octant is: 3 or 7

                    if( pPosition->x >= ( ( pNode->vPosition[0].x + pNode->vPosition[7].x ) / 2.0f ) )
                    {
                        return FindEnclosingNode( pPosition, pNode->pChildren[7] );
                    }
                    else
                    {
                        return FindEnclosingNode( pPosition, pNode->pChildren[3] );
                    }
                }
                else
                {
                    // octant is: 1 or 5

                    if( pPosition->x >= ( ( pNode->vPosition[0].x + pNode->vPosition[7].x ) / 2.0f ) )
                    {
                        return FindEnclosingNode( pPosition, pNode->pChildren[5] );
                    }
                    else
                    {
                        return FindEnclosingNode( pPosition, pNode->pChildren[1] );
                    }
                }
            }
            else
            {
                // octant is: 0, 2, 4, or 6

                if( pPosition->y >= ( ( pNode->vPosition[0].y + pNode->vPosition[7].y ) / 2.0f ) )
                {
                    // octant is: 2 or 6

                    if( pPosition->x >= ( ( pNode->vPosition[0].x + pNode->vPosition[7].x ) / 2.0f ) )
                    {
                        return FindEnclosingNode( pPosition, pNode->pChildren[6] );
                    }
                    else
                    {
                        return FindEnclosingNode( pPosition, pNode->pChildren[2] );
                    }
                }
                else
                {
                    // octant is: 0 or 4

                    if( pPosition->x >= ( ( pNode->vPosition[0].x + pNode->vPosition[7].x ) / 2.0f ) )
                    {
                        return FindEnclosingNode( pPosition, pNode->pChildren[4] );
                    }
                    else
                    {
                        return FindEnclosingNode( pPosition, pNode->pChildren[0] );
                    }
                }
            }

        }
        else
        {
            return pNode;
        }
    }

    return NULL;

}

//=====================================================//
// Returns the number of nodes or voxels in the octree //
// starting with the node passed in.  For example, if  //
// only the root node exists, the cound with be 1.     //
//=====================================================//
DWORD CIrradianceCacheOctree::GetNodeCount( CIrradianceCacheOctree::OctreeNode* pNode )
{
    if( NULL == pNode )
    {
        return m_dwNodeCount;
    }

    DWORD count = 1; // for this node

    if( pNode->bHasChildren )
    {
        for( int i = 0; i < 8; i++ )
        {
            count += GetNodeCount( pNode->pChildren[i] );
        }
    }

    return count;
}

//==============================================================================================//
// Fills a pre-allocated array with the positions of all voxel vertices in the tree starting at //
// pNode and traversing in a depth first manner.  pPositions should point to an array that is   //
// big enough to store (GetNodeCount(pNode) * 8) entries (8 vertices for each voxel corner).    //
// Returns the number of positions that were placed in the array.                               //
//==============================================================================================//
DWORD CIrradianceCacheOctree::GetNodePositions( CIrradianceCacheOctree::OctreeNode* pNode,
                                                D3DXVECTOR3* pPositions ) const
{
    if( NULL == pNode )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return false;
    }

    DWORD numAdded = 0;

    for( int i = 0; i < 8; i++ )
    {
        pPositions[i] = pNode->vPosition[i];
    }

    numAdded += 8;

    if( pNode->bHasChildren )
    {
        for( int i = 0; i < 8; i++ )
        {
            numAdded += GetNodePositions( pNode->pChildren[i], pPositions + numAdded );
        }
    }

    return numAdded;
}

//================================================//
// Creates and attaches 8 children for the parent //
//================================================//
bool CIrradianceCacheOctree::AddChildNodes( CIrradianceCacheOctree::OctreeNode* pParentNode )
{
    if( NULL == pParentNode )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return false;
    }

    //====================================================//
    // Make sure this node doesn't already have children! //
    //====================================================//
    if( pParentNode->bHasChildren )
    {
        OUTPUT_ERROR_MESSAGE( L"Can't add children to a node that has children!\n" );
        return false;
    }

    //=====================//
    // Create the children //
    //=====================//
    for( int i = 0; i < 8; i++ )
    {
        pParentNode->pChildren[i] = new OctreeNode;

        //===========================================================//
        // If creation failed, kill all children and return an error //
        //===========================================================//
        if( NULL == pParentNode->pChildren[i] )
        {
            OUTPUT_ERROR_MESSAGE( L"Ran out of memory!\n" );
            DeleteChildren( pParentNode );
            return false;
        }
    }

    pParentNode->bHasChildren = true;

    D3DXVECTOR3 vMin0 = D3DXVECTOR3( pParentNode->vPosition[0] );
    D3DXVECTOR3 vMax0 = D3DXVECTOR3( 0.5f * ( pParentNode->vPosition[0] + pParentNode->vPosition[7] ) );
    SetNodeBounds( pParentNode->pChildren[0], &vMin0, &vMax0 );

    D3DXVECTOR3 vMin7 = D3DXVECTOR3( pParentNode->pChildren[0]->vPosition[7] );
    D3DXVECTOR3 vMax7 = D3DXVECTOR3( pParentNode->vPosition[7] );
    SetNodeBounds( pParentNode->pChildren[7], &vMin7, &vMax7 );

    D3DXVECTOR3 vMin1 = D3DXVECTOR3( pParentNode->pChildren[0]->vPosition[1] );
    D3DXVECTOR3 vMax1 = D3DXVECTOR3( pParentNode->pChildren[7]->vPosition[1] );
    SetNodeBounds( pParentNode->pChildren[1], &vMin1, &vMax1 );

    D3DXVECTOR3 vMin2 = D3DXVECTOR3( pParentNode->pChildren[0]->vPosition[2] );
    D3DXVECTOR3 vMax2 = D3DXVECTOR3( pParentNode->pChildren[7]->vPosition[2] );
    SetNodeBounds( pParentNode->pChildren[2], &vMin2, &vMax2 );

    D3DXVECTOR3 vMin3 = D3DXVECTOR3( pParentNode->pChildren[0]->vPosition[3] );
    D3DXVECTOR3 vMax3 = D3DXVECTOR3( pParentNode->pChildren[7]->vPosition[3] );
    SetNodeBounds( pParentNode->pChildren[3], &vMin3, &vMax3 );

    D3DXVECTOR3 vMin4 = D3DXVECTOR3( pParentNode->pChildren[0]->vPosition[4] );
    D3DXVECTOR3 vMax4 = D3DXVECTOR3( pParentNode->pChildren[7]->vPosition[4] );
    SetNodeBounds( pParentNode->pChildren[4], &vMin4, &vMax4 );

    D3DXVECTOR3 vMin5 = D3DXVECTOR3( pParentNode->pChildren[0]->vPosition[5] );
    D3DXVECTOR3 vMax5 = D3DXVECTOR3( pParentNode->pChildren[7]->vPosition[5] );
    SetNodeBounds( pParentNode->pChildren[5], &vMin5, &vMax5 );

    D3DXVECTOR3 vMin6 = D3DXVECTOR3( pParentNode->pChildren[0]->vPosition[6] );
    D3DXVECTOR3 vMax6 = D3DXVECTOR3( pParentNode->pChildren[7]->vPosition[6] );
    SetNodeBounds( pParentNode->pChildren[6], &vMin6, &vMax6 );

    m_dwNodeCount += 8;

    return true;
}

//====================================================//
// Deletes a node's children (and grandchildren, etc) //
//====================================================//
bool CIrradianceCacheOctree::DeleteChildren( CIrradianceCacheOctree::OctreeNode* pNode )
{
    if( NULL == pNode )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return false;
    }

    if( !pNode->bHasChildren )
    {
        return true;
    }

    //=====================//
    // Delete the children //
    //=====================//
    for( int i = 0; i < 8; i++ )
    {
        //=======================================================================//
        // Grandchildren (and their children, etc) are deleted in the destructor //
        //=======================================================================//
        DeleteChildren( pNode->pChildren[i] );
    }

    m_dwNodeCount -= 8;

    pNode->bHasChildren = false;

    return true;
}

//==========================================================================================//
// Subdivides the volume starting at pNode. Subdivision continues for dwSubdivisions levels //
//==========================================================================================//
bool CIrradianceCacheOctree::Subdivide( CIrradianceCacheOctree::OctreeNode* pNode, DWORD dwSubdivisions )
{
    if( NULL == pNode )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return false;
    }

    if( 0 >= dwSubdivisions )
    {
        DeleteChildren( pNode );
        return true;
    }

    if( !( pNode->bHasChildren ) )
    {
        if( !( AddChildNodes( pNode ) ) )
        {
            OUTPUT_ERROR_MESSAGE( L"AddChildNodes() failed!\n" );
            return false;
        }
    }

    for( DWORD i = 0; i < 8; i++ )
    {
        if( !( Subdivide( pNode->pChildren[i], dwSubdivisions - 1 ) ) )
        {
            return false;
        }
    }

    return true;
}

//==================================================================================//
// Depth first search for the first cache miss (node that hasn't been sampled yet). //
// Returns NULL if nothing can be found.                                            //
//==================================================================================//
CIrradianceCacheOctree::OctreeNode* CIrradianceCacheOctree::FindUnsampledNode( CIrradianceCacheOctree::OctreeNode*
                                                                               pNode, DWORD* pDepth )
{
    if( NULL == pNode )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return NULL;
    }

    for( int i = 0; i < 8; i++ )
    {
        if( !( pNode->bSampleInCache[i] ) )
        {
            return pNode;
        }
    }

    if( pNode->bHasChildren )
    {
        CIrradianceCacheOctree::OctreeNode* pNodeTemp = NULL;

        for( int i = 0; i < 8; i++ )
        {
            pNodeTemp = FindUnsampledNode( pNode->pChildren[i], pDepth );
            if( NULL != pNodeTemp )
            {
                *pDepth = *pDepth + 1;
                return pNodeTemp;
            }
        }
    }

    return NULL;
}

//=============================================//
// Set node's vertices at bounding box corners //
//=============================================//
bool CIrradianceCacheOctree::SetNodeBounds( CIrradianceCacheOctree::OctreeNode* pNode, D3DXVECTOR3* pBoundBoxMin,
                                            D3DXVECTOR3* pBoundBoxMax )
{
    if( ( NULL == pNode ) || ( NULL == pBoundBoxMin ) || ( NULL == pBoundBoxMax ) )
    {
        OUTPUT_ERROR_MESSAGE( L"Received NULL pointer!\n" );
        return false;
    }

    //=============================================================//
    // You can't modify a node's bounds once it's been subdivided! //
    //=============================================================//
    if( pNode->bHasChildren )
    {
        OUTPUT_ERROR_MESSAGE( L"You can't modify a node's bounds once it's been subdivided!\n" );
        return false;
    }

    pNode->vPosition[0] = D3DXVECTOR3( pBoundBoxMin->x, pBoundBoxMin->y, pBoundBoxMin->z );
    pNode->vPosition[1] = D3DXVECTOR3( pBoundBoxMin->x, pBoundBoxMin->y, pBoundBoxMax->z );
    pNode->vPosition[2] = D3DXVECTOR3( pBoundBoxMin->x, pBoundBoxMax->y, pBoundBoxMin->z );
    pNode->vPosition[3] = D3DXVECTOR3( pBoundBoxMin->x, pBoundBoxMax->y, pBoundBoxMax->z );
    pNode->vPosition[4] = D3DXVECTOR3( pBoundBoxMax->x, pBoundBoxMin->y, pBoundBoxMin->z );
    pNode->vPosition[5] = D3DXVECTOR3( pBoundBoxMax->x, pBoundBoxMin->y, pBoundBoxMax->z );
    pNode->vPosition[6] = D3DXVECTOR3( pBoundBoxMax->x, pBoundBoxMax->y, pBoundBoxMin->z );
    pNode->vPosition[7] = D3DXVECTOR3( pBoundBoxMax->x, pBoundBoxMax->y, pBoundBoxMax->z );

    return true;
}

