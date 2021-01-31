//--------------------------------------------------------------------------------------
// File: SubDMesh.cpp
//
// This class encapsulates the mesh loading and housekeeping functions for a SubDMesh.
// The mesh loads .obj files from disk and converts them to a Catmull-Clark patch based
// format.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SubDMesh.h"
#include "sdkmisc.h"
#include "DXUTRes.h"

#pragma warning(disable: 4995)
#pragma warning(disable: 4530)
#include <fstream>
using namespace std;
#pragma warning(default: 4995)
#pragma warning(default: 4530)

#define WRAPPOINT(a) ((a)%4)

template <class T, class Q, class W> void QuickSort( T* indices, Q* pTanQuad, W* sizes, int lo, int hi );

//--------------------------------------------------------------------------------------
// Loads an obj mesh file from disk.  We use the obj format here because it's one of
// the few formats that supports quads as a primitive type.
//--------------------------------------------------------------------------------------
HRESULT CSubDMesh::LoadSubDFromObj( const WCHAR* strFileName )
{
    WCHAR strMaterialFilename[MAX_PATH] = {0};
    WCHAR wstr[MAX_PATH];
    char str[MAX_PATH];
    HRESULT hr;

    // Find the file
    V_RETURN( DXUTFindDXSDKMediaFileCch( wstr, MAX_PATH, strFileName ) );
    WideCharToMultiByte( CP_ACP, 0, wstr, -1, str, MAX_PATH, NULL, NULL );

    // Create temporary storage for the input data. Once the data has been loaded into
    // a reasonable format we can create a D3DXMesh object and load it with the mesh data.
    CGrowableArray <D3DXVECTOR4> Positions;
    CGrowableArray <D3DXVECTOR2> TexCoords;
    CGrowableArray <D3DXVECTOR3> Normals;

    // Max/Min out the Min/Max mesh extents
    m_vMeshExtentsMin = D3DXVECTOR3( FLT_MAX, FLT_MAX, FLT_MAX );
    m_vMeshExtentsMax = D3DXVECTOR3( -FLT_MAX, -FLT_MAX, -FLT_MAX );

    // File input
    WCHAR strCommand[256] = {0};
    wifstream InFile( str );
    if( !InFile )
        return DXTRACE_ERR( L"wifstream::open", E_FAIL );

    for(; ; )
    {
        InFile >> strCommand;
        if( !InFile )
            break;

        if( 0 == wcscmp( strCommand, L"#" ) )
        {
            // Comment
        }
        else if( 0 == wcscmp( strCommand, L"v" ) )
        {
            // Vertex Position
            float x, y, z;
            InFile >> x >> y >> z;
            D3DXVECTOR3 VertexPos( x, y, z );

            // Track out bounding volume
            D3DXVec3Minimize( &m_vMeshExtentsMin, &m_vMeshExtentsMin, &VertexPos );
            D3DXVec3Maximize( &m_vMeshExtentsMax, &m_vMeshExtentsMax, &VertexPos );

            Positions.Add( D3DXVECTOR4( VertexPos, 1 ) );
        }
        else if( 0 == wcscmp( strCommand, L"vt" ) )
        {
            // Vertex TexCoord
            float u, v;
            InFile >> u >> v;
            TexCoords.Add( D3DXVECTOR2( u, v ) );
        }
        else if( 0 == wcscmp( strCommand, L"vn" ) )
        {
            // Vertex Normal
            float x, y, z;
            InFile >> x >> y >> z;
            Normals.Add( D3DXVECTOR3( x, y, z ) );
        }
        else if( 0 == wcscmp( strCommand, L"f" ) )
        {
            // Face
            UINT iPosition, iTexCoord, iNormal;
            VERTEX vertex;

            for( UINT iFace = 0; iFace < 4; iFace++ )
            {
                ZeroMemory( &vertex, sizeof( VERTEX ) );

                // OBJ format uses 1-based arrays
                InFile >> iPosition;
                vertex.m_Position = Positions[ iPosition - 1 ];

                if( '/' == InFile.peek() )
                {
                    InFile.ignore();

                    if( '/' != InFile.peek() )
                    {
                        // Optional texture coordinate
                        InFile >> iTexCoord;
                        vertex.m_Texcoord = TexCoords[ iTexCoord - 1 ];
                    }

                    if( '/' == InFile.peek() )
                    {
                        InFile.ignore();

                        // Optional vertex normal
                        InFile >> iNormal;
                        vertex.m_Normal = Normals[ iNormal - 1 ];
                    }
                }

                // If a duplicate vertex doesn't exist, add this vertex to the Vertices
                // list. Store the index in the Indices array. The Vertices and Indices
                // lists will eventually become the Vertex Buffer and Index Buffer for
                // the mesh.
                DWORD index = AddVertex( iPosition, &vertex );
                m_Indices.Add( index );
            }
        }
        else if( 0 == wcscmp( strCommand, L"mtllib" ) )
        {
            // Material library
            InFile >> strMaterialFilename;
        }

        InFile.ignore( 1000, '\n' );
    }

    // Cleanup
    InFile.close();
    DeleteCache();

    // Convert into the patch structure
    int NumIndices = m_Indices.GetSize();
    for( int i = 0; i < NumIndices; i += 4 )
    {
        SUBDPATCH* pPatch = new SUBDPATCH;
        pPatch->m_Points[0] = m_Indices.GetAt( i );
        pPatch->m_Points[1] = m_Indices.GetAt( i + 1 );
        pPatch->m_Points[2] = m_Indices.GetAt( i + 2 );
        pPatch->m_Points[3] = m_Indices.GetAt( i + 3 );

        m_QuadArray.Add( pPatch );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Adds a vertex to the running vertex cache.  This is an optimization to remove
// duplicate verts.
//--------------------------------------------------------------------------------------
DWORD CSubDMesh::AddVertex( UINT hash, VERTEX* pVertex )
{
    // If this vertex doesn't already exist in the Vertices list, create a new entry.
    // Add the index of the vertex to the Indices list.
    bool bFoundInList = false;
    DWORD index = 0;

    // Since it's very slow to check every element in the vertex list, a hashtable stores
    // vertex indices according to the vertex position's index as reported by the OBJ file
    if( ( UINT )m_VertexCache.GetSize() > hash )
    {
        CacheEntry* pEntry = m_VertexCache.GetAt( hash );
        while( pEntry != NULL )
        {
            VERTEX* pCacheVertex = m_Vertices.GetData() + pEntry->m_Index;

            // If this vertex is identical to the vertex already in the list, simply
            // point the index buffer to the existing vertex
            if( 0 == memcmp( pVertex, pCacheVertex, sizeof( VERTEX ) ) )
            {
                bFoundInList = true;
                index = pEntry->m_Index;
                break;
            }

            pEntry = pEntry->m_pNext;
        }
    }

    // Vertex was not found in the list. Create a new entry, both within the Vertices list
    // and also within the hashtable cache
    if( !bFoundInList )
    {
        // Add to the Vertices list
        index = m_Vertices.GetSize();
        m_Vertices.Add( *pVertex );

        // Add this to the hashtable
        CacheEntry* pNewEntry = new CacheEntry;
        if( pNewEntry == NULL )
            return ( DWORD )-1;

        pNewEntry->m_Index = index;
        pNewEntry->m_pNext = NULL;

        // Grow the cache if needed
        while( ( UINT )m_VertexCache.GetSize() <= hash )
        {
            m_VertexCache.Add( NULL );
        }

        // Add to the end of the linked list
        CacheEntry* pCurEntry = m_VertexCache.GetAt( hash );
        if( pCurEntry == NULL )
        {
            // This is the head element
            m_VertexCache.SetAt( hash, pNewEntry );
        }
        else
        {
            // Find the tail
            while( pCurEntry->m_pNext != NULL )
            {
                pCurEntry = pCurEntry->m_pNext;
            }

            pCurEntry->m_pNext = pNewEntry;
        }
    }

    return index;
}


//--------------------------------------------------------------------------------------
void CSubDMesh::DeleteCache()
{
    // Iterate through all the elements in the cache and subsequent linked lists
    for( int i = 0; i < m_VertexCache.GetSize(); i++ )
    {
        CacheEntry* pEntry = m_VertexCache.GetAt( i );
        while( pEntry != NULL )
        {
            CacheEntry* pNext = pEntry->m_pNext;
            SAFE_DELETE( pEntry );
            pEntry = pNext;
        }
    }

    m_VertexCache.RemoveAll();
}

//--------------------------------------------------------------------------------------
// A sample extraordinary SubD quad is represented by the following diagram:
//
//                        15              Valences:
//                       /  \               Vertex 0: 5
//                      /    14             Vertex 1: 4
//          17---------16   /  \            Vertex 2: 5
//          | \         |  /    \           Vertex 3: 3
//          |  \        | /      13
//          |   \       |/      /         Prefixes:
//          |    3------2------12           Vertex 0: 9
//          |    |      |      |            Vertex 1: 12
//          |    |      |      |            Vertex 2: 16
//          4----0------1------11           Vertex 3: 18
//         /    /|      |      |
//        /    / |      |      |
//       5    /  8------9------10
//        \  /  /
//         6   /
//          \ /
//           7
//
// Where the quad bounded by vertices 0,1,2,3 represents the actual subd surface of interest
// The 1-ring neighborhood of the quad is represented by vertices 4 through 17.  The counter-
// clockwise winding of this 1-ring neighborhood is important, especially when it comes to compute
// the corner vertices of the bicubic patch that we will use to approximate the subd quad (0,1,2,3).
// 
// The resulting bicubic patch fits within the subd quad (0,1,2,3) and has the following control
// point layout:
//
//     12--13--14--15
//      8---9--10--11
//      4---5---6---7
//      0---1---2---3
//
// The inner 4 control points of the bicubic patch are a combination of only the vertices (0,1,2,3)
// of the subd quad.  However, the corner control points for the bicubic patch (0,3,15,12) are actually
// a much more complex weighting of the subd patch and the 1-ring neighborhood.  In the example above
// the bicubic control point 0 is actually a weighted combination of subd points 0,1,2,3 and 1-ring
// neighborhood points 17, 4, 5, 6, 7, 8, and 9.  We can see that the 1-ring neighbor hood is simply
// walked from the prefix value from the previous corner (corner 3 in this case) to the prefix 
// prefix value for the current corner.  We add one more vertex on either side of the prefix values
// and we have all the data necessary to calculate the value for the corner points.
//
// The edge control points of the bicubic patch (1,2,13,14,4,8,7,11) are also combinations of their 
// neighbors, but fortunately each one is only a combination of 6 values and no walk is required.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Condition each patch in the mesh.  Conditioning precomputes the prefixes and 1-ring 
// neighborhood data mentioned above.  This is done on a per-patch basis in a very simple
// and inefficient manner.  However, this work should ideally be part of the production
// pipeline and the data should be saved to the app specific file format before loading.
//
// We are handling this on load here, so that the user can experiment with different
// obj files without having to create a special exporter.
//--------------------------------------------------------------------------------------
bool CSubDMesh::ConditionMesh()
{
    // Condition each patch
    int NumPatches = m_QuadArray.GetSize();
    for( int i = 0; i < NumPatches; i++ )
    {
        SUBDPATCH* pQuad = m_QuadArray.GetAt( i );

        if( !ConditionPatch( pQuad ) )
        {
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------
// Condition a subd patch.  Find the 1-ring neighborhood and precompute the prefixes
// around it to make conversion to bezier easier.
//--------------------------------------------------------------------------------------
bool CSubDMesh::ConditionPatch( SUBDPATCH* pQuad )
{
    CGrowableArray <int> NeighborPoints;

    D3DXVECTOR4 v0 = m_Vertices[ pQuad->m_Points[ 0 ] ].m_Position;
    D3DXVECTOR4 v1 = m_Vertices[ pQuad->m_Points[ 1 ] ].m_Position;
    D3DXVECTOR4 v2 = m_Vertices[ pQuad->m_Points[ 2 ] ].m_Position;
    D3DXVECTOR4 v3 = m_Vertices[ pQuad->m_Points[ 3 ] ].m_Position;
    D3DXVECTOR4 vOther[3];

    // v0
    vOther[0] = v1; vOther[1] = v2; vOther[2] = v3;
    pQuad->m_Valences[0] = ConditionPoint( &v0, vOther, &NeighborPoints );
    pQuad->m_Prefixes[0] = ( BYTE )NeighborPoints.GetSize() + 4;

    // v1
    vOther[0] = v2; vOther[1] = v3; vOther[2] = v0;
    pQuad->m_Valences[1] = ConditionPoint( &v1, vOther, &NeighborPoints );
    pQuad->m_Prefixes[1] = ( BYTE )NeighborPoints.GetSize() + 4;

    // v2
    vOther[0] = v3; vOther[1] = v0; vOther[2] = v1;
    pQuad->m_Valences[2] = ConditionPoint( &v2, vOther, &NeighborPoints );
    pQuad->m_Prefixes[2] = ( BYTE )NeighborPoints.GetSize() + 4;

    // v3
    vOther[0] = v0; vOther[1] = v1; vOther[2] = v2;
    pQuad->m_Valences[3] = ConditionPoint( &v3, vOther, &NeighborPoints );
    pQuad->m_Prefixes[3] = ( BYTE )NeighborPoints.GetSize() + 4;

    int NumNeighbors = NeighborPoints.GetSize();

    // Move neighbors into the points list
    for( int i = 0; i < min( NumNeighbors, MAX_POINTS - 4 ); i++ )
    {
        pQuad->m_Points[4 + i] = NeighborPoints.GetAt( i );
    }

    return true;
}

//--------------------------------------------------------------------------------------
// Condition a point in a patch.  This is really inefficient, but it is only done
// during load.  Ideally, you would have something akin to this in your content
// pipeline and would save the results into your custom file format that your game uses.
//--------------------------------------------------------------------------------------
BYTE CSubDMesh::ConditionPoint( D3DXVECTOR4* pV, D3DXVECTOR4* vOtherPatchV, CGrowableArray <int>* pNeighborPoints )
{
    int OriginalNeighborPoints = pNeighborPoints->GetSize();

    // Find the outer face that shares the edge formed by the current vertex in the quad and
    // the previous vertex in the quad (previous based on winding order)
    SUBDPATCH* pCurrentQuad = FindQuadWithPointsABButNotC( pV, &vOtherPatchV[2], &vOtherPatchV[0] );
    SUBDPATCH* pEndQuad = FindQuadWithPointsABButNotC( pV, &vOtherPatchV[0], &vOtherPatchV[2] );

    int iFarEdgePoint = FindLocalIndexForPointInQuad( pCurrentQuad, &vOtherPatchV[2] );

    int iOffEdgePoint = pCurrentQuad->m_Points[ WRAPPOINT( iFarEdgePoint + 1 ) ];
    int iFanPoint = pCurrentQuad->m_Points[ WRAPPOINT( iFarEdgePoint + 2 ) ];
    D3DXVECTOR4 vOffEdgePoint = m_Vertices[ iOffEdgePoint ].m_Position;
    D3DXVECTOR4 vFanPoint = m_Vertices[ iFanPoint ].m_Position;

    // Add the first point
    pNeighborPoints->Add( iFanPoint );

    // Move to the next quad
    pCurrentQuad = FindQuadWithPointsABButNotC( pV, &vFanPoint, &vOffEdgePoint );

    while( pCurrentQuad != pEndQuad )
    {
        int iFarEdgePoint = FindLocalIndexForPointInQuad( pCurrentQuad, &vFanPoint );
        iOffEdgePoint = pCurrentQuad->m_Points[WRAPPOINT( iFarEdgePoint + 1 )];
        iFanPoint = pCurrentQuad->m_Points[WRAPPOINT( iFarEdgePoint + 2 )];
        vOffEdgePoint = m_Vertices[iOffEdgePoint].m_Position;
        vFanPoint = m_Vertices[iFanPoint].m_Position;

        // Add the off-edge point and fan point
        pNeighborPoints->Add( iOffEdgePoint );
        pNeighborPoints->Add( iFanPoint );

        pCurrentQuad = FindQuadWithPointsABButNotC( pV, &vFanPoint, &vOffEdgePoint );
    }

    // Valence is a function of neighbor points
    int NewNeighborPoints = pNeighborPoints->GetSize() - OriginalNeighborPoints;
    BYTE valence = ( BYTE )( NewNeighborPoints + 5 ) / 2;

    return valence;
}

//--------------------------------------------------------------------------------------
// Does the quad contain this point?
//--------------------------------------------------------------------------------------
bool CSubDMesh::QuadContainsPoint( SUBDPATCH* pQuad, D3DXVECTOR4* pV )
{
    if( *pV == m_Vertices[ pQuad->m_Points[0] ].m_Position ||
        *pV == m_Vertices[ pQuad->m_Points[1] ].m_Position ||
        *pV == m_Vertices[ pQuad->m_Points[2] ].m_Position ||
        *pV == m_Vertices[ pQuad->m_Points[3] ].m_Position )
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------
// Find the index for a specific point
//--------------------------------------------------------------------------------------
int CSubDMesh::FindLocalIndexForPointInQuad( SUBDPATCH* pQuad, D3DXVECTOR4* pV )
{
    for( int i = 0; i < 4; i++ )
    {
        if( *pV == m_Vertices[ pQuad->m_Points[i] ].m_Position )
        {
            return i;
        }
    }

    return -1;
}

//--------------------------------------------------------------------------------------
// Find a quad with two out of three points
//--------------------------------------------------------------------------------------
SUBDPATCH* CSubDMesh::FindQuadWithPointsABButNotC( D3DXVECTOR4* pA, D3DXVECTOR4* pB, D3DXVECTOR4* pC )
{
    int MaxQuads = m_QuadArray.GetSize();
    for( int i = 0; i < MaxQuads; i++ )
    {
        SUBDPATCH* pQuad = m_QuadArray.GetAt( i );
        if( QuadContainsPoint( pQuad, pA ) &&
            QuadContainsPoint( pQuad, pB ) &&
            !QuadContainsPoint( pQuad, pC ) )
        {
            return pQuad;
        }
    }

    return NULL;
}

//--------------------------------------------------------------------------------------
CSubDMesh::CSubDMesh()
{
    m_pTransforms = NULL;
}

//--------------------------------------------------------------------------------------
CSubDMesh::~CSubDMesh()
{
    Destroy();
}

//--------------------------------------------------------------------------------------
// Create a spine for the mesh along the passed in axis using the number of bones
// specified.
//--------------------------------------------------------------------------------------
void CSubDMesh::AddBones( UINT NumBones, D3DXVECTOR3 vDir, float falloff )
{
    D3DXVECTOR3 vMin = m_vMeshExtentsMin;
    D3DXVECTOR3 vMax = m_vMeshExtentsMax;
    D3DXVECTOR3 vExtentsSize = vMax - vMin;
    D3DXVECTOR3 vCenter = ( vMin + vMax ) / 2.0f;
    float ExtentLength = D3DXVec3Dot( &vDir, &vExtentsSize );
    float BoneLength = ExtentLength / NumBones;

    // Add a bunch of straight bones
    D3DXVECTOR3 vStart = vCenter - ( vDir * ExtentLength * 0.5f );
    for( UINT i = 0; i < NumBones; i++ )
    {
        BONETRANSFORM* pBoneTransform = new BONETRANSFORM;
        pBoneTransform->m_Translation = vStart;
        pBoneTransform->m_Rotation = D3DXVECTOR3( 0, 0, 0 );
        m_Bones.Add( pBoneTransform );

        vStart += vDir * BoneLength;
    }

    // Create a place to store the transformed bone matrices
    m_pTransforms = new D3DXMATRIX[ NumBones ];

    int NumVertices = m_Vertices.GetSize();
    VERTEX* pVerts = m_Vertices.GetData();

    // Compute weights of the vertices based on their proximity to bones
    for( int i = 0; i < NumVertices; i++ )
    {
        D3DXVECTOR3 vDelta( pVerts[i].m_Position.x, pVerts[i].m_Position.y, pVerts[i].m_Position.z );
        vDelta = vDelta - vMin;
        float fDistAlong = D3DXVec3Dot( &vDir, &vDelta );

        float fBoneFrac = fDistAlong / BoneLength;
        float fCurrBone = floor( fBoneFrac - 0.5f );
        BYTE NextBone = ( BYTE )fCurrBone + 1;

        float fWeight1 = fBoneFrac - fCurrBone;
        float fInfluence1 = max( 0.0f, 1.0f - fabs( fWeight1 - 0.5f ) );
        float fInfluence2 = max( 0.0f, 1.0f - fabs( fWeight1 - 1.5f ) );

        if( fCurrBone < 0 )
        {
            fCurrBone = 0.0f;
            NextBone = 0;
            fInfluence1 = 1.0f;
            fInfluence2 = 0.0f;
        }
        if( NextBone >= NumBones )
        {
            fCurrBone = NumBones - 1.0f;
            NextBone = 0;
            fInfluence1 = 1.0f;
            fInfluence2 = 0.0f;
        }

        pVerts[i].m_Bones[0] = ( BYTE )fCurrBone;
        pVerts[i].m_Bones[1] = NextBone;
        pVerts[i].m_Weights[0] = ( BYTE )( fInfluence1 * 255.0f );
        pVerts[i].m_Weights[1] = ( BYTE )( fInfluence2 * 255.0f );

        // Make sure that the total weights add up to 1 (255) or we'll get weird skinning
        pVerts[i].m_Weights[0] = 255 - pVerts[i].m_Weights[1];
    }
}

//--------------------------------------------------------------------------------------
void CSubDMesh::RotateBone( UINT iBone, D3DXVECTOR3 vRot )
{
    BONETRANSFORM* pBone = m_Bones.GetAt( iBone );
    pBone->m_Rotation = vRot;
}

//--------------------------------------------------------------------------------------
// Create bones matrices from bone transforms
//--------------------------------------------------------------------------------------
void CSubDMesh::CreateSkinningMatrices()
{
    D3DXMATRIX mPrevious;
    D3DXMatrixIdentity( &mPrevious );
    int NumBones = m_Bones.GetSize();
    for( int i = 0; i < NumBones; i++ )
    {
        BONETRANSFORM* pBone = m_Bones.GetAt( i );

        // Translate to origin and back
        D3DXMATRIX mTranslateOrigin;
        D3DXMATRIX mTranslateBack;
        D3DXMatrixTranslation( &mTranslateOrigin, -pBone->m_Translation.x, -pBone->m_Translation.y,
                               -pBone->m_Translation.z );
        D3DXMatrixTranslation( &mTranslateBack, pBone->m_Translation.x, pBone->m_Translation.y,
                               pBone->m_Translation.z );

        D3DXMATRIX mRot;
        D3DXMatrixRotationYawPitchRoll( &mRot, pBone->m_Rotation.y, pBone->m_Rotation.x, pBone->m_Rotation.z );

        D3DXMATRIX mLocalTransform = mTranslateOrigin * mRot * mTranslateBack;

        m_pTransforms[i] = mLocalTransform * mPrevious;

        mPrevious = m_pTransforms[i];
    }
}

//--------------------------------------------------------------------------------------
// Computes tangent data for a mesh using d3dxcomputetangentframeex.
// There is no version of this for d3d10, so we load the old D3D9 version, put everything
// into an ID3DXMesh and let it compute the tangents for us.
// This is a lot of code just to use D3DXComputeTangentFrameEx.
//--------------------------------------------------------------------------------------
HRESULT CSubDMesh::ComputeTangents()
{
    HRESULT hr = S_OK;

    // Create a fake d3d9 device
    IDirect3D9* pd3d9 = NULL;
    IDirect3DDevice9* pd3dDevice = NULL;

    // Create a d3d9 object
    pd3d9 = Direct3DCreate9( D3D_SDK_VERSION );
    if( pd3d9 == NULL )
        return E_FAIL;

    // Create a d3d9 device with some fake present params
    D3DPRESENT_PARAMETERS pp;
    pp.BackBufferWidth = 320;
    pp.BackBufferHeight = 240;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.MultiSampleQuality = 0;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = GetShellWindow();
    pp.Windowed = true;
    pp.Flags = 0;
    pp.FullScreen_RefreshRateInHz = 0;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    pp.EnableAutoDepthStencil = false;

    hr = pd3d9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
                              D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pd3dDevice );
    if( FAILED( hr ) )
    {
        hr = pd3d9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, NULL,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pd3dDevice );
        if( FAILED( hr ) )
            return E_FAIL;
    }

    int uNumPatches = m_QuadArray.GetSize();

    struct LOCAL_VERTEX
    {
        D3DXVECTOR3 pos;
        D3DXVECTOR3 norm;
        D3DXVECTOR2 tex;
        D3DXVECTOR3 tan;
        D3DXVECTOR3 bitan;
    };

    // put everything into a d3dxmesh
    D3DVERTEXELEMENT9 meshDecl[] =
    {
        {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL, 0},
        {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
        {0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TANGENT, 0},
        {0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_BINORMAL, 0},

        {0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
    };

    ID3DXMesh* pMesh = NULL;
    D3DXCreateMesh( uNumPatches * 2, uNumPatches * 2 * 3, D3DXMESH_32BIT | D3DXMESH_SYSTEMMEM, meshDecl,
                    pd3dDevice, &pMesh );

    // Create the data for the mesh
    LOCAL_VERTEX* pVertices = NULL;
    pMesh->LockVertexBuffer( 0, ( void** )&pVertices );
    int index = 0;
    int verts[6] = {0,1,2,0,2,3};
    for( int i = 0; i < uNumPatches; i++ )
    {
        SUBDPATCH* pPatch = m_QuadArray.GetAt( i );

        VERTEX vertex[4];
        vertex[0] = m_Vertices.GetAt( pPatch->m_Points[0] );
        vertex[1] = m_Vertices.GetAt( pPatch->m_Points[1] );
        vertex[2] = m_Vertices.GetAt( pPatch->m_Points[2] );
        vertex[3] = m_Vertices.GetAt( pPatch->m_Points[3] );

        for( int v = 0; v < 6; v++ )
        {
            pVertices[index].pos = D3DXVECTOR3( vertex[verts[v]].m_Position.x, vertex[verts[v]].m_Position.y,
                                                vertex[verts[v]].m_Position.z );
            pVertices[index].norm = vertex[verts[v]].m_Normal;
            pVertices[index].tex = vertex[verts[v]].m_Texcoord;
            index ++;
        }
    }
    pMesh->UnlockVertexBuffer();

    // index buffer too
    DWORD* pIndices = NULL;
    pMesh->LockIndexBuffer( 0, ( void** )&pIndices );
    for( int i = 0; i < uNumPatches * 2 * 3; i++ )
    {
        pIndices[i] = i;
    }
    pMesh->UnlockIndexBuffer();

    // create adjacency
    DWORD* rgdwAdjacency = new DWORD[uNumPatches * 2 * 3];
    pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency );

    hr = D3DXComputeTangentFrameEx( pMesh,
                                    D3DDECLUSAGE_TEXCOORD, 0,
                                    D3DDECLUSAGE_TANGENT, 0,
                                    D3DX_DEFAULT, 0,
                                    D3DX_DEFAULT, 0,
                                    D3DXTANGENT_GENERATE_IN_PLACE,
                                    NULL, 0.01f, 0.25f, 0.01f, NULL, NULL );
    if( FAILED( hr ) )
    {
        MessageBox( NULL, L"Unable to Create Tangent Frame", L"Error", MB_OK );
    }

    // Get the data out
    pMesh->LockVertexBuffer( 0, ( void** )&pVertices );
    index = 0;
    int tans[4] = {0,1,2,5};
    for( int i = 0; i < uNumPatches; i++ )
    {
        for( int t = 0; t < 4; t++ )
        {
            D3DXVECTOR4 vec4( pVertices[ i * 6 + tans[t] ].tan, 1.0f );
            m_Tangents.Add( vec4 );
        }
    }

    // Cleanup
    SAFE_RELEASE( pMesh );
    SAFE_RELEASE( pd3dDevice );
    SAFE_RELEASE( pd3d9 );
    delete []rgdwAdjacency;

    return hr;
}

struct TANQUAD
{
    D3DXVECTOR4 quads[4];
};


//--------------------------------------------------------------------------------------
// Sort three things at once based upon sizes
//--------------------------------------------------------------------------------------
template <class T, class Q, class W> void QuickSort( T* indices, Q* pTanQuad, W* sizes, int lo, int hi )
{
    //  lo is the lower index, hi is the upper index
    //  of the region of array a that is to be sorted
    int i = lo, j = hi;
    int h;
    T index;
    Q index2;
    int x = sizes[( lo + hi ) / 2 ];

    //  partition
    do
    {
        while( sizes[i] < x ) i++;
        while( sizes[j] > x ) j--;
        if( i <= j )
        {
            // Swap
            h = sizes[i];
            sizes[i] = sizes[j];
            sizes[j] = h;

            // Swap
            index = indices[i];
            indices[i] = indices[j];
            indices[j] = index;

            // Swap
            index2 = pTanQuad[i];
            pTanQuad[i] = pTanQuad[j];
            pTanQuad[j] = index2;

            i++;
            j--;
        }
    } while( i <= j );

    if( lo < j )
        QuickSort( indices, pTanQuad, sizes, lo, j );
    if( i < hi )
        QuickSort( indices, pTanQuad, sizes, i, hi );
}


//--------------------------------------------------------------------------------------
// Sort patches by size (number of verts) so we can separate regular and extraordinary
// patches.  Not only will doing regular and extraordinary in different passes help with 
// performance, but also ordering them by size will make more efficient use of the GPU 
// so that the majority of the patches being processed at once will have the same number 
// of vertices.  This will help exploit the SIMD nature of the GPU.
//--------------------------------------------------------------------------------------
void CSubDMesh::SortPatchesBySize()
{
    int NumPatches = m_QuadArray.GetSize();
    int* sizes = new int[ NumPatches ];
    for( int i = 0; i < NumPatches; i++ )
    {
        SUBDPATCH* pPatch = m_QuadArray.GetAt( i );
        sizes[i] = pPatch->m_Prefixes[3];
    }

    TANQUAD* pTangents = ( TANQUAD* )m_Tangents.GetData();
    QuickSort( m_QuadArray.GetData(), pTangents, sizes, 0, NumPatches - 1 );
    delete []sizes;

    CGrowableArray <SUBDPATCH*> m_TempExtraArray;
    CGrowableArray <SUBDPATCH*> m_TempRegArray;
    CGrowableArray <TANQUAD> m_TempTanExtraArray;
    CGrowableArray <TANQUAD> m_TempTanRegArray;

    // Store the regular ones in their own array
    for( int i = 0; i < NumPatches; i++ )
    {
        SUBDPATCH* pPatch = m_QuadArray.GetAt( i );
        if( pPatch->m_Valences[0] == 4 &&
            pPatch->m_Valences[1] == 4 &&
            pPatch->m_Valences[2] == 4 &&
            pPatch->m_Valences[3] == 4 )
        {
            SUBDPATCHREGULAR* pNewPatch = new SUBDPATCHREGULAR;

            for( int v = 0; v < NUM_REGULAR_POINTS; v++ )
            {
                VERTEX vertex = m_Vertices.GetAt( pPatch->m_Points[v] );
                pNewPatch->m_Points[v] = D3DXVECTOR4( vertex.m_Position.x, vertex.m_Position.y, vertex.m_Position.z,
                                                      0 );
                UINT BoneNWeight = ( ( UINT )vertex.m_Bones[0] ) << 24;
                BoneNWeight |= ( ( UINT )vertex.m_Bones[1] ) << 16;
                BoneNWeight |= ( ( UINT )vertex.m_Weights[0] ) << 8;
                BoneNWeight |= ( UINT )vertex.m_Weights[1];

                pNewPatch->m_Points[v].w = *reinterpret_cast<FLOAT*>( &BoneNWeight );
            }

            m_RegularQuadArray.Add( pNewPatch );
            m_TempRegArray.Add( pPatch );
            m_TempTanRegArray.Add( pTangents[i] );
        }
        else
        {
            m_TempExtraArray.Add( pPatch );
            m_TempTanExtraArray.Add( pTangents[i] );
        }
    }

    // Repack the quad array
    //   Extraordinary patches are stored in sorted order from smallest to largest
    //   Regular patches are at the end
    m_QuadArray.Reset();
    int NumReg = m_TempRegArray.GetSize();
    int NumExtra = m_TempExtraArray.GetSize();
    int tanindex = 0;

    for( int i = 0; i < NumExtra; i++ )
    {
        SUBDPATCH* pPatch = m_TempExtraArray.GetAt( i );
        m_QuadArray.Add( pPatch );
        pTangents[ tanindex ] = m_TempTanExtraArray.GetAt( i );
        tanindex++;
    }

    for( int i = 0; i < NumReg; i++ )
    {
        SUBDPATCH* pPatch = m_TempRegArray.GetAt( i );
        m_QuadArray.Add( pPatch );
        pTangents[ tanindex ] = m_TempTanRegArray.GetAt( i );
        tanindex++;
    }

    m_TempExtraArray.RemoveAll();
    m_TempRegArray.RemoveAll();
    m_TempTanExtraArray.RemoveAll();
    m_TempTanRegArray.RemoveAll();

}

//--------------------------------------------------------------------------------------
void CSubDMesh::Destroy()
{
    SAFE_RELEASE( m_pHeightSRV );
    SAFE_RELEASE( m_pPatchesBufferB );
    SAFE_RELEASE( m_pPatchesBufferUV );
    SAFE_RELEASE( m_pPatchesBufferBSRV );
    SAFE_RELEASE( m_pPatchesBufferUVSRV );

    SAFE_RELEASE( m_pSubDPatchVB );
    SAFE_RELEASE( m_pSubDPatchRegVB );
    SAFE_RELEASE( m_pControlPointVB );
    SAFE_RELEASE( m_pControlPointBonesVB );
    SAFE_RELEASE( m_pControlPointUV );
    SAFE_RELEASE( m_pControlPointSRV );
    SAFE_RELEASE( m_pControlPointBonesSRV );
    SAFE_RELEASE( m_pControlPointUVSRV );

    int size = m_QuadArray.GetSize();
    for( int i = 0; i < size; i++ )
    {
        SUBDPATCH* pQuad = m_QuadArray.GetAt( i );
        delete []pQuad;
    }

    size = m_RegularQuadArray.GetSize();
    for( int i = 0; i < size; i++ )
    {
        SUBDPATCHREGULAR* pQuad = m_RegularQuadArray.GetAt( i );
        delete []pQuad;
    }

    size = m_Bones.GetSize();
    for( int i = 0; i < size; i++ )
    {
        BONETRANSFORM* pBone = m_Bones.GetAt( i );
        delete []pBone;
    }

    SAFE_DELETE_ARRAY( m_pTransforms );

    m_Bones.RemoveAll();
    m_RegularQuadArray.RemoveAll();
    m_QuadArray.RemoveAll();
    m_Vertices.RemoveAll();
    m_Indices.RemoveAll();
}

//--------------------------------------------------------------------------------------
// Creates buffers that hold the computed bezier data.
//--------------------------------------------------------------------------------------
HRESULT CSubDMesh::CreatePatchesBuffer( ID3D10Device* pd3dDevice )
{
    UINT NumPatches = GetNumPatches();
    HRESULT hr = S_OK;

    // Release any old data in case we need to recreate these
    SAFE_RELEASE( m_pPatchesBufferBSRV );
    SAFE_RELEASE( m_pPatchesBufferUVSRV );
    SAFE_RELEASE( m_pPatchesBufferB );
    SAFE_RELEASE( m_pPatchesBufferUV );

    UINT RegularWidth = PATCH_STRIDE * sizeof( D3DXVECTOR4 ) * NumPatches;

    // Create buffers to hold the streamed-out patch data
    D3D10_BUFFER_DESC Desc;
    Desc.Usage = D3D10_USAGE_DEFAULT;
    Desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    Desc.BindFlags |= D3D10_BIND_STREAM_OUTPUT;
    Desc.CPUAccessFlags = 0;
    Desc.MiscFlags = 0;

    // Create a buffer for the bicubic patch
    Desc.ByteWidth = RegularWidth;
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &m_pPatchesBufferB ) );

    // Create a buffer for the compact UV tangent patches
    Desc.ByteWidth = NumPatches * 9 * sizeof( D3DXVECTOR4 );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &m_pPatchesBufferUV ) );

    // Create views for these buffers
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.ElementOffset = 0;

    // View for the bicubic patch
    SRVDesc.Buffer.ElementWidth = PATCH_STRIDE * NumPatches;
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pPatchesBufferB, &SRVDesc, &m_pPatchesBufferBSRV ) );

    // View for the compact UV tangent patches
    SRVDesc.Buffer.ElementWidth = 9 * NumPatches;
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pPatchesBufferUV, &SRVDesc, &m_pPatchesBufferUVSRV ) );

    return hr;
}

//--------------------------------------------------------------------------------------
// Create subd patches (regular and extraordinary) from the loaded subd mesh.
// Also creates bones.
//--------------------------------------------------------------------------------------
HRESULT CSubDMesh::CreateBuffersFromSubDMesh( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    int NumVertices = GetNumVertices();
    int NumPatches = GetNumPatches();
    int NumRegularPatches = GetNumRegularPatches();

    // Create control point buffer
    D3D10_BUFFER_DESC DescCP;
    DescCP.ByteWidth = sizeof( D3DXVECTOR4 ) * NumVertices;
    DescCP.Usage = D3D10_USAGE_IMMUTABLE;
    DescCP.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    DescCP.CPUAccessFlags = 0;
    DescCP.MiscFlags = 0;

    D3DXVECTOR4* pVertices = new D3DXVECTOR4[ NumVertices ];
    for( int i = 0; i < NumVertices; i++ )
    {
        pVertices[i] = GetVertex( i ).m_Position;
    }

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertices;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;

    V_RETURN( pd3dDevice->CreateBuffer( &DescCP, &InitData, &m_pControlPointVB ) );
    delete []pVertices;

    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.ElementOffset = 0;
    SRVDesc.Buffer.ElementWidth = NumVertices;

    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pControlPointVB, &SRVDesc, &m_pControlPointSRV ) );

    // Create the bones buffer for extraordinary patches
    DescCP.ByteWidth = sizeof( BONESSTRUCT ) * NumVertices;

    BONESSTRUCT* pBones = new BONESSTRUCT[ NumVertices ];
    for( int i = 0; i < NumVertices; i++ )
    {
        VERTEX v = GetVertex( i );
        pBones[i].m_Bones[0] = v.m_Bones[0];
        pBones[i].m_Bones[1] = v.m_Bones[1];
        pBones[i].m_Weights[0] = v.m_Weights[0];
        pBones[i].m_Weights[1] = v.m_Weights[1];
    }

    InitData.pSysMem = pBones;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;

    V_RETURN( pd3dDevice->CreateBuffer( &DescCP, &InitData, &m_pControlPointBonesVB ) );
    delete []pBones;

    SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UINT;
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pControlPointBonesVB, &SRVDesc, &m_pControlPointBonesSRV ) );


    // Create the patch buffers for the extraordinary and regular patches
    D3D10_BUFFER_DESC DescPatch;
    DescPatch.ByteWidth = sizeof( SUBDPATCH ) * NumPatches;
    DescPatch.Usage = D3D10_USAGE_IMMUTABLE;
    DescPatch.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    DescPatch.CPUAccessFlags = 0;
    DescPatch.MiscFlags = 0;

    SUBDPATCH* pPatches = new SUBDPATCH[NumPatches];
    for( int i = 0; i < NumPatches; i++ )
    {
        pPatches[i] = *GetPatch( i );
    }

    InitData.pSysMem = pPatches;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;

    V_RETURN( pd3dDevice->CreateBuffer( &DescPatch, &InitData, &m_pSubDPatchVB ) );

    // Regular
    if( NumRegularPatches > 0 )
    {
        DescPatch.ByteWidth = sizeof( SUBDPATCHREGULAR ) * NumRegularPatches;

        SUBDPATCHREGULAR* pPatchesReg = new SUBDPATCHREGULAR[NumRegularPatches];
        for( int i = 0; i < NumRegularPatches; i++ )
        {
            pPatchesReg[i] = *GetPatchRegular( i );
        }

        InitData.pSysMem = pPatchesReg;
        InitData.SysMemPitch = 0;
        InitData.SysMemSlicePitch = 0;

        V_RETURN( pd3dDevice->CreateBuffer( &DescPatch, &InitData, &m_pSubDPatchRegVB ) );

        delete []pPatchesReg;
    }

    delete []pPatches;

    // Create a buffer to hold the UV coordinates and tangent vectors
    const UINT TangentSize = 3;
    D3D10_BUFFER_DESC DescUV;
    DescUV.ByteWidth = sizeof( D3DXVECTOR4 ) * NumPatches * ( 2 + TangentSize );
    DescUV.Usage = D3D10_USAGE_IMMUTABLE;
    DescUV.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    DescUV.CPUAccessFlags = 0;
    DescUV.MiscFlags = 0;

    D3DXVECTOR4* pUVs = new D3DXVECTOR4[ NumPatches * ( 2 + TangentSize ) ];

    int index = 0;
    for( int i = 0; i < NumPatches; i++ )
    {
        SUBDPATCH* pPatch = GetPatch( i );

        // Store the texture coordinates in the UV buffer
        for( int v = 0; v < 4; v += 2 )
        {
            pUVs[index].x = GetVertex( pPatch->m_Points[v  ] ).m_Texcoord.x;
            pUVs[index].y = GetVertex( pPatch->m_Points[v  ] ).m_Texcoord.y;
            pUVs[index].z = GetVertex( pPatch->m_Points[v + 1] ).m_Texcoord.x;
            pUVs[index].w = GetVertex( pPatch->m_Points[v + 1] ).m_Texcoord.y;

            index ++;
        }

        // Store the tangents in the UV buffer
        int sourceTanStride = TangentSize + 1;
        pUVs[index    ] = GetTangent( i * sourceTanStride + 0 );
        pUVs[index + 1] = GetTangent( i * sourceTanStride + 1 );
        pUVs[index + 2] = GetTangent( i * sourceTanStride + 2 );

        // Pack the last tangent into the w coordinates of the previous three tangents
        D3DXVECTOR4 tan4 = GetTangent( i * sourceTanStride + 3 );
        pUVs[index    ].w = tan4.x;
        pUVs[index + 1].w = tan4.y;
        pUVs[index + 2].w = tan4.z;

        index += 3;

    }

    InitData.pSysMem = pUVs;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;

    V_RETURN( pd3dDevice->CreateBuffer( &DescUV, &InitData, &m_pControlPointUV ) );
    delete []pUVs;

    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDescUV;
    SRVDescUV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SRVDescUV.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
    SRVDescUV.Buffer.ElementOffset = 0;
    SRVDescUV.Buffer.ElementWidth = NumPatches * ( 2 + TangentSize );

    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pControlPointUV, &SRVDescUV, &m_pControlPointUVSRV ) );

    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT CSubDMesh::LoadHeightTexture( ID3D10Device* pd3dDevice, WCHAR* strfile )
{
    HRESULT hr = S_OK;
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strfile ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &m_pHeightSRV, NULL ) );

    return hr;
}
