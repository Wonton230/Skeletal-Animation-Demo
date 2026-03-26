//------------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float3 modelTangent : TANGENT;
	float3 modelBitangent : BITANGENT;
	float3 modelNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 clipPosition : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float4 worldTangent : TANGENT;
	float4 worldBitangent : BITANGENT;
	float4 worldNormal : NORMAL;
	float4 worldPosition : TEXCOORD4; 
	float3 modelTangent	: MODEL_TANGENT;
	float3 modelBitangent	: MODEL_BITANGENT;
	float3 modelNormal	: MODEL_NORMAL;
};

struct PointLight
{
	float3 LightPosition;
	float  Intensity;
	float4 LightColor;
};

struct Light
{
	float4	Color;					// Alpha (w) is intensity/brightness in [0,1]
	float3	WorldPosition;			// World position of point/spot light source
	float	EMPTY_PADDING;			// Used in constant buffers, must follow strict alignment rules
	float3	SpotForward;			// Forward normal for spotlights (can be zero for omnidirectional point-lights)
	float	Ambience;				// Portion of indirect light this source gives to objects in its affected volume
	float	InnerRadius;			// Inside the inner radius, the light is at full strength
	float	OuterRadius;			// Outside the outer radius, the light has no effect
	float	InnerDotThreshold;		// If dot with forward is greater than inner threshold, full strength; -1 for point lights
	float	OuterDotThreshold;		// if dot with forward is less than outer threshold, zero strength; -2 for point lights
};
#define  MAX_LIGHTS 64

//------------------------------------------------------------------------------------------------
cbuffer LightConstants : register(b1)
{
	float3		SunDirection;
	float		SunIntensity;
	float4		SunColor;
	float		AmbientIntensity;
	int			NumPointLights;
	float2		ByteKill2;
	PointLight	PointLightList[64];
	int			NumLights;
	float3		ByteKill3;
	Light		LightList[MAX_LIGHTS];
};
static const int k_lightBufferSlot = 1;

//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
	float4x4 WorldToCameraTransform;	// View transform
	float4x4 CameraToRenderTransform;	// Non-standard transform from game to DirectX conventions
	float4x4 RenderToClipTransform;		// Projection transform
	float3	 CameraWorldPosition;
	float	 Padding;
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
	float4x4 ModelToWorldTransform;		// Model transform
	float4 ModelColor;
};
//------------------------------------------------------------------------------------------------
cbuffer PerFrameConstants : register(b4)
{
	float c_time; //time elapsed
	int   c_debugInt; 
	float c_debugFloat;
	float bytekill;
};

//------------------------------------------------------------------------------------------------
// TEXTURE and SAMPLER constants
//
// There are 16 (on mobile) or 128 (on desktop) texture binding "slots" or "registers" (t0 through t15, or t127).
// There are 16 sampler slots (s0 through s15).
//
// In C++ code we bind textures into texture slots (t0 through t15 or t127) for use in the Pixel Shader when we call:
//	m_d3dContext->PSSetShaderResources( textureSlot, 1, &texture->m_shaderResourceView ); // e.g. (t3) if textureSlot==3
//
// In C++ code we bind texture samplers into sampler slots (s0 through s15) for use in the Pixel Shader when we call:
//	m_d3dContext->PSSetSamplers( samplerSlot, 1, &samplerState );  // e.g. (s3) if samplerSlot==3
//
// If we want to sample textures from within the Vertex Shader (VS), e.g. for displacement maps, we can also
//	use the VS versions of these C++ functions:
//	m_d3dContext->VSSetShaderResources( textureSlot, 1, &texture->m_shaderResourceView );
//	m_d3dContext->VSSetSamplers( samplerSlot, 1, &samplerState );
//------------------------------------------------------------------------------------------------
Texture2D<float4>	diffuseTexture		: register(t0);	// Texture bound in texture constant slot #0 (t0)
Texture2D<float4>	normalTexture		: register(t1);	// Texture bound in texture constant slot #1 (t1)
Texture2D<float4>	sgeTexture			: register(t2); // Texture bound in texture constant slot #2 (t2)
SamplerState		diffuseSampler		: register(s0);	// Sampler is bound in sampler constant slot #0 (s0)
SamplerState		normalSampler		: register(s1);	// Sampler is bound in sampler constant slot #1 (s1)

//------------------------------------------------------------------------------------------------
float RangeMap( float inValue, float inStart, float inEnd, float outStart, float outEnd )
{
	float fraction = (inValue - inStart) / (inEnd - inStart);
	float outValue = outStart + fraction * (outEnd - outStart);
	return outValue;
}


//------------------------------------------------------------------------------------------------
float RangeMapClamped( float inValue, float inStart, float inEnd, float outStart, float outEnd )
{
	float fraction = saturate( (inValue - inStart) / (inEnd - inStart) );
	float outValue = outStart + fraction * (outEnd - outStart);
	return outValue;
}

//------------------------------------------------------------------------------------------------
// Used standard normal color encoding, mapping xyz in [-1,1] to rgb in [0,1]
//------------------------------------------------------------------------------------------------
float3 EncodeXYZToRGB( float4 vec )
{
	return (vec.xyz + 1.0) * 0.5;
}


//------------------------------------------------------------------------------------------------
// Used standard normal color encoding, mapping rgb in [0,1] to xyz in [-1,1]
//------------------------------------------------------------------------------------------------
float3 DecodeRGBToXYZ( float4 color )
{
	return (color.xyz * 2.0) - 1.0;
}

//------------------------------------------------------------------------------------------------
float SmoothStep3( float x )
{
	return (3.0*(x*x)) - (2.0*x)*(x*x);
}


//------------------------------------------------------------------------------------------------
float3 SmoothStop2( float3 v )
{
	float3 inverse = 1.0 - v;
	return 1.0 - (inverse * inverse);
}


//------------------------------------------------------------------------------------------------
float3 SmoothStop3( float3 v )
{
	float3 inverse = 1.0 - v;
	return 1.0 - (inverse * inverse * inverse);
}


//------------------------------------------------------------------------------------------------
float3 SmoothStart3( float3 v )
{
	return v * v * v;
}


//------------------------------------------------------------------------------------------------
float SnapToNearestFractional( float x, int numIntervals )
{
	float intervalSize = 1.0f / (float) numIntervals;
	x *= (float) numIntervals;
	x -= frac( x );
	x /= (float) numIntervals;
	return x;
}

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 modelPosition	= float4(input.modelPosition, 1);
	float4 worldPosition	= mul(ModelToWorldTransform, modelPosition);
	float4 cameraPosition	= mul(WorldToCameraTransform, worldPosition);
	float4 renderPosition	= mul(CameraToRenderTransform, cameraPosition);
	float4 clipPosition		= mul(RenderToClipTransform, renderPosition);

	float4 modelTangent		= float4( input.modelTangent, 0.0 );
	float4 modelBitangent	= float4( input.modelBitangent, 0.0 );
	float4 modelNormal		= float4( input.modelNormal, 0.0 );
	float4 worldTangent		= mul(ModelToWorldTransform, modelTangent);
	float4 worldBitangent	= mul(ModelToWorldTransform, modelBitangent);
	float4 worldNormal		= mul(ModelToWorldTransform, modelNormal);

	v2p_t v2p;
	v2p.clipPosition = clipPosition;
	v2p.worldPosition = worldPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	v2p.worldTangent = worldTangent;
	v2p.worldBitangent = worldBitangent;
	v2p.worldNormal = worldNormal;
	v2p.modelTangent = modelTangent.xyz;
	v2p.modelBitangent = modelBitangent.xyz;
	v2p.modelNormal = modelNormal.xyz;
	return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{	
	float2 uvCoords		= input.uv;
	float4 diffuseTexel = diffuseTexture.Sample( diffuseSampler, uvCoords );
	float4 normalTexel	= normalTexture.Sample( normalSampler, uvCoords );
	float4 sgeTexel		= sgeTexture.Sample( diffuseSampler, uvCoords );
	float4 surfaceColor = input.color;
	float4 modelColor	= ModelColor;

	// Decode normalTexel RGB into XYZ then renormalize; this is the per-pixel normal, in TBN space a.k.a. tangent space
	float3 pixelNormalTBNSpace = normalize( DecodeRGBToXYZ( normalTexel ));
	float4 diffuseColor = diffuseTexel * surfaceColor * modelColor;

	// Get TBN basis vectors
	float3 surfaceTangentWorldSpace		= normalize( input.worldTangent.xyz );
	float3 surfaceBitangentWorldSpace	= normalize( input.worldBitangent.xyz );
	float3 surfaceNormalWorldSpace		= normalize( input.worldNormal.xyz );

	float3 surfaceTangentModelSpace		= normalize( input.modelTangent );
	float3 surfaceBitangentModelSpace	= normalize( input.modelBitangent );
	float3 surfaceNormalModelSpace		= normalize( input.modelNormal );

	float3x3 tbnToWorld = float3x3( surfaceTangentWorldSpace, surfaceBitangentWorldSpace, surfaceNormalWorldSpace );
	float3 pixelNormalWorldSpace = mul( pixelNormalTBNSpace, tbnToWorld ); // V*M order because this matrix is component-major (not basis-major!)

	//Lights-------------------------------------------------------------------------------------------------------------------------------------------
	float specularity	= sgeTexel.r;
	float glossiness	= sgeTexel.g;
	float emissiveness	= sgeTexel.b;
	float ambience		= AmbientIntensity;
	float3 totalDiffuseLight = float3( 0.f, 0.f, 0.f ); // Accumulate light into here from all sources
	float3 totalSpecularLight = float3( 0.f, 0.f, 0.f );
	float specularExponent = RangeMap( glossiness, 0.f, 1.f, 1.f, 32.f );
	float3 pixelToCameraDir = normalize( CameraWorldPosition - input.worldPosition.xyz ); // e.g. "V" in most Blinn-Phong diagrams
	
	float3 selectedNormal = pixelNormalWorldSpace;
	if( c_debugInt == 10 || c_debugInt == 12 )
	{
		selectedNormal = surfaceNormalWorldSpace;
	}

	//Sunlight-------------------------------------------------------------------------------------------------------------------------
	float sunlightStrength = SunColor.a * saturate( RangeMap( dot( -SunDirection, selectedNormal ), -ambience, 1.0, 0.0, 1.0 ) );
	float3 diffuseLightFromSun = sunlightStrength * SunColor.rgb;
	totalDiffuseLight += diffuseLightFromSun;
	//Sun Specular
	float3 pixelToSunDir = float3(-SunDirection.xyz);
	float3 sunIdealReflectionDir = normalize( pixelToSunDir + pixelToCameraDir );
	float sunSpecularDot = saturate( dot( sunIdealReflectionDir, selectedNormal ) );
	float sunSpecularStrength = glossiness * SunColor.a * pow( sunSpecularDot, specularExponent );
	float3 sunSpecularLight = sunSpecularStrength * SunColor.rgb;
	totalSpecularLight += sunSpecularLight;
	
	//Point + Spot Lights--------------------------------------------------------------------------------------------------------------
	for(int i = 0; i < NumLights; i++)
	{
		// Point/spot diffuse lighting
		float ambience			= LightList[i].Ambience;
		float3 lightPos			= LightList[i].WorldPosition;
		float3 lightColor		= LightList[i].Color.rgb;
		float lightBrightness	= LightList[i].Color.a;
		float innerRadius		= LightList[i].InnerRadius;
		float outerRadius		= LightList[i].OuterRadius;
		float innerPenumbraDot	= LightList[i].InnerDotThreshold;
		float outerPenumbraDot	= LightList[i].OuterDotThreshold;
		float3 pixelToLightDisp = lightPos - input.worldPosition.xyz;
		float3 pixelToLightDir	= normalize( pixelToLightDisp );
		float3 lightToPixelDir	= -pixelToLightDir;
		float distToLight		= length( pixelToLightDisp );
		float falloff			= saturate( RangeMap( distToLight, innerRadius, outerRadius, 1.f, 0.f ) );
		falloff					= SmoothStep3( falloff );
		float penumbra			= saturate( RangeMap( dot( LightList[i].SpotForward, lightToPixelDir ), outerPenumbraDot, innerPenumbraDot, 0.f, 1.f ) );
		penumbra				= SmoothStep3( penumbra );
		float lightStrength		= penumbra * falloff * lightBrightness * saturate( RangeMap( dot( pixelToLightDir, pixelNormalWorldSpace ), -ambience, 1.0, 0.0, 1.0 ) );
		float3 diffuseLight		= lightStrength * lightColor;
		totalDiffuseLight		+= diffuseLight;

		// Specular Highlighting (glare)
		float3 idealReflectionDir	= normalize( pixelToCameraDir + pixelToLightDir );
		float specularDot			= saturate( dot( idealReflectionDir, pixelNormalWorldSpace ) ); // how perfect is the reflection angle?
		float specularStrength		= glossiness * lightBrightness * pow( specularDot, specularExponent );
		specularStrength			*= falloff * penumbra;
		float3 specularLight		= specularStrength * lightColor;
		totalSpecularLight			+= specularLight;
	}

	float3 emissiveLight = diffuseTexel.rgb * emissiveness;

	float3 finalRGB = (saturate(totalDiffuseLight) * diffuseColor.rgb) + (specularity * totalSpecularLight) + emissiveLight;
	float4 finalColor = float4( finalRGB, diffuseColor.a );	
	clip(finalColor.a - 0.01f);

	if (c_debugInt == 1)
	{
	    finalColor.rgba = diffuseTexel.rgba;
	}
	else if(c_debugInt == 2 )
	{
		finalColor.rgba = surfaceColor.rgba;
	}
	else if(c_debugInt == 3 )
	{
	    finalColor.rgba = float4(uvCoords.x, uvCoords.y, 0.f, 1.f);
	}
	else if(c_debugInt == 4 )
	{
		finalColor.rgb = EncodeXYZToRGB( float4(surfaceTangentModelSpace,0.f) );
	}
	else if(c_debugInt == 5 )
	{
		finalColor.rgb = EncodeXYZToRGB( float4(surfaceBitangentModelSpace,0.f) );
	}
	else if(c_debugInt == 6 )
	{
		finalColor.rgb = EncodeXYZToRGB( float4(surfaceNormalModelSpace,0.f) );
	}
	else if(c_debugInt == 7 )
	{
		finalColor.rgba = normalTexel.rgba;
	}
	else if(c_debugInt == 8 )
	{
		finalColor.rgb = EncodeXYZToRGB( float4(pixelNormalTBNSpace,0.f) );
	}
	else if(c_debugInt == 9 )
	{
		finalColor.rgb = EncodeXYZToRGB( float4(pixelNormalWorldSpace,0.f) );
	}
	else if(c_debugInt == 10 )
	{
		// Lit, but ignore normal maps (use surface normals only) -- see above
	}
	else if(c_debugInt == 11 || c_debugInt == 12 )
	{
		finalColor.rgb = sunlightStrength.xxx;
	}
	else if(c_debugInt == 13 )
	{
		//color.rgba = sgeTexel.rgba;
	}
	else if(c_debugInt == 14 )
	{
		finalColor.rgb = EncodeXYZToRGB( float4(surfaceTangentWorldSpace,0.f) );
	}
	else if(c_debugInt == 15 )
	{
		finalColor.rgb = EncodeXYZToRGB( float4(surfaceBitangentWorldSpace,0.f) );
	}
	else if(c_debugInt == 16 )
	{
		finalColor.rgb = EncodeXYZToRGB( float4(surfaceNormalWorldSpace,0.f) );
	}
	else if(c_debugInt == 17 )
	{
		float3 modelIBasisWorld = mul( ModelToWorldTransform, float4(1,0,0,0) ).xyz;
		finalColor.rgb = EncodeXYZToRGB( normalize( float4(modelIBasisWorld,0.f) ) );
	}
	else if(c_debugInt == 18 )
	{
		float3 modelJBasisWorld = mul( ModelToWorldTransform, float4(0,1,0,0) ).xyz;
		finalColor.rgb = EncodeXYZToRGB( normalize( float4(modelJBasisWorld,0.f) ) );
	}
	else if(c_debugInt == 19 )
	{
		float3 modelKBasisWorld = mul( ModelToWorldTransform, float4(0,0,1,0) ).xyz;
		finalColor.rgb = EncodeXYZToRGB( normalize( float4(modelKBasisWorld,0.f) ) );
	}
	else if(c_debugInt == 20 )
	{
		finalColor.rgb = emissiveLight.rgb;
	}
	else if(c_debugInt == 21 )
	{

	}
	else if(c_debugInt == 22 )
	{

	}
		
	return finalColor;
}
