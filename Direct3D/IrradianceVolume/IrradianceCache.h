//--------------------------------------------------------------------------------------
// File: IrradianceCache.cpp
// Copyright ATI Research, Inc. 
//--------------------------------------------------------------------------------------
#pragma once

#include "SceneMesh.h"

#define IRRADIANCE_CACHE_MAX_SH_ORDER 6
#define IRRADIANCE_CACHE_MAX_SH_COEF 36

class CIrradianceCache;
class CIrradianceCacheOctree;
class CIrradianceCacheGenerator;

//========================================================================//
//=== CIrradianceCacheGenerator:: Generates/Fills an irradiance cache. ===//
//========================================================================//
class CIrradianceCacheGenerator
{
public :

    //=======================//
    // Contructor/Destructor //
    //=======================//
            CIrradianceCacheGenerator( void );
            ~CIrradianceCacheGenerator( void );

    //=========================================================================================//
    // Add a scene mesh to the list of meshes from which radiance/irradiance will be captured. //
    // No safety checks are implemented to ensure that you don't add the same SceneMesh more   //
    // than once, if you do this then the mesh will end up getting drawn twice.                //
    //=========================================================================================//
    bool    AddSceneMesh( CSceneMesh* pSceneMesh, bool bUseForAdaptiveTest = true );

    //=========================================================//
    // Set how the scene's radiance/irradiance will be sampled //
    //=========================================================//
    bool    SetSamplingInfo( DWORD dwMaxSubdivision, bool bAdaptiveSubdivision, DWORD dwMinSubdivision,
                             float fHMDepthSubdivThreshold );

    //======================================================================================================//
    // Pass pointers to the GPU resources that should be used for capturing the scene's radiance/irradiance //
    // These pointers must remain valid until FillCache() finishes!  This function will increment the ref   //
    // count.  Ref count is decremented by ReleaseGPUResources() or ~CIrradianceCacheGenerator() which ever //
    // happens first.  The cubemap must be D3DFMT_A32B32G32R32F.                                            //
    //======================================================================================================//
    bool    SetGPUResources( IDirect3DDevice9* pD3DDevice, ID3DXRenderToEnvMap* pRenderToEnvMap,
                             IDirect3DCubeTexture9* pCubeTexture );

    //====================================================================================================================//
    // Decrement ref count of GPU resources and set internal pointers to these resources to NULL.  If you don't call this //
    // then it will happen during destruction (if necessary).                                                             //
    //====================================================================================================================//
    bool    ReleaseGPUResources( void );

    //===========================================================================================================//
    // Calculate the bounding box for all meshes in the scene:                                                   //
    // pMin: Pointer to a D3DXVECTOR3 structure, describing the returned lower-left corner of the bounding box.  //
    // pMax: Pointer to a D3DXVECTOR3 structure, describing the returned upper-right corner of the bounding box. //
    //===========================================================================================================//
    bool    GetSceneBounds( D3DXVECTOR3* pMin, D3DXVECTOR3* pMax );

    //==============================================================================================================//
    // Creates a cache and optionally fills it.  While the cache is being filled, you may not use the GPU resources //
    // that were passed to SetGPUResources.  This function may take a long time to complete.  If you need to use    //
    // the GPU resources to continue updating a window's UI or if you wish to display progress information to the   //
    // user, set bFillCache to false and use the ProgressiveCacheFill to fill the cache.                            //
    //==============================================================================================================//
    bool    CreateCache( CIrradianceCache** ppCache, bool bFillCache );

    //============================//
    // Create a cache from a file //
    //============================//
    bool    CreateCache( WCHAR* strFileName, CIrradianceCache** ppCache );

    //======================================================================================================//
    // Progressively fill cache.  This function should be called iteratively until *pDone is true (or until //
    // the function returns an error.  This allows you to continue using the GPU resources between calls to //
    // this function (to continue updating a window's UI for example or to display progress information).   //
    //======================================================================================================//
    bool    ProgressiveCacheFill( CIrradianceCache* pCache, bool* pDone, float* pPercent );

