// File provided by Masaki Kawase 
// http://www.daionet.gr.jp/~masa/rthdribl/

// GlareDefD3D.h : Define glare information
//

#ifndef _GLAREDEFD3DD3D_H_
#define _GLAREDEFD3DD3D_H_

#include <d3dx9.h>


//----------------------------------------------------------
// Star generation

// Define each line of the star.
typedef struct STARLINE
{
    int nPasses;
    float fSampleLength;
    float fAttenuation;
    float fInclination;

}*  LPSTARLINE;


// Simple definition of the star.
typedef struct STARDEF
{
    TCHAR* szStarName;
    int nStarLines;
    int nPasses;
    float fSampleLength;
    float fAttenuation;
    float fInclination;
    bool bRotation;

}*  LPSTARDEF;


// Simple definition of the sunny cross filter
typedef struct STARDEF_SUNNYCROSS
{
    TCHAR* szStarName;
    float fSampleLength;
    float fAttenuation;
    float fInclination;

}*  LPSTARDEF_SUNNYCROSS;


// Star form library
enum ESTARLIBTYPE
{
    STLT_DISABLE    = 0,

    STLT_CROSS,
    STLT_CROSSFILTER,
    STLT_SNOWCROSS,
    STLT_VERTICAL,
    NUM_BASESTARLIBTYPES,

    STLT_SUNNYCROSS = NUM_BASESTARLIBTYPES,

    NUM_STARLIBTYPES,
};


//----------------------------------------------------------
// Star generation object

class CStarDef
{
public:
    TCHAR               m_strStarName[256];

    int m_nStarLines;
    LPSTARLINE m_pStarLine;   // [m_nStarLines]
    float m_fInclination;
    bool m_bRotation;   // Rotation is available from outside ?

    // Static library
public:
    static CStarDef* ms_pStarLib;
    static D3DXCOLOR    ms_avChromaticAberrationColor[8];

    // Public method
public:
                        CStarDef();
                        CStarDef( const CStarDef& src );
                        ~CStarDef();

    CStarDef& operator =( const CStarDef& src )
    {
        Initialize( src );
        return *this;
    }

    HRESULT             Construct();
    void                Destruct();
    void                Release();

    HRESULT             Initialize( const CStarDef& src );

    HRESULT             Initialize( ESTARLIBTYPE eType )
    {
        return Initialize( ms_pStarLib[eType] );
    }

    /// Generic simple star generation
    HRESULT             Initialize( const TCHAR* szStarName,
                                    int nStarLines,
                                    int nPasses,
                                    float fSampleLength,
                                    float fAttenuation,
                                    float fInclination,
                                    bool bRotation );

    HRESULT             Initialize( const STARDEF& starDef )
    {
        return Initialize( starDef.szStarName,
                           starDef.nStarLines,
                           starDef.nPasses,
                           starDef.fSampleLength,
                           starDef.fAttenuation,
                           starDef.fInclination,
                           starDef.bRotation );
    }

    /// Specific star generation
    //  Sunny cross filter
    HRESULT             Initialize_SunnyCrossFilter( const TCHAR* szStarName = TEXT( "SunnyCross" ),
                                                     float fSampleLength = 1.0f,
                                                     float fAttenuation = 0.88f,
                                                     float fLongAttenuation = 0.95f,
                                                     float fInclination = D3DXToRadian( 0.0f ) );


    // Public static method
public:
    /// Create star library
    static HRESULT      InitializeStaticStarLibs();
    static HRESULT      DeleteStaticStarLibs();

    /// Access to the star library
    static const CStarDef& GetLib( DWORD dwType )
    {
        return ms_pStarLib[dwType];
    }

    static const D3DXCOLOR& GetChromaticAberrationColor( DWORD dwID )
    {
        return ms_avChromaticAberrationColor[dwID];
    }
};



//----------------------------------------------------------
// Clare definition

// Glare form library
enum EGLARELIBTYPE
{
    GLT_DISABLE = 0,

