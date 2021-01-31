// CubeMapGS.h
// Copyright (c) Microsoft Corporation. All rights reserved.
//
#include "D3D10Test.h"

class CCubeMapTest : public CD3D10Test
{
public:
                    CCubeMapTest();
    virtual         ~CCubeMapTest();

    virtual bool    Init();
    virtual void    Cleanup();

    bool            SetupFPC();
    void            ReleaseFPC();

    bool            UpdateFrame();
    bool            RenderScene( D3DX10MATRIX& mView, D3DX10MATRIX& mProj, bool bRenderCubeMap,
                                 ID3D10EffectTechnique* pTechnique );
    void            RenderSceneIntoCubeMap();
    bool            Render();

    HRESULT         SnapScreenshot( LPCTSTR szFileName );
    virtual void    DoKeyDown( DWORD dwKey );

private:

    bool m_bVisualize;
    float m_fLightRotation;  // Current light rotation about the Y axis
    bool m_bUseRenderTargetArray; // Whether render target array is used to render cube map in one pass

    ID3D10Effect* m_pEffect;
    ID3D10ElementLayout* m_pVertexLayout;
    ID3D10ElementLayout* m_pVertexLayoutCM;

    D3DX10VECTOR3 m_vLightPos;
    D3DX10VECTOR3 m_vCameraPos;

    CTempMesh m_MeshCar;
    CTempMesh m_MeshCarInnards;
    CTempMesh m_MeshCarGlass;
    CTempMesh m_MeshRoom;
    CTempMesh m_MeshMonitors;
    CTempMesh m_MeshArm;
    ID3D10TextureCube* m_pEnvMap;       // Environment map
    ID3D10RenderTargetView* m_pEnvMapRTV; // Render target view for the alpha map
    ID3D10RenderTargetView* m_apEnvMapOneRTV[6]; // 6 render target view, each view is used for 1 face of the env map
    ID3D10ShaderResourceView* m_pEnvMapSRV;  // Shader resource view for the cubic env map
    ID3D10ShaderResourceView* m_apEnvMapOneSRV[6]; // Single-face shader resource view
    ID3D10TextureCube* m_pEnvMapDepth;  // Depth stencil for the environment map
    ID3D10DepthStencilView* m_pEnvMapDSV; // Depth stencil view for environment map for all 6 faces
    ID3D10DepthStencilView* m_pEnvMapOneDSV; // Depth stencil view for environment map for all 1 face
    ID3D10Buffer* m_pVBVisual;    // Vertex buffer for quad used for visualization
    ID3D10Texture2D* m_pFalloffTexture;     //simple falloff texture for the car shader
    ID3D10ShaderResourceView* m_pFalloffTexRV;  //resource view for the falloff texture

    D3DX10MATRIX m_mWorldRoom;    // World matrix of the room
    D3DX10MATRIX m_mWorldArm;     // World matrix of the Arm
    D3DX10MATRIX m_mWorldCar;     // World matrix of the car

    D3DX10MATRIX    m_amCubeMapViewAdjust[6]; // Adjustment for view matrices when rendering the cube map
    D3DX10MATRIX m_mView;         // View matrix for the camera (2 for zoomed in/zoomed out)
    D3DX10MATRIX m_mProj;         // Projection matrix for final rendering
    D3DX10MATRIX m_mProjCM;       // Projection matrix for cubic env map rendering
    bool m_bAnimateCamera;        // Whether the camera movement is on

    // Effect handles
    ID3D10EffectVariable* m_pmWorldViewProj;
    ID3D10EffectVariable* m_pmWorldView;
    ID3D10EffectVariable* m_pmWorld;
    ID3D10EffectVariable* m_pmView;
    ID3D10EffectVariable* m_pmProj;
    ID3D10EffectVariable* m_ptxEnvMap;
    ID3D10EffectVariable* m_ptxFalloffMap;

    ID3D10EffectTechnique* m_pRenderCubeMapTech;
    ID3D10EffectTechnique* m_pRenderSceneTech;
    ID3D10EffectTechnique* m_pRenderEnvMappedSceneTech;
    ID3D10EffectTechnique* m_pRenderEnvMappedSceneNoTexTech;
    ID3D10EffectTechnique* m_pRenderEnvMappedGlassTech;
};
