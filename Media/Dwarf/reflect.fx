//-----------------------------------------------------------------------------
// File: Reflect.fx
//
// Desc: The technique implements:
//
//       Texture mapping
//       Diffuse lighting
//       Specular lighting
//       Environment mapping
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


int sas : SasGlobal
<
	bool SasUiVisible = false;
	int3 SasVersion= {1,1,0};
>;

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
texture g_txEnvMap 
<
	string SasUiLabel = "Environment Map";
	string SasUiControl = "FilePicker";
	string type="cube";
>; // texture for environment map

texture g_txScene
<
	string SasUiLabel = "Texture Map";
	string SasUiControl = "FilePicker";
>;  // texture for scene rendering

float4x4 g_mWorld : WORLD
<
	bool SasUiVisible = false;
	string SasBindAddress= "Sas.Skeleton.MeshToJointToWorld[0]";
>;        


float4x4 g_mView : VIEW
<
	bool SasUiVisible = false;
	string SasBindAddress= "Sas.Camera.WorldToView";
>;   


float4x4 g_mProj : PROJECTION
<
	bool SasUiVisible = false;
	string SasBindAddress= "Sas.Camera.Projection";
>;

float4 g_vLightColor
<  
	bool SasUiVisible = false;
	string SasBindAddress= "Sas.PointLight[0].Color";
> = {1.0f, 1.0f, 1.0f, 1.0f}; // Light value


float3 g_vLight
<  
	bool SasUiVisible = false;
	string SasBindAddress= "Sas.PointLight[0].Position";
> = {0.0f, 3.0f, -100.0f};

// Object material attributes
float4 Diffuse
<
	string SasUiControl = "ColorPicker";
> = { 0.5f, 0.5f, 0.5, 1.0f};      // Diffuse color of the material

float4 Specular
<
	string SasUiControl = "ColorPicker";
> = {1.0f, 1.0f, 1.0f, 1.0f};  // Specular color of the material
float  Power
<
    string SasUiLabel = "Specular Power";
    string SasUiControl = "Slider"; 
    float SasUiMin = 1.0f; 
    float SasUiMax = 32.0f; 
    int SasUiSteps = 31;

> = 8.0f;

float  Reflectivity
<
    string SasUiLabel = "Material Reflectivity";
    string SasUiControl = "Slider"; 
    float SasUiMin = 0.0f; 
    float SasUiMax = 1.0f; 
    int SasUiSteps = 100;
> = 0.5f; // Reflectivity of the material

//-----------------------------------------------------------------------------
// Texture samplers
//-----------------------------------------------------------------------------
sampler g_samScene<bool SasUiVisible = false;> =
sampler_state
{
    Texture = <g_txScene>;
    MinFilter = Linear;
    MagFilter = Linear;
    MipFilter = None;
};

sampler g_samEnv<bool SasUiVisible = false;> =
sampler_state
{
    Texture = <g_txEnvMap>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
};


//-----------------------------------------------------------------------------
// Name: VertScene
// Type: Vertex shader
// Desc: This shader computes standard transform and lighting
//-----------------------------------------------------------------------------
void VertScene( float4 vPos : POSITION,
                float3 vNormal : NORMAL,
                float2 vTex0 : TEXCOORD0,
                out float4 oPos : POSITION,
                out float4 oDiffuse : COLOR0,
                out float2 oTex0 : TEXCOORD0,
                out float3 oViewPos : TEXCOORD1,
                out float3 oViewNormal : TEXCOORD2,
                out float3 oEnvTex : TEXCOORD3 )
{
    // Transform the position from object space to homogeneous projection space
    float4x4 g_mWorldView= mul(g_mWorld, g_mView);
    float4x4 g_mWorldViewProjection= mul(g_mWorldView, g_mProj);
 	float4 lightInView= mul( g_vLight, g_mView);
  
    oPos = mul( vPos, g_mWorldViewProjection );
//    oPos = mul( vPos, g_mWorldView );
//    oPos = mul( oPos, g_mProj );

    // Compute the view-space position
    oViewPos = mul( vPos, g_mWorldView );

    // Compute view-space normal
    oViewNormal = normalize( mul( vNormal, (float3x3)g_mWorldView ) );

    // Compute lighting
    oDiffuse = float4( dot( oViewNormal, normalize( lightInView - oViewPos ) ) * Diffuse.xyz, 1.0f);;

    // Just copy the texture coordinate through
    oTex0 = vTex0;

    // Compute the texture coord for the environment map.
    oEnvTex = 2 * dot( -oViewPos, oViewNormal ) * oViewNormal + oViewPos;
    oEnvTex = mul( oEnvTex, transpose((float3x3)g_mView) );
}


