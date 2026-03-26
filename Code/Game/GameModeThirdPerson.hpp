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
#include "Engine/Skeletal/SkeletalAnimator.hpp"
#include "Engine/Skeletal/SkeletalAnimationBlender.hpp"
#include "Engine/Skeletal/FootIKSolver.hpp"
#include "Engine/Skeletal/ArmIKSolver.hpp"
#include "Engine/Math/IKUtils.hpp"
#include "GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/Staircase.hpp"
#include <vector>

class Player;
struct PerFrameConstants;

enum eAnimationState
{
	IDLE,
	WALKING,
	RUNNING,
	FALLING,
	LANDING,
	IDLE2WALKING,
	WALKING2IDLE,
	IDLE2RUNNING,
	RUNNING2IDLE,
	NUM_ANIM_STATES
};

class GameModeThirdPerson: public Game 
{
public:
	GameModeThirdPerson();
	~GameModeThirdPerson();

	void Startup();
	void StartGame();

	void Update();
	void UpdateGame();
	void HandleInputGame();
	void HandleInputCamera();
	void HandleInputCharacter();
	void UpdateCameraPosition();
	void UpdateConstantBuffers();
	void UpdateDebugMessages(float deltaSeconds);

	void UpdateCharacterWorldPosition(float deltaSeconds);
	void UpdateSkeletonVertexArrays(const SkeletonMeshData& originalMesh, std::vector<Vertex_SKEL>& outputVertices, std::vector<unsigned int>& outputIndices);

	void	UpdateFootIKFrameData();
	void	UpdateFootTraceCurves(); //Called every frame but can be more optimized to not do that
	void	UpdateFootPathPredictions();
	void	UpdateFootGroundedStates();
	void	UpdateArmIKFrameData();
	float	GetLeftFootHeightAtTime(float time);
	float	GetRightFootHeightAtTime(float time);
	void	UpdateHeadLookAt();

	RaycastResult3D RaycastVsWorldVisual(Vec3 const& startPos, Vec3 const& fwdNormal, float maxLen);
	RaycastResult3D RaycastVsWorldCollision(Vec3 const& startPos, Vec3 const& fwdNormal, float maxLen);

	void	Render() const;
	void	RenderGame() const;

	void	Shutdown();
	void	ShutdownGame();

	void			LoadAllFbx();
	void			InitializeAnimationStatesForCharacter();
	std::string		GetShaderDebugModeDescription();
	std::string		GetRenderDebugModeDescription();
	void			SetSkeletonToBindPose(SkeletonData& skeleton);

	FbxContext*					m_fbxContext = nullptr;
	std::string					m_fbxFileName = "";
	FbxSceneData*				m_fbxSceneData;

	//Player
	Camera*						m_thirdPersonCam;
	float						m_cameraLookSpeed = 0.125f;
	float						m_cameraFollowDistance = 5.f;
	float						m_cameraZoomStep = .3f;

	//Rendering
	std::vector<Vertex_PCU>		m_verts;				//Screen verts
	VertexBuffer*				m_vBuffer;
	IndexBuffer*				m_iBuffer;
	LightConstants				m_lightConstants;
	ConstantBuffer*				m_lightBuffer;
	PerFrameConstants			m_perFrameConstants;
	ConstantBuffer*				m_perFrameBuffer;

	std::vector<Vertex_PCU>		m_worldVerts;			//World unlit verts
	std::vector<Vertex_PCU>		m_groundVerts;			//World unlit verts
	std::vector<Vertex_PCUTBN>	m_worldVertTBNs;		//World Lit Verts: Stores Mesh verts
	std::vector<unsigned int>	m_worldVertsIndexes;	//For lighting when I need it: Stores mesh verts

	std::vector<Vertex_SKEL>	m_skeletonMeshVerts;		//Skinned verts
	std::vector<unsigned int>	m_skeletonMeshIndexes;		//Skinned indexes

	//Character Vars
	Vec3						m_animatedCharacterPosPreviousFrame;
	Vec3						m_animatedCharacterPos;
	Mat44						m_animatedCharacterOrientation;
	Mat44						m_targetCharacterOrientation;
	float						m_characterIdleToWalkTime = .4f;
	float						m_characterSpeed = 1.f;
	float						m_characterTurnSpeed = 250.f;
	
	//Animation Vars
	FootIKFrameData					m_footIKFrameData;
	FootIKSolver*					m_footIKController;
	ArmIKFrameData					m_armIKFrameData;
	ArmIKSolver*					m_armIKController;
	SkeletalAnimator*				m_skeletalAnimator;
	SkeletonData					m_skeleton;
	SkeletonMeshData				m_staticMeshData;
	int								m_currentAnimationIndex = 0;
	SkeletalAnimation				m_currentAnimation;
	int								m_targetAnimationIndex = 0;
	SkeletalAnimation				m_targetAnimation;
	std::vector<SkeletalAnimation>	m_loadedAnimations;
	std::vector<Vertex_PCU>			m_debugSkeletonVerts;
	Timer*							m_animationTimer;
	Timer*							m_animationTransitionTimer;
	bool							m_animationTransition = false;
	bool							m_playAnimation = true;
	float							m_maxBoneTravelSpeed = 500.f;

	//Feet animation
	std::vector<float>				m_leftFootHeightTrace;
	std::vector<FloatRange>			m_leftFootGroundedFractionRangeSet;
	float							m_leftFootMaxHeight = -1;
	float							m_leftFootMinHeight = 100;
	int								m_leftFootMinHeightIndex = -1;
	std::vector<float>				m_rightFootHeightTrace;
	std::vector<FloatRange>			m_rightFootGroundedFractionRangeSet;
	float							m_rightFootMaxHeight = -1;
	float							m_rightFootMinHeight = 100;
	int								m_rightFootMinHeightIndex = -1;
	bool							m_IKEnabled = true;
	Vec3							m_nextPredictedGroundedPosLeft;
	Vec3							m_nextPredictedGroundedPosRight;
	float							m_lastGroundedLeftFootFraction;
	float							m_lastGroundedRightFootFraction;

	//Arm animation
	IKChain2						m_rightArmChain;
	Vec3							m_reachTarget = Vec3(.5, .5, 1.5);
	Vec3							m_startingReachPosition;
	Vec3							m_travelPlane;
	float							m_reachFraction;

	//Head animation
	bool							m_lookAtPOI;
	float							m_headAngleConstraint = 75.f;
	Vec3							m_lookAtTargetPos = Vec3(5, -2.5, 3);

	//Environment:	
	Staircase					m_stairs;
	std::vector<OBB3>			m_obstacleBlocks;
	std::vector<Vec3>			m_grabbableLocations;
	bool						m_angleSlope = false;
	OBB3						m_groundBlock = OBB3(Vec3(-5,-5,-1.f), Vec3(1,0,0), Vec3(0,1,0), Vec3(0,0,1), Vec3(5,5,1.f));
	const Vec3					m_sunLightDirection = Vec3(-1, -1, -.5).GetNormalized();
	const float					m_sunIntensity = 15.f;
	const float					m_sunAmbience = 20.f;

private:
	float m_ringSize = 50.f;
	float m_elapsedTime = 0.f;

	int	  m_frameCounter = 0;
	int	  m_framesbetweenFPSUpdate = 100;
	float m_fps = 0.f;

	int	  m_debugRenderMode = 0;
	int	  m_debugShaderMode = 0;
};