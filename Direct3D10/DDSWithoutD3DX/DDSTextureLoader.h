//--------------------------------------------------------------------------------------
// File: DDSTextureLoader.h
//
// Functions for loading a DDS texture without using D3DX
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------

#include <d3d9.h>
#include <d3d10_1.h>

HRESULT CreateDDSTextureFromFile( __in LPDIRECT3DDEVICE9 pDev, __in_z const WCHAR* szFileName, __out_opt LPDIRECT3DBASETEXTURE9* ppTex );
HRESULT CreateDDSTextureFromFile( __in LPDIRECT3DDEVICE9 pDev, __in_z const WCHAR* szFileName, __out_opt LPDIRECT3DTEXTURE9* ppTex );
HRESULT CreateDDSTextureFromFile( __in LPDIRECT3DDEVICE9 pDev, __in_z const WCHAR* szFileName, __out_opt LPDIRECT3DCUBETEXTURE9* ppTex );
HRESULT CreateDDSTextureFromFile( __in LPDIRECT3DDEVICE9 pDev, __in_z const WCHAR* szFileName, __out_opt LPDIRECT3DVOLUMETEXTURE9* ppTex );

HRESULT CreateDDSTextureFromFile( __in ID3D10Device1* pDev, __in_z const WCHAR* szFileName, __out_opt ID3D10ShaderResourceView1** ppSRV, bool sRGB = false );