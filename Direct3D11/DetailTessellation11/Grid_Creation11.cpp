//--------------------------------------------------------------------------------------
// File: Grid_Creation11.cpp
//
// Various functions to create a grid.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "Grid_Creation11.h"

//--------------------------------------------------------------------------------------
// Macros
//--------------------------------------------------------------------------------------
// Only used for tangent space version of grid creation
#define GRID_OPTIMIZE_INDICES
#define GRID_OPTIMIZE_VERTICES

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
void GridOptimizeIndices( WORD* pIndexBuffer, int nNumIndex, int nNumVertex );
void GridOptimizeVertices( WORD* pIndexBuffer, void *pVertex, DWORD dwVertexSize, 
                           int nNumIndex, int nNumVertex );


//--------------------------------------------------------------------------------------
// FillGrid_NonIndexed
// Creates a regular grid of non-indexed triangles.
// The w coordinate of each vertex contains its index (vertex number). 
//
// Parameters:
//
// IN
// pd3dDevice:                  The D3D device
// dwWidth, dwLength:           Number of grid vertices in X and Z direction
// fGridSizeX, fGridSizeZ:      Grid extents in local space units
// uDummyStartVertices, uDummyEndVertices:  Add padding vertices to the start or end of buffer
//
// OUT
// lplpVB:                      A pointer to the vertex buffer containing grid vertices
//--------------------------------------------------------------------------------------
void FillGrid_NonIndexed( ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                          float fGridSizeX, float fGridSizeZ, 
                          DWORD uDummyStartVertices, DWORD uDummyEndVertices,
                          ID3D11Buffer** lplpVB )
{
    HRESULT hr;
    DWORD   nNumVertex = 3 * 2 * dwWidth * dwLength;
    float   fStepX = fGridSizeX / dwWidth;
    float   fStepZ = fGridSizeZ / dwLength;
    
    // Allocate memory for buffer of vertices in system memory
    SIMPLEVERTEX*    pVertexBuffer = new SIMPLEVERTEX[nNumVertex + uDummyStartVertices + uDummyEndVertices];
    SIMPLEVERTEX*    pVertex = &pVertexBuffer[0];

    // Skip a number of dummy start vertices if required
    pVertex += uDummyStartVertices;

    for ( DWORD i=0; i<dwLength; ++i )
    {
        for ( DWORD j=0; j<dwWidth; ++j )
        {
            // Vertex 0
            pVertex->x = -fGridSizeX/2.0f + j*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - i*fStepZ;
            pVertex->u = 0.0f + ( (float)j / dwWidth );
            pVertex->v = 0.0f + ( (float)i / dwLength );
            pVertex++;

            // Vertex 1
            pVertex->x = -fGridSizeX/2.0f + ( j+1 )*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - i*fStepZ;
            pVertex->u = 0.0f + ( (float)( j+1 ) / dwWidth );
            pVertex->v = 0.0f + ( (float)i      / dwLength );
            pVertex++;

            // Vertex 3
            pVertex->x = -fGridSizeX/2.0f + j*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - (i+1)*fStepZ;
            pVertex->u = 0.0f + ( (float)j / dwWidth );
            pVertex->v = 0.0f + ( (float)(i+1) / dwLength );
            pVertex++;

            // Vertex 3
            pVertex->x = -fGridSizeX/2.0f + j*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - (i+1)*fStepZ;
            pVertex->u = 0.0f + ( (float)j / dwWidth );
            pVertex->v = 0.0f + ( (float)( i+1 ) / dwLength );
            pVertex++;

            // Vertex 1
            pVertex->x = -fGridSizeX/2.0f + ( j+1 )*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - i*fStepZ;
            pVertex->u = 0.0f + ((float)(j+1)/dwWidth);
            pVertex->v = 0.0f + ((float)i/dwLength);
            pVertex++;

            // Vertex 2
            pVertex->x = -fGridSizeX/2.0f + ( j+1 )*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - ( i+1 )*fStepZ;
            pVertex->u = 0.0f + ( (float)( j+1 ) / dwWidth );
            pVertex->v = 0.0f + ( (float)( i+1 ) / dwLength );
            pVertex++;
        }
    }

    // Set initial data info
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertexBuffer;

    // Fill DX11 vertex buffer description
    D3D11_BUFFER_DESC     bd;
    bd.Usage =            D3D11_USAGE_DEFAULT;
    bd.ByteWidth =        sizeof( SIMPLEVERTEX ) * (nNumVertex + uDummyStartVertices + uDummyEndVertices);
    bd.BindFlags =        D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags =   0;
    bd.MiscFlags =        0;

    // Create DX11 vertex buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpVB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_NonIndexed: Failed to create vertex buffer.\n" );
        return;
    }
    DXUT_SetDebugName( *lplpVB, "FillGrid VB" );

    // Release host memory vertex buffer
    delete [] pVertexBuffer;
}


//--------------------------------------------------------------------------------------
// FillGrid_Indexed
// Creates a regular grid of indexed triangles
//
// Parameters:
//
// IN
// pd3dDevice:                The D3D device
// dwWidth, dwLength:         Number of grid vertices in X and Z direction
// fGridSizeX, fGridSizeZ:    Grid extents in local space units
//
// OUT
// lplpVB:                    A pointer to the vertex buffer containing grid vertices
// lplpIB:                    A pointer to the index buffer containing grid indices
//--------------------------------------------------------------------------------------
void FillGrid_Indexed( ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                       float fGridSizeX, float fGridSizeZ, 
                       ID3D11Buffer** lplpVB, ID3D11Buffer** lplpIB )
{
    HRESULT      hr;
    DWORD        nNumVertex = ( dwWidth+1 ) * ( dwLength+1 );
    DWORD        nNumIndex = 3 * 2 * dwWidth * dwLength;
    float        fStepX = fGridSizeX / dwWidth;
    float        fStepZ = fGridSizeZ / dwLength;
    
    // Allocate memory for buffer of vertices in system memory
    SIMPLEVERTEX*    pVertexBuffer = new SIMPLEVERTEX[nNumVertex];
    SIMPLEVERTEX*    pVertex = &pVertexBuffer[0];

    // Fill vertex buffer
    for ( DWORD i=0; i<=dwLength; ++i )
    {
        for ( DWORD j=0; j<=dwWidth; ++j )
        {
            pVertex->x = -fGridSizeX/2.0f + j*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - i*fStepZ;
            pVertex->u = 0.0f + ( (float)j / dwWidth );
            pVertex->v = 0.0f + ( (float)i / dwLength );
            pVertex++;
        }
    }
    
    // Set initial data info
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertexBuffer;

    // Fill DX11 vertex buffer description
    D3D11_BUFFER_DESC     bd;
    bd.Usage =            D3D11_USAGE_DEFAULT;
    bd.ByteWidth =        sizeof( SIMPLEVERTEX ) * nNumVertex;
    bd.BindFlags =        D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags =   0;
    bd.MiscFlags =        0;

    // Create DX11 vertex buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpVB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_Indexed: Failed to create vertex buffer.\n" );
        return;
    }
    DXUT_SetDebugName( *lplpVB, "FillGrid VB Idx");

    // Allocate memory for buffer of indices in system memory
    WORD*    pIndexBuffer = new WORD [nNumIndex];
    WORD*    pIndex = &pIndexBuffer[0];

    // Fill index buffer
    for ( DWORD i=0; i<dwLength; ++i )
    {
        for ( DWORD j=0; j<dwWidth; ++j )
        {
            *pIndex++ = (WORD)(   i     * ( dwWidth+1 ) + j );
            *pIndex++ = (WORD)(   i     * ( dwWidth+1 ) + j + 1 );
            *pIndex++ = (WORD)( ( i+1 ) * ( dwWidth+1 ) + j );

            *pIndex++ = (WORD)( ( i+1 ) * ( dwWidth+1 ) + j );
            *pIndex++ = (WORD)(   i     * ( dwWidth+1 ) + j + 1 );
            *pIndex++ = (WORD)( ( i+1 ) * ( dwWidth+1 ) + j + 1 );
        }
    }

    // Set initial data info
    InitData.pSysMem = pIndexBuffer;

    // Fill DX11 vertex buffer description
    bd.ByteWidth = sizeof( WORD ) * nNumIndex;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    
    // Create DX11 index buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpIB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_Indexed: Failed to create index buffer.\n" );
        return;
    }
    DXUT_SetDebugName( *lplpIB, "FillGrid IB" );

    // Release host memory index buffer
    delete [] pIndexBuffer;

    // Release host memory vertex buffer
    delete [] pVertexBuffer;
}


//--------------------------------------------------------------------------------------
// Helper function to calculate a normalized normal from a triangle
//--------------------------------------------------------------------------------------
D3DXVECTOR3 CalculateTriangleNormal( D3DXVECTOR3 *pP1, D3DXVECTOR3 *pP2, D3DXVECTOR3 *pP3 )
{
    D3DXVECTOR3 P21 = *pP2 - *pP1;
    D3DXVECTOR3 P31 = *pP3 - *pP1;

    D3DXVECTOR3 Normal;
    D3DXVec3Cross( &Normal, &P21, &P31 );
    D3DXVec3Normalize( &Normal, &Normal );

    return Normal;
}


//--------------------------------------------------------------------------------------
// FillGrid_WithNormals_Indexed
// Creates a regular grid of indexed triangles, including normals.
//
// Parameters:
//
// IN
// pd3dDevice:               The D3D device
// dwWidth, dwLength:        Number of grid vertices in X and Z direction
// fGridSizeX, fGridSizeZ:   Grid extents in local space units
//
// OUT
// lplpVB:                   A pointer to the vertex buffer containing grid vertices
// lplpIB:                   A pointer to the index buffer containing grid indices
//--------------------------------------------------------------------------------------
void FillGrid_WithNormals_Indexed( ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                                   float fGridSizeX, float fGridSizeZ, 
                                   ID3D11Buffer** lplpVB, ID3D11Buffer** lplpIB )
{
    HRESULT      hr;
    DWORD        nNumVertex = ( dwWidth + 1 ) * ( dwLength + 1 );
    DWORD        nNumIndex = 3 * 2 * dwWidth * dwLength;
    float        fStepX = fGridSizeX / dwWidth;
    float        fStepZ = fGridSizeZ / dwLength;
    
    // Allocate memory for buffer of vertices in system memory
    EXTENDEDVERTEX*    pVertexBuffer = new EXTENDEDVERTEX[nNumVertex];
    EXTENDEDVERTEX*    pVertex = &pVertexBuffer[0];

    // Fill vertex buffer
    for ( DWORD i=0; i<=dwLength; ++i )
    {
        for ( DWORD j=0; j<=dwWidth; ++j )
        {
            pVertex->x = -fGridSizeX/2.0f + j*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - i*fStepZ;
            pVertex->nx = 0.0f;
            pVertex->ny = 0.0f;
            pVertex->nz = 0.0f;
            pVertex->u = 0.0f + ( (float)j / dwWidth  );
            pVertex->v = 0.0f + ( (float)i / dwLength );
            pVertex++;
        }
    }

    // Allocate memory for buffer of indices in system memory
    WORD*    pIndexBuffer = new WORD [nNumIndex];
    WORD*    pIndex = &pIndexBuffer[0];

    // Fill index buffer
    for ( DWORD i=0; i<dwLength; ++i )
    {
        for ( DWORD j=0; j<dwWidth; ++j )
        {
            *pIndex++ = (WORD)(   i     * ( dwWidth+1 ) + j );
            *pIndex++ = (WORD)(   i     * ( dwWidth+1 ) + j + 1 );
            *pIndex++ = (WORD)( ( i+1 ) * ( dwWidth+1 ) + j );

            *pIndex++ = (WORD)( ( i+1 ) * (dwWidth+1) + j );
            *pIndex++ = (WORD)(   i     * (dwWidth+1) + j + 1 );
            *pIndex++ = (WORD)( ( i+1 ) * (dwWidth+1) + j + 1 );
        }
    }

    // Set initial data info
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pIndexBuffer;

    // Fill DX11 index buffer description
    D3D11_BUFFER_DESC    bd;
    bd.Usage =           D3D11_USAGE_DEFAULT;
    bd.ByteWidth =       sizeof( WORD ) * nNumIndex;
    bd.BindFlags =       D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags =  0;
    bd.MiscFlags =       0;
    
    // Create DX11 index buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpIB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_WithNormals_Indexed: Failed to create index buffer.\n" );
        return;
    }
    DXUT_SetDebugName( *lplpIB, "FillGrid IB Nrmls" );

    // Write normals into vertex buffer
    pVertex = &pVertexBuffer[0];

    // Loop through all indices
    for ( DWORD i=0; i<nNumIndex/3; ++i )
    {
        WORD i1 = pIndexBuffer[3*i + 0];
        WORD i2 = pIndexBuffer[3*i + 1];
        WORD i3 = pIndexBuffer[3*i + 2];
        D3DXVECTOR3 Normal = CalculateTriangleNormal( (D3DXVECTOR3 *)&pVertexBuffer[i1].x, 
                                                      (D3DXVECTOR3 *)&pVertexBuffer[i2].x, 
                                                      (D3DXVECTOR3 *)&pVertexBuffer[i3].x );

        // Add normal to each vertex for this triangle
        *( (D3DXVECTOR3 *)&pVertexBuffer[i1].nx ) += Normal;
        *( (D3DXVECTOR3 *)&pVertexBuffer[i2].nx ) += Normal;
        *( (D3DXVECTOR3 *)&pVertexBuffer[i3].nx ) += Normal;
    }

    // Final normalization pass
    for ( DWORD i=0; i<nNumVertex; ++i )
    {
        D3DXVec3Normalize( (D3DXVECTOR3 *)&pVertexBuffer[i].nx, (D3DXVECTOR3 *)&pVertexBuffer[i].nx );
    }


    // Set initial data info
    InitData.pSysMem = pVertexBuffer;

    // Fill DX11 vertex buffer description
    bd.Usage =            D3D11_USAGE_DEFAULT;
    bd.ByteWidth =        sizeof( EXTENDEDVERTEX ) * nNumVertex;
    bd.BindFlags =        D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags =   0;
    bd.MiscFlags =        0;

    // Create DX11 vertex buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpVB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_WithNormals_Indexed: Failed to create vertex buffer.\n" );
        return;
    }
    DXUT_SetDebugName( *lplpVB, "FillGrid VB Nrmls Idx" );

    // Release host memory index buffer
    delete [] pIndexBuffer;

    // Release host memory vertex buffer
    delete [] pVertexBuffer;

}


//--------------------------------------------------------------------------------------
// FillGrid_Quads_Indexed
// Creates a regular grid using indexed quads.
//
// Parameters:
//
// IN
// pd3dDevice:                  The D3D device
// dwWidth, dwLength:           Number of grid vertices in X and Z direction
// fGridSizeX, fGridSizeZ:      Grid extents in local space units
//
// OUT
// lplpVB:                      A pointer to the vertex buffer containing grid vertices
//
//--------------------------------------------------------------------------------------
void FillGrid_Quads_Indexed( ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                             float fGridSizeX, float fGridSizeZ, 
                             ID3D11Buffer** lplpVB, ID3D11Buffer** lplpIB )
{
    HRESULT      hr;
    DWORD        nNumVertex = ( dwWidth + 1 ) * ( dwLength + 1 );
    DWORD        nNumIndex = 4 * dwWidth * dwLength;
    float        fStepX = fGridSizeX / dwWidth;
    float        fStepZ = fGridSizeZ / dwLength;
    
    // Allocate memory for buffer of vertices in system memory
    SIMPLEVERTEX*    pVertexBuffer = new SIMPLEVERTEX[nNumVertex];
    SIMPLEVERTEX*    pVertex = &pVertexBuffer[0];

    // Fill vertex buffer
    for ( DWORD i=0; i<=dwLength; ++i )
    {
        for ( DWORD j=0; j<=dwWidth; ++j )
        {
            pVertex->x = -fGridSizeX/2.0f + j*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - i*fStepZ;
            pVertex->u = 0.0f + ( (float)j / dwWidth  );
            pVertex->v = 0.0f + ( (float)i / dwLength );
            pVertex++;
        }
    }
    
    // Set initial data info
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertexBuffer;

    // Fill DX11 vertex buffer description
    D3D11_BUFFER_DESC    bd;
    bd.Usage =           D3D11_USAGE_DEFAULT;
    bd.ByteWidth =       sizeof( SIMPLEVERTEX ) * nNumVertex;
    bd.BindFlags =       D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags =  0;
    bd.MiscFlags =       0;

    // Create DX11 vertex buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpVB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_Quads_Indexed: Failed to create vertex buffer.\n" );
        return;
    }
    DXUT_SetDebugName( *lplpVB, "FillGrid VB Quads (Idx)" );

    // Release host memory vertex buffer
    delete [] pVertexBuffer;


    // Allocate memory for buffer of indices in system memory
    WORD* pIndexBuffer = new WORD [nNumIndex];
    WORD* pIndex = &pIndexBuffer[0];

    // Fill index buffer
    for ( DWORD i=0; i<dwLength; ++i )
    {
        for ( DWORD j=0; j<dwWidth; ++j )
        {
            *pIndex++ = (WORD)(  i     * ( dwWidth+1 ) + j     );
            *pIndex++ = (WORD)(  i     * ( dwWidth+1 ) + j + 1 );
            *pIndex++ = (WORD)( ( i+1 )* ( dwWidth+1 ) + j + 1 );
            *pIndex++ = (WORD)( ( i+1 )* ( dwWidth+1 ) + j     );
        }
    }

    // Set initial data info
    InitData.pSysMem = pVertexBuffer;

    // Fill DX11 vertex buffer description
    bd.ByteWidth = sizeof( WORD ) * nNumIndex;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    
    // Create DX11 index buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpIB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_Quads_Indexed: Failed to create index buffer.\n" );
        return;
    }
    DXUT_SetDebugName( *lplpVB, "FillGrid IB Quads" );

    // Release host memory vertex buffer
    delete [] pIndexBuffer;
}

//--------------------------------------------------------------------------------------
// FillGrid_Quads_NonIndexed
// Creates a regular grid using non-indexed quads.
//
// Parameters:
//
// IN
// pd3dDevice:                The D3D device
// dwWidth, dwLength:         Number of grid vertices in X and Z direction
// fGridSizeX, fGridSizeZ:    Grid extents in local space units
//
// OUT
// lplpVB:                    A pointer to the vertex buffer containing grid vertices
//
//--------------------------------------------------------------------------------------
void FillGrid_Quads_NonIndexed( ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                                float fGridSizeX, float fGridSizeZ, 
                                ID3D11Buffer** lplpVB )
{
    HRESULT  hr;
    DWORD    nNumQuads = dwWidth * dwLength;
    DWORD    nNumVertex = 4 * nNumQuads;
    float    fStepX = fGridSizeX / dwWidth;
    float    fStepZ = fGridSizeZ / dwLength;
    
    // Allocate memory for buffer of vertices in system memory
    SIMPLEVERTEX*    pVertexBuffer = new SIMPLEVERTEX[nNumVertex];
    SIMPLEVERTEX*    pVertex = &pVertexBuffer[0];

    for ( DWORD i=0; i<dwLength; ++i )
    {
        for ( DWORD j=0; j<dwWidth; ++j )
        {
            // Vertex 0
            pVertex->x = -fGridSizeX/2.0f + j*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - i*fStepZ;
            pVertex->u = 0.0f + ( (float)j / dwWidth  );
            pVertex->v = 0.0f + ( (float)i / dwLength );
            pVertex++;

            // Vertex 1
            pVertex->x = -fGridSizeX/2.0f + (j+1)*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - i*fStepZ;
            pVertex->u = 0.0f + ( (float)(j+1)/ dwWidth);
            pVertex->v = 0.0f + ( (float) i   / dwLength);
            pVertex++;

            // Vertex 2
            pVertex->x = -fGridSizeX/2.0f + (j+1)*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - (i+1)*fStepZ;
            pVertex->u = 0.0f + ( (float)(j+1) / dwWidth  );
            pVertex->v = 0.0f + ( (float)(i+1) / dwLength );
            pVertex++;

            // Vertex 3
            pVertex->x = -fGridSizeX/2.0f + j*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - (i+1)*fStepZ;
            pVertex->u = 0.0f + ( (float) j   / dwWidth );
            pVertex->v = 0.0f + ( (float)(i+1)/ dwLength );
            pVertex++;
        }
    }
    
    // Set initial data info
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertexBuffer;

    // Fill DX11 vertex buffer description
    D3D11_BUFFER_DESC    bd;
    bd.Usage =           D3D11_USAGE_DEFAULT;
    bd.ByteWidth =       sizeof( SIMPLEVERTEX ) * nNumVertex;
    bd.BindFlags =       D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags =  0;
    bd.MiscFlags =       0;

    // Create DX11 vertex buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpVB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_Quads_NonIndexed: Failed to create vertex buffer.\n" );
        return;
    }
    DXUT_SetDebugName( *lplpVB, "FillGrid VB Quads" );

    // Release host memory vertex buffer
    delete [] pVertexBuffer;
}


//--------------------------------------------------------------------------------------
// FillGrid_Indexed_WithTangentSpace
// Creates a regular grid of indexed triangles
// Includes tangent space vectors: NORMAL, TANGENT & BINORMAL
//
// Parameters:
//
// IN
// pd3dDevice:                   The D3D device
// dwWidth, dwLength:            Number of grid vertices in X and Z direction
// fGridSizeX, fGridSizeZ:       Grid extents in local space units
//
// OUT
// lplpVB:                       A pointer to the vertex buffer containing grid vertices
// lplpIB:                       A pointer to the index buffer containing grid indices
//--------------------------------------------------------------------------------------
void FillGrid_Indexed_WithTangentSpace( ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                                        float fGridSizeX, float fGridSizeZ, 
                                        ID3D11Buffer** lplpVB, ID3D11Buffer** lplpIB )
{
    HRESULT hr;
    DWORD   nNumVertex = ( dwWidth + 1 ) * ( dwLength + 1 );
    DWORD   nNumIndex = 3 * 2 * dwWidth * dwLength;
    float   fStepX = fGridSizeX / dwWidth;
    float   fStepZ = fGridSizeZ / dwLength;
    
    // Allocate memory for buffer of vertices in system memory
    TANGENTSPACEVERTEX*    pVertexBuffer = new TANGENTSPACEVERTEX[nNumVertex];
    TANGENTSPACEVERTEX*    pVertex = &pVertexBuffer[0];

    // Fill vertex buffer
    for ( DWORD i=0; i<=dwLength; ++i )
    {
        for ( DWORD j=0; j<=dwWidth; ++j )
        {
            pVertex->x = -fGridSizeX/2.0f + j*fStepX;
            pVertex->y = 0.0f;
            pVertex->z = fGridSizeZ/2.0f - i*fStepZ;
            pVertex->u = 0.0f + ( (float)j / dwWidth  );
            pVertex->v = 0.0f + ( (float)i / dwLength );

            // Flat grid so tangent space is very basic; with more complex geometry 
            // you would have to export calculated tangent space vectors
            pVertex->nx = 0.0f;
            pVertex->ny = 1.0f;
            pVertex->nz = 0.0f;
            pVertex->bx = 0.0f;
            pVertex->by = 0.0f;
            pVertex->bz = -1.0f;
            pVertex->tx = 1.0f;
            pVertex->ty = 0.0f;
            pVertex->tz = 0.0f;
            pVertex++;
        }
    }
    
    // Allocate memory for buffer of indices in system memory
    WORD*    pIndexBuffer = new WORD [nNumIndex];
    WORD*    pIndex = &pIndexBuffer[0];

    // Fill index buffer
    for ( DWORD i=0; i<dwLength; ++i )
    {
        for ( DWORD j=0; j<dwWidth; ++j )
        {
            *pIndex++ = (WORD)(  i    * (dwWidth+1) + j     );
            *pIndex++ = (WORD)(  i    * (dwWidth+1) + j + 1 );
            *pIndex++ = (WORD)( (i+1) * (dwWidth+1) + j     );

            *pIndex++ = (WORD)( (i+1) * (dwWidth+1) + j     );
            *pIndex++ = (WORD)(  i    * (dwWidth+1) + j + 1 );
            *pIndex++ = (WORD)( (i+1) * (dwWidth+1) + j + 1 );
        }
    }

#ifdef GRID_OPTIMIZE_INDICES
    GridOptimizeIndices(pIndexBuffer, nNumIndex, nNumVertex);
#endif

#ifdef GRID_OPTIMIZE_VERTICES
    GridOptimizeVertices(pIndexBuffer, pVertexBuffer, sizeof(TANGENTSPACEVERTEX), 
                         nNumIndex, nNumVertex);
#endif


    // Actual VB creation

    // Set initial data info
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertexBuffer;

    // Fill DX11 vertex buffer description
    D3D11_BUFFER_DESC    bd;
    bd.Usage =           D3D11_USAGE_DEFAULT;
    bd.ByteWidth =       sizeof( TANGENTSPACEVERTEX ) * nNumVertex;
    bd.BindFlags =       D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags =  0;
    bd.MiscFlags =       0;

    // Create DX11 vertex buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpVB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_Indexed_WithTangentSpace: Failed to create vertex buffer.\n" );
        return;
    }
    DXUT_SetDebugName( *lplpVB, "FillGrid VB Tan Idx" );

    // Actual IB creation

    // Set initial data info
    InitData.pSysMem = pIndexBuffer;

    // Fill DX11 vertex buffer description
    bd.ByteWidth = sizeof( WORD ) * nNumIndex;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    
    // Create DX11 index buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpIB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_Indexed_WithTangentSpace: Failed to create index buffer.\n" );
        return;
    }
    DXUT_SetDebugName( *lplpIB, "FillGrid IB Tan" );

    // Release host memory vertex buffer
    delete [] pIndexBuffer;

    // Release host memory vertex buffer
    delete [] pVertexBuffer;
}


