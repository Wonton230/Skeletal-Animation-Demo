#pragma once
#include "Engine/Fbx/FbxContext.hpp"
#include "Engine/Core/ObjLoader.hpp"
#include "Engine/Fbx/FbxSceneData.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/OBB3.hpp"
#include "Engine/Math/VertexUtils.hpp"
#include "Engine/Math/Quaternion.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Timer.hpp"
#include "Engine/Skeletal/SkeletalAnimationBlender.hpp"
#include "Engine/Math/IKUtils.hpp"
#include "GameCommon.hpp"
#include "Game/Game.hpp"
#include <vector>

class Player;
struct PerFrameConstants;

class GameModeAnimationViewer: public Game 
{
public:
	GameModeAnimationViewer();
	~GameModeAnimationViewer();

	void Startup();
	void StartGame();

	void Update();
	void UpdateGame();
	void HandleInputGame();
	void UpdateConstantBuffers();

	void UpdateSkeletonPosition(float deltaSeconds);
	void UpdateSkeletonVertexArrays(const SkeletonMeshData& originalMesh, std::vector<Vertex_SKEL>& outputVertices, std::vector<unsigned int>& outputIndices);
	void UpdateSkeletonMatrices(SkeletonData& skeleton, const SkeletalAnimation& animation, float currentTimeInSeconds);
	void UpdateSkeletonMatricesLocally(SkeletonData& skeleton, const SkeletalAnimation& animation, float currentTimeInSeconds);
	void SetSkeletonToBindPose(SkeletonData& skeleton);
	void SetSkeletonToBindPoseLocal(SkeletonData& skeleton);

	void UpdateIKChains();
	void ApplyLeftLegIKChainToSkeleton(Vec3 const& footNormal);
	void ApplyRightLegIKChainToSkeleton(Vec3 const& footNormal);
	void InitializeFootTraceCurves();
	float GetLeftFootHeightAtTime(float time);
	float GetRightFootHeightAtTime(float time);

	void Render() const;
	void RenderGame() const;

	void Shutdown();
	void ShutdownGame();

	void LoadAllFbx();
	std::string  GetShaderDebugModeDescription();
	std::string  GetRenderDebugModeDescription();

	Timer* m_debugQuatTimer;
	Quaternion					m_targetQuat;

	Player* m_player;
	Vec3						m_animatedCharacterPos;

	FbxContext*					m_fbxContext = nullptr;
	std::string					m_fbxFileName = "";
	FbxSceneData*				m_fbxSceneData;

	std::vector<Vertex_PCU>		m_worldVerts;			//World unlit verts
	std::vector<Vertex_PCU>		m_groundVerts;			//World unlit verts
	std::vector<Vertex_PCU>		m_debug1Verts;			//World unlit verts
	std::vector<Vertex_PCU>		m_debug2Verts;			//World unlit verts
	std::vector<Vertex_PCUTBN>	m_worldVertTBNs;		//World Lit Verts: Stores Mesh verts
	std::vector<unsigned int>	m_worldVertsIndexes;	//For lighting when I need it: Stores mesh verts

	std::vector<Vertex_SKEL>	m_skeletonMeshVerts;		//Skinned verts
	std::vector<unsigned int>	m_skeletonMeshIndexes;		//Skinned indexes

	//Animation Vars
	SkeletalAnimationBlender	m_blender;
	SkeletonData				m_skeleton;
	SkeletonMeshData			m_staticMeshData;
	int							m_currentAnimationIndex;
	SkeletalAnimation			m_currentAnimation;
	std::vector<SkeletalAnimation> m_loadedAnimations;
	std::vector<Vertex_PCU>		m_debugSkeletonVerts;
	Timer*						m_animationTimer;
	bool						m_playAnimation = true;

	bool						m_blendTestState = false;
	float						m_blendFraction = 0.f;

	std::vector<float>			m_leftFootHeightTrace;
	float						m_leftFootMaxHeight = -1;
	float						m_leftFootMinHeight = 100;
	std::vector<float>			m_rightFootHeightTrace;
	float						m_rightFootMaxHeight = -1;
	float						m_rightFootMinHeight = 100;

	std::vector<Vertex_PCU>		m_verts;				//Screen verts

	VertexBuffer*				m_vBuffer;
	IndexBuffer*				m_iBuffer;
	LightConstants				m_lightConstants;
	ConstantBuffer*				m_lightBuffer;
	PerFrameConstants			m_perFrameConstants;
	ConstantBuffer*				m_perFrameBuffer;

	const Vec3					m_sunLightDirection = Vec3(-1, -1, -.5).GetNormalized();
	const float					m_sunIntensity = 15.f;
	const float					m_sunAmbience = 20.f;

	//Environment:	
	bool						m_angleSlope = false;
	OBB3						m_groundBlock = OBB3(Vec3(), Vec3(1,0,0), Vec3(0,1,0), Vec3(0,0,1), Vec3(5,5,1.f));
	OBB3						m_groundSlopeBlock = OBB3();

	bool						m_IKEnabled = true;
	IKChain2					m_leftLegChain;
	IKChain2					m_rightLegChain;

private:
	float m_ringSize = 50.f;
	float m_elapsedTime = 0.f;

	int	  m_frameCounter = 0;
	int	  m_framesbetweenFPSUpdate = 100;
	float m_fps = 0.f;

	int	  m_debugRenderMode = 0;
	int	  m_debugShaderMode = 0;
};