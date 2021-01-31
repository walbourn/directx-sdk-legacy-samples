//----------------------------------------------------------------------------
// File: Config.h
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------

struct INPUT_MESH
{
    WCHAR strMeshFile[MAX_PATH];
    bool bIsBlockerMesh;
    D3DXVECTOR3 vTranslate;
    D3DXVECTOR3 vScale;
    float fYaw;
    float fPitch;
    float fRoll;
    DWORD dwNumSHMaterials;
    D3DXSHMATERIAL* pSHMaterials;
};


//--------------------------------------------------------------------------------------
struct SIMULATOR_OPTIONS
{
    DWORD dwNumMeshes;
    INPUT_MESH* pInputMeshes;

    // Settings
    DWORD dwOrder;
    DWORD dwNumRays;
    DWORD dwNumBounces;
    float fLengthScale;
    DWORD dwNumChannels;

    // Compression
    bool bEnableCompression;
    DWORD dwNumClusters;
    D3DXSHCOMPRESSQUALITYTYPE Quality;
    DWORD dwNumPCA;

    // Mesh Tessellation 
    bool bEnableTessellation;
    bool bRobustMeshRefine;
    float fRobustMeshRefineMinEdgeLength;
    DWORD dwRobustMeshRefineMaxSubdiv;
    bool bAdaptiveDL;
    float fAdaptiveDLMinEdgeLength;
    float fAdaptiveDLThreshold;
    DWORD dwAdaptiveDLMaxSubdiv;
    bool bAdaptiveBounce;
    float fAdaptiveBounceMinEdgeLength;
    float fAdaptiveBounceThreshold;
    DWORD dwAdaptiveBounceMaxSubdiv;

    // Output
    WCHAR   strOutputConcatPRTMesh[MAX_PATH];
    WCHAR   strOutputConcatBlockerMesh[MAX_PATH];
    WCHAR   strOutputTessellatedMesh[MAX_PATH];
    WCHAR   strOutputPRTBuffer[MAX_PATH];
    WCHAR   strOutputCompPRTBuffer[MAX_PATH];
    bool bBinaryXFile;
};


class COptionsFile
{
public:
            COptionsFile();
            ~COptionsFile();

    HRESULT LoadOptions( WCHAR* strFile, SIMULATOR_OPTIONS* pOptions );
    HRESULT SaveOptions( WCHAR* strFile, SIMULATOR_OPTIONS* pOptions );
    HRESULT ResetOptions( SIMULATOR_OPTIONS* pOptions );
    void    FreeOptions( SIMULATOR_OPTIONS* pOptions );
};


class CXMLHelper
{
public:
    static void     CreateNewValue( IXMLDOMDocument* pDoc, IXMLDOMNode* pNode, WCHAR* strName, WCHAR* strValue );
    static void     CreateNewValue( IXMLDOMDocument* pDoc, IXMLDOMNode* pNode, WCHAR* strName, DWORD nValue );
    static void     CreateNewValue( IXMLDOMDocument* pDoc, IXMLDOMNode* pNode, WCHAR* strName, float fValue );
    static void     CreateChildNode( IXMLDOMDocument* pDoc, IXMLDOMNode* pParentNode, WCHAR* strName, int nType,
                                     IXMLDOMNode** ppNewNode );

    static void     GetValue( IXMLDOMNode*& pNode, WCHAR* strName, WCHAR* strValue, int cchValue );
    static void     GetValue( IXMLDOMNode*& pNode, WCHAR* strName, int* pnValue );
    static void     GetValue( IXMLDOMNode*& pNode, WCHAR* strName, bool* pbValue );
    static void     GetValue( IXMLDOMNode*& pNode, WCHAR* strName, float* pfValue );
    static void     GetValue( IXMLDOMNode*& pNode, WCHAR* strName, D3DXCOLOR* pclrValue );
    static void     GetValue( IXMLDOMNode*& pNode, WCHAR* strName, DWORD* pdwValue );
    static HRESULT  GetChild( IXMLDOMNode*& pNode, WCHAR* strName );
    static void     GetParent( IXMLDOMNode*& pNode );
    static void     GetSibling( IXMLDOMNode*& pNode );
    static void     GetParentSibling( IXMLDOMNode*& pNode );
    static void     SkipCommentNodes( IXMLDOMNode*& pNode );
    static DWORD    GetNumberOfChildren( IXMLDOMNode* pNode, WCHAR* strName );
};