//--------------------------------------------------------------------------------------
// Optimize grid indices for post-vertex cache 
//--------------------------------------------------------------------------------------
void GridOptimizeIndices( WORD* pIndexBuffer, int nNumIndex, int nNumVertex )
{
    // Optimize faces for post-transform cache
    DWORD* pRemappedFaces = new DWORD [nNumIndex/3];
    D3DXOptimizeFaces( pIndexBuffer, nNumIndex/3, nNumVertex, FALSE, pRemappedFaces );

    // Allocate temporary index buffer and copy current indices into it
    WORD* pTmpIndexBuffer = new WORD [nNumIndex];
    memcpy( pTmpIndexBuffer, pIndexBuffer, nNumIndex * sizeof( WORD ) );
    
    // Remap triangles
    for ( int i=0; i < (int)nNumIndex/3; ++i )
    {
        int newFace = (int)pRemappedFaces[i];
        for ( int j=0; j < 3; ++j )
        {
            pIndexBuffer[newFace*3+j] = pTmpIndexBuffer[i*3+j];
        }
    }
    delete[] pTmpIndexBuffer;
    delete[] pRemappedFaces;
}


//--------------------------------------------------------------------------------------
// Optimize grid vertices and indices for pre-vertex cache 
//--------------------------------------------------------------------------------------
void GridOptimizeVertices( WORD* pIndexBuffer, void *pVertexBuffer, DWORD dwVertexSize, 
                           int nNumIndex, int nNumVertex )
{
    // Optimize vertices for pre-transform cache
    DWORD* pRemappedVertices = new DWORD [nNumVertex];
    D3DXOptimizeVertices( pIndexBuffer, nNumIndex/3, nNumVertex, FALSE, pRemappedVertices );

    // Allocate temporary vertex buffer and copy current vertices into it
    BYTE* pTmpVertexBuffer = new BYTE [nNumVertex * dwVertexSize];
    memcpy( pTmpVertexBuffer, (BYTE *)pVertexBuffer, nNumVertex*dwVertexSize );

    // Remap vertices
    for ( int i=0; i < (int)nNumVertex; ++i )
    {
        int newVertexIndex = (int)pRemappedVertices[i];
        
        memcpy( (BYTE *)pVertexBuffer+newVertexIndex*dwVertexSize, 
                (BYTE *)pTmpVertexBuffer + (i*dwVertexSize), 
                dwVertexSize );
    }

    // Remap indices
    for ( int i=0; i < (int)nNumIndex; ++i )
    {
        pIndexBuffer[i] = (WORD)pRemappedVertices[pIndexBuffer[i]];
    }

    delete[] pTmpVertexBuffer;
    delete[] pRemappedVertices;
}

