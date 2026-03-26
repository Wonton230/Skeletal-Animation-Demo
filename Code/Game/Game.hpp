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
#include "Engine/Math/VertexUtils.hpp"
#include "Engine/Math/Quaternion.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Timer.hpp"
#include "Engine/Skeletal/SkeletalAnimationBlender.hpp"
#include "GameCommon.hpp"
#include <vector>

class Player;

enum GameMode
{
	ATTRACT,
	ANIMATION,
	FABRIK,
	PLAYGROUND,
	COUNT
};

struct PerFrameConstants
{
	float c_time = 0;
	int c_debugInt = 0;
	float c_debugFloat = 0;
	float skeletonWeightThreshold = 0.f;
	Mat44 boneMatrices[65] = {};
};

class Game
{
public:
public:
	//methods
	Game();
	virtual ~Game() = default;

	virtual void Startup() = 0;
	virtual void Shutdown() = 0;
	virtual void Update() = 0;
	virtual void Render() const = 0;

	Camera					m_worldCamera;
	Camera					m_screenCamera;
	Clock*					m_gameClock;
};