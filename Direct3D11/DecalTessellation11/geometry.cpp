///--------------------------------------------------------------------------------------
// File: geometry.cpp
//
// Builds the geometry for the scene. The model structure includes data for
// optimized ray casting.
//
// Developed by AMD Developer Relations team.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "geometry.h"

//--------------------------------------------------------------------------------------
// Assign the projected vertices depending on the dominant axis
//--------------------------------------------------------------------------------------
void AssignProjections( Triangle* pTri )
{
   D3DXVECTOR3 N;

   N.x = abs( pTri->faceNormal.x );
   N.y = abs( pTri->faceNormal.y );
   N.z = abs( pTri->faceNormal.z );

    if( N.x > N.y )
    {
        if( N.x > N.z )
        {
            // x is the dominant axis
            pTri->dominantAxis = 0;
            pTri->v0Proj.x = pTri->v0.position.y;
            pTri->v0Proj.y = pTri->v0.position.z;
            pTri->v1Proj.x = pTri->v1.position.y;
            pTri->v1Proj.y = pTri->v1.position.z;
            pTri->v2Proj.x = pTri->v2.position.y;
            pTri->v2Proj.y = pTri->v2.position.z;
        }
        else
        {
            // z is the dominant axis
            pTri->dominantAxis = 2;
            pTri->v0Proj.x = pTri->v0.position.x;
            pTri->v0Proj.y = pTri->v0.position.y;
            pTri->v1Proj.x = pTri->v1.position.x;
            pTri->v1Proj.y = pTri->v1.position.y;
            pTri->v2Proj.x = pTri->v2.position.x;
            pTri->v2Proj.y = pTri->v2.position.y;
        }
    }
    else
    {
        if( N.y > N.z )
        {
            // y is the dominant axis
            pTri->dominantAxis = 1;
            pTri->v0Proj.x = pTri->v0.position.x;
            pTri->v0Proj.y = pTri->v0.position.z;
            pTri->v1Proj.x = pTri->v1.position.x;
            pTri->v1Proj.y = pTri->v1.position.z;
            pTri->v2Proj.x = pTri->v2.position.x;
            pTri->v2Proj.y = pTri->v2.position.z;
        }
        else
        {
            // z is the dominant axis
            pTri->dominantAxis = 2;
            pTri->v0Proj.x = pTri->v0.position.x;
            pTri->v0Proj.y = pTri->v0.position.y;
            pTri->v1Proj.x = pTri->v1.position.x;
            pTri->v1Proj.y = pTri->v1.position.y;
            pTri->v2Proj.x = pTri->v2.position.x;
            pTri->v2Proj.y = pTri->v2.position.y;
        }
    }
}

//--------------------------------------------------------------------------------------
// Create an axis aligned rectangular prism model
//--------------------------------------------------------------------------------------
void CreateBoxModel( float xOrigin, float yOrigin, float zOrigin, float width, float height, float depth,
                     Model* pModel, Vertex** ppVBData )
{
    pModel->triCount = 12;
    float xMax = width / 2.0f;
    float yMax = height / 2.0f;
    float zMax = depth / 2.0f;
    pModel->bounds.bottomLeft.x = xOrigin - xMax;
    pModel->bounds.bottomLeft.y = yOrigin - yMax;
    pModel->bounds.bottomLeft.z = yOrigin - zMax;
    pModel->bounds.topRight.x = xOrigin + xMax;
    pModel->bounds.topRight.y = yOrigin + yMax;
    pModel->bounds.topRight.z = yOrigin + zMax;
    pModel->triArray = new Triangle[pModel->triCount];

    Triangle* pTri = pModel->triArray;
    D3DXVECTOR3 vertex;

    // Front Face
    pTri->v0.position.x = xOrigin + xMax;
    pTri->v0.position.y = yOrigin + yMax;
    pTri->v0.position.z = zOrigin - zMax;
    pTri->v1.position.x = xOrigin + xMax;
    pTri->v1.position.y = yOrigin - yMax;
    pTri->v1.position.z = zOrigin - zMax;
    pTri->v2.position.x = xOrigin - xMax;
    pTri->v2.position.y = yOrigin - yMax;
    pTri->v2.position.z = zOrigin - zMax;
    pTri->v0.texCoord.x = 0.5f;
    pTri->v0.texCoord.y = 0.25f;
    pTri->v1.texCoord.x = 0.5f;
    pTri->v1.texCoord.y = 0.5f;
    pTri->v2.texCoord.x = 0.25f;
    pTri->v2.texCoord.y = 0.5f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = 0.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = 0.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = -1.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    pTri->v0.position.x = xOrigin - xMax;
    pTri->v0.position.y = yOrigin - yMax;
    pTri->v0.position.z = zOrigin - zMax;
    pTri->v1.position.x = xOrigin - xMax;
    pTri->v1.position.y = yOrigin + yMax;
    pTri->v1.position.z = zOrigin - zMax;
    pTri->v2.position.x = xOrigin + xMax;
    pTri->v2.position.y = yOrigin + yMax;
    pTri->v2.position.z = zOrigin - zMax;
    pTri->v0.texCoord.x = 0.25f;
    pTri->v0.texCoord.y = 0.5f;
    pTri->v1.texCoord.x = 0.25f;
    pTri->v1.texCoord.y = 0.25f;
    pTri->v2.texCoord.x = 0.5f;
    pTri->v2.texCoord.y = 0.25f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = 0.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = 0.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = -1.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    // Back Face
    pTri->v0.position.x = xOrigin + xMax;
    pTri->v0.position.y = yOrigin - yMax;
    pTri->v0.position.z = zOrigin + zMax;
    pTri->v1.position.x = xOrigin + xMax;
    pTri->v1.position.y = yOrigin + yMax;
    pTri->v1.position.z = zOrigin + zMax;
    pTri->v2.position.x = xOrigin - xMax;
    pTri->v2.position.y = yOrigin + yMax;
    pTri->v2.position.z = zOrigin + zMax;
    pTri->v0.texCoord.x = 0.5f;
    pTri->v0.texCoord.y = 0.75f;
    pTri->v1.texCoord.x = 0.5f;
    pTri->v1.texCoord.y = 1.0f;
    pTri->v2.texCoord.x = 0.25f;
    pTri->v2.texCoord.y = 1.0f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = 0.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = 0.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = 1.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    pTri->v0.position.x = xOrigin - xMax;
    pTri->v0.position.y = yOrigin + yMax;
    pTri->v0.position.z = zOrigin + zMax;
    pTri->v1.position.x = xOrigin - xMax;
    pTri->v1.position.y = yOrigin - yMax;
    pTri->v1.position.z = zOrigin + zMax;
    pTri->v2.position.x = xOrigin + xMax;
    pTri->v2.position.y = yOrigin - yMax;
    pTri->v2.position.z = zOrigin + zMax;
    pTri->v0.texCoord.x = 0.25f;
    pTri->v0.texCoord.y = 1.0f;
    pTri->v1.texCoord.x = 0.25f;
    pTri->v1.texCoord.y = 0.75f;
    pTri->v2.texCoord.x = 0.5f;
    pTri->v2.texCoord.y = 0.75f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = 0.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = 0.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = 1.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    // Top Face
    pTri->v0.position.x = xOrigin + xMax;
    pTri->v0.position.y = yOrigin + yMax;
    pTri->v0.position.z = zOrigin + zMax;
    pTri->v1.position.x = xOrigin + xMax;
    pTri->v1.position.y = yOrigin + yMax;
    pTri->v1.position.z = zOrigin - zMax;
    pTri->v2.position.x = xOrigin - xMax;
    pTri->v2.position.y = yOrigin + yMax;
    pTri->v2.position.z = zOrigin - zMax;
    pTri->v0.texCoord.x = 0.5f;
    pTri->v0.texCoord.y = 0.0f;
    pTri->v1.texCoord.x = 0.5f;
    pTri->v1.texCoord.y = 0.25f;
    pTri->v2.texCoord.x = 0.25f;
    pTri->v2.texCoord.y = 0.25f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = 0.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = 1.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = 0.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    pTri->v0.position.x = xOrigin - xMax;
    pTri->v0.position.y = yOrigin + yMax;
    pTri->v0.position.z = zOrigin - zMax;
    pTri->v1.position.x = xOrigin - xMax;
    pTri->v1.position.y = yOrigin + yMax;
    pTri->v1.position.z = zOrigin + zMax;
    pTri->v2.position.x = xOrigin + xMax;
    pTri->v2.position.y = yOrigin + yMax;
    pTri->v2.position.z = zOrigin + zMax;
    pTri->v0.texCoord.x = 0.25f;
    pTri->v0.texCoord.y = 0.25f;
    pTri->v1.texCoord.x = 0.25f;
    pTri->v1.texCoord.y = 0.0f;
    pTri->v2.texCoord.x = 0.5f;
    pTri->v2.texCoord.y = 0.0f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = 0.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = 1.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = 0.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    // Bottom Face
    pTri->v0.position.x = xOrigin + xMax;
    pTri->v0.position.y = yOrigin - yMax;
    pTri->v0.position.z = zOrigin - zMax;
    pTri->v1.position.x = xOrigin + xMax;
    pTri->v1.position.y = yOrigin - yMax;
    pTri->v1.position.z = zOrigin + zMax;
    pTri->v2.position.x = xOrigin - xMax;
    pTri->v2.position.y = yOrigin - yMax;
    pTri->v2.position.z = zOrigin + zMax;
    pTri->v0.texCoord.x = 0.5f;
    pTri->v0.texCoord.y = 0.5f;
    pTri->v1.texCoord.x = 0.5f;
    pTri->v1.texCoord.y = 0.75f;
    pTri->v2.texCoord.x = 0.25f;
    pTri->v2.texCoord.y = 0.75f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = 0.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = -1.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = 0.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    pTri->v0.position.x = xOrigin - xMax;
    pTri->v0.position.y = yOrigin - yMax;
    pTri->v0.position.z = zOrigin + zMax;
    pTri->v1.position.x = xOrigin - xMax;
    pTri->v1.position.y = yOrigin - yMax;
    pTri->v1.position.z = zOrigin - zMax;
    pTri->v2.position.x = xOrigin + xMax;
    pTri->v2.position.y = yOrigin - yMax;
    pTri->v2.position.z = zOrigin - zMax;
    pTri->v0.texCoord.x = 0.25f;
    pTri->v0.texCoord.y = 0.75f;
    pTri->v1.texCoord.x = 0.25f;
    pTri->v1.texCoord.y = 0.5f;
    pTri->v2.texCoord.x = 0.5f;
    pTri->v2.texCoord.y = 0.5f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = 0.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = -1.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = 0.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    // Left Face
    pTri->v0.position.x = xOrigin - xMax;
    pTri->v0.position.y = yOrigin + yMax;
    pTri->v0.position.z = zOrigin - zMax;
    pTri->v1.position.x = xOrigin - xMax;
    pTri->v1.position.y = yOrigin - yMax;
    pTri->v1.position.z = zOrigin - zMax;
    pTri->v2.position.x = xOrigin - xMax;
    pTri->v2.position.y = yOrigin - yMax;
    pTri->v2.position.z = zOrigin + zMax;
    pTri->v0.texCoord.x = 0.25f;
    pTri->v0.texCoord.y = 0.25f;
    pTri->v1.texCoord.x = 0.25f;
    pTri->v1.texCoord.y = 0.5f;
    pTri->v2.texCoord.x = 0.0f;
    pTri->v2.texCoord.y = 0.5f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = -1.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = 0.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = 0.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    pTri->v0.position.x = xOrigin - xMax;
    pTri->v0.position.y = yOrigin - yMax;
    pTri->v0.position.z = zOrigin + zMax;
    pTri->v1.position.x = xOrigin - xMax;
    pTri->v1.position.y = yOrigin + yMax;
    pTri->v1.position.z = zOrigin + zMax;
    pTri->v2.position.x = xOrigin - xMax;
    pTri->v2.position.y = yOrigin + yMax;
    pTri->v2.position.z = zOrigin - zMax;
    pTri->v0.texCoord.x = 0.0f;
    pTri->v0.texCoord.y = 0.5f;
    pTri->v1.texCoord.x = 0.0f;
    pTri->v1.texCoord.y = 0.25f;
    pTri->v2.texCoord.x = 0.25f;
    pTri->v2.texCoord.y = 0.25f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = -1.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = 0.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = 0.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    // Right Face
    pTri->v0.position.x = xOrigin + xMax;
    pTri->v0.position.y = yOrigin + yMax;
    pTri->v0.position.z = zOrigin + zMax;
    pTri->v1.position.x = xOrigin + xMax;
    pTri->v1.position.y = yOrigin - yMax;
    pTri->v1.position.z = zOrigin + zMax;
    pTri->v2.position.x = xOrigin + xMax;
    pTri->v2.position.y = yOrigin - yMax;
    pTri->v2.position.z = zOrigin - zMax;
    pTri->v0.texCoord.x = 0.75f;
    pTri->v0.texCoord.y = 0.25f;
    pTri->v1.texCoord.x = 0.75f;
    pTri->v1.texCoord.y = 0.5f;
    pTri->v2.texCoord.x = 0.5f;
    pTri->v2.texCoord.y = 0.5f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = 1.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = 0.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = 0.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );
    pTri++;
    pTri->v0.position.x = xOrigin + xMax;
    pTri->v0.position.y = yOrigin - yMax;
    pTri->v0.position.z = zOrigin - zMax;
    pTri->v1.position.x = xOrigin + xMax;
    pTri->v1.position.y = yOrigin + yMax;
    pTri->v1.position.z = zOrigin - zMax;
    pTri->v2.position.x = xOrigin + xMax;
    pTri->v2.position.y = yOrigin + yMax;
    pTri->v2.position.z = zOrigin + zMax;
    pTri->v0.texCoord.x = 0.5f;
    pTri->v0.texCoord.y = 0.5f;
    pTri->v1.texCoord.x = 0.5f;
    pTri->v1.texCoord.y = 0.25f;
    pTri->v2.texCoord.x = 0.75f;
    pTri->v2.texCoord.y = 0.25f;
    pTri->faceNormal.x = pTri->v0.normal.x = pTri->v1.normal.x = pTri->v2.normal.x = 1.0f;
    pTri->faceNormal.y = pTri->v0.normal.y = pTri->v1.normal.y = pTri->v2.normal.y = 0.0f;
    pTri->faceNormal.z = pTri->v0.normal.z = pTri->v1.normal.z = pTri->v2.normal.z = 0.0f;
    vertex.x = pTri->v0.position.x;
    vertex.y = pTri->v0.position.y;
    vertex.z = pTri->v0.position.z;
    pTri->d = D3DXVec3Dot( &pTri->faceNormal, &vertex );
    AssignProjections( pTri );

    // initialize the mesh vertex buffer data
    Vertex* tmpVBData = new Vertex[pModel->triCount * 3];
    Vertex* pVBData = tmpVBData;
    for( UINT i = 0; i < pModel->triCount; i++ )
    {
        pVBData->position = pModel->triArray[i].v0.position;
        pVBData->texCoord = pModel->triArray[i].v0.texCoord;
        pVBData->normal = pModel->triArray[i].v0.normal;
        pVBData++;
        pVBData->position = pModel->triArray[i].v1.position;
        pVBData->texCoord = pModel->triArray[i].v1.texCoord;
        pVBData->normal = pModel->triArray[i].v1.normal;
        pVBData++;
        pVBData->position = pModel->triArray[i].v2.position;
        pVBData->texCoord = pModel->triArray[i].v2.texCoord;
        pVBData->normal = pModel->triArray[i].v2.normal;
        pVBData++;
    }

    *ppVBData = tmpVBData;
}

//--------------------------------------------------------------------------------------
// Create a 2 triangle quad
//--------------------------------------------------------------------------------------
void CreateQuadMesh( float minX, float minY, float maxX, float maxY, SimpleVertex** ppSimpleVBData )
{
    SimpleVertex* tmpSimpleVBData = new SimpleVertex[4];
    SimpleVertex* pSimpleVBData = tmpSimpleVBData;

    // vertex 0
    pSimpleVBData->position.x = minX;
    pSimpleVBData->position.y = minY;
    pSimpleVBData->position.z = 0;
    pSimpleVBData->texCoord.x = 0;
    pSimpleVBData->texCoord.y = 0;
    pSimpleVBData++;

    // vertex 1
    pSimpleVBData->position.x = maxX;
    pSimpleVBData->position.y = minY;
    pSimpleVBData->position.z = 0;
    pSimpleVBData->texCoord.x = 1;
    pSimpleVBData->texCoord.y = 0;
    pSimpleVBData++;

    // vertex 2
    pSimpleVBData->position.x = minX;
    pSimpleVBData->position.y = maxY;
    pSimpleVBData->position.z = 0;
    pSimpleVBData->texCoord.x = 0;
    pSimpleVBData->texCoord.y = 1;
    pSimpleVBData++;

    // vertex 3
    pSimpleVBData->position.x = maxX;
    pSimpleVBData->position.y = maxY;
    pSimpleVBData->position.z = 0;
    pSimpleVBData->texCoord.x = 1;
    pSimpleVBData->texCoord.y = 1;

    *ppSimpleVBData = tmpSimpleVBData;
}

//--------------------------------------------------------------------------------------
// Ray cast from the eye to see where on the mesh gets hit with the decal
//
// Algorithm based on Badouel's "An Efficient Ray-Polygon Intersection", Graphics Gems
//--------------------------------------------------------------------------------------
bool RayCastForDecalPosition( Ray ray, Model model, D3DXVECTOR3* pLocation, Triangle** ppIntTri )
{
    const float epsilon = 0.001f;
    const float maxRayDistance = 100000;
    float tMin = maxRayDistance;
    Intersection rayTriIntersection;
    rayTriIntersection.valid = false;
    
    // test for a ray/triangle intersection with the mesh polys
    for( UINT i = 0; i < model.triCount; i++ )
    {
        Triangle* pTri = &model.triArray[i];
        float NdotD = D3DXVec3Dot( &pTri->faceNormal, &ray.direction );
        if( abs( NdotD ) < epsilon )
        {
            // ray is parallel or nearly parallel to polygon plane
            continue;
        }
        float t = (pTri->d - D3DXVec3Dot(& pTri->faceNormal, &ray.origin ))/ NdotD;
        if( t <= 0 )
        {
            // ray origin is behind the triangle;
            continue;
        }
        if( t >= tMin )
        {
            // a closer intersection has been found already
            continue;
        }
        // compute the intersection between the ray and the triangle's plane
        D3DXVECTOR3 intersectionPoint = ray.origin + ray.direction * t;
        // find the interpolation parameters alpha and beta using 2D projections
        float alpha;
        float beta;
        D3DXVECTOR2 P;    // projection of the intersection onto an axis aligned plane
        switch( pTri->dominantAxis )
        {
            case 0:
                P.x = intersectionPoint.y;
                P.y = intersectionPoint.z;
                break;
            case 1:
                P.x = intersectionPoint.x;
                P.y = intersectionPoint.z;
                break;
            case 2:
            default:
                P.x = intersectionPoint.x;
                P.y = intersectionPoint.y;
        }
        float u0, u1, u2, v0, v1, v2;
        u0 = P.x - pTri->v0Proj.x;
        v0 = P.y - pTri->v0Proj.y;
        u1 = pTri->v1Proj.x - pTri->v0Proj.x;
        u2 = pTri->v2Proj.x - pTri->v0Proj.x;
        v1 = pTri->v1Proj.y - pTri->v0Proj.y;
        v2 = pTri->v2Proj.y - pTri->v0Proj.y;
        if( abs( u1 ) < epsilon )
        {
            beta = u0/u2;
            if( (beta >= 0) && (beta <= 1) )
            {
                alpha = (v0 - beta * v2) / v1;
                if( (alpha >= 0) && ((alpha + beta) <= 1) )
                {
                    rayTriIntersection.valid = true;
                    rayTriIntersection.alpha = alpha;
                    rayTriIntersection.beta = beta;
                    rayTriIntersection.pTri = pTri;
                    tMin = t;
                }
            }
        }
        else
        {
            beta = (v0*u1 - u0*v1) / (v2*u1 - u2*v1);
            if( (beta >= 0) && (beta <= 1) )
            {
                alpha = (u0 - beta * u2) / u1;
                if( (alpha >= 0) && ((alpha + beta) <= 1) )
                {
                    rayTriIntersection.valid = true;
                    rayTriIntersection.alpha = alpha;
                    rayTriIntersection.beta = beta;
                    rayTriIntersection.pTri = pTri;
                    tMin = t;
                }
            }
        }
    }

    if( !rayTriIntersection.valid )
    {
        *ppIntTri = NULL;
        return false;
    }
    else
    {
        // compute the location using barycentric coordinates
        Triangle* pTri = rayTriIntersection.pTri;
        float alpha = rayTriIntersection.alpha;
        float beta = rayTriIntersection.beta;
        pLocation->x = ((1 - (alpha + beta)) * pTri->v0.position.x) + alpha * pTri->v1.position.x
            + beta * pTri->v2.position.x;
        pLocation->y = ((1 - (alpha + beta)) * pTri->v0.position.y) + alpha * pTri->v1.position.y
            + beta * pTri->v2.position.y;
        pLocation->z = ((1 - (alpha + beta)) * pTri->v0.position.z) + alpha * pTri->v1.position.z
            + beta * pTri->v2.position.z;
        *ppIntTri = pTri;
        return true;
    }
}

//--------------------------------------------------------------------------------------
// Given an input vector, creates an orthonormal basis which can be used as a 
// tangent space
//--------------------------------------------------------------------------------------
void CreateOrthonormalBasis( D3DXVECTOR3* v, D3DXVECTOR3* vNormal, D3DXVECTOR3* vBinormal, D3DXVECTOR3* vTangent )
{
   D3DXVECTOR3 vAbsN;
   D3DXVECTOR3 vTemp;
   D3DXVECTOR3 vN;
   D3DXVECTOR3 vB;
   D3DXVECTOR3 vT;

   vAbsN.x = abs( v->x );
   vAbsN.y = abs( v->y );
   vAbsN.z = abs( v->z );

   // the normal is a vector in the opposite direction of v
   vN.x = -v->x;
   vN.y = -v->y;
   vN.z = -v->z;

    if( vAbsN.x > vAbsN.y )
    {
        if( vAbsN.x > vAbsN.z )
        {
            // x is the dominant axis
            vTemp.x = 0;
            vTemp.y = vN.x > 0.0f ? 1.0f : -1.0f;
            vTemp.z = 0;
        }
        else
        {
            // z is the dominant axis
            vTemp.x = 0;
            vTemp.y = vN.z > 0.0f ? 1.0f : -1.0f;
            vTemp.z = 0;
        }
    }
    else
    {
        if( vAbsN.y > vAbsN.z )
        {
            // y is the dominant axis
            vTemp.x = 0;
            vTemp.y = 0;
            vTemp.z = vN.y > 0.0f ? -1.0f : 1.0f;;
        }
        else
        {
            // z is the dominant axis
            vTemp.x = 0;
            vTemp.y = vN.z > 0.0f ? 1.0f : -1.0f;
            vTemp.z = 0;
        }
    }

    D3DXVec3Cross( &vB, &vTemp, &vN );
    D3DXVec3Cross( &vT, &vN, &vB );

    *vNormal = vN;
    *vBinormal = vB;
    *vTangent = vT;
}

