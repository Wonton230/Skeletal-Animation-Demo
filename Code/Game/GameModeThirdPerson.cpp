#include "GameModeThirdPerson.hpp"
#include "Player.hpp"
#include "App.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Fbx/FbxFileBaker.hpp"
#include "Engine/Math/RaycastUtils.hpp"
#include "Engine/Math/Plane3.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Math/MathUtils.hpp"

extern App* g_theApp;
extern Renderer* g_theRenderer;
extern RandomNumberGenerator* g_rng;
extern InputSystem* g_theInputSystem;
extern AudioSystem* g_theAudioSystem;
extern Clock* g_theSystemClock;
extern BitmapFont* g_testFont;
extern Window* g_theWindow;
extern DevConsole* g_theDevConsole;

GameModeThirdPerson::GameModeThirdPerson()
{
	m_screenCamera = Camera();
	m_screenCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));

	LightConstants light;
	light.c_sunDirection = m_sunLightDirection;
	light.c_ambientIntensity = m_sunAmbience;
	light.c_sunIntensity = m_sunIntensity;
	m_lightBuffer = g_theRenderer->CreateConstantBuffer(sizeof(light));
	g_theRenderer->CopyCPUToGPU(&light, sizeof(light), m_lightBuffer);
	g_theRenderer->BindConstantBuffer(1, m_lightBuffer);

	for (int i = 0; i < 65; i++)
	{
		m_perFrameConstants.boneMatrices[i] = Mat44();
	}
	int size = sizeof(PerFrameConstants);
	m_perFrameBuffer = g_theRenderer->CreateConstantBuffer(size);
	g_theRenderer->CopyCPUToGPU(&m_perFrameConstants, sizeof(PerFrameConstants), m_perFrameBuffer);

	g_theRenderer->CreateOrGetShader("SkinningBlinnPhong", VertexType::SKEL);
	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP, 1);

	//Load Fbxs-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	m_fbxContext = new FbxContext();
	//m_fbxContext->LoadFbxFile("Data/FBXs/PickUp.fbx");
	//SaveSkeletalAnimationToSkan(m_fbxContext->GetSceneData()->m_animationClips[1], "Data/SKANs/pickup.skan");
	//m_fbxContext->LoadFbxFile("Data/FBXs/Running.fbx");
	//SaveSkeletalAnimationToSkan(m_fbxContext->GetSceneData()->m_animationClips[0], "Data/SKANs/running.skan");
	//m_fbxContext->LoadFbxFile("Data/FBXs/SillyDancing.fbx");
	//SaveSkeletalAnimationToSkan(m_fbxContext->GetSceneData()->m_animationClips[1], "Data/SKANs/dance.skan");
	//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ -

	m_skeletonMeshVerts = LoadSkeletalMeshFromSkesh("Data/SKESHs/dummy.skesh").m_vertices;
	m_skeletonMeshIndexes = LoadSkeletalMeshFromSkesh("Data/SKESHs/dummy.skesh").m_indices;

	m_loadedAnimations.push_back(LoadSkeletalAnimationFromSkan("Data/SKANs/idle.skan"));
	m_loadedAnimations.push_back(LoadSkeletalAnimationFromSkan("Data/SKANs/walking.skan"));
	m_loadedAnimations.push_back(LoadSkeletalAnimationFromSkan("Data/SKANs/dance.skan"));
	m_loadedAnimations.push_back(LoadSkeletalAnimationFromSkan("Data/SKANs/pickup.skan"));
}

GameModeThirdPerson::~GameModeThirdPerson()
{
	delete m_gameClock;
	delete m_skeletalAnimator;
	delete m_footIKController;
}

void GameModeThirdPerson::Startup()
{
	m_gameClock = new Clock(*g_theSystemClock);

	//m_skeleton = m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/Walking.fbx")->m_skeleton;
	//SaveSkeletonToSkel(m_skeleton, "Data/SKELs/dummy.skel");
	m_skeleton = LoadSkeletonFromSkel("Data/SKELs/dummy.skel");
	StartGame();

	FootIKConfig footConfig;
	footConfig.m_leftHipIndex = 55;
	footConfig.m_leftKneeIndex = 56;
	footConfig.m_leftAnkleIndex = 57;
	footConfig.m_leftBallIndex = 58;
	footConfig.m_leftToeIndex = 59;

	footConfig.m_rightHipIndex = 60;
	footConfig.m_rightKneeIndex = 61;
	footConfig.m_rightAnkleIndex = 62;
	footConfig.m_rightBallIndex = 63;
	footConfig.m_rightToeIndex = 64;

	m_footIKController = new FootIKSolver(footConfig);
	m_footIKController->Initialize(m_skeleton);

	ArmIKConfig armConfig;
	armConfig.m_rightShoulderIndex = 32;
	armConfig.m_rightElbowIndex = 33;
	armConfig.m_rightHandIndex = 34;
	armConfig.m_rightFingerStartIndex = 35;
	armConfig.m_rightFingerEndIndex = 54;

	m_armIKController = new ArmIKSolver(armConfig);
	m_armIKController->Initialize(m_skeleton);

	m_skeletalAnimator = new SkeletalAnimator(m_gameClock);
	InitializeAnimationStatesForCharacter();
}

void GameModeThirdPerson::StartGame()
{
	m_thirdPersonCam = new Camera();
	m_thirdPersonCam->SetOrthographicView(Vec2(WORLD_MINS_X, WORLD_MINS_Y), Vec2(WORLD_MAXS_X, WORLD_MAXS_Y));
	m_thirdPersonCam->SetPerspectiveView(g_theWindow->GetConfig().m_aspectRatio, 50.f, .1f, 100.f);
	m_thirdPersonCam->SetCameraToRenderTransform(Mat44(Vec3(0.f, 0.f, 1.f), Vec3(-1.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f), Vec3(0.f, 0.f, 0.f)));

	m_stairs = Staircase(18, Vec3(3, 3, 0), Vec3(9, 9, 0), 1.5f, .17f);

	m_worldVerts.clear();
	int gridSize = 50;
	float halfWidth = .01f;
	float startingY = -(float)gridSize * .5f;

	m_stairs.AddVertsToVector(m_worldVerts, Rgba8(255,255,255,150));
	AddVertsForOBB3D(m_worldVerts, m_stairs.m_physicsRampOBB, Rgba8(155,255,255,100));

	DebugAddWorldSphere(m_lookAtTargetPos, .1f, -1.f, Rgba8::YELLOW);
	m_obstacleBlocks.push_back(OBB3(Vec3(0, 0, -1.f), Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1), Vec3(10.f, 10.f, 1.f)));
	
	for (int i = 0; i < gridSize; i++)
	{
		if (i == 25)
		{
			AddVertsForAABB3D(m_worldVerts, AABB3(-25.f, (startingY + i - halfWidth * 4), -halfWidth * 4, 25.f, (startingY + i + halfWidth * 4), halfWidth * 4), Rgba8(255, 0, 0, 70));
		}
		else if (i % 5 == 0)
		{
			AddVertsForAABB3D(m_worldVerts, AABB3(-25.f, (startingY + i - halfWidth * 2), -halfWidth * 2, 25.f, (startingY + i + halfWidth * 2), halfWidth * 2), Rgba8(255, 0, 0, 70));
		}
		else
		{
			AddVertsForAABB3D(m_worldVerts, AABB3(-25.f, (startingY + i - halfWidth), -halfWidth, 25.f, (startingY + i + halfWidth), halfWidth), Rgba8(150, 150, 150, 70));
		}
	}

	for (int i = 0; i < gridSize; i++)
	{
		if (i == 25)
		{
			AddVertsForAABB3D(m_worldVerts, AABB3((startingY + i - halfWidth * 4), -25.f, -halfWidth * 4, (startingY + i + halfWidth * 4), 25.f, halfWidth * 4), Rgba8(0, 255, 0, 70));
		}
		else if (i % 5 == 0)
		{
			AddVertsForAABB3D(m_worldVerts, AABB3((startingY + i - halfWidth * 2), -25.f, -halfWidth * 2, (startingY + i + halfWidth * 2), 25.f, halfWidth * 2), Rgba8(0, 255, 0, 70));
		}
		else
		{
			AddVertsForAABB3D(m_worldVerts, AABB3((startingY + i - halfWidth), -25.f, -halfWidth, (startingY + i + halfWidth), 25.f, halfWidth), Rgba8(150, 150, 150, 70));
		}
	}

	//Level creation
	EulerAngles obbOrientation = EulerAngles(0.f, -20.f, -20.f);
	Vec3 iB;
	Vec3 jB;
	Vec3 kB;
	obbOrientation.GetAsVectors_IFwd_JLeft_KUp(iB, jB, kB);
	m_groundBlock = OBB3(Vec3(1, 0, 0), iB, jB, kB, Vec3(5, 5, .5f));
	AddVertsForOBB3D(m_groundVerts, m_groundBlock, Rgba8(100, 100, 100, 170));
	m_obstacleBlocks.push_back(m_groundBlock);
}

void GameModeThirdPerson::Update()
{
	UpdateGame();
}

void GameModeThirdPerson::UpdateGame()
{
	float deltaSeconds = (float)m_gameClock->GetDeltaSeconds();
	m_verts.clear();
	m_debugSkeletonVerts.clear();

	HandleInputGame();
	HandleInputCamera();
	HandleInputCharacter();
	UpdateCharacterWorldPosition(deltaSeconds);
	UpdateFootTraceCurves();

	Mat44 worldTransform = Mat44::MakeTranslation3D(m_animatedCharacterPos);
	worldTransform.Append(m_animatedCharacterOrientation);
	m_skeleton.m_bones[0].m_finalSkinningMatrix = worldTransform;
	m_skeletalAnimator->Update(m_skeleton, deltaSeconds);
	if (m_IKEnabled)
	{
		UpdateFootPathPredictions();
		UpdateFootIKFrameData();
		m_footIKController->Update(m_skeleton, m_footIKFrameData);
		UpdateArmIKFrameData();
		m_armIKController->Update(m_skeleton, m_armIKFrameData);
		//UpdateHeadLookAt();
	}
	UpdateCameraPosition();
	UpdateConstantBuffers();
	UpdateDebugMessages(deltaSeconds);
}

void GameModeThirdPerson::HandleInputGame()
{
	if (g_theInputSystem->WasKeyJustPressed('P'))
	{
		m_gameClock->TogglePause();
	}

	if (g_theInputSystem->IsKeyDown('Y'))
	{
		m_gameClock->SetTimeScale(.3f);
	}
	else if(!m_gameClock->IsPaused())
	{
		m_gameClock->SetTimeScale(1.f);
	}

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_LEFTBRACKET))
	{
		m_debugRenderMode--;
		if (m_debugRenderMode == -1)
		{
			m_debugRenderMode = 2;
		}
	}

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_RIGHTBRACKET))
	{
		m_debugRenderMode++;
		if (m_debugRenderMode == 3)
		{
			m_debugRenderMode = 0;
		}
	}

	if (g_theInputSystem->WasKeyJustPressed('I'))
	{
		m_debugShaderMode--;
		if (m_debugShaderMode == -1)
		{
			m_debugShaderMode = 20;
		}
	}

	if (g_theInputSystem->WasKeyJustPressed('O'))
	{
		m_debugShaderMode++;
		if (m_debugShaderMode == 21)
		{
			m_debugShaderMode = 0;
		}
	}

	if (g_theInputSystem->WasKeyJustPressed('L'))
	{
		m_playAnimation = !m_playAnimation;
	}

	if (g_theInputSystem->WasKeyJustPressed('M'))
	{
		m_angleSlope = !m_angleSlope;
		m_groundVerts.clear();
	}

	if (g_theInputSystem->WasKeyJustPressed('K'))
	{
		m_IKEnabled = !m_IKEnabled;
	}
}

void GameModeThirdPerson::HandleInputCamera()
{
	//Orientation
	float deltaY = g_theInputSystem->GetCursorClientDelta().y;
	EulerAngles temp;
	if (deltaY > 0)
	{
		deltaY = 0;
	}
	temp = m_thirdPersonCam->GetOrientation();
	temp.m_yawDegrees -= m_cameraLookSpeed * g_theInputSystem->GetCursorClientDelta().x;
	temp.m_pitchDegrees = GetClamped(temp.m_pitchDegrees + m_cameraLookSpeed * g_theInputSystem->GetCursorClientDelta().y, -85.f, 85.f);
	m_thirdPersonCam->SetOrientation(temp);

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_MOUSEWHEEL_UP))
	{
		m_cameraFollowDistance = GetClamped(m_cameraFollowDistance - m_cameraZoomStep, 0.f, 10.f);
	}
	else if(g_theInputSystem->WasKeyJustPressed(KEYCODE_MOUSEWHEEL_DOWN))
	{
		m_cameraFollowDistance = GetClamped(m_cameraFollowDistance + m_cameraZoomStep, 0.f, 10.f);
	}
}

void GameModeThirdPerson::HandleInputCharacter()
{
	float deltaSeconds = (float)m_gameClock->GetDeltaSeconds();

	// Get camera-relative directions, flattened to XY plane
	Vec3 forward, left, up;
	m_thirdPersonCam->GetOrientation().GetAsVectors_IFwd_JLeft_KUp(forward, left, up);
	forward = forward.GetFlattenedNormalXY();
	left = left.GetFlattenedNormalXY();

	int    animatorState = m_skeletalAnimator->GetCurrentStateIndex();
	Vec3 inputDir = Vec3(0, 0, 0);
	if (g_theInputSystem->IsKeyDown('W') && animatorState != 2)
		inputDir += forward;
	if (g_theInputSystem->IsKeyDown('S') && animatorState != 2)
		inputDir -= forward;
	if (g_theInputSystem->IsKeyDown('A') && animatorState != 2)
		inputDir += left;
	if (g_theInputSystem->IsKeyDown('D') && animatorState != 2)
		inputDir -= left;

	bool isMoving = (inputDir != Vec3(0, 0, 0));
	if (isMoving)
		inputDir = inputDir.GetNormalized();

	// Get displacement magnitude from current animation state
	bool   inTransition = m_skeletalAnimator->IsInTransition();
	float  elapsedTime = (float)m_skeletalAnimator->GetElapsedTime();
	float  blendFraction = (float)m_skeletalAnimator->GetTransitionFraction();

	float animationDisplacementMagnitude = 0.f;
	if (!inTransition)
	{
		animationDisplacementMagnitude = m_loadedAnimations[animatorState].GetXYDisplacementFromTime(deltaSeconds, elapsedTime).GetLength();
	}
	else
	{
		float currentDisp = m_loadedAnimations[m_skeletalAnimator->GetCurrentStateIndex()].GetXYDisplacementFromTime(deltaSeconds, elapsedTime).GetLength();
		float targetDisp = m_loadedAnimations[m_skeletalAnimator->GetTargetStateIndex()].GetXYDisplacementFromTime(deltaSeconds, elapsedTime).GetLength();
		animationDisplacementMagnitude = Interpolate(currentDisp, targetDisp, blendFraction);
	}

	// Request animation transitions based on movement
	if (isMoving && animatorState == 0 && !inTransition)
	{
		m_skeletalAnimator->RequestTransitionTo("walk");
		//m_footIKController->InitializeFootTraceCurves(m_loadedAnimations[1]);
	}
	else if (!isMoving && animatorState == 1 && !inTransition)
	{
		m_skeletalAnimator->RequestTransitionTo("idle");
		//m_footIKController->InitializeFootTraceCurves(m_loadedAnimations[0]);
	}

	// Grab input - only when not grabbing and not mid-transition
	if (g_theInputSystem->WasKeyJustPressed('E') && animatorState != 2 && !inTransition)
	{
		// Compute travel plane for arm IK
		Mat44 worldHandPos = m_skeleton.m_bones[34].m_finalSkinningMatrix;
		worldHandPos.Append(m_skeleton.m_bones[34].m_bindPose);
		Mat44 worldElbowPos = m_skeleton.m_bones[33].m_finalSkinningMatrix;
		worldElbowPos.Append(m_skeleton.m_bones[33].m_bindPose);

		Vec3 midToTarget = m_reachTarget - worldElbowPos.GetTranslation3D();
		Vec3 midToEnd = worldHandPos.GetTranslation3D() - worldElbowPos.GetTranslation3D();
		m_armIKFrameData.m_elbowPlaneVector = -CrossProduct3D(midToEnd, midToTarget).GetNormalized();
		m_armIKFrameData.m_reachStartPos = worldHandPos.GetTranslation3D();
		m_armIKFrameData.m_reachTargetPos = m_reachTarget;

		m_skeletalAnimator->RequestTransitionTo("grab");
	}

	if (g_theInputSystem->WasKeyJustPressed('M'))
	{
		m_lookAtPOI = !m_lookAtPOI;
	}

	Vec3 displacement = inputDir * animationDisplacementMagnitude;

	if (!isMoving && inTransition && m_skeletalAnimator->GetTargetStateIndex() == 0)
	{
		displacement = m_animatedCharacterOrientation.GetIBasis3D() * animationDisplacementMagnitude;
	}

	m_animatedCharacterPos += displacement;

	if (displacement != Vec3(0, 0, 0))
	{
		Vec3 movementDir = (m_animatedCharacterPos - m_animatedCharacterPosPreviousFrame).GetNormalized();
		movementDir = movementDir.GetFlattenedNormalXY();

		// Get signed angles via Atan2 - no sign ambiguity
		float targetAngle = ConvertRadiansToDegrees(atan2f(movementDir.y, movementDir.x));
		float currentAngle = ConvertRadiansToDegrees(atan2f(
			m_animatedCharacterOrientation.GetIBasis3D().y,
			m_animatedCharacterOrientation.GetIBasis3D().x
		));

		float turnedAngle = GetTurnedTowardDegrees(currentAngle, targetAngle, deltaSeconds * m_characterTurnSpeed);

		Vec3 newForward = Vec3(CosDegrees(turnedAngle), SinDegrees(turnedAngle), 0.f);
		m_animatedCharacterOrientation = Mat44(
			newForward,
			CrossProduct3D(Vec3(0, 0, 1), newForward),
			Vec3(0, 0, 1),
			Vec3()
		);
	}
}

void GameModeThirdPerson::UpdateCameraPosition()
{
	Vec3 cameraLookDirection = m_thirdPersonCam->GetOrientation().GetForwardNormal();
	Vec3 cameraArmFollow = -cameraLookDirection * m_cameraFollowDistance;
	m_thirdPersonCam->SetPosition(m_animatedCharacterPos + cameraArmFollow);
}

void GameModeThirdPerson::UpdateConstantBuffers()
{
	m_perFrameConstants.c_debugInt = m_debugShaderMode;
	for (int i = 0; i < (int)m_skeleton.m_bones.size(); i++)
	{
		m_perFrameConstants.boneMatrices[i] = m_skeleton.m_bones[i].m_finalSkinningMatrix;
	}
	g_theRenderer->CopyCPUToGPU(&m_perFrameConstants, sizeof(PerFrameConstants), m_perFrameBuffer);
	g_theRenderer->BindConstantBuffer(4, m_perFrameBuffer);
}

void GameModeThirdPerson::UpdateDebugMessages(float deltaSeconds)
{
	if (m_frameCounter >= m_framesbetweenFPSUpdate)
	{
		m_frameCounter = 0;
		m_fps = 1.f / deltaSeconds;
	}
	else
	{
		m_frameCounter++;
	}

	std::string fpsString = Stringf("Current FPS: %.2f", m_fps);
	std::string instructions = "WASD - Move, Mouse - Look, M - Look at Target, E - Reach Target";
	std::string IKEnabled;
	std::string SlopeDegrees;
	if (m_IKEnabled)
	{
		IKEnabled = "IK Legs (K): Enabled";
	}
	else
	{
		IKEnabled = "IK Legs (K): Disabled";
	}

	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .04f)), SCREEN_SIZE_Y * .02f, fpsString, Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .06f)), SCREEN_SIZE_Y * .02f, GetShaderDebugModeDescription(), Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .08f)), SCREEN_SIZE_Y * .02f, GetRenderDebugModeDescription(), Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .12f)), SCREEN_SIZE_Y * .02f, IKEnabled, Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .18f)), SCREEN_SIZE_Y * .02f, instructions, Rgba8::WHITE, .7f);

	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .2f)), SCREEN_SIZE_Y * .02f, Stringf("Animation Timer Fraction: %.5f", m_skeletalAnimator->GetElapsedFraction()), Rgba8::WHITE, .7f);
	if (m_leftFootGroundedFractionRangeSet[0].IsOnRange(m_skeletalAnimator->GetElapsedFraction()))
	{
		g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .22f)), SCREEN_SIZE_Y * .02f, "Left Foot Grounded", Rgba8::GREEN, .7f);
	}
	else
	{
		g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .22f)), SCREEN_SIZE_Y * .02f, "Left Foot Not Grounded", Rgba8::RED, .7f);
	}

	bool rightFound = false;
	for (int i = 0; i < m_rightFootGroundedFractionRangeSet.size(); i++)
	{
		if (m_rightFootGroundedFractionRangeSet[i].IsOnRange(m_skeletalAnimator->GetElapsedFraction()))
		{
			rightFound = true;
		}
	}
	if (rightFound)
	{
		g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .24f)), SCREEN_SIZE_Y * .02f, "Right Foot Grounded", Rgba8::GREEN, .7f);
	}
	else
	{
		g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .24f)), SCREEN_SIZE_Y * .02f, "Right Foot Not Grounded", Rgba8::RED, .7f);
	}
}

void GameModeThirdPerson::UpdateCharacterWorldPosition(float deltaSeconds)
{
	deltaSeconds;
	m_animatedCharacterPosPreviousFrame = m_animatedCharacterPos;

	RaycastResult3D result = RaycastVsWorldCollision(m_animatedCharacterPos + Vec3(0, 0, .6f), Vec3(0, 0, -1), 5.f);
	if (result.m_didImpact)
	{
		m_animatedCharacterPos = Vec3(m_animatedCharacterPos.x, m_animatedCharacterPos.y, result.m_impactPos.z);
	}
	else
	{
		m_animatedCharacterPos = Vec3(m_animatedCharacterPos.x, m_animatedCharacterPos.y, 0.f);
	}
}

void GameModeThirdPerson::UpdateSkeletonVertexArrays(const SkeletonMeshData& originalMesh, std::vector<Vertex_SKEL>& outputVertices, std::vector<unsigned int>& outputIndices)
{
	outputVertices.clear();
	outputVertices.reserve(originalMesh.m_vertices.size());
	outputIndices.clear();
	outputIndices.reserve(originalMesh.m_vertices.size());

	unsigned int indexCount = 0;
	for (int i = 0; i < (int)originalMesh.m_vertices.size(); i++)
	{
		Vertex_SKEL skinnedVertex = originalMesh.m_vertices[i];

		outputVertices.push_back(skinnedVertex);
		outputIndices.push_back(indexCount);
		indexCount++;
	}
}

void GameModeThirdPerson::UpdateFootPathPredictions()
{
	float currentElapsedFraction = (float)m_skeletalAnimator->GetElapsedFraction();

	int		currentleftFootRangeIndex = -1;
	float	elapsedFractionToNextLeftFootPlacement = 0.f;
	for (int li = 0; li < (int)m_leftFootGroundedFractionRangeSet.size(); li++)
	{
		if (currentElapsedFraction < m_leftFootGroundedFractionRangeSet[li].m_min)
		{
			currentleftFootRangeIndex = li;
			break;
		}
		else if (m_leftFootGroundedFractionRangeSet[li].IsOnRange(currentElapsedFraction))
		{
			currentleftFootRangeIndex = li + 1;
			break;
		}
	}
	if (currentleftFootRangeIndex == (int)m_leftFootGroundedFractionRangeSet.size() || currentleftFootRangeIndex == -1)
	{
		elapsedFractionToNextLeftFootPlacement = m_leftFootGroundedFractionRangeSet[0].m_min + (1.f - currentElapsedFraction);
	}
	else
	{
		elapsedFractionToNextLeftFootPlacement = m_leftFootGroundedFractionRangeSet[currentleftFootRangeIndex].m_min - currentElapsedFraction;
	}
	float	timeToNextleftFootPlacement = elapsedFractionToNextLeftFootPlacement * m_currentAnimation.m_durationSeconds;
	float	predictedXYDisplacement = m_currentAnimation.GetXYDisplacementFromTime(timeToNextleftFootPlacement, (float)m_skeletalAnimator->GetElapsedTime()).GetLength();
	JointPose leftFootPose = m_currentAnimation.GetBoneTransformAtTime(58, (float)m_skeletalAnimator->GetElapsedTime() + timeToNextleftFootPlacement);
	Mat44 LeftFootMatrix = leftFootPose.GetPoseMatrixRelativeToPositionAndOrientation(m_animatedCharacterPos + (m_animatedCharacterOrientation.GetIBasis3D() * predictedXYDisplacement), m_animatedCharacterOrientation);
	Vec3 predictedLeftFootPos = LeftFootMatrix.GetTranslation3D();
	RaycastResult3D leftLegHeight = RaycastVsWorldVisual(predictedLeftFootPos + Vec3(0, 0, 1), Vec3(0, 0, -1), 10);
	predictedLeftFootPos = leftLegHeight.m_impactPos;
	m_footIKFrameData.m_predictedLeftFootPos = predictedLeftFootPos;
	DebugAddWorldSphere(predictedLeftFootPos, .03f, 0.f, Rgba8::GREEN, Rgba8::GREEN);

	int		currentRightFootRangeIndex = -1;
	float	elapsedFractionToNextRightFootPlacement = 0.f;
	for (int ri = 0; ri < (int)m_leftFootGroundedFractionRangeSet.size(); ri++)
	{
		if (currentElapsedFraction < m_rightFootGroundedFractionRangeSet[ri].m_min)
		{
			currentRightFootRangeIndex = ri;
			break;
		}
		else if (m_rightFootGroundedFractionRangeSet[ri].IsOnRange(currentElapsedFraction))
		{
			currentRightFootRangeIndex = ri + 1;
			break;
		}
	}
	if (currentRightFootRangeIndex == (int)m_rightFootGroundedFractionRangeSet.size() || currentRightFootRangeIndex == -1)
	{
		elapsedFractionToNextRightFootPlacement = m_rightFootGroundedFractionRangeSet[0].m_min + (1.f - currentElapsedFraction);
	}
	else
	{
		elapsedFractionToNextRightFootPlacement = m_rightFootGroundedFractionRangeSet[currentRightFootRangeIndex].m_min - currentElapsedFraction;
	}
	float	timeToNextRightFootPlacement = elapsedFractionToNextRightFootPlacement * m_currentAnimation.m_durationSeconds;
	predictedXYDisplacement = m_currentAnimation.GetXYDisplacementFromTime(timeToNextRightFootPlacement, (float)m_skeletalAnimator->GetElapsedTime()).GetLength();
	JointPose rightFootPose = m_currentAnimation.GetBoneTransformAtTime(63, (float)m_skeletalAnimator->GetElapsedTime() + timeToNextRightFootPlacement);
	Mat44 rightFootMatrix = rightFootPose.GetPoseMatrixRelativeToPositionAndOrientation(m_animatedCharacterPos + (m_animatedCharacterOrientation.GetIBasis3D() * predictedXYDisplacement), m_animatedCharacterOrientation);
	Vec3 predictedRightFootPos = rightFootMatrix.GetTranslation3D();
	RaycastResult3D rightLegHeight = RaycastVsWorldVisual(predictedRightFootPos + Vec3(0, 0, 1), Vec3(0, 0, -1), 10);
	predictedRightFootPos = rightLegHeight.m_impactPos;
	m_footIKFrameData.m_predictedRightFootPos = predictedRightFootPos;
	DebugAddWorldSphere(predictedRightFootPos, .03f, 0.f, Rgba8::BLUE, Rgba8::BLUE);
}

void GameModeThirdPerson::UpdateFootGroundedStates()
{
	float currentElapsedFraction = (float)m_skeletalAnimator->GetElapsedFraction();
	//Left foot
	int		currentleftFootRangeIndex = -1;
	int		previousleftFootRangeIndex = (int)m_leftFootGroundedFractionRangeSet.size() - 1;
	for (int li = 0; li < (int)m_leftFootGroundedFractionRangeSet.size(); li++)
	{
		if (m_leftFootGroundedFractionRangeSet[li].IsOnRange(currentElapsedFraction))
		{
			currentleftFootRangeIndex = li;
		}
		if (m_leftFootGroundedFractionRangeSet[li].m_max < currentElapsedFraction)
		{
			previousleftFootRangeIndex = li;
		}
	}
	if (currentleftFootRangeIndex == -1)
	{
		m_footIKFrameData.m_leftFootGrounded = false;
		if (previousleftFootRangeIndex == (int)m_leftFootGroundedFractionRangeSet.size() - 1)
		{
			m_footIKFrameData.m_leftStepFraction = RangeMapClamped(currentElapsedFraction,
				m_leftFootGroundedFractionRangeSet[previousleftFootRangeIndex].m_max, 1.f + m_leftFootGroundedFractionRangeSet[0].m_min,
				0.f, 1.f);
		}
		else
		{
			m_footIKFrameData.m_leftStepFraction = RangeMapClamped(currentElapsedFraction,
				m_leftFootGroundedFractionRangeSet[previousleftFootRangeIndex].m_max, m_leftFootGroundedFractionRangeSet[previousleftFootRangeIndex + 1].m_min,
				0.f, 1.f);
		}
	}
	else
	{
		m_footIKFrameData.m_leftFootGrounded = true;
		Mat44 leftFootMat = m_skeleton.m_bones[58].m_finalSkinningMatrix;
		leftFootMat.Append(m_skeleton.m_bones[58].m_bindPose);
		m_footIKFrameData.m_leftCurrentGroundedPos = leftFootMat.GetTranslation3D();
		m_lastGroundedLeftFootFraction = currentElapsedFraction;
	}

	//Right foot
	int		currentRightFootRangeIndex = -1;
	int		previousRightFootRangeIndex = (int)m_rightFootGroundedFractionRangeSet.size() - 1;;
	for (int ri = 0; ri < (int)m_rightFootGroundedFractionRangeSet.size(); ri++)
	{
		if (m_rightFootGroundedFractionRangeSet[ri].IsOnRange(currentElapsedFraction))
		{
			currentRightFootRangeIndex = ri;
		}
		if (m_rightFootGroundedFractionRangeSet[ri].m_max < currentElapsedFraction)
		{
			previousRightFootRangeIndex = ri;
		}
	}
	if (currentRightFootRangeIndex == -1)
	{
		m_footIKFrameData.m_rightFootGrounded = false;
		if (previousRightFootRangeIndex == (int)m_rightFootGroundedFractionRangeSet.size() - 1)
		{
			m_footIKFrameData.m_rightStepFraction = RangeMapClamped(currentElapsedFraction,
				m_rightFootGroundedFractionRangeSet[previousRightFootRangeIndex].m_max, 1.f + m_rightFootGroundedFractionRangeSet[0].m_min,
				0.f, 1.f);
		}
		else
		{
			m_footIKFrameData.m_rightStepFraction = RangeMapClamped(currentElapsedFraction,
				m_rightFootGroundedFractionRangeSet[previousRightFootRangeIndex].m_max, m_rightFootGroundedFractionRangeSet[previousRightFootRangeIndex + 1].m_min,
				0.f, 1.f);
		}
	}
	else
	{
		m_footIKFrameData.m_rightFootGrounded = true;
		Mat44 rightFootMat = m_skeleton.m_bones[63].m_finalSkinningMatrix;
		rightFootMat.Append(m_skeleton.m_bones[63].m_bindPose);
		m_footIKFrameData.m_rightCurrentGroundedPos = rightFootMat.GetTranslation3D();
		m_lastGroundedRightFootFraction = currentElapsedFraction;
	}
}

void GameModeThirdPerson::SetSkeletonToBindPose(SkeletonData& skeleton)
{
	for (int boneIndex = 0; boneIndex < skeleton.m_bones.size(); ++boneIndex)
	{
		// Identity matrix = no deformation, should show original mesh
		skeleton.m_bones[boneIndex].m_finalSkinningMatrix = Mat44();
	}
}

void GameModeThirdPerson::UpdateFootTraceCurves()
{
	m_currentAnimation = m_skeletalAnimator->GetCurrentAnimation();
	m_leftFootHeightTrace.clear();
	m_rightFootHeightTrace.clear();
	m_leftFootGroundedFractionRangeSet.clear();
	m_rightFootGroundedFractionRangeSet.clear();
	m_leftFootMaxHeight = -1;
	m_leftFootMinHeight = 100;
	m_rightFootMaxHeight = -1;
	m_rightFootMinHeight = 100;

	//Initialize Foot Curves
	for (int i = 0; i < (int)m_currentAnimation.m_keyFrames.size(); i++)
	{
		m_leftFootHeightTrace.push_back(m_currentAnimation.m_keyFrames[i].m_worldJointPoses[57].m_position.z);
		m_rightFootHeightTrace.push_back(m_currentAnimation.m_keyFrames[i].m_worldJointPoses[62].m_position.z);

		if (m_leftFootMinHeight >= m_leftFootHeightTrace[i])
		{
			m_leftFootMinHeight = m_leftFootHeightTrace[i];
			m_leftFootMinHeightIndex = i;
		}
		if (m_leftFootMaxHeight <= m_leftFootHeightTrace[i])
		{
			m_leftFootMaxHeight = m_leftFootHeightTrace[i];
		}

		if (m_rightFootMinHeight >= m_rightFootHeightTrace[i])
		{
			m_rightFootMinHeight = m_rightFootHeightTrace[i];
			m_rightFootMinHeightIndex = i;
		}
		if (m_rightFootMaxHeight <= m_rightFootHeightTrace[i])
		{
			m_rightFootMaxHeight = m_rightFootHeightTrace[i];
		}
	}

	bool leftGrounded = false;
	bool rightGrounded = false;
	FloatRange leftHolder;
	FloatRange rightHolder;

	if (m_currentAnimation.m_keyFrames[0].m_worldJointPoses[57].m_position.z <= m_leftFootMinHeight + .01f)
	{
		leftGrounded = true;
		leftHolder.m_min = 0.f;
	}
	if (m_currentAnimation.m_keyFrames[0].m_worldJointPoses[62].m_position.z <= m_rightFootMinHeight + .01f)
	{
		rightGrounded = true;
		rightHolder.m_min = 0.f;
	}

	for (int i = 0; i < (int)m_currentAnimation.m_keyFrames.size(); i++)
	{
		//58 heel of left foot
		//63 heel of right foot

		if (m_leftFootMinHeight + .01f >= m_leftFootHeightTrace[i])
		{
			if (!leftGrounded)
			{
				leftGrounded = true;
				leftHolder.m_min = (float)i / (float)m_currentAnimation.m_keyFrames.size();
			}
		}
		else
		{
			if (leftGrounded)
			{
				leftHolder.m_max = (float)i / (float)m_currentAnimation.m_keyFrames.size();
				leftGrounded = false;
				m_leftFootGroundedFractionRangeSet.push_back(leftHolder);
				leftHolder = FloatRange();
			}
		}

		if (m_rightFootMinHeight + .01f >= m_rightFootHeightTrace[i])
		{
			if (!rightGrounded)
			{
				rightGrounded = true;
				rightHolder.m_min = (float)i / (float)m_currentAnimation.m_keyFrames.size();
			}
		}
		else
		{
			if (rightGrounded)
			{
				rightHolder.m_max = (float)i / (float)m_currentAnimation.m_keyFrames.size();
				rightGrounded = false;
				m_rightFootGroundedFractionRangeSet.push_back(rightHolder);
				rightHolder = FloatRange();
			}
		}
	}

	if (leftGrounded)
	{
		leftHolder.m_max = 1.f;
		m_leftFootGroundedFractionRangeSet.push_back(leftHolder);
	}
	if (rightGrounded)
	{
		rightHolder.m_max = 1.f;
		m_rightFootGroundedFractionRangeSet.push_back(rightHolder);
	}
}

void GameModeThirdPerson::InitializeAnimationStatesForCharacter()
{
	AnimationTransitionRule newRule;
	AnimationState idleState;
	idleState.m_animation = &m_loadedAnimations[0]; //Idle
	idleState.m_isLooping = true;
	idleState.m_name = "idle";

	AnimationState walkingState;
	walkingState.m_animation = &m_loadedAnimations[1];//Walk
	walkingState.m_isLooping = true;
	walkingState.m_name = "walk";

	AnimationState grabbingState;
	grabbingState.m_animation = &m_loadedAnimations[3]; //Grab
	grabbingState.m_isLooping = false;
	grabbingState.m_onCompleteTransition = "idle";
	grabbingState.m_name = "grab";

	AnimationTransitionRule idleToWalk;
	idleToWalk.m_targetStateIndex = 1; // walk index
	idleToWalk.m_transitionDuration = 0.15f;
	idleState.m_transitions.push_back(idleToWalk);

	AnimationTransitionRule walkToIdle;
	walkToIdle.m_targetStateIndex = 0; // idle index
	walkToIdle.m_transitionDuration = 0.15f;
	walkingState.m_transitions.push_back(walkToIdle);

	// walk -> grab (can grab while walking, slightly longer to look natural)
	AnimationTransitionRule walkToGrab;
	walkToGrab.m_targetStateIndex = 2; // grab index
	walkToGrab.m_transitionDuration = 0.15f;
	walkingState.m_transitions.push_back(walkToGrab);

	// idle -> grab
	AnimationTransitionRule idleToGrab;
	idleToGrab.m_targetStateIndex = 2;
	idleToGrab.m_transitionDuration = 0.1f;
	idleState.m_transitions.push_back(idleToGrab);

	// grab -> idle only (not back to walk, let input decide after)
	AnimationTransitionRule grabToIdle;
	grabToIdle.m_targetStateIndex = 0;
	grabToIdle.m_transitionDuration = 0.2f;
	grabbingState.m_transitions.push_back(grabToIdle);

	m_skeletalAnimator->AddState(idleState);
	m_skeletalAnimator->AddState(walkingState);
	m_skeletalAnimator->AddState(grabbingState);

	m_skeletalAnimator->Startup(0);
}

float GameModeThirdPerson::GetLeftFootHeightAtTime(float time)
{
	if (m_currentAnimation.m_keyFrames.empty())
	{
		return 0.f;
	}

	if (m_currentAnimation.m_isLooping)
	{
		while (time >= m_currentAnimation.m_durationSeconds)
		{
			time -= m_currentAnimation.m_durationSeconds;
		}
	}
	else
	{
		return m_leftFootHeightTrace.back();
	}


	int prev = -1;
	int next = -1;

	// Find the two keyframes around the current time
	for (int i = 0; i < (float)m_currentAnimation.m_keyFrames.size(); ++i)
	{
		if (m_currentAnimation.m_keyFrames[i].m_time >= time)
		{
			next = i;
			prev = (i > 0) ? i-1 : i;
			break;
		}
	}

	if (next == -1)
	{
		return m_leftFootHeightTrace.back();
	}

	if (prev == next)
	{
		return m_leftFootHeightTrace[prev];
	}

	float segmentDuration = m_currentAnimation.m_keyFrames[next].m_time - m_currentAnimation.m_keyFrames[prev].m_time;
	float t = (segmentDuration > 0.0f) ? (time - m_currentAnimation.m_keyFrames[prev].m_time) / segmentDuration : 0.0f;

	float interpolated = Interpolate(m_leftFootHeightTrace[prev], m_leftFootHeightTrace[next], t);

	return interpolated;
}

float GameModeThirdPerson::GetRightFootHeightAtTime(float time)
{
	if (m_currentAnimation.m_keyFrames.empty())
	{
		return 0.f;
	}

	if (m_currentAnimation.m_isLooping)
	{
		while (time >= m_currentAnimation.m_durationSeconds)
		{
			time -= m_currentAnimation.m_durationSeconds;
		}
	}
	else
	{
		return m_leftFootHeightTrace.back();
	}


	int prev = -1;
	int next = -1;

	// Find the two keyframes around the current time
	for (int i = 0; i < (float)m_currentAnimation.m_keyFrames.size(); ++i)
	{
		if (m_currentAnimation.m_keyFrames[i].m_time >= time)
		{
			next = i;
			prev = (i > 0) ? i - 1 : i;
			break;
		}
	}

	if (next == -1)
	{
		return m_rightFootHeightTrace.back();
	}

	if (prev == next)
	{
		return m_rightFootHeightTrace[prev];
	}

	float segmentDuration = m_currentAnimation.m_keyFrames[next].m_time - m_currentAnimation.m_keyFrames[prev].m_time;
	float t = (segmentDuration > 0.0f) ? (time - m_currentAnimation.m_keyFrames[prev].m_time) / segmentDuration : 0.0f;

	float interpolated = Interpolate(m_rightFootHeightTrace[prev], m_rightFootHeightTrace[next], t);

	return interpolated;
}

void GameModeThirdPerson::UpdateArmIKFrameData()
{
	m_armIKFrameData.m_isReaching = m_skeletalAnimator->GetCurrentStateIndex() == 2;

	Mat44 worldHandPos = m_skeleton.m_bones[34].m_finalSkinningMatrix;
	worldHandPos.Append(m_skeleton.m_bones[34].m_bindPose);
	Mat44 worldElbowPos = m_skeleton.m_bones[33].m_finalSkinningMatrix;
	worldElbowPos.Append(m_skeleton.m_bones[33].m_bindPose);

	Vec3 midToTarget = m_reachTarget - worldElbowPos.GetTranslation3D();
	Vec3 midToEnd = worldHandPos.GetTranslation3D() - worldElbowPos.GetTranslation3D();

	//m_armIKFrameData.m_elbowPlaneVector = -CrossProduct3D(midToEnd, midToTarget).GetNormalized();
	m_armIKFrameData.m_characterOrientation = m_animatedCharacterOrientation;

	m_reachFraction = m_skeletalAnimator->GetElapsedFraction();
	if (m_reachFraction >= .5)
	{
		m_reachFraction = (1.f - m_reachFraction) * 2.f;
	}
	else
	{
		m_reachFraction = m_reachFraction * 2.f;
	}
	m_armIKFrameData.m_reachBlendFraction = m_reachFraction;
}

void GameModeThirdPerson::UpdateFootIKFrameData()
{
	BoneData leftToe = m_skeleton.m_bones[m_footIKController->m_config.m_leftBallIndex];
	Mat44 leftToeBoneMatrix = leftToe.m_finalSkinningMatrix;
	leftToeBoneMatrix.Append(leftToe.m_bindPose);

	BoneData rightToe = m_skeleton.m_bones[m_footIKController->m_config.m_rightBallIndex];
	Mat44 rightToeBoneMatrix = rightToe.m_finalSkinningMatrix;
	rightToeBoneMatrix.Append(rightToe.m_bindPose);

	RaycastResult3D raycastLeft = RaycastVsWorldVisual(leftToeBoneMatrix.GetTranslation3D() + Vec3(0, 0, 4), Vec3(0, 0, -1), 10.f);
	RaycastResult3D raycastRight = RaycastVsWorldVisual(rightToeBoneMatrix.GetTranslation3D() + Vec3(0, 0, 4), Vec3(0, 0, -1), 10.f);

	m_footIKFrameData.m_leftDidImpact = raycastLeft.m_didImpact;
	m_footIKFrameData.m_leftImpactPos = raycastLeft.m_impactPos;
	m_footIKFrameData.m_leftImpactNormal = raycastLeft.m_impactNormal;

	m_footIKFrameData.m_rightDidImpact = raycastRight.m_didImpact;
	m_footIKFrameData.m_rightImpactPos = raycastRight.m_impactPos;
	m_footIKFrameData.m_rightImpactNormal = raycastRight.m_impactNormal;

	UpdateFootGroundedStates();
}

void GameModeThirdPerson::UpdateHeadLookAt()
{
	if (m_lookAtPOI)
	{
		//Neck, Head, Top Head = 4, 5, 6
		Mat44 currentHeadPos = m_skeleton.m_bones[5].m_finalSkinningMatrix;
		currentHeadPos.Append(m_skeleton.m_bones[5].m_bindPose);
		Mat44 newHeadTransform = Mat44::MakeTranslation3D(currentHeadPos.GetTranslation3D());

		Vec3 targetDir = (m_lookAtTargetPos - newHeadTransform.GetTranslation3D()).GetNormalized();
		float angleFromForward = acosf(GetClamped(DotProduct3D(targetDir, m_animatedCharacterOrientation.GetIBasis3D()), -1.f, 1.f));
		if (angleFromForward > ConvertDegreesToRadians(m_headAngleConstraint))
		{
			Vec3 axis = CrossProduct3D(m_animatedCharacterOrientation.GetIBasis3D(), targetDir).GetNormalized();
			Quaternion rot = Quaternion::MakeFromAxisAngleDegrees(axis, m_headAngleConstraint);
			targetDir = rot.RotateVector(m_animatedCharacterOrientation.GetIBasis3D());
		}
	
		Vec3 iBasis = targetDir;
		Vec3 kBasis = iBasis.GetFlattenedNormalXY().GetRotatedAboutZDegrees(90.f);
		Vec3 jBasis = CrossProduct3D(iBasis, kBasis);
		Mat44 lookAtMatrix = Mat44(iBasis,kBasis,jBasis,Vec3());

		newHeadTransform.Append(lookAtMatrix);

		newHeadTransform.Append(m_skeleton.m_bones[5].m_inverseBindPose);

		m_skeleton.m_bones[5].m_finalSkinningMatrix = newHeadTransform;
	}
}

RaycastResult3D GameModeThirdPerson::RaycastVsWorldVisual(Vec3 const& startPos, Vec3 const& fwdNormal, float maxLen)
{
	RaycastResult3D overallResult;
	overallResult.m_didImpact = false;
	overallResult.m_impactDist = maxLen;
	overallResult.m_rayFwdNormal = fwdNormal;
	overallResult.m_impactNormal = fwdNormal;
	overallResult.m_rayMaxLength = maxLen;
	overallResult.m_impactPos = Vec3(startPos.x, startPos.y, 0.f); //Default raycast to hit at z = 0

	RaycastResult3D stairs = m_stairs.RaycastVsVisualStairs(startPos, fwdNormal, maxLen);

	if (stairs.m_didImpact && stairs.m_impactDist < overallResult.m_impactDist)
	{
		overallResult = stairs;
	}

	RaycastResult3D blockResult;
	for (int i = 0; i < (int)m_obstacleBlocks.size(); i++)
	{
		blockResult = RaycastVsOBB3D(startPos, fwdNormal, maxLen, m_obstacleBlocks[i]);
		if (blockResult.m_didImpact && blockResult.m_impactDist < overallResult.m_impactDist)
		{
			overallResult = blockResult;
		}
	}
	overallResult.m_impactPos.z += .01f;
	return overallResult;
}

