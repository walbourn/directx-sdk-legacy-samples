//-----------------------------------------------------------------------------
// File: MultiAnimation.h
//
// Desc: Header file for the MultiAnimation library.  This contains the
//       declarations of
//
//       MultiAnimFrame              (no .cpp file)
//       MultiAnimMC                 (MultiAnimationLib.cpp)
//       CMultiAnimAllocateHierarchy (AllocHierarchy.cpp)
//       CMultiAnim                  (MultiAnimationLib.cpp)
//       CAnimInstance               (AnimationInstance.cpp)
//
// Copyright (c) Microsoft Corporation. All rights reserved
//-----------------------------------------------------------------------------


#ifndef __MULTIANIMATION_H__
#define __MULTIANIMATION_H__


#pragma warning( push, 3 )
#pragma warning(disable:4786 4788)
#include <vector>
#pragma warning( pop )
#pragma warning(disable:4786 4788)

#include <d3d9.h>
#include <d3dx9.h>


//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 




//-----------------------------------------------------------------------------
// Forward declarations

class CMultiAnim;
class CAnimInstance;




//-----------------------------------------------------------------------------
// Name: class CMultiAnimAllocateHierarchy
// Desc: Inheriting from ID3DXAllocateHierarchy, this class handles the
//       allocation and release of the memory used by animation frames and
//       meshes.  Applications derive their own version of this class so
//       that they can customize the behavior of allocation and release.
//-----------------------------------------------------------------------------
class CMultiAnimAllocateHierarchy : public ID3DXAllocateHierarchy
{
    // callback to create a D3DXFRAME-derived object and initialize it
    STDMETHOD( CreateFrame )( THIS_ LPCSTR Name, LPD3DXFRAME *ppNewFrame );
    // callback to create a D3DXMESHCONTAINER-derived object and initialize it
    STDMETHOD( CreateMeshContainer )( THIS_ LPCSTR Name, CONST D3DXMESHDATA * pMeshData,
                            CONST D3DXMATERIAL * pMaterials, CONST D3DXEFFECTINSTANCE * pEffectInstances,
                            DWORD NumMaterials, CONST DWORD * pAdjacency, LPD3DXSKININFO pSkinInfo,
                            LPD3DXMESHCONTAINER * ppNewMeshContainer );
    // callback to release a D3DXFRAME-derived object
    STDMETHOD( DestroyFrame )( THIS_ LPD3DXFRAME pFrameToFree );
    // callback to release a D3DXMESHCONTAINER-derived object
    STDMETHOD( DestroyMeshContainer )( THIS_ LPD3DXMESHCONTAINER pMeshContainerToFree );

public:
    CMultiAnimAllocateHierarchy();

    // Setup method
    STDMETHOD( SetMA )( THIS_ CMultiAnim *pMA );

private:
    CMultiAnim* m_pMA;
};




//-----------------------------------------------------------------------------
// Name: struct MultiAnimFrame
// Desc: Inherits from D3DXFRAME.  This represents an animation frame, or
//       bone.
//-----------------------------------------------------------------------------
struct MultiAnimFrame : public D3DXFRAME
{
};




//-----------------------------------------------------------------------------
// Name: struct MultiAnimMC
// Desc: Inherits from D3DXMESHCONTAINER.  This represents a mesh object
//       that gets its vertices blended and rendered based on the frame
//       information in its hierarchy.
//-----------------------------------------------------------------------------
struct MultiAnimMC : public D3DXMESHCONTAINER
{
    LPDIRECT3DTEXTURE9* m_apTextures;
    LPD3DXMESH m_pWorkingMesh;
    D3DXMATRIX* m_amxBoneOffsets;  // Bone offset matrices retrieved from pSkinInfo
    D3DXMATRIX** m_apmxBonePointers;  // Provides index to bone matrix lookup

    DWORD m_dwNumPaletteEntries;
    DWORD m_dwMaxNumFaceInfls;
    DWORD m_dwNumAttrGroups;
    LPD3DXBUFFER m_pBufBoneCombos;

    HRESULT SetupBonePtrs( D3DXFRAME* pFrameRoot );
};




//-----------------------------------------------------------------------------
// Name: class CMultiAnim
// Desc: This class encapsulates a mesh hierarchy (typically loaded from an
//       .X file).  It has a list of CAnimInstance objects that all share
//       the mesh hierarchy here, as well as using a copy of our animation
//       controller.  CMultiAnim loads and keeps an effect object that it
//       renders the meshes with.
//-----------------------------------------------------------------------------
class CMultiAnim
{
    friend class CMultiAnimAllocateHierarchy;
    friend class CAnimInstance;
    friend struct MultiAnimFrame;
    friend struct MultiAnimMC;

protected:

    LPDIRECT3DDEVICE9 m_pDevice;

    LPD3DXEFFECT m_pEffect;
    char* m_sTechnique;           // character rendering technique
    DWORD m_dwWorkingPaletteSize;
    D3DXMATRIX* m_amxWorkingPalette;

    std::vector <CAnimInstance*> m_v_pAnimInstances;     // must be at lesat 1; otherwise, clear all

    MultiAnimFrame* m_pFrameRoot;           // shared between all instances
    LPD3DXANIMATIONCONTROLLER m_pAC;                  // AC that all children clone from -- to clone clean, no keys

    // useful data an app can retrieve
    float m_fBoundingRadius;

private:

    HRESULT             CreateInstance( CAnimInstance** ppAnimInstance );
    HRESULT             SetupBonePtrs( MultiAnimFrame* pFrame );

public:

                        CMultiAnim();
    virtual             ~CMultiAnim();

    virtual HRESULT     Setup( LPDIRECT3DDEVICE9 pDevice, TCHAR sXFile[], TCHAR sFxFile[],
                               CMultiAnimAllocateHierarchy* pAH, LPD3DXLOADUSERDATA pLUD = NULL );
    virtual HRESULT     Cleanup( CMultiAnimAllocateHierarchy* pAH );

    LPDIRECT3DDEVICE9   GetDevice();
    LPD3DXEFFECT        GetEffect();
    DWORD               GetNumInstances();
    CAnimInstance* GetInstance( DWORD dwIdx );
    float               GetBoundingRadius();

    virtual HRESULT     CreateNewInstance( DWORD* pdwNewIdx );

    virtual void        SetTechnique( char* sTechnique );

    virtual HRESULT     Draw();
};




//-----------------------------------------------------------------------------
// Name: class CAnimInstance
// Desc: Encapsulates an animation instance, with its own animation controller.
//-----------------------------------------------------------------------------
class CAnimInstance
{
    friend class CMultiAnim;

protected:

    CMultiAnim* m_pMultiAnim;
    D3DXMATRIX m_mxWorld;
    LPD3DXANIMATIONCONTROLLER m_pAC;

private:

    virtual HRESULT Setup( LPD3DXANIMATIONCONTROLLER pAC );
    virtual void    UpdateFrames( MultiAnimFrame* pFrame, D3DXMATRIX* pmxBase );
    virtual void    DrawFrames( MultiAnimFrame* pFrame );
    virtual void    DrawMeshFrame( MultiAnimFrame* pFrame );

public:

                    CAnimInstance( CMultiAnim* pMultiAnim );
    virtual         ~CAnimInstance();

    virtual void    Cleanup();

    CMultiAnim* GetMultiAnim();
    void            GetAnimController( LPD3DXANIMATIONCONTROLLER* ppAC );

    D3DXMATRIX      GetWorldTransform();
    void            SetWorldTransform( const D3DXMATRIX* pmxWorld );

    virtual HRESULT AdvanceTime( DOUBLE dTimeDelta, ID3DXAnimationCallbackHandler* pCH );
    virtual HRESULT ResetTime();
    virtual HRESULT Draw();
};


#endif // #ifndef __MULTIANIMATION_H__