//--------------------------------------------------------------------------------------
// Converts an SDK Mesh to the Model format so hit detection can be done.
// Note this only works on a small subset of all possible SDKMeshes, however this seems
// to be the most common type used in the DX SDK.
//--------------------------------------------------------------------------------------
void ConvertSDKMeshToModel( CDXUTSDKMesh* pCMesh, Model* pModel )
{

    assert( NULL != pCMesh );

    if( 0 < pCMesh->GetOutstandingBufferResources() )
    {
        pModel->triCount = 0;
        return;
    }

    assert( 1 == pCMesh->GetNumMeshes() ); // only allow 1 mesh for now

    SDKMESH_MESH* pMesh = pCMesh->GetMesh( 0 );

    assert( 1 == pMesh->NumVertexBuffers ); // only allow 1 VB for now

#if DEBUG
    DXGI_FORMAT ibFormat = pCMesh->GetIBFormat11( pMesh->IndexBuffer );
    assert( DXGI_FORMAT_R32_UINT == ibFormat );    // only allow 16 bit indices for now
#endif

    UINT* pIBData = (UINT*)pCMesh->GetRawIndicesAt( pMesh->IndexBuffer );

    assert( 1 == pMesh->NumSubsets ); // only allow 1 subset for now

    SDKMESH_SUBSET* pSubset = pCMesh->GetSubset( 0, 0 );

#if DEBUG
    D3D11_PRIMITIVE_TOPOLOGY primType = pCMesh->GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
    assert( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST == primType);    // only allow triangle lists for now
#endif

    SDKMESH_MATERIAL* pMat = pCMesh->GetMaterial( pSubset->MaterialID );
    if( pMat->pDiffuseRV11 )
    {
        pModel->textured = true;
    }
    else
    {
        pModel->textured = false;
    }
  
    UINT IndexCount = ( UINT )pSubset->IndexCount;
    UINT IndexStart = ( UINT )pSubset->IndexStart;

    pModel->triCount = IndexCount / 3;
    pModel->triArray = new Triangle[pModel->triCount];

    // get the bounds
    D3DXVECTOR3 vUpper = pMesh->BoundingBoxCenter + pMesh->BoundingBoxExtents;
    D3DXVECTOR3 vLower = pMesh->BoundingBoxCenter - pMesh->BoundingBoxExtents;
    pModel->bounds.bottomLeft = pMesh->BoundingBoxCenter - pMesh->BoundingBoxExtents;
    pModel->bounds.topRight = pMesh->BoundingBoxCenter + pMesh->BoundingBoxExtents;
    
    Triangle* pTri = pModel->triArray;
    Vertex* pVBData = (Vertex*)pCMesh->GetRawVerticesAt( 0 );
    D3DXVECTOR3 vEdge1;
    D3DXVECTOR3 vEdge2;
    UINT index;
    UINT* pIB = &pIBData[IndexStart];

    // Assign the triangle and vertex information for the model
    for( UINT i = 0; i < pModel->triCount; i++ )
    {
        // assign the VB data
        index = *pIB++;
        pTri->v0.position = pVBData[index].position;
        pTri->v0.normal = pVBData[index].normal;
        pTri->v0.texCoord = pVBData[index].texCoord;
        index = *pIB++;
        pTri->v1.position = pVBData[index].position;
        pTri->v1.normal = pVBData[index].normal;
        pTri->v1.texCoord = pVBData[index].texCoord;
        index = *pIB++;
        pTri->v2.position = pVBData[index].position;
        pTri->v2.normal = pVBData[index].normal;
        pTri->v2.texCoord = pVBData[index].texCoord;
        // calculate the face normal
        vEdge1 = pTri->v1.position - pTri->v0.position;
        vEdge2 = pTri->v2.position - pTri->v0.position;
        D3DXVec3Cross( &pTri->faceNormal, &vEdge1, &vEdge2 );
        D3DXVec3Normalize( &pTri->faceNormal, &pTri->faceNormal );
        pTri->d = D3DXVec3Dot( &pTri->faceNormal, &pTri->v0.position );
        AssignProjections( pTri );
        pTri++;
    }
}