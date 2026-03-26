#pragma once
#include "Engine/Fbx/FbxContext.hpp"
#include "Engine/Fbx/FbxSceneData.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Math/VertexUtils.hpp"
#include "Engine/Math/Quaternion.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Timer.hpp"
#include "Engine/Math/IKUtils.hpp"
#include "GameCommon.hpp"
#include "Game/Game.hpp"
#include <vector>

class Player;
struct PerFrameConstants;

enum IKMode {
	FREE,
	SWING_CONSTRAINT,
	SWING_ANGLE_CONSTRAINT,
	ARM_CONSTRAINT,
	NUM_MODES
};

class GameModeIK : public Game
{
public:
	GameModeIK();
	~GameModeIK();

	void Startup();
	void Update();
	void HandleInputGame();

	void Render() const;
	void Shutdown();

	Player* m_player;

	std::vector<Vertex_PCU>		m_screenVerts;				//Screen verts			
	std::vector<Vertex_PCU>		m_worldverts;				//World unlit verts
	std::vector<Vertex_PCU>		m_gridVerts;

	//IK
	IKChain2 m_IKDemoChain = IKChain2();
	Vec3	 m_IKrootPos = Vec3(0, 0, 1);
	Vec3	 m_IKTarget = Vec3(0,0,0);
	float	 m_IKLen0 = .5f;
	float	 m_IKLen1 = .5f;
	float	 m_IKTargetSpeed = .8f;
	IKMode	 m_IKMode = FREE;

private:
	int	  m_frameCounter = 0;
	int	  m_framesbetweenFPSUpdate = 100;
	float m_fps = 0.f;
};