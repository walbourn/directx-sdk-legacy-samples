//--------------------------------------------------------------------------------------
// File: Grid_Creation11.h
//
// Header file for Grid_Creation11.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#ifndef GRID_CREATION11_H
#define GRID_CREATION11_H

struct SIMPLEVERTEX 
{
		FLOAT x, y, z;
		FLOAT u, v;
};

struct EXTENDEDVERTEX 
{
		FLOAT x, y, z;
		FLOAT nx, ny, nz;
		FLOAT u, v;
};

struct TANGENTSPACEVERTEX 
{
		FLOAT x, y, z;
		FLOAT u, v;
		FLOAT nx, ny, nz;
		FLOAT bx, by, bz;
		FLOAT tx, ty, tz;
};

void FillGrid_NonIndexed(ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                         float fGridSizeX, float fGridSizeZ, 
                         DWORD uDummyStartVertices, DWORD uDummyEndVertices,
                         ID3D11Buffer** lplpVB);

void FillGrid_Indexed(ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                      float fGridSizeX, float fGridSizeZ, 
                      ID3D11Buffer** lplpVB, ID3D11Buffer** lplpIB);

void FillGrid_WithNormals_Indexed(ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                                  float fGridSizeX, float fGridSizeZ, 
                                  ID3D11Buffer** lplpVB, ID3D11Buffer** lplpIB);

void FillGrid_Quads_Indexed(ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                            float fGridSizeX, float fGridSizeZ, 
                            ID3D11Buffer** lplpVB, ID3D11Buffer** lplpIB);

void FillGrid_Quads_NonIndexed(ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                               float fGridSizeX, float fGridSizeZ, 
                               ID3D11Buffer** lplpVB);

void FillGrid_Indexed_WithTangentSpace(ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                                       float fGridSizeX, float fGridSizeZ, 
                                       ID3D11Buffer** lplpVB, ID3D11Buffer** lplpIB);

#endif
