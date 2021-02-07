///--------------------------------------------------------------------------------------
// File: geometry.h
//
// Developed by AMD Developer Relations team.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <d3dx11.h>
#include "SDKMesh.h"

// model vertex
struct Vertex {
    D3DXVECTOR3 position;
    D3DXVECTOR3 normal;
    D3DXVECTOR2 texCoord;
};

// model triangle
typedef struct {
    Vertex        v0;
    Vertex        v1;
    Vertex        v2;
    // cached data for optimized ray-triangle intersections
    D3DXVECTOR2   v0Proj;           // 2D projection of vertices along the dominant axis
    D3DXVECTOR2   v1Proj;
    D3DXVECTOR2   v2Proj;
    D3DXVECTOR3   faceNormal;
    float         d;                // distance from triangle plane to origin
    int           dominantAxis;     // dominant axis of the triangle plane
} Triangle;

// Bounding box structure
struct BBox {
    D3DXVECTOR3    bottomLeft;
    D3DXVECTOR3    topRight;
};

// Simple triangle mesh structure for hit detection
struct Model {
    Triangle    *triArray;
    unsigned    triCount;
    BBox        bounds;
    bool        textured;
};

// ray-triangle intersection data
struct Intersection {
    Triangle        *pTri;      // Pointer to mesh triangle that was intersected by ray
    D3DXVECTOR3     position;   // Intersection point on the triangle
    float           alpha;      // Alpha and beta are two of the barycentric coordinates of the intersection 
    float           beta;       // ... the third barycentric coordinate can be calculated by: 1- (alpha + beta).
    bool            valid;      // "valid" is set to true if an intersection was found
};

// simple vertex for creating normal and displacement maps
struct SimpleVertex {
    D3DXVECTOR3    position;
    D3DXVECTOR2    texCoord;
};

// Ray structure for calculating intersections
struct Ray {
    D3DXVECTOR3    origin;
    D3DXVECTOR3    direction;
};

// Creates a 3D box model mesh, including the associated vertex array
void CreateBoxModel( float xOrigin, float yOrigin, float zOrigin, float width, float height, float depth,
                     Model* pModel, Vertex** ppVB );

// Creates a 2D quadralateral mesh
void CreateQuadMesh( float minX, float minY, float maxX, float maxY, SimpleVertex** ppVB );

// Checks for an intersection between a ray and a model. Returns true if an intersection was found.
// If an intersection was found, the location in the model's decal texture is given in the offsets 
bool RayCastForDecalPosition( Ray ray, Model model, D3DXVECTOR3* pIntersection, Triangle** ppIntTri );

// Creates an orthonormal basis from a single vector
void CreateOrthonormalBasis( D3DXVECTOR3* v, D3DXVECTOR3* vNormal, D3DXVECTOR3* vBinormal, D3DXVECTOR3* vTangent );

// Converts a DXUT SDKMesh object to an internal Model format for hit detection. 
// Allocates the appropriate sized model structures.
void ConvertSDKMeshToModel( CDXUTSDKMesh* pMesh, Model* pModel );
