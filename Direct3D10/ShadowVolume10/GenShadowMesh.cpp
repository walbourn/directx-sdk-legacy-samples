//--------------------------------------------------------------------------------------
// File: GenShadowMesh.cpp
//
// Functions to help generate a mesh with degenerate triangles between edges for shadow volumes
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "GenShadowMesh.h"

struct CEdgeMapping
{
    int m_anOldEdge[2];  // vertex index of the original edge
    int m_aanNewEdge[2][2]; // vertex indexes of the new edge
    // First subscript = index of the new edge
    // Second subscript = index of the vertex for the edge

public:
        CEdgeMapping()
        {
            FillMemory( m_anOldEdge, sizeof( m_anOldEdge ), -1 );
            FillMemory( m_aanNewEdge, sizeof( m_aanNewEdge ), -1 );
        }
};

//--------------------------------------------------------------------------------------
// Takes an array of CEdgeMapping objects, then returns an index for the edge in the
// table if such entry exists, or returns an index at which a new entry for the edge
// can be written.
// nV1 and nV2 are the vertex indexes for the old edge.
// nCount is the number of elements in the array.
// The function returns -1 if an available entry cannot be found.  In reality,
// this should never happens as we should have allocated enough memory.
//--------------------------------------------------------------------------------------
int FindEdgeInMappingTable( int nV1, int nV2, CEdgeMapping* pMapping, int nCount )
{
    for( int i = 0; i < nCount; ++i )
    {
        // If both vertex indexes of the old edge in mapping entry are -1, then
        // we have searched every valid entry without finding a match.  Return
        // this index as a newly created entry.
        if( ( pMapping[i].m_anOldEdge[0] == -1 && pMapping[i].m_anOldEdge[1] == -1 ) ||

            // Or if we find a match, return the index.
            ( pMapping[i].m_anOldEdge[1] == nV1 && pMapping[i].m_anOldEdge[0] == nV2 ) )
        {
            return i;
        }
    }

    return -1;  // We should never reach this line
}