    //=========================================================================//
    // Free all dynamic resources.  Call this between calls to CreateCache().  //
    // This function calls ReleaseGPUResources().                              //
    //=========================================================================//
    void    Reset( void );

protected :

    //======================================//
    // Maximum level for octree subdivision //
    //======================================//
    DWORD m_dwMaxOctreeSubdivision;

    //===============================================================================//
    // Minimum level for octree subdivision (use if Adaptive subdivision is enabled) //
    //===============================================================================//
    DWORD m_dwMinOctreeSubdivision;

    //========================================================================================//
    // If adaptive octree subdivision is enabled, this threshold controls how sensitive the   //
    // subdivision test should be with respect to the Harmonic Mean of scene depth in a voxel //
    //========================================================================================//
    float m_fHMDepthSubdivThreshold;

    //==================================================================================================//
    // If TRUE, octree will be adaptively subdivided up to m_maxOctreeSubdivision levels of subdivision //
    // If FALSE, octree will be uniformly subdivided to m_maxOctreeSubdivision  levels of subdivision   //
    //==================================================================================================//
    bool m_bAdaptiveOctreeSubdivision;

    //========================================================================//
    // Pointers to GPU resources used to capture scene's radiance/irradiance. //
    //========================================================================//
    IDirect3DDevice9* m_pD3DDevice;
    ID3DXRenderToEnvMap* m_pRenderToEnvMap;
    IDirect3DCubeTexture9* m_pCubeTexture;

    //===============================================================//
    // List of meshes for which radiance/irradiance will be sampled. //
    //===============================================================//
    CGrowableArray <CSceneMesh*> m_pSceneMeshes;
    CGrowableArray <bool> m_bAdaptiveTest;

    //===============================================================================================//
    // Samples incident radiance at a point and projects this into Spherical Harmonics.  Optionally, //
    // gradients may be found (using a finite differencing method).                                  //
    //===============================================================================================//
    bool    SampleIncidentRadiance( D3DXVECTOR3* pPosition, float* pRed, float* pGreen, float* pBlue );

    //===================================================================//
    // Computes the harmonic mean of scene depth at a point in the scene //
    //===================================================================//
    bool    SampleDepth( D3DXVECTOR3* pPosition, float* pHMDepth, float* pMinDepth, float* pMaxDepth );

    //====================================================================================//
    // Bounding box of scene (this gets updated every time a mesh is added to the scene). //
    //====================================================================================//
    D3DXVECTOR3 m_vBoundingBoxMin;
    D3DXVECTOR3 m_vBoundingBoxMax;

    //==================//
    // Disallow copying //
    //==================//
            CIrradianceCacheGenerator( CIrradianceCacheGenerator& o )
            {
                assert( false );
            };
    CIrradianceCacheGenerator& operator =( CIrradianceCacheGenerator& o )
    {
        assert( false );
    };
};


//==============================================================================================//
//=== CIrradianceCacheOctree:: Octree for spatial partitioning of cached irradiance samples. ===//
//==============================================================================================//
class CIrradianceCache
{
    friend class CIrradianceCacheGenerator;

public :

    //==============================================================================//
    // User should call CIrradianceCacheGenerator::CreateCache() to create a cache. //
    // The destructor is public, so be sure to delete the caches you create!        //
    //==============================================================================//
            ~CIrradianceCache( void );

    //===================================//
    // Irradiance sample type definition //
    //===================================//
    typedef struct IrradianceSample
    {
        DWORD dwRefCount;

        D3DXVECTOR3 vPosition;

        float   pRedCoefs[IRRADIANCE_CACHE_MAX_SH_COEF];
        float   pGreenCoefs[IRRADIANCE_CACHE_MAX_SH_COEF];
        float   pBlueCoefs[IRRADIANCE_CACHE_MAX_SH_COEF];

        float fHMDepth;

    } IrradianceSample;