//-----------------------------------------------------------------------------
// Name: PixScene
// Type: Pixel shader
// Desc: This shader outputs the pixel's color by modulating the texture's
//		 color with diffuse material color
//-----------------------------------------------------------------------------
float4 PixScene( float4 MatDiffuse : COLOR0,
                 float2 Tex0 : TEXCOORD0,
                 float3 ViewPos : TEXCOORD1,
                 float3 ViewNormal : TEXCOORD2,
                 float3 EnvTex : TEXCOORD3 ) : COLOR0
{

	float4 lightInView= mul( g_vLight, g_mView);

    // Compute half vector for specular lighting
    float3 vHalf = normalize( normalize( -ViewPos ) + normalize( lightInView - ViewPos ) );

    // Compute normal dot half for specular light
    float fSpecular = pow( saturate( dot( vHalf, normalize( ViewNormal ) ) ), Power );

    // Lookup mesh texture and modulate it with diffuse

    return float4( (float3)( g_vLightColor * ( tex2D( g_samScene, Tex0 ) * MatDiffuse + fSpecular ) * ( 1 - Reflectivity )
                             + texCUBE( g_samEnv, EnvTex ) * Reflectivity
                           ), 1.0f );

                           
}


void VertScene1x( float4 vPos : POSITION,
                  float3 vNormal : NORMAL,
                  float2 vTex0 : TEXCOORD0,
                  out float4 oPos : POSITION,
                  out float4 oDiffuse : COLOR0,
                  out float4 oSpecular : COLOR1,
                  out float2 oTex0 : TEXCOORD0,
                  out float3 oEnvTex : TEXCOORD1 )
{
    float4x4 g_mWorldView= mul(g_mWorld, g_mView);
    float4x4 g_mWorldViewProjection= mul(g_mWorldView, g_mProj);
 	float4 lightInView= mul( g_vLight, g_mView);

    // Transform the position from object space to homogeneous projection space
    oPos = mul( vPos, g_mWorldViewProjection );

    // Compute the view-space position
    float4 ViewPos = mul( vPos, g_mWorldView );

    // Compute view-space normal
    float3 ViewNormal = normalize( mul( vNormal, (float3x3)g_mWorldView ) );

    // Compute diffuse lighting
    oDiffuse = dot( ViewNormal, normalize( lightInView - ViewPos ) ) * Diffuse;

    // Compute specular lighting
    // Compute half vector
    float3 vHalf = normalize( normalize( -ViewPos ) + normalize( lightInView - ViewPos ) );

    // Compute normal dot half for specular light
    oSpecular = pow( saturate( dot( vHalf, ViewNormal ) ) * Specular, Power );

    // Just copy the texture coordinate through
    oTex0 = vTex0;

    // Compute the texture coord for the environment map.
    oEnvTex = 2 * dot( -ViewPos, ViewNormal ) * ViewNormal + ViewPos;
    oEnvTex = mul( oEnvTex, transpose((float3x3)g_mView) );
}


float4 PixScene1x( float4 MatDiffuse : COLOR0,
                   float4 MatSpecular : COLOR1,
                   float2 Tex0 : TEXCOORD0,
                   float3 EnvTex : TEXCOORD1 ) : COLOR0
{
    // Lookup mesh texture and modulate it with diffuse
    return ( MatDiffuse * tex2D( g_samScene, Tex0 ) + MatSpecular ) * ( 1 - Reflectivity ) + texCUBE( g_samEnv, EnvTex ) * Reflectivity;
}


//-----------------------------------------------------------------------------
// Name: RenderScene
// Type: Technique
// Desc: Renders scene to render target
//-----------------------------------------------------------------------------
technique RenderScene
{
    pass P0
    {
        VertexShader = compile vs_2_0 VertScene();
        PixelShader  = compile ps_2_0 PixScene();
        ZEnable = true;
        AlphaBlendEnable = false;
    }
}


technique RenderScene1x
{
    pass P0
    {
        VertexShader = compile vs_1_1 VertScene1x();
        PixelShader  = compile ps_1_1 PixScene1x();
        ZEnable = true;
        AlphaBlendEnable = false;
    }
}