    GLT_CAMERA,
    GLT_NATURAL,
    GLT_CHEAPLENS,
    //GLT_AFTERIMAGE,
    GLT_FILTER_CROSSSCREEN,
    GLT_FILTER_CROSSSCREEN_SPECTRAL,
    GLT_FILTER_SNOWCROSS,
    GLT_FILTER_SNOWCROSS_SPECTRAL,
    GLT_FILTER_SUNNYCROSS,
    GLT_FILTER_SUNNYCROSS_SPECTRAL,
    GLT_CINECAM_VERTICALSLITS,
    GLT_CINECAM_HORIZONTALSLITS,

    NUM_GLARELIBTYPES,
    GLT_USERDEF = -1,
    GLT_DEFAULT = GLT_FILTER_CROSSSCREEN,
};


// Simple glare definition
typedef struct GLAREDEF
{
    TCHAR* szGlareName;
    float fGlareLuminance;

    float fBloomLuminance;
    float fGhostLuminance;
    float fGhostDistortion;
    float fStarLuminance;
    ESTARLIBTYPE eStarType;
    float fStarInclination;

    float fChromaticAberration;

    float fAfterimageSensitivity;    // Current weight
    float fAfterimageRatio;          // Afterimage weight
    float fAfterimageLuminance;

}*  LPGLAREDEF;


//----------------------------------------------------------
// Glare definition

class CGlareDef
{
public:
    
    WCHAR           m_strGlareName[256];

    float m_fGlareLuminance;     // Total glare intensity (not effect to "after image")
    float m_fBloomLuminance;
    float m_fGhostLuminance;
    float m_fGhostDistortion;
    float m_fStarLuminance;
    float m_fStarInclination;

    float m_fChromaticAberration;

    float m_fAfterimageSensitivity;  // Current weight
    float m_fAfterimageRatio;        // Afterimage weight
    float m_fAfterimageLuminance;

    CStarDef m_starDef;

    // Static library
public:
    static CGlareDef* ms_pGlareLib;

    // Public method
public:
                    CGlareDef();
                    CGlareDef( const CGlareDef& src );
                    ~CGlareDef();

    CGlareDef& operator =( const CGlareDef& src )
    {
        Initialize( src );
        return *this;
    }

    HRESULT         Construct();
    void            Destruct();
    void            Release();

    HRESULT         Initialize( const CGlareDef& src );

    HRESULT         Initialize( const TCHAR* szStarName,
                                float fGlareLuminance,
                                float fBloomLuminance,
                                float fGhostLuminance,
                                float fGhostDistortion,
                                float fStarLuminance,
                                ESTARLIBTYPE eStarType,
                                float fStarInclination,
                                float fChromaticAberration,
                                float fAfterimageSensitivity,    // Current weight
                                float fAfterimageRatio,          // After Image weight
                                float fAfterimageLuminance );

    HRESULT         Initialize( const GLAREDEF& glareDef )
    {
        return Initialize( glareDef.szGlareName,
                           glareDef.fGlareLuminance,
                           glareDef.fBloomLuminance,
                           glareDef.fGhostLuminance,
                           glareDef.fGhostDistortion,
                           glareDef.fStarLuminance,
                           glareDef.eStarType,
                           glareDef.fStarInclination,
                           glareDef.fChromaticAberration,
                           glareDef.fAfterimageSensitivity,
                           glareDef.fAfterimageRatio,
                           glareDef.fAfterimageLuminance );
    }

    HRESULT         Initialize( EGLARELIBTYPE eType )
    {
        return Initialize( ms_pGlareLib[eType] );
    }


    // Public static method
public:
    /// Create glare library
    static HRESULT  InitializeStaticGlareLibs();
    static HRESULT  DeleteStaticGlareLibs();

    /// Access to the glare library
    static const CGlareDef& GetLib( DWORD dwType )
    {
        return ms_pGlareLib[dwType];
    }
};


//----------------------------------------------------------
// Dummy to generate static object of glare
class __CGlare_GenerateStaticObjects
{
public:
__CGlare_GenerateStaticObjects()
{
    CStarDef::InitializeStaticStarLibs();
    CGlareDef::InitializeStaticGlareLibs();
}

~__CGlare_GenerateStaticObjects()
{
    CGlareDef::DeleteStaticGlareLibs();
    CStarDef::DeleteStaticStarLibs();
}

    static __CGlare_GenerateStaticObjects ms_staticObject;
};


#endif  // #ifndef _GLAREDEFD3DD3D_H_