    //==================================================//
    // Clear all the cached samples (flushes the cache) //
    //==================================================//
    void    ClearCache( void );

    //==========================================//
    // Samples volume with trilinear filtering. //
    //==========================================//
    bool    SampleTrilinear( D3DXVECTOR3* pPosition, CIrradianceCache::IrradianceSample* pSample,
                             D3DXVECTOR3 pBox[8] );

    //=========================================================================//
    // Returns the number of nodes or voxels in the octree.  This can be used  //
    // to create a correctly sized array for passing to CreateOctreeLineList() //
    //=========================================================================//
    DWORD   GetNodeCount( void );

    //===========================================================================================//
    // Fills an array of D3DXVECTOR3 with line primative vertices.  This array should be         //
    // pre-allocated with NumNodes*16 elements.  NumNodes can be found by calling GetNodeCount() //
    // This array may be used for visualizing the Octree hierarchy by passing the array to       //
    // ID3DXLine::DrawTransform().  This draw function should be called NumNodes times.  Each    //
    // call should advance the pointer by a stride of 16 elements since each cube is drawn with  //
    // with a 16 point line list (this is inefficient but that's the problem with drawing cubes  //
    // using line lists).                                                                        //
    //===========================================================================================//
    bool    CreateOctreeLineList( D3DXVECTOR3* pVertexList );

    //====================//
    // Save cache to disk //
    //====================//
    bool    SaveCache( WCHAR* strFileName );

protected :

    //=====================================================================================================================//
    // Contructor: Use CIrradianceCacheGenerator::CreateCache() to create a cache.  But remember to delete it on your own! //
    //=====================================================================================================================//
            CIrradianceCache( D3DXVECTOR3* pBoundBoxMin, D3DXVECTOR3* pBoundBoxMax );

    //======================//
    // Load cache from disk //
    //======================//
    bool    LoadCache( WCHAR* strFileName );

    //===================================================//
    // Octree for partitioning cached irradiance samples //
    //===================================================//
    CIrradianceCacheOctree* m_pOctree;

    //===========================//
    // Cached irradiance samples //
    //===========================//
    CGrowableArray <CIrradianceCache::IrradianceSample*> m_pCache;

    //===============================================================================//
    // Keep track of the number of samples that get skipped due to adaptive sampling //
    //===============================================================================//
    DWORD m_dwNumSkippedSamples;

    //======================================//
    // Disallow copying and void contructor //
    //======================================//
            CIrradianceCache( void )
            {
                assert( false );
            };
            CIrradianceCache( CIrradianceCache& o )
            {
                assert( false );
            };
    CIrradianceCache& operator =( CIrradianceCache& o )
    {
        assert( false );
    };
};


//==============================================================================================//
//=== CIrradianceCacheOctree:: Octree for spatial partitioning of cached irradiance samples. ===//
//==============================================================================================//
class CIrradianceCacheOctree
{
    friend class CIrradianceCacheGenerator;
    friend class CIrradianceCache;

public :

    //=============================//
    // Octree node type definition //
    //=============================//
    typedef struct OctreeNode
    {
        //=============//
        // Constructor //
        //=============//
                    OctreeNode()
                    {
                        for( int i = 0; i < 8; i++ )
                        {
                            pChildren[i] = NULL;
                            bSampleInCache[i] = false;
                            dwSampleIndex[i] = 0;
                            vPosition[i] = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
                        }

                        bHasChildren = false;

                    }

        //============//
        // Destructor //
        //============//
                    ~OctreeNode()
                    {
                        for( int i = 0; i < 8; i++ )
                        {
                            SAFE_DELETE( pChildren[i] );
                        }

                        bHasChildren = false;
                    }

        bool bHasChildren;
        CIrradianceCacheOctree::OctreeNode* pChildren[8];
        bool        bSampleInCache[8];
        DWORD       dwSampleIndex[8];
        D3DXVECTOR3 vPosition[8];

    } OctreeNode;

    //=======================//
    // Contructor/Destructor //
    //=======================//
            CIrradianceCacheOctree( D3DXVECTOR3* pBoundBoxMin, D3DXVECTOR3* pBoundBoxMax );
            ~CIrradianceCacheOctree( void );

    //================================//
    // Returns the Octree's root node //
    //================================//
    CIrradianceCacheOctree::OctreeNode* GetRootNode( void )
    {
        return &m_root;
    };

    //======================================================================================================//
    // Return the smallest Octree node that encloses the specified point, tree traversal starts with pNode. //
    //======================================================================================================//
    CIrradianceCacheOctree::OctreeNode* FindEnclosingNode( D3DXVECTOR3* pPosition,
                                                           CIrradianceCacheOctree::OctreeNode* pNode );

    //=====================================================//
    // Returns the number of nodes or voxels in the octree //
    // starting with the node passed in.  For example, if  //
    // only the root node exists, the cound with be 1.     //
    //=====================================================//
    DWORD   GetNodeCount( CIrradianceCacheOctree::OctreeNode* pNode );

    //==============================================================================================//
    // Fills a pre-allocated array with the positions of all voxel vertices in the tree starting at //
    // pNode and traversing in a depth first manner.  pPositions should point to an array that is   //
    // big enough to store (GetNodeCount(pNode) * 8) entries (8 vertices for each voxel corner).    //
    //==============================================================================================//
    DWORD   GetNodePositions( CIrradianceCacheOctree::OctreeNode* pNode, D3DXVECTOR3* pPositions ) const;

    //================================================//
    // Creates and attaches 8 children for the parent //
    //================================================//
    bool    AddChildNodes( CIrradianceCacheOctree::OctreeNode* pParentNode );

    //====================================================//
    // Deletes a node's children (and grandchildren, etc) //
    //====================================================//
    bool    DeleteChildren( CIrradianceCacheOctree::OctreeNode* pNode );

    //==========================================================================================//
    // Subdivides the volume starting at pNode. Subdivision continues for dwSubdivisions levels //
    //==========================================================================================//
    bool    Subdivide( CIrradianceCacheOctree::OctreeNode* pNode, DWORD dwSubdivisions );

protected :

    //==================================================================================//
    // Depth first search for the first cache miss (node that hasn't been sampled yet). //
    // Returns NULL if nothing can be found. pDepth, if not NULL, will be incremented   //
    // for the number of levels the function had to traverse to find the returned node. //
    // For maximum usefulness, this pointer should point to a DWORD that's been         //
    // initialized to 0 so that in the end you will have the relative depth from pNode  //
    // to the node that was returned.                                                   //
    //==================================================================================//
    CIrradianceCacheOctree::OctreeNode* FindUnsampledNode( CIrradianceCacheOctree::OctreeNode* pNode, DWORD* pDepth =
                                                           NULL );

    //====================//
    // Octree's root node //
    //====================//
    CIrradianceCacheOctree::OctreeNode m_root;

    //=============================================//
    // Set node's vertices at bounding box corners //
    //=============================================//
    bool    SetNodeBounds( CIrradianceCacheOctree::OctreeNode* pNode, D3DXVECTOR3* pBoundBoxMin,
                           D3DXVECTOR3* pBoundBoxMax );

    //============================================================================================//
    // Keep track of nodes added and removed so that we don't have to walk the tree every time we //
    // want to know how many nodes there are.                                                     //
    //============================================================================================//
    DWORD m_dwNodeCount;

    //========================================================//
    // Disallow copying, void contructor, and copy contructor //
    //========================================================//
            CIrradianceCacheOctree( void )
            {
                assert( false );
            };
            CIrradianceCacheOctree( CIrradianceCacheOctree& o )
            {
                assert( false );
            };
    CIrradianceCacheOctree& operator =( CIrradianceCacheOctree& o )
    {
        assert( false );
    };
};