RaycastResult3D GameModeThirdPerson::RaycastVsWorldCollision(Vec3 const& startPos, Vec3 const& fwdNormal, float maxLen)
{
	RaycastResult3D overallResult;
	overallResult.m_didImpact = false;
	overallResult.m_impactDist = maxLen;
	overallResult.m_rayFwdNormal = fwdNormal;
	overallResult.m_impactNormal = fwdNormal;
	overallResult.m_rayMaxLength = maxLen;

	RaycastResult3D stairs = m_stairs.RaycastVsRamp(startPos, fwdNormal, maxLen);

	if (stairs.m_didImpact && stairs.m_impactDist < overallResult.m_impactDist)
	{
		overallResult = stairs;
	}

	RaycastResult3D blockResult;
	for (int i = 0; i < (int)m_obstacleBlocks.size(); i++)
	{
		blockResult = RaycastVsOBB3D(startPos, fwdNormal, maxLen, m_obstacleBlocks[i]);
		if (blockResult.m_didImpact && blockResult.m_impactDist < overallResult.m_impactDist)
		{
			overallResult = blockResult;
		}
	}
	return overallResult;
}

void GameModeThirdPerson::Render() const
{
	RenderGame();
}

void GameModeThirdPerson::RenderGame() const
{
	g_theRenderer->ClearScreen(Rgba8(0, 0, 0, 255));

	Vec3 iBasis;
	Vec3 jBasis;
	Vec3 kBasis;
	m_thirdPersonCam->GetOrientation().GetAsVectors_IFwd_JLeft_KUp(iBasis, jBasis, kBasis);
	Mat44 basisTranslation = Mat44();
	basisTranslation.SetTranslation3D(m_thirdPersonCam->GetPosition() + (iBasis * 0.2f));
	DebugAddBasis(basisTranslation, 0, .0075f, .0005f, 1.f, 1.f);

	g_theRenderer->BeginCamera(*m_thirdPersonCam);
	g_theRenderer->BindTexture(nullptr, 0);
	g_theRenderer->BindTexture(nullptr, 1);


	//Handle Mesh Drawing ------------------------------------------------------------------------------------------------------------------------------------
	if (m_debugRenderMode == 0) //Shaded mesh
	{
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->BindShader(g_theRenderer->CreateOrGetShader("SkinningBlinnPhong", VertexType::SKEL));
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetStatesIfChanged();
		g_theRenderer->DrawIndexArray(m_skeletonMeshVerts, m_skeletonMeshIndexes);
	}
	else if (m_debugRenderMode == 1) //Wireframe mesh
	{
		g_theRenderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_BACK);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->BindShader(g_theRenderer->CreateOrGetShader("SkinningBlinnPhong", VertexType::SKEL));
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetStatesIfChanged();
		g_theRenderer->DrawIndexArray(m_skeletonMeshVerts, m_skeletonMeshIndexes);
	}
	else if (m_debugRenderMode == 2) //No mesh
	{
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetStatesIfChanged();
		m_skeleton.DebugRenderSkeleton();
	}

	//Draw grid ---------------------------------------------------------------------------------------------------------------------------------------------
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(m_worldVerts);
	g_theRenderer->DrawVertexArray(m_groundVerts);
	
	//DEBUG DRAW IK------------------------------------------------------------------------------------------------------------------------------------------
	if (m_IKEnabled)
	{
		m_footIKController->GetRightLegChain().DebugAddChain();
		m_footIKController->GetLeftLegChain().DebugAddChain();

		m_rightArmChain.DebugAddChain();
	}

	//DebugAddWorldSphere(m_animatedCharacterPos, .1f, 0.f);

	DebugRenderWorld(*m_thirdPersonCam);


	//Draw Screen ---------------------------------------------------------------------------------------------------------------------------------------------
	g_theRenderer->BeginCamera(m_screenCamera);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->BindTexture(&g_testFont->GetTexture());
	g_theRenderer->DrawVertexArray(m_verts);
	g_theRenderer->EndCamera(m_screenCamera);
}

void GameModeThirdPerson::Shutdown()
{
	ShutdownGame();
}

void GameModeThirdPerson::ShutdownGame()
{
	m_worldVerts.clear();
	m_worldVertTBNs.clear();
	m_worldVertsIndexes.clear();
	m_verts.clear();
	delete m_lightBuffer;
	delete m_perFrameBuffer;
	delete m_fbxContext;
	delete m_thirdPersonCam;
}

void GameModeThirdPerson::LoadAllFbx()
{
	m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/SimpleBox/Box.fbx");
	//m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/Capoeira.fbx");
	m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/HipHop.fbx");
	m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/Walking.fbx");
	m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/SillyDancing.fbx");
}

std::string GameModeThirdPerson::GetShaderDebugModeDescription()
{
	switch (m_debugShaderMode)
	{
	case 1:  return "Shader Debug Mode: Diffuse Texel";
	case 2:  return "Shader Debug Mode: Surface Color";
	case 3:  return "Shader Debug Mode: UV Coordinates";
	case 4:  return "Shader Debug Mode: Tangent (Model Space)";
	case 5:  return "Shader Debug Mode: Bitangent (Model Space)";
	case 6:  return "Shader Debug Mode: Normal (Model Space)";
	case 7:  return "Shader Debug Mode: Normal Map Texel";
	case 8:  return "Shader Debug Mode: Normal (TBN Space)";
	case 9:  return "Shader Debug Mode: Normal (World Space)";
	case 10: return "Shader Debug Mode: Lit (No Normal Map)";
	case 11: return "Shader Debug Mode: Sunlight Strength (Mode 11)";
	case 12: return "Shader Debug Mode: Sunlight Strength (Mode 12)";
	case 13: return "Shader Debug Mode: SGE Texel";
	case 14: return "Shader Debug Mode: Tangent (World Space)";
	case 15: return "Shader Debug Mode: Bitangent (World Space)";
	case 16: return "Shader Debug Mode: Normal (World Space)";
	case 17: return "Shader Debug Mode: Model I Basis (World Space)";
	case 18: return "Shader Debug Mode: Model J Basis (World Space)";
	case 19: return "Shader Debug Mode: Model K Basis (World Space)";
	case 20: return "Shader Debug Mode: Emissive Light";
	default: return "Shader Debug Mode: None";
	}
}

std::string GameModeThirdPerson::GetRenderDebugModeDescription()
{
	switch (m_debugRenderMode)
	{
	case 1:  return "Render Debug Mode: Wire Frame";
	case 2:  return "Render Debug Mode: No Mesh";
	default: return "Render Debug Mode: None";
	}
}