//--------------------------------------------------------------------------------------
// Takes a mesh and generate a new mesh from it that contains the degenerate invisible
// quads for shadow volume extrusion.
//--------------------------------------------------------------------------------------
HRESULT GenerateShadowMesh( IDirect3DDevice9* pd3dDevice, ID3DXMesh* pMesh, ID3DXMesh** ppOutMesh )
{
    HRESULT hr = S_OK;
    ID3DXMesh* pInputMesh;

    if( !ppOutMesh )
        return E_INVALIDARG;
    *ppOutMesh = NULL;

    // Convert the input mesh to a format same as the output mesh using 32-bit index.
    hr = pMesh->CloneMesh( D3DXMESH_32BIT, SHADOWVERT::Decl, pd3dDevice, &pInputMesh );
    if( FAILED( hr ) )
        return hr;

    DXUTTRACE( L"Input mesh has %u vertices, %u faces\n", pInputMesh->GetNumVertices(), pInputMesh->GetNumFaces() );

    // Generate adjacency information
    DWORD* pdwAdj = new DWORD[3 * pInputMesh->GetNumFaces()];
    DWORD* pdwPtRep = new DWORD[pInputMesh->GetNumVertices()];
    if( !pdwAdj || !pdwPtRep )
    {
        delete[] pdwAdj; delete[] pdwPtRep;
        pInputMesh->Release();
        return E_OUTOFMEMORY;
    }

    hr = pInputMesh->GenerateAdjacency( ADJACENCY_EPSILON, pdwAdj );
    if( FAILED( hr ) )
    {
        delete[] pdwAdj; delete[] pdwPtRep;
        pInputMesh->Release();
        return hr;
    }

    pInputMesh->ConvertAdjacencyToPointReps( pdwAdj, pdwPtRep );
    delete[] pdwAdj;

    SHADOWVERT* pVBData = NULL;
    DWORD* pdwIBData = NULL;

    pInputMesh->LockVertexBuffer( 0, ( LPVOID* )&pVBData );
    pInputMesh->LockIndexBuffer( 0, ( LPVOID* )&pdwIBData );

    if( pVBData && pdwIBData )
    {
        // Maximum number of unique edges = Number of faces * 3
        DWORD dwNumEdges = pInputMesh->GetNumFaces() * 3;
        CEdgeMapping* pMapping = new CEdgeMapping[dwNumEdges];
        if( pMapping )
        {
            int nNumMaps = 0;  // Number of entries that exist in pMapping

            // Create a new mesh
            ID3DXMesh* pNewMesh;
            hr = D3DXCreateMesh( pInputMesh->GetNumFaces() + dwNumEdges * 2,
                                 pInputMesh->GetNumFaces() * 3,
                                 D3DXMESH_32BIT,
                                 SHADOWVERT::Decl,
                                 pd3dDevice,
                                 &pNewMesh );
            if( SUCCEEDED( hr ) )
            {
                SHADOWVERT* pNewVBData = NULL;
                DWORD* pdwNewIBData = NULL;

                pNewMesh->LockVertexBuffer( 0, ( LPVOID* )&pNewVBData );
                pNewMesh->LockIndexBuffer( 0, ( LPVOID* )&pdwNewIBData );

                // nNextIndex is the array index in IB that the next vertex index value
                // will be store at.
                int nNextIndex = 0;

                if( pNewVBData && pdwNewIBData )
                {
                    ZeroMemory( pNewVBData, pNewMesh->GetNumVertices() * pNewMesh->GetNumBytesPerVertex() );
                    ZeroMemory( pdwNewIBData, sizeof( DWORD ) * pNewMesh->GetNumFaces() * 3 );

                    // pNextOutVertex is the location to write the next
                    // vertex to.
                    SHADOWVERT* pNextOutVertex = pNewVBData;

                    // Iterate through the faces.  For each face, output new
                    // vertices and face in the new mesh, and write its edges
                    // to the mapping table.

                    for( UINT f = 0; f < pInputMesh->GetNumFaces(); ++f )
                    {
                        // Copy the vertex data for all 3 vertices
                        CopyMemory( pNextOutVertex, pVBData + pdwIBData[f * 3], sizeof( SHADOWVERT ) );
                        CopyMemory( pNextOutVertex + 1, pVBData + pdwIBData[f * 3 + 1], sizeof( SHADOWVERT ) );
                        CopyMemory( pNextOutVertex + 2, pVBData + pdwIBData[f * 3 + 2], sizeof( SHADOWVERT ) );

                        // Write out the face
                        pdwNewIBData[nNextIndex++] = f * 3;
                        pdwNewIBData[nNextIndex++] = f * 3 + 1;
                        pdwNewIBData[nNextIndex++] = f * 3 + 2;

                        // Compute the face normal and assign it to
                        // the normals of the vertices.
                        D3DXVECTOR3 v1, v2;  // v1 and v2 are the edge vectors of the face
                        D3DXVECTOR3 vNormal;
                        v1 = *( D3DXVECTOR3* )( pNextOutVertex + 1 ) - *( D3DXVECTOR3* )pNextOutVertex;
                        v2 = *( D3DXVECTOR3* )( pNextOutVertex + 2 ) - *( D3DXVECTOR3* )( pNextOutVertex + 1 );
                        D3DXVec3Cross( &vNormal, &v1, &v2 );
                        D3DXVec3Normalize( &vNormal, &vNormal );

                        pNextOutVertex->Normal = vNormal;
                        ( pNextOutVertex + 1 )->Normal = vNormal;
                        ( pNextOutVertex + 2 )->Normal = vNormal;

                        pNextOutVertex += 3;

                        // Add the face's edges to the edge mapping table

                        // Edge 1
                        int nIndex;
                        int nVertIndex[3] =
                        {
                            pdwPtRep[pdwIBData[f * 3]],
                            pdwPtRep[pdwIBData[f * 3 + 1]],
                            pdwPtRep[pdwIBData[f * 3 + 2]]
                        };
                        nIndex = FindEdgeInMappingTable( nVertIndex[0], nVertIndex[1], pMapping, dwNumEdges );

                        // If error, we are not able to proceed, so abort.
                        if( -1 == nIndex )
                        {
                            hr = E_INVALIDARG;
                            goto cleanup;
                        }

                        if( pMapping[nIndex].m_anOldEdge[0] == -1 && pMapping[nIndex].m_anOldEdge[1] == -1 )
                        {
                            // No entry for this edge yet.  Initialize one.
                            pMapping[nIndex].m_anOldEdge[0] = nVertIndex[0];
                            pMapping[nIndex].m_anOldEdge[1] = nVertIndex[1];
                            pMapping[nIndex].m_aanNewEdge[0][0] = f * 3;
                            pMapping[nIndex].m_aanNewEdge[0][1] = f * 3 + 1;

                            ++nNumMaps;
                        }
                        else
                        {
                            // An entry is found for this edge.  Create
                            // a quad and output it.
                            assert( nNumMaps > 0 );

                            pMapping[nIndex].m_aanNewEdge[1][0] = f * 3;      // For clarity
                            pMapping[nIndex].m_aanNewEdge[1][1] = f * 3 + 1;

                            // First triangle
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[0][1];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[0][0];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[1][0];

                            // Second triangle
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[1][1];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[1][0];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[0][0];

                            // pMapping[nIndex] is no longer needed. Copy the last map entry
                            // over and decrement the map count.

                            pMapping[nIndex] = pMapping[nNumMaps - 1];
                            FillMemory( &pMapping[nNumMaps - 1], sizeof( pMapping[nNumMaps - 1] ), 0xFF );
                            --nNumMaps;
                        }

                        // Edge 2
                        nIndex = FindEdgeInMappingTable( nVertIndex[1], nVertIndex[2], pMapping, dwNumEdges );

                        // If error, we are not able to proceed, so abort.
                        if( -1 == nIndex )
                        {
                            hr = E_INVALIDARG;
                            goto cleanup;
                        }

                        if( pMapping[nIndex].m_anOldEdge[0] == -1 && pMapping[nIndex].m_anOldEdge[1] == -1 )
                        {
                            pMapping[nIndex].m_anOldEdge[0] = nVertIndex[1];
                            pMapping[nIndex].m_anOldEdge[1] = nVertIndex[2];
                            pMapping[nIndex].m_aanNewEdge[0][0] = f * 3 + 1;
                            pMapping[nIndex].m_aanNewEdge[0][1] = f * 3 + 2;

                            ++nNumMaps;
                        }
                        else
                        {
                            // An entry is found for this edge.  Create
                            // a quad and output it.
                            assert( nNumMaps > 0 );

                            pMapping[nIndex].m_aanNewEdge[1][0] = f * 3 + 1;
                            pMapping[nIndex].m_aanNewEdge[1][1] = f * 3 + 2;

                            // First triangle
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[0][1];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[0][0];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[1][0];

                            // Second triangle
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[1][1];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[1][0];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[0][0];

                            // pMapping[nIndex] is no longer needed. Copy the last map entry
                            // over and decrement the map count.

                            pMapping[nIndex] = pMapping[nNumMaps - 1];
                            FillMemory( &pMapping[nNumMaps - 1], sizeof( pMapping[nNumMaps - 1] ), 0xFF );
                            --nNumMaps;
                        }

                        // Edge 3
                        nIndex = FindEdgeInMappingTable( nVertIndex[2], nVertIndex[0], pMapping, dwNumEdges );

                        // If error, we are not able to proceed, so abort.
                        if( -1 == nIndex )
                        {
                            hr = E_INVALIDARG;
                            goto cleanup;
                        }

                        if( pMapping[nIndex].m_anOldEdge[0] == -1 && pMapping[nIndex].m_anOldEdge[1] == -1 )
                        {
                            pMapping[nIndex].m_anOldEdge[0] = nVertIndex[2];
                            pMapping[nIndex].m_anOldEdge[1] = nVertIndex[0];
                            pMapping[nIndex].m_aanNewEdge[0][0] = f * 3 + 2;
                            pMapping[nIndex].m_aanNewEdge[0][1] = f * 3;

                            ++nNumMaps;
                        }
                        else
                        {
                            // An entry is found for this edge.  Create
                            // a quad and output it.
                            assert( nNumMaps > 0 );

                            pMapping[nIndex].m_aanNewEdge[1][0] = f * 3 + 2;
                            pMapping[nIndex].m_aanNewEdge[1][1] = f * 3;

                            // First triangle
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[0][1];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[0][0];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[1][0];

                            // Second triangle
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[1][1];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[1][0];
                            pdwNewIBData[nNextIndex++] = pMapping[nIndex].m_aanNewEdge[0][0];

                            // pMapping[nIndex] is no longer needed. Copy the last map entry
                            // over and decrement the map count.

                            pMapping[nIndex] = pMapping[nNumMaps - 1];
                            FillMemory( &pMapping[nNumMaps - 1], sizeof( pMapping[nNumMaps - 1] ), 0xFF );
                            --nNumMaps;
                        }
                    }

                    // Now the entries in the edge mapping table represent
                    // non-shared edges.  What they mean is that the original
                    // mesh has openings (holes), so we attempt to patch them.
                    // First we need to recreate our mesh with a larger vertex
                    // and index buffers so the patching geometry could fit.

                    DXUTTRACE( L"Faces to patch: %d\n", nNumMaps );

                    // Create a mesh with large enough vertex and
                    // index buffers.

                    SHADOWVERT* pPatchVBData = NULL;
                    DWORD* pdwPatchIBData = NULL;

                    ID3DXMesh* pPatchMesh = NULL;
                    // Make enough room in IB for the face and up to 3 quads for each patching face
                    hr = D3DXCreateMesh( nNextIndex / 3 + nNumMaps * 7,
                                         ( pInputMesh->GetNumFaces() + nNumMaps ) * 3,
                                         D3DXMESH_32BIT,
                                         SHADOWVERT::Decl,
                                         pd3dDevice,
                                         &pPatchMesh );

                    if( FAILED( hr ) )
                        goto cleanup;

                    hr = pPatchMesh->LockVertexBuffer( 0, ( LPVOID* )&pPatchVBData );
                    if( SUCCEEDED( hr ) )
                        hr = pPatchMesh->LockIndexBuffer( 0, ( LPVOID* )&pdwPatchIBData );

                    if( pPatchVBData && pdwPatchIBData )
                    {
                        ZeroMemory( pPatchVBData, sizeof( SHADOWVERT ) * ( pInputMesh->GetNumFaces() + nNumMaps ) *
                                    3 );
                        ZeroMemory( pdwPatchIBData, sizeof( DWORD ) * ( nNextIndex + 3 * nNumMaps * 7 ) );

                        // Copy the data from one mesh to the other

                        CopyMemory( pPatchVBData, pNewVBData, sizeof( SHADOWVERT ) * pInputMesh->GetNumFaces() * 3 );
                        CopyMemory( pdwPatchIBData, pdwNewIBData, sizeof( DWORD ) * nNextIndex );
                    }
                    else
                    {
                        // Some serious error is preventing us from locking.
                        // Abort and return error.

                        pPatchMesh->Release();
                        goto cleanup;
                    }

                    // Replace pNewMesh with the updated one.  Then the code
                    // can continue working with the pNewMesh pointer.

                    pNewMesh->UnlockVertexBuffer();
                    pNewMesh->UnlockIndexBuffer();
                    pNewVBData = pPatchVBData;
                    pdwNewIBData = pdwPatchIBData;
                    pNewMesh->Release();
                    pNewMesh = pPatchMesh;

                    // Now, we iterate through the edge mapping table and
                    // for each shared edge, we generate a quad.
                    // For each non-shared edge, we patch the opening
                    // with new faces.

                    // nNextVertex is the index of the next vertex.
                    int nNextVertex = pInputMesh->GetNumFaces() * 3;

                    for( int i = 0; i < nNumMaps; ++i )
                    {
                        if( pMapping[i].m_anOldEdge[0] != -1 &&
                            pMapping[i].m_anOldEdge[1] != -1 )
                        {
                            // If the 2nd new edge indexes is -1,
                            // this edge is a non-shared one.
                            // We patch the opening by creating new
                            // faces.
                            if( pMapping[i].m_aanNewEdge[1][0] == -1 ||  // must have only one new edge
                                pMapping[i].m_aanNewEdge[1][1] == -1 )
                            {
                                // Find another non-shared edge that
                                // shares a vertex with the current edge.
                                for( int i2 = i + 1; i2 < nNumMaps; ++i2 )
                                {
                                    if( pMapping[i2].m_anOldEdge[0] != -1 &&       // must have a valid old edge
                                        pMapping[i2].m_anOldEdge[1] != -1 &&
                                        ( pMapping[i2].m_aanNewEdge[1][0] == -1 || // must have only one new edge
                                          pMapping[i2].m_aanNewEdge[1][1] == -1 ) )
                                    {
                                        int nVertShared = 0;
                                        if( pMapping[i2].m_anOldEdge[0] == pMapping[i].m_anOldEdge[1] )
                                            ++nVertShared;
                                        if( pMapping[i2].m_anOldEdge[1] == pMapping[i].m_anOldEdge[0] )
                                            ++nVertShared;

                                        if( 2 == nVertShared )
                                        {
                                            // These are the last two edges of this particular
                                            // opening. Mark this edge as shared so that a degenerate
                                            // quad can be created for it.

                                            pMapping[i2].m_aanNewEdge[1][0] = pMapping[i].m_aanNewEdge[0][0];
                                            pMapping[i2].m_aanNewEdge[1][1] = pMapping[i].m_aanNewEdge[0][1];
                                            break;
                                        }
                                        else if( 1 == nVertShared )
                                        {
                                            // nBefore and nAfter tell us which edge comes before the other.
                                            int nBefore, nAfter;
                                            if( pMapping[i2].m_anOldEdge[0] == pMapping[i].m_anOldEdge[1] )
                                            {
                                                nBefore = i;
                                                nAfter = i2;
                                            }
                                            else
                                            {
                                                nBefore = i2;
                                                nAfter = i;
                                            }

                                            // Found such an edge. Now create a face along with two
                                            // degenerate quads from these two edges.

                                            pNewVBData[nNextVertex] = pNewVBData[pMapping[nAfter].m_aanNewEdge[0][1]];
                                            pNewVBData[nNextVertex +
                                                1] = pNewVBData[pMapping[nBefore].m_aanNewEdge[0][1]];
                                            pNewVBData[nNextVertex +
                                                2] = pNewVBData[pMapping[nBefore].m_aanNewEdge[0][0]];
                                            // Recompute the normal
                                            D3DXVECTOR3 v1 = pNewVBData[nNextVertex + 1].Position -
                                                pNewVBData[nNextVertex].Position;
                                            D3DXVECTOR3 v2 = pNewVBData[nNextVertex + 2].Position -
                                                pNewVBData[nNextVertex + 1].Position;
                                            D3DXVec3Normalize( &v1, &v1 );
                                            D3DXVec3Normalize( &v2, &v2 );
                                            D3DXVec3Cross( &pNewVBData[nNextVertex].Normal, &v1, &v2 );
                                            pNewVBData[nNextVertex + 1].Normal = pNewVBData[nNextVertex +
                                                2].Normal = pNewVBData[nNextVertex].Normal;

                                            pdwNewIBData[nNextIndex] = nNextVertex;
                                            pdwNewIBData[nNextIndex + 1] = nNextVertex + 1;
                                            pdwNewIBData[nNextIndex + 2] = nNextVertex + 2;

                                            // 1st quad

                                            pdwNewIBData[nNextIndex + 3] = pMapping[nBefore].m_aanNewEdge[0][1];
                                            pdwNewIBData[nNextIndex + 4] = pMapping[nBefore].m_aanNewEdge[0][0];
                                            pdwNewIBData[nNextIndex + 5] = nNextVertex + 1;

                                            pdwNewIBData[nNextIndex + 6] = nNextVertex + 2;
                                            pdwNewIBData[nNextIndex + 7] = nNextVertex + 1;
                                            pdwNewIBData[nNextIndex + 8] = pMapping[nBefore].m_aanNewEdge[0][0];

                                            // 2nd quad

                                            pdwNewIBData[nNextIndex + 9] = pMapping[nAfter].m_aanNewEdge[0][1];
                                            pdwNewIBData[nNextIndex + 10] = pMapping[nAfter].m_aanNewEdge[0][0];
                                            pdwNewIBData[nNextIndex + 11] = nNextVertex;

                                            pdwNewIBData[nNextIndex + 12] = nNextVertex + 1;
                                            pdwNewIBData[nNextIndex + 13] = nNextVertex;
                                            pdwNewIBData[nNextIndex + 14] = pMapping[nAfter].m_aanNewEdge[0][0];

                                            // Modify mapping entry i2 to reflect the third edge
                                            // of the newly added face.

                                            if( pMapping[i2].m_anOldEdge[0] == pMapping[i].m_anOldEdge[1] )
                                            {
                                                pMapping[i2].m_anOldEdge[0] = pMapping[i].m_anOldEdge[0];
                                            }
                                            else
                                            {
                                                pMapping[i2].m_anOldEdge[1] = pMapping[i].m_anOldEdge[1];
                                            }
                                            pMapping[i2].m_aanNewEdge[0][0] = nNextVertex + 2;
                                            pMapping[i2].m_aanNewEdge[0][1] = nNextVertex;

                                            // Update next vertex/index positions

                                            nNextVertex += 3;
                                            nNextIndex += 15;

                                            break;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                // This is a shared edge.  Create the degenerate quad.

                                // First triangle
                                pdwNewIBData[nNextIndex++] = pMapping[i].m_aanNewEdge[0][1];
                                pdwNewIBData[nNextIndex++] = pMapping[i].m_aanNewEdge[0][0];
                                pdwNewIBData[nNextIndex++] = pMapping[i].m_aanNewEdge[1][0];

                                // Second triangle
                                pdwNewIBData[nNextIndex++] = pMapping[i].m_aanNewEdge[1][1];
                                pdwNewIBData[nNextIndex++] = pMapping[i].m_aanNewEdge[1][0];
                                pdwNewIBData[nNextIndex++] = pMapping[i].m_aanNewEdge[0][0];
                            }
                        }
                    }
                }

cleanup:;
                if( pNewVBData )
                {
                    pNewMesh->UnlockVertexBuffer();
                    pNewVBData = NULL;
                }
                if( pdwNewIBData )
                {
                    pNewMesh->UnlockIndexBuffer();
                    pdwNewIBData = NULL;
                }

                if( SUCCEEDED( hr ) )
                {
                    // At this time, the output mesh may have an index buffer
                    // bigger than what is actually needed, so we create yet
                    // another mesh with the exact IB size that we need and
                    // output it.  This mesh also uses 16-bit index if
                    // 32-bit is not necessary.

                    DXUTTRACE( L"Shadow volume has %u vertices, %u faces.\n",
                               ( pInputMesh->GetNumFaces() + nNumMaps ) * 3, nNextIndex / 3 );

                    bool bNeed32Bit = ( pInputMesh->GetNumFaces() + nNumMaps ) * 3 > 65535;
                    ID3DXMesh* pFinalMesh;
                    hr = D3DXCreateMesh( nNextIndex / 3,  // Exact number of faces
                                         ( pInputMesh->GetNumFaces() + nNumMaps ) * 3,
                                         D3DXMESH_WRITEONLY | ( bNeed32Bit ? D3DXMESH_32BIT : 0 ),
                                         SHADOWVERT::Decl,
                                         pd3dDevice,
                                         &pFinalMesh );
                    if( SUCCEEDED( hr ) )
                    {
                        pNewMesh->LockVertexBuffer( 0, ( LPVOID* )&pNewVBData );
                        pNewMesh->LockIndexBuffer( 0, ( LPVOID* )&pdwNewIBData );

                        SHADOWVERT* pFinalVBData = NULL;
                        WORD* pwFinalIBData = NULL;

                        pFinalMesh->LockVertexBuffer( 0, ( LPVOID* )&pFinalVBData );
                        pFinalMesh->LockIndexBuffer( 0, ( LPVOID* )&pwFinalIBData );

                        if( pNewVBData && pdwNewIBData && pFinalVBData && pwFinalIBData )
                        {
                            CopyMemory( pFinalVBData, pNewVBData, sizeof( SHADOWVERT ) *
                                        ( pInputMesh->GetNumFaces() + nNumMaps ) * 3 );

                            if( bNeed32Bit )
                                CopyMemory( pwFinalIBData, pdwNewIBData, sizeof( DWORD ) * nNextIndex );
                            else
                            {
                                for( int i = 0; i < nNextIndex; ++i )
                                    pwFinalIBData[i] = ( WORD )pdwNewIBData[i];
                            }
                        }

                        if( pNewVBData )
                            pNewMesh->UnlockVertexBuffer();
                        if( pdwNewIBData )
                            pNewMesh->UnlockIndexBuffer();
                        if( pFinalVBData )
                            pFinalMesh->UnlockVertexBuffer();
                        if( pwFinalIBData )
                            pFinalMesh->UnlockIndexBuffer();

                        // Release the old
                        pNewMesh->Release();
                        pNewMesh = pFinalMesh;
                    }

                    *ppOutMesh = pNewMesh;
                }
                else
                    pNewMesh->Release();
            }
            delete[] pMapping;
        }
        else
            hr = E_OUTOFMEMORY;
    }
    else
        hr = E_FAIL;

    if( pVBData )
        pInputMesh->UnlockVertexBuffer();

    if( pdwIBData )
        pInputMesh->UnlockIndexBuffer();

    delete[] pdwPtRep;
    pInputMesh->Release();

    return hr;
}
