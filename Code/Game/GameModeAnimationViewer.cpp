#include "GameModeAnimationViewer.hpp"
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

extern App* g_theApp;
extern Renderer* g_theRenderer;
extern RandomNumberGenerator* g_rng;
extern InputSystem* g_theInputSystem;
extern AudioSystem* g_theAudioSystem;
extern Clock* g_theSystemClock;
extern BitmapFont* g_testFont;
extern Window* g_theWindow;
extern DevConsole* g_theDevConsole;

GameModeAnimationViewer::GameModeAnimationViewer()
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

	//Load Fbxs-------------------------------------------------------------------------------------
	m_fbxContext = new FbxContext();
	//LoadAllFbx();

	//m_fbxContext->LoadFbxFile("Data/FBXs/Walking.fbx");
	//SaveSkeletalAnimationToSkan(m_fbxContext->GetSceneData()->m_animationClips[1], "Data/SKANs/walking.skan");
	//m_fbxContext->LoadFbxFile("Data/FBXs/HipHop.fbx");
	//SaveSkeletalAnimationToSkan(m_fbxContext->GetSceneData()->m_animationClips[1], "Data/SKANs/hiphop.skan");
	//m_fbxContext->LoadFbxFile("Data/FBXs/SillyDancing.fbx");
	//SaveSkeletalAnimationToSkan(m_fbxContext->GetSceneData()->m_animationClips[1], "Data/SKANs/dance.skan");

	m_skeletonMeshVerts = LoadSkeletalMeshFromSkesh("Data/SKESHs/dummy.skesh").m_vertices;
	m_skeletonMeshIndexes = LoadSkeletalMeshFromSkesh("Data/SKESHs/dummy.skesh").m_indices;

	m_loadedAnimations.push_back(LoadSkeletalAnimationFromSkan("Data/SKANs/hiphop.skan"));
	m_loadedAnimations.push_back(LoadSkeletalAnimationFromSkan("Data/SKANs/walking.skan"));
	m_loadedAnimations.push_back(LoadSkeletalAnimationFromSkan("Data/SKANs/dance.skan"));
	m_loadedAnimations.push_back(LoadSkeletalAnimationFromSkan("Data/SKANs/idle.skan"));


	m_blender.AddAnimationLayer(&m_loadedAnimations[1], 1.0f, false, lowerBodyMask);
	m_blender.AddAnimationLayer(&m_loadedAnimations[0], 1.0f, false, upperBodyMask);
}

GameModeAnimationViewer::~GameModeAnimationViewer()
{
	delete m_gameClock;
}

void GameModeAnimationViewer::Startup()
{
	m_gameClock = new Clock(*g_theSystemClock);

	//m_fbxFileName = "Data/FBXs/SimpleBox/Box.fbx";
	//m_fbxFileName = "Data/FBXs/James.fbx";

	m_fbxFileName = "Data/FBXs/Walking.fbx";
	m_animationTimer = new Timer(m_loadedAnimations[0].m_durationSeconds, m_gameClock);
	m_fbxFileName = "Data/FBXs/HipHop.fbx";
	m_currentAnimation = m_loadedAnimations[0];
	//m_skeleton = m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/Walking.fbx")->m_skeleton;
	//SaveSkeletonToSkel(m_skeleton, "Data/SKELs/dummy.skel");
	m_skeleton = LoadSkeletonFromSkel("Data/SKELs/dummy.skel");
	StartGame();

	//Left leg: 55, 56, 57
	//Right leg: 60, 61, 62
	m_leftLegChain.start = m_skeleton.m_bones[55].m_bindPose.GetTranslation3D();
	m_leftLegChain.mid = m_skeleton.m_bones[56].m_bindPose.GetTranslation3D();
	m_leftLegChain.end = m_skeleton.m_bones[57].m_bindPose.GetTranslation3D();
	m_leftLegChain.lenStartMid = (m_leftLegChain.mid - m_leftLegChain.start).GetLength();
	m_leftLegChain.lenMidEnd = (m_leftLegChain.end - m_leftLegChain.mid).GetLength();

	m_rightLegChain.start = m_skeleton.m_bones[60].m_bindPose.GetTranslation3D();
	m_rightLegChain.mid = m_skeleton.m_bones[61].m_bindPose.GetTranslation3D();
	m_rightLegChain.end = m_skeleton.m_bones[62].m_bindPose.GetTranslation3D();
	m_rightLegChain.lenStartMid = (m_rightLegChain.mid - m_rightLegChain.start).GetLength();
	m_rightLegChain.lenMidEnd = (m_rightLegChain.end - m_rightLegChain.mid).GetLength();

	InitializeFootTraceCurves();
}

void GameModeAnimationViewer::StartGame()
{
	m_player = new Player(this, Vec3(-1.f, 0.f, 2.f));
	m_player->m_playerCamera = new Camera();
	m_player->m_playerCamera->SetOrthographicView(Vec2(WORLD_MINS_X, WORLD_MINS_Y), Vec2(WORLD_MAXS_X, WORLD_MAXS_Y));
	m_player->m_playerCamera->SetPerspectiveView(g_theWindow->GetConfig().m_aspectRatio, 50.f, .1f, 100.f);
	m_player->m_playerCamera->SetCameraToRenderTransform(Mat44(Vec3(0.f, 0.f, 1.f), Vec3(-1.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f), Vec3(0.f, 0.f, 0.f)));

	m_worldVerts.clear();
	m_debug2Verts.clear();
	m_debug2Verts.clear();
	int gridSize = 50;
	float halfWidth = .01f;
	float startingY = -(float)gridSize * .5f;

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

	if (m_angleSlope)
	{
		EulerAngles obbOrientation = EulerAngles(0.f, -30.f, -30.f);
		Vec3 iB;
		Vec3 jB;
		Vec3 kB;
		obbOrientation.GetAsVectors_IFwd_JLeft_KUp(iB, jB, kB);
		m_groundSlopeBlock = OBB3(Vec3(1, 0, 0), iB, jB, kB, Vec3(5, 5, .5f));
		AddVertsForOBB3D(m_groundVerts, m_groundSlopeBlock, Rgba8(100, 100, 100, 170));
	}
	else
	{
		m_groundSlopeBlock = OBB3(Vec3(1, 0, 0), Vec3(1,0,0), Vec3(0,1,0), Vec3(0,0,1), Vec3(5, 5, .02f));
		AddVertsForOBB3D(m_groundVerts, m_groundSlopeBlock, Rgba8(100, 100, 100, 170));
	}


	//Animation testing --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	m_animationTimer->Start();

	//UpdateSkeletonVertexArrays(m_staticMeshData, m_skeletonMeshVerts, m_skeletonMeshIndexes);
}

void GameModeAnimationViewer::Update()
{
	UpdateGame();
}

void GameModeAnimationViewer::UpdateGame()
{
	float deltaSeconds = (float)m_gameClock->GetDeltaSeconds();

	m_verts.clear();
	m_debug1Verts.clear();
	m_debug2Verts.clear();
	m_debugSkeletonVerts.clear();

	if (m_frameCounter >= m_framesbetweenFPSUpdate)
	{
		m_frameCounter = 0;
		m_fps = 1.f / deltaSeconds;
	}
	else
	{
		m_frameCounter++;
	}

	int numTris = (int)m_worldVertsIndexes.size() / 3;

	std::string fpsString = Stringf("Current FPS: %.2f", m_fps);
	std::string trisString = Stringf("Number of Triangles: %i", numTris);
	std::string instructions = "(1) HipHop (2) Walk (3) Dancin (4) Walk + Dancin";
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
	if (m_angleSlope)
	{
		SlopeDegrees = Stringf("Current Ground Slope (M): %f", 30.f);
	}
	else
	{
		SlopeDegrees = Stringf("Current Ground Slope (M): %f", 0.f);
	}

	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - SCREEN_SIZE_Y * .02f), SCREEN_SIZE_Y * .02f, "Scene A", Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .04f)), SCREEN_SIZE_Y * .02f, fpsString, Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .06f)), SCREEN_SIZE_Y * .02f, GetShaderDebugModeDescription(), Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .08f)), SCREEN_SIZE_Y * .02f, GetRenderDebugModeDescription(), Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .10f)), SCREEN_SIZE_Y * .02f, trisString, Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .12f)), SCREEN_SIZE_Y * .02f, IKEnabled, Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .14f)), SCREEN_SIZE_Y * .02f, SlopeDegrees, Rgba8::WHITE, .7f);
	g_testFont->AddVertsForText2D(m_verts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .18f)), SCREEN_SIZE_Y * .02f, instructions, Rgba8::WHITE, .7f);

	//SkinnedMeshDrawing
	if (!m_animationTimer->DecrementPeriodIfElapsed())
	{
		if (m_playAnimation)
		{
			if (m_blendTestState)
			{
				m_blendFraction = GetClampedZeroToOne((float)(m_animationTimer->GetElapsedFraction() * 2.f));
				UpdateSkeletonPosition(deltaSeconds);
				UpdateSkeletonMatrices(m_skeleton, m_currentAnimation, (float)m_animationTimer->GetElapsedTime());
			}
			else
			{
				UpdateSkeletonPosition(deltaSeconds);
				UpdateSkeletonMatrices(m_skeleton, m_currentAnimation, (float)m_animationTimer->GetElapsedTime());
				//UpdateSkeletonMatricesLocally(m_skeleton, m_currentAnimation, (float)m_animationTimer->GetElapsedTime());


				if (m_IKEnabled)
				{
					UpdateIKChains();
				}
				else
				{
					BoneData leftHip = m_skeleton.m_bones[55];
					BoneData rightHip = m_skeleton.m_bones[60];

					BoneData leftKnee = m_skeleton.m_bones[56];
					BoneData rightKnee = m_skeleton.m_bones[61];

					BoneData leftFoot = m_skeleton.m_bones[57];
					BoneData rightFoot = m_skeleton.m_bones[62];

					BoneData leftToe = m_skeleton.m_bones[59];
					BoneData rightToe = m_skeleton.m_bones[64];

					Mat44 leftHipBoneMatrix = leftHip.m_finalSkinningMatrix;
					leftHipBoneMatrix.Append(leftHip.m_bindPose);
					Mat44 rightHipBoneMatrix = rightHip.m_finalSkinningMatrix;
					rightHipBoneMatrix.Append(rightHip.m_bindPose);

					Mat44 leftKneeBoneMatrix = leftKnee.m_finalSkinningMatrix;
					leftKneeBoneMatrix.Append(leftKnee.m_bindPose);
					Mat44 rightKneeBoneMatrix = rightKnee.m_finalSkinningMatrix;
					rightKneeBoneMatrix.Append(rightKnee.m_bindPose);

					Mat44 leftFootBoneMatrix = leftFoot.m_finalSkinningMatrix;
					leftFootBoneMatrix.Append(leftFoot.m_bindPose);
					Mat44 rightFootBoneMatrix = rightFoot.m_finalSkinningMatrix;
					rightFootBoneMatrix.Append(rightFoot.m_bindPose);

					Mat44 leftToeBoneMatrix = leftToe.m_finalSkinningMatrix;
					leftFootBoneMatrix.Append(leftToe.m_bindPose);
					Mat44 rightToeBoneMatrix = rightToe.m_finalSkinningMatrix;
					rightFootBoneMatrix.Append(rightToe.m_bindPose);

					Vec3 leftHipPos = leftHipBoneMatrix.GetTranslation3D();
					Vec3 rightHipPos = rightHipBoneMatrix.GetTranslation3D();

					Vec3 leftKneePos = leftKneeBoneMatrix.GetTranslation3D();
					Vec3 rightKneePos = rightKneeBoneMatrix.GetTranslation3D();

					Vec3 leftFootTargetPos = leftFootBoneMatrix.GetTranslation3D();
					Vec3 rightFootTargetPos = rightFootBoneMatrix.GetTranslation3D();

					JointPose leftToePose = m_currentAnimation.GetBoneTransformAtTime(59, (float)m_animationTimer->GetElapsedTime());
					leftToePose.m_position;
					JointPose rightToePose = m_currentAnimation.GetBoneTransformAtTime(64, (float)m_animationTimer->GetElapsedTime());
					rightToePose.m_position;

					Vec3 leftToeTargetPos = leftToePose.m_position;
					Vec3 rightToeTargetPos = rightToePose.m_position;

					Vec3 leftToeToLeftFoot = leftToeTargetPos - leftFootTargetPos;
					Vec3 rightToeToRightFoot = rightToeTargetPos - rightFootTargetPos;

					RaycastResult3D raycastLeft = RaycastVsOBB3D(leftToeTargetPos + Vec3(0, 0, 4), Vec3(0, 0, -1), 10.f, m_groundSlopeBlock);
					RaycastResult3D raycastRight = RaycastVsOBB3D(rightToeTargetPos + Vec3(0, 0, 4), Vec3(0, 0, -1), 10.f, m_groundSlopeBlock);
					DebugAddWorldArrow(leftToeTargetPos + Vec3(0, 0, 4), raycastLeft.m_impactPos, .02f, 0.f, Rgba8::CYAN);
					DebugAddWorldArrow(rightToeTargetPos + Vec3(0, 0, 4), raycastRight.m_impactPos, .02f, 0.f, Rgba8::YELLOW);
					DebugAddWorldSphere(raycastLeft.m_impactPos, .02f, 0.f, Rgba8::GREEN);
					DebugAddWorldSphere(raycastRight.m_impactPos, .02f, 0.f, Rgba8::PINK);
				}
			}
			
		}
		else
		{
			SetSkeletonToBindPose(m_skeleton); //testing pipeline with Identity Matrix
			//SetSkeletonToBindPoseLocal(m_skeleton); //testing pipeline with Identity Matrix
		}

		// Apply CPU skinning
		//ApplyCPUSkinning(m_staticMeshData, m_skeleton, m_worldVertTBNs, m_worldVertsIndexes);
	}
	else
	{
		m_animatedCharacterPos = Vec3();
	}

	m_player->Update();
	HandleInputGame();
	UpdateConstantBuffers();
}

void GameModeAnimationViewer::HandleInputGame()
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

	if (g_theInputSystem->WasKeyJustPressed('1'))
	{
		m_currentAnimation = m_loadedAnimations[0];
		m_currentAnimationIndex = 0;
		delete m_animationTimer;
		m_animationTimer = new Timer(m_currentAnimation.m_durationSeconds, m_gameClock);
		m_animatedCharacterPos = Vec3();
		m_animationTimer->Start();

		m_blendTestState = false;
		InitializeFootTraceCurves();
	}

	if (g_theInputSystem->WasKeyJustPressed('2'))
	{
		m_currentAnimation = m_loadedAnimations[1];
		m_currentAnimationIndex = 1;
		delete m_animationTimer;
		m_animationTimer = new Timer(1.7f, m_gameClock);
		m_animatedCharacterPos = Vec3();
		m_animationTimer->Start();

		m_blendTestState = false;
		InitializeFootTraceCurves();
	}

	if (g_theInputSystem->WasKeyJustPressed('3'))
	{
		m_currentAnimation = m_loadedAnimations[2];
		m_currentAnimationIndex = 2;
		delete m_animationTimer;
		m_animationTimer = new Timer(m_currentAnimation.m_durationSeconds, m_gameClock);
		m_animatedCharacterPos = Vec3();
		m_animationTimer->Start();

		m_blendTestState = false;
		InitializeFootTraceCurves();
	}

	if (g_theInputSystem->WasKeyJustPressed('4'))
	{
		m_currentAnimation = m_loadedAnimations[0];
		m_currentAnimationIndex = 3;
		delete m_animationTimer;
		m_animationTimer = new Timer(m_currentAnimation.m_durationSeconds, m_gameClock);
		m_animatedCharacterPos = Vec3();
		m_animationTimer->Start();

		m_blendTestState = false;
		InitializeFootTraceCurves();
	}

	if (g_theInputSystem->WasKeyJustPressed('5'))
	{
		m_currentAnimation = m_loadedAnimations[1];
		m_currentAnimationIndex = 1;
		delete m_animationTimer;
		m_animationTimer = new Timer(m_currentAnimation.m_durationSeconds * 2.f, m_gameClock);
		m_animatedCharacterPos = Vec3();
		m_animationTimer->Start();

		m_blendTestState = true;
		InitializeFootTraceCurves();
	}

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_UPARROW))
	{
		m_animatedCharacterPos.x++;
	}
	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_DOWNARROW))
	{
		m_animatedCharacterPos.x--;
	}
	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_LEFTARROW))
	{
		m_animatedCharacterPos.y++;
	}
	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_RIGHTARROW))
	{
		m_animatedCharacterPos.y--;
	}

	if (g_theInputSystem->WasKeyJustPressed('M'))
	{
		m_angleSlope = !m_angleSlope;
		m_groundVerts.clear();
		if (m_angleSlope)
		{
			EulerAngles obbOrientation = EulerAngles(0.f, -30.f, -30.f);
			Vec3 iB;
			Vec3 jB;
			Vec3 kB;
			obbOrientation.GetAsVectors_IFwd_JLeft_KUp(iB, jB, kB);
			m_groundSlopeBlock = OBB3(Vec3(1, 0, 0), iB, jB, kB, Vec3(5, 5, .5f));
			AddVertsForOBB3D(m_groundVerts, m_groundSlopeBlock, Rgba8(100, 100, 100, 170));
		}
		else
		{
			m_groundSlopeBlock = OBB3(Vec3(1, 0, 0), Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1), Vec3(5, 5, .02f));
			AddVertsForOBB3D(m_groundVerts, m_groundSlopeBlock, Rgba8(100, 100, 100, 170));
		}
	}

	if (g_theInputSystem->WasKeyJustPressed('K'))
	{
		m_IKEnabled = !m_IKEnabled;
	}
}

void GameModeAnimationViewer::UpdateConstantBuffers()
{
	m_perFrameConstants.c_debugInt = m_debugShaderMode;
	for (int i = 0; i < (int)m_skeleton.m_bones.size(); i++)
	{
		m_perFrameConstants.boneMatrices[i] = m_skeleton.m_bones[i].m_finalSkinningMatrix;
	}
	g_theRenderer->CopyCPUToGPU(&m_perFrameConstants, sizeof(PerFrameConstants), m_perFrameBuffer);
	g_theRenderer->BindConstantBuffer(4, m_perFrameBuffer);
}

void GameModeAnimationViewer::UpdateSkeletonPosition(float deltaSeconds)
{
	if (m_blendTestState)
	{
		Vec3 displacement = m_currentAnimation.GetXYDisplacementFromTime(deltaSeconds, (float)m_animationTimer->GetElapsedTime());
		m_animatedCharacterPos = m_animatedCharacterPos + (displacement * m_blendFraction);
	}
	else if (m_currentAnimationIndex == 1)
	{
		Vec3 rootOffsetFrameZero = m_currentAnimation.m_keyFrames[0].m_rootOffset;
		float zDistance = rootOffsetFrameZero.z;
		Vec3 displacement = m_currentAnimation.GetXYDisplacementFromTime(deltaSeconds, (float)m_animationTimer->GetElapsedTime());
		m_animatedCharacterPos = m_animatedCharacterPos + displacement;

		RaycastResult3D rootRaycast = RaycastVsOBB3D(m_animatedCharacterPos + rootOffsetFrameZero, Vec3(0, 0, -1), 10.f, m_groundSlopeBlock);
		if (rootRaycast.m_didImpact && abs(rootRaycast.m_impactDist - rootOffsetFrameZero.z) > .08f)
		{
			float difference = (zDistance - rootRaycast.m_impactDist) * .08f;
			m_animatedCharacterPos.z += difference;
		}
	}
}

void GameModeAnimationViewer::UpdateSkeletonVertexArrays(const SkeletonMeshData& originalMesh, std::vector<Vertex_SKEL>& outputVertices, std::vector<unsigned int>& outputIndices)
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


void GameModeAnimationViewer::UpdateSkeletonMatrices(SkeletonData& skeleton, const SkeletalAnimation& animation, float currentTimeInSeconds)
{
	if (m_blendTestState)
	{
		KeyFrame blendFrame = m_blender.GetFrameOfBlendedAnimations(m_loadedAnimations[3], m_loadedAnimations[1], m_blendFraction, (float)m_animationTimer->GetElapsedTime(), currentTimeInSeconds);
		for (int boneIndex = 0; boneIndex < skeleton.m_bones.size(); ++boneIndex)
		{
			JointPose currentTransform;
			if (m_currentAnimationIndex == 3)
			{
				currentTransform = m_blender.GetBlendedBoneAtTime(boneIndex, currentTimeInSeconds);
			}
			else
			{
				currentTransform = blendFrame.m_worldJointPoses[boneIndex];
				//currentTransform = animation.GetBoneTransformAtTimeLocal(boneIndex, currentTimeInSeconds, true);
			}

			Mat44 worldMatrix;
			Mat44 rotationMatrix;
			worldMatrix.Append(Mat44::MakeTranslation3D(currentTransform.m_position));
			worldMatrix.Append(Mat44::MakeTranslation3D(m_animatedCharacterPos));
			worldMatrix.Append(currentTransform.m_quaternionRotation.GetAsMatrix());
			//worldMatrix.Append(Mat44::MakeNonUniformScale3D(currentTransform.m_scale));

			Mat44 finalMatrix = worldMatrix;
			finalMatrix.Append(skeleton.m_bones[boneIndex].m_inverseBindPose);
			skeleton.m_bones[boneIndex].m_finalSkinningMatrix = finalMatrix;
		}
	}
	else
	{
		for (int boneIndex = 0; boneIndex < skeleton.m_bones.size(); ++boneIndex)
		{
			JointPose currentTransform;
			if (m_currentAnimationIndex == 3)
			{
				currentTransform = m_blender.GetBlendedBoneAtTime(boneIndex, currentTimeInSeconds);
			}
			else
			{
				currentTransform = animation.GetBoneTransformAtTime(boneIndex, currentTimeInSeconds, true);
				//currentTransform = animation.GetBoneTransformAtTimeLocal(boneIndex, currentTimeInSeconds, true);
			}

			Mat44 worldMatrix;
			Mat44 rotationMatrix;
			worldMatrix.Append(Mat44::MakeTranslation3D(currentTransform.m_position));
			worldMatrix.Append(Mat44::MakeTranslation3D(m_animatedCharacterPos));
			worldMatrix.Append(currentTransform.m_quaternionRotation.GetAsMatrix());
			//worldMatrix.Append(Mat44::MakeNonUniformScale3D(currentTransform.m_scale));

			Mat44 finalMatrix = worldMatrix;
			finalMatrix.Append(skeleton.m_bones[boneIndex].m_inverseBindPose);
			skeleton.m_bones[boneIndex].m_finalSkinningMatrix = finalMatrix;
		}
	}
}

void GameModeAnimationViewer::UpdateSkeletonMatricesLocally(SkeletonData& skeleton, const SkeletalAnimation& animation, float currentTimeInSeconds)
{
	Mat44 worldMatrix;
	Mat44 rotationMatrix;
	worldMatrix.Append(Mat44::MakeTranslation3D(m_animatedCharacterPos));

	for (int i = 0; i < (int)skeleton.m_bones.size(); ++i)
	{
		BoneData& bone = skeleton.m_bones[i];
		JointPose currentTransform = animation.GetBoneTransformAtTimeLocal(i, currentTimeInSeconds, true);
		Mat44 localAnimated = Mat44::MakeTranslation3D(currentTransform.m_position);
		localAnimated.Append(currentTransform.m_quaternionRotation.GetAsMatrix());
		//localAnimated.Append(Mat44::MakeNonUniformScale3D(currentTransform.m_scale));

		Mat44 modelAnimated;
		if (bone.m_parentIndex < 0)
		{
			modelAnimated = localAnimated;
			bone.m_finalSkinningMatrix = modelAnimated;
			bone.m_finalSkinningMatrix.AppendTranslation3D(m_animatedCharacterPos);
			bone.m_finalSkinningMatrix.Append(bone.m_inverseBindPose);
		}
		else
		{
			BoneData& parent = skeleton.m_bones[bone.m_parentIndex];
			Mat44 parentWorldLocation = parent.m_finalSkinningMatrix;
			parentWorldLocation.Append(parent.m_bindPose);
			modelAnimated = parentWorldLocation;
			modelAnimated.Append(localAnimated);

			bone.m_finalSkinningMatrix = modelAnimated;
			bone.m_finalSkinningMatrix.Append(bone.m_inverseBindPose);
		}
	}
}


void GameModeAnimationViewer::SetSkeletonToBindPose(SkeletonData& skeleton)
{
	for (int boneIndex = 0; boneIndex < skeleton.m_bones.size(); ++boneIndex)
	{
		// Identity matrix = no deformation, should show original mesh
		skeleton.m_bones[boneIndex].m_finalSkinningMatrix = Mat44();
	}
}

void GameModeAnimationViewer::SetSkeletonToBindPoseLocal(SkeletonData& skeleton)
{
	skeleton;
}

void GameModeAnimationViewer::UpdateIKChains()
{
	BoneData leftHip = m_skeleton.m_bones[55];
	BoneData rightHip = m_skeleton.m_bones[60];

	BoneData leftKnee = m_skeleton.m_bones[56];
	BoneData rightKnee = m_skeleton.m_bones[61];

	BoneData leftFoot = m_skeleton.m_bones[57];
	BoneData rightFoot = m_skeleton.m_bones[62];

	BoneData leftToe = m_skeleton.m_bones[59];
	BoneData rightToe = m_skeleton.m_bones[64];

	Mat44 leftHipBoneMatrix = leftHip.m_finalSkinningMatrix;
	leftHipBoneMatrix.Append(leftHip.m_bindPose);
	Mat44 rightHipBoneMatrix = rightHip.m_finalSkinningMatrix;
	rightHipBoneMatrix.Append(rightHip.m_bindPose);

	Mat44 leftKneeBoneMatrix = leftKnee.m_finalSkinningMatrix;
	leftKneeBoneMatrix.Append(leftKnee.m_bindPose);
	Mat44 rightKneeBoneMatrix = rightKnee.m_finalSkinningMatrix;
	rightKneeBoneMatrix.Append(rightKnee.m_bindPose);

	Mat44 leftFootBoneMatrix = leftFoot.m_finalSkinningMatrix;
	leftFootBoneMatrix.Append(leftFoot.m_bindPose);
	Mat44 rightFootBoneMatrix = rightFoot.m_finalSkinningMatrix;
	rightFootBoneMatrix.Append(rightFoot.m_bindPose);

	Mat44 leftToeBoneMatrix = leftToe.m_finalSkinningMatrix;
	leftFootBoneMatrix.Append(leftToe.m_bindPose);
	Mat44 rightToeBoneMatrix = rightToe.m_finalSkinningMatrix;
	rightFootBoneMatrix.Append(rightToe.m_bindPose);

	Vec3 leftHipPos = leftHipBoneMatrix.GetTranslation3D();
	Vec3 rightHipPos = rightHipBoneMatrix.GetTranslation3D();

	Vec3 leftKneePos = leftKneeBoneMatrix.GetTranslation3D();
	Vec3 rightKneePos = rightKneeBoneMatrix.GetTranslation3D();

	Vec3 leftFootTargetPos = leftFootBoneMatrix.GetTranslation3D();
	Vec3 rightFootTargetPos = rightFootBoneMatrix.GetTranslation3D();

	JointPose leftToePose = m_currentAnimation.GetBoneTransformAtTime(59, (float)m_animationTimer->GetElapsedTime());
	leftToePose.m_position;
	JointPose rightToePose = m_currentAnimation.GetBoneTransformAtTime(64, (float)m_animationTimer->GetElapsedTime());
	rightToePose.m_position;

	JointPose leftFootPose = m_currentAnimation.GetBoneTransformAtTime(57, (float)m_animationTimer->GetElapsedTime());
	leftToePose.m_position;
	JointPose rightFootPose = m_currentAnimation.GetBoneTransformAtTime(62, (float)m_animationTimer->GetElapsedTime());
	rightToePose.m_position;

	Vec3 leftToeTargetPos = leftToePose.m_position + m_animatedCharacterPos;
	Vec3 rightToeTargetPos = rightToePose.m_position + m_animatedCharacterPos;

	Vec3 leftToeToLeftFoot = leftToeTargetPos - (leftFootPose.m_position + m_animatedCharacterPos);
	Vec3 rightToeToRightFoot = rightToeTargetPos - (rightFootPose.m_position + m_animatedCharacterPos);

	RaycastResult3D raycastLeft = RaycastVsOBB3D(leftToeTargetPos + Vec3(0, 0, 4), Vec3(0, 0, -1), 10.f, m_groundSlopeBlock);
	RaycastResult3D raycastRight = RaycastVsOBB3D(rightToeTargetPos + Vec3(0, 0, 4), Vec3(0, 0, -1), 10.f, m_groundSlopeBlock);
	DebugAddWorldArrow(leftToeTargetPos + Vec3(0, 0, 4), raycastLeft.m_impactPos, .02f, 0.f, Rgba8::CYAN);
	DebugAddWorldArrow(rightToeTargetPos + Vec3(0, 0, 4), raycastRight.m_impactPos, .02f, 0.f, Rgba8::YELLOW);
	DebugAddWorldSphere(raycastLeft.m_impactPos, .02f, 0.f, Rgba8::GREEN);
	DebugAddWorldSphere(raycastRight.m_impactPos, .02f, 0.f, Rgba8::PINK);

	Vec3 leftNormal = Vec3(0, 0, 1);
	Vec3 rightNormal = Vec3(0, 0, 1);

	//if (raycastLeft.m_impactPos.z >= leftFootTargetPos.z)
	//{
	//	leftFootTargetPos = raycastLeft.m_impactPos + raycastLeft.m_impactNormal * GetLeftFootHeightAtTime((float)m_animationTimer->GetElapsedTime());
	//	leftNormal = raycastLeft.m_impactNormal;
	//}
	//else
	//{
	//	Mat44 footLeftRotationMat = Mat44(m_skeleton.m_bones[57].m_finalSkinningMatrix);
	//	footLeftRotationMat.Append(m_skeleton.m_bones[57].m_bindPose.GetOrthonormalInverse());
	//	leftNormal = footLeftRotationMat.GetIBasis3D();
	//}
	//if (raycastRight.m_impactPos.z >= rightFootTargetPos.z)
	//{
	//	rightFootTargetPos = raycastRight.m_impactPos + raycastRight.m_impactNormal * GetRightFootHeightAtTime((float)m_animationTimer->GetElapsedTime());
	//	rightNormal = raycastRight.m_impactNormal;
	//}
	//else
	//{
	//	Mat44 footRightRotationMat = Mat44(m_skeleton.m_bones[62].m_finalSkinningMatrix);
	//	footRightRotationMat.Append(m_skeleton.m_bones[62].m_bindPose.GetOrthonormalInverse());
	//	rightNormal = footRightRotationMat.GetIBasis3D();
	//}

	//Temp Debug Testing Foot trace
	leftFootTargetPos = raycastLeft.m_impactPos - leftToeToLeftFoot;
	leftNormal = raycastLeft.m_impactNormal;
	rightFootTargetPos = raycastRight.m_impactPos - rightToeToRightFoot;
	rightNormal = raycastRight.m_impactNormal;
	//-------------------------------------------

	Vec3 planeVectorLeft = leftKneeBoneMatrix.GetJBasis3D();
	m_leftLegChain.start = leftHipPos;
	m_leftLegChain = FABRIKSolveChain2SwingConstraint(m_leftLegChain, leftFootTargetPos, planeVectorLeft);
	ApplyLeftLegIKChainToSkeleton(leftNormal);

	Vec3 planeVectorRight = rightKneeBoneMatrix.GetJBasis3D();
	m_rightLegChain.start = rightHipPos;
	m_rightLegChain = FABRIKSolveChain2SwingConstraint(m_rightLegChain, rightFootTargetPos, planeVectorRight);
	ApplyRightLegIKChainToSkeleton(rightNormal);
}

void GameModeAnimationViewer::ApplyLeftLegIKChainToSkeleton(Vec3 const& footNormal)
{
	footNormal;
	Mat44 leftHip;
	Mat44 leftKnee;
	Mat44 leftFoot;

	Vec3 upperLeft = m_leftLegChain.mid - m_leftLegChain.start;
	Vec3 lowerLeft = m_leftLegChain.end - m_leftLegChain.mid;
	Vec3 leftJVector = CrossProduct3D(-upperLeft, lowerLeft).GetNormalized();
	if (DotProduct3D(upperLeft.GetNormalized(), lowerLeft.GetNormalized()) >= .9f)
	{
		Mat44 hipMatrix = m_skeleton.m_bones[55].m_finalSkinningMatrix;
		hipMatrix.Append(m_skeleton.m_bones[55].m_bindPose);
		leftJVector = hipMatrix.GetJBasis3D().GetNormalized();
	}

	leftHip.AppendTranslation3D(m_leftLegChain.start);
	leftKnee.AppendTranslation3D(m_leftLegChain.mid);
	leftFoot.AppendTranslation3D(m_leftLegChain.end);

	Vec3 upperLeftKVector = upperLeft.GetNormalized();
	Vec3 upperLeftIVector = CrossProduct3D(leftJVector, upperLeftKVector);
	Mat44 upperLeftRotationMat = Mat44(upperLeftIVector, leftJVector, upperLeftKVector, Vec3());
	Vec3 kneeLeftKVector = lowerLeft.GetNormalized();
	Vec3 kneeLeftIVector = CrossProduct3D(leftJVector, kneeLeftKVector);
	Mat44 kneeLeftRotationMat = Mat44(kneeLeftIVector, leftJVector, kneeLeftKVector, Vec3());

	Mat44 footLeftRotationMat = Mat44(m_skeleton.m_bones[57].m_finalSkinningMatrix);
	footLeftRotationMat.Append(m_skeleton.m_bones[57].m_bindPose.GetOrthonormalInverse());

	//Rotate foot forward to account for foot to toe base bone joint
	Vec3 footLeftIVector = footNormal.GetNormalized();
	Vec3 footLeftJVector = footLeftRotationMat.GetJBasis3D().GetNormalized();
	Vec3 footLeftKVector = CrossProduct3D(footLeftIVector, footLeftJVector);
	Quaternion rotationForward = Quaternion(footLeftJVector, 35.f);
	footLeftIVector = rotationForward.RotateVector(footLeftIVector);
	footLeftKVector = CrossProduct3D(footLeftIVector, footLeftJVector);
	footLeftRotationMat = Mat44(footLeftIVector, footLeftJVector, footLeftKVector, Vec3());

	leftHip.Append(upperLeftRotationMat);
	leftKnee.Append(kneeLeftRotationMat);
	leftFoot.Append(m_currentAnimation.GetBoneTransformAtTimeLocal(57, (float)m_animationTimer->GetElapsedTime()).m_quaternionRotation.GetAsMatrix());
	leftFoot.Append(footLeftRotationMat);


	leftHip.Append(m_skeleton.m_bones[55].m_bindPose.GetOrthonormalInverse());
	leftKnee.Append(m_skeleton.m_bones[56].m_bindPose.GetOrthonormalInverse());
	leftFoot.Append(m_skeleton.m_bones[57].m_bindPose.GetOrthonormalInverse());

	m_skeleton.m_bones[55].m_finalSkinningMatrix = leftHip;
	m_skeleton.m_bones[56].m_finalSkinningMatrix = leftKnee;
	m_skeleton.m_bones[57].m_finalSkinningMatrix = leftFoot;

	//m_skeleton.m_bones[59].m_finalSkinningMatrix = aggregateFootToeMat;
	//BoneData& parent = m_skeleton.m_bones[m_skeleton.m_bones[57].m_parentIndex];
	Mat44 footWorld = m_skeleton.m_bones[57].m_finalSkinningMatrix;
	m_skeleton.m_bones[57].m_finalSkinningMatrix = footWorld;

	//Bone 58: Toe Base
	Mat44 toeBaseMat = footWorld;
	toeBaseMat.Append(m_skeleton.m_bones[57].m_bindPose);
	toeBaseMat.Append(Mat44::MakeTranslation3D(m_currentAnimation.GetBoneTransformAtTimeLocal(58, (float)m_animationTimer->GetElapsedTime()).m_position));
	toeBaseMat.Append(m_currentAnimation.GetBoneTransformAtTimeLocal(58, (float)m_animationTimer->GetElapsedTime()).m_quaternionRotation.GetAsMatrix());
	toeBaseMat.Append(m_skeleton.m_bones[58].m_inverseBindPose);
	m_skeleton.m_bones[58].m_finalSkinningMatrix = toeBaseMat;

	// Bone 59: Toe
	Mat44 toeMat = toeBaseMat;
	toeMat.Append(m_skeleton.m_bones[58].m_bindPose);
	toeMat.Append(Mat44::MakeTranslation3D(m_currentAnimation.GetBoneTransformAtTimeLocal(59, (float)m_animationTimer->GetElapsedTime()).m_position));
	toeMat.Append(m_currentAnimation.GetBoneTransformAtTimeLocal(59, (float)m_animationTimer->GetElapsedTime()).m_quaternionRotation.GetAsMatrix());
	toeMat.Append(m_skeleton.m_bones[59].m_inverseBindPose);
	m_skeleton.m_bones[59].m_finalSkinningMatrix = toeMat;
}

void GameModeAnimationViewer::ApplyRightLegIKChainToSkeleton(Vec3 const& footNormal)
{
	footNormal;
	Mat44 rightHip;
	Mat44 rightKnee;
	Mat44 rightFoot;

	Vec3 upperRight = m_rightLegChain.mid - m_rightLegChain.start;
	Vec3 lowerRight = m_rightLegChain.end - m_rightLegChain.mid;
	Vec3 rightJVector = CrossProduct3D(-upperRight, lowerRight).GetNormalized();
	if (DotProduct3D(upperRight.GetNormalized(), lowerRight.GetNormalized()) >= .9f)
	{
		Mat44 hipMatrix = m_skeleton.m_bones[60].m_finalSkinningMatrix;
		hipMatrix.Append(m_skeleton.m_bones[60].m_bindPose);
		rightJVector = hipMatrix.GetJBasis3D().GetNormalized();
	}

	rightHip.AppendTranslation3D(m_rightLegChain.start);
	rightKnee.AppendTranslation3D(m_rightLegChain.mid);
	rightFoot.AppendTranslation3D(m_rightLegChain.end);

	Vec3 upperRightKVector = upperRight.GetNormalized();
	Vec3 upperRightIVector = CrossProduct3D(rightJVector, upperRightKVector);
	Mat44 upperRightRotationMat = Mat44(upperRightIVector, rightJVector, upperRightKVector, Vec3());
	Vec3 kneeRightKVector = lowerRight.GetNormalized();
	Vec3 kneeRightIVector = CrossProduct3D(rightJVector, kneeRightKVector);
	Mat44 kneeRightRotationMat = Mat44(kneeRightIVector, rightJVector, kneeRightKVector, Vec3());

	Mat44 footRightRotationMat = Mat44(m_skeleton.m_bones[62].m_finalSkinningMatrix);
	footRightRotationMat.Append(m_skeleton.m_bones[62].m_bindPose);

	//Rotate foot forward to account for foot to toe base bone joint
	Vec3 footRightIVector = footRightRotationMat.GetIBasis3D().GetNormalized();
	Vec3 footRightJVector = footRightRotationMat.GetJBasis3D().GetNormalized();
	Vec3 footRightKVector = CrossProduct3D(footRightIVector, footRightJVector).GetNormalized();
	Quaternion rotationForward = Quaternion(footRightJVector, 33.f);
	footRightIVector = rotationForward.RotateVector(footRightIVector);
	footRightKVector = CrossProduct3D(footRightIVector, footRightJVector);
	footRightRotationMat = Mat44(footRightIVector, footRightJVector, footRightKVector, Vec3());

	rightHip.Append(upperRightRotationMat);
	rightKnee.Append(kneeRightRotationMat);
	rightFoot.Append(footRightRotationMat);

	rightHip.Append(m_skeleton.m_bones[60].m_bindPose.GetOrthonormalInverse());
	rightKnee.Append(m_skeleton.m_bones[61].m_bindPose.GetOrthonormalInverse());
	rightFoot.Append(m_skeleton.m_bones[62].m_bindPose.GetOrthonormalInverse());

	m_skeleton.m_bones[60].m_finalSkinningMatrix = rightHip;
	m_skeleton.m_bones[61].m_finalSkinningMatrix = rightKnee;
	m_skeleton.m_bones[62].m_finalSkinningMatrix = rightFoot;

	//m_skeleton.m_bones[59].m_finalSkinningMatrix = aggregateFootToeMat;
	//BoneData& parent = m_skeleton.m_bones[m_skeleton.m_bones[62].m_parentIndex];
	Mat44 footWorld = m_skeleton.m_bones[62].m_finalSkinningMatrix;
	m_skeleton.m_bones[62].m_finalSkinningMatrix = footWorld;

	//Bone 58: Toe Base
	Mat44 toeBaseMat = footWorld;
	toeBaseMat.Append(m_skeleton.m_bones[62].m_bindPose);
	toeBaseMat.Append(Mat44::MakeTranslation3D(m_currentAnimation.GetBoneTransformAtTimeLocal(63, (float)m_animationTimer->GetElapsedTime()).m_position));
	toeBaseMat.Append(m_currentAnimation.GetBoneTransformAtTimeLocal(63, (float)m_animationTimer->GetElapsedTime()).m_quaternionRotation.GetAsMatrix());
	toeBaseMat.Append(m_skeleton.m_bones[63].m_inverseBindPose);
	m_skeleton.m_bones[63].m_finalSkinningMatrix = toeBaseMat;

	// Bone 59: Toe
	Mat44 toeMat = toeBaseMat;
	toeMat.Append(m_skeleton.m_bones[63].m_bindPose);
	toeMat.Append(Mat44::MakeTranslation3D(m_currentAnimation.GetBoneTransformAtTimeLocal(64, (float)m_animationTimer->GetElapsedTime()).m_position));
	toeMat.Append(m_currentAnimation.GetBoneTransformAtTimeLocal(64, (float)m_animationTimer->GetElapsedTime()).m_quaternionRotation.GetAsMatrix());
	toeMat.Append(m_skeleton.m_bones[64].m_inverseBindPose);
	m_skeleton.m_bones[64].m_finalSkinningMatrix = toeMat;
}

void GameModeAnimationViewer::InitializeFootTraceCurves()
{
	m_leftFootHeightTrace.clear();
	m_rightFootHeightTrace.clear();
	m_leftFootMaxHeight = -1;
	m_leftFootMinHeight = 100;
	m_rightFootMaxHeight = -1;
	m_rightFootMinHeight = 100;
	//Initialize Foot Curves
	for (int i = 0; i < (int)m_currentAnimation.m_keyFrames.size(); i++)
	{
		m_leftFootHeightTrace.push_back(m_currentAnimation.m_keyFrames[i].m_worldJointPoses[57].m_position.z);
		m_rightFootHeightTrace.push_back(m_currentAnimation.m_keyFrames[i].m_worldJointPoses[62].m_position.z);

		if (m_leftFootMinHeight > m_leftFootHeightTrace[i])
		{
			m_leftFootMinHeight = m_leftFootHeightTrace[i];
		}
		if (m_leftFootMinHeight > m_leftFootHeightTrace[i])
		{
			m_leftFootMaxHeight = m_leftFootHeightTrace[i];
		}

		if (m_rightFootMinHeight > m_rightFootHeightTrace[i])
		{
			m_rightFootMinHeight = m_rightFootHeightTrace[i];
		}
		if (m_rightFootMaxHeight < m_rightFootHeightTrace[i])
		{
			m_rightFootMaxHeight = m_rightFootHeightTrace[i];
		}
	}
}

float GameModeAnimationViewer::GetLeftFootHeightAtTime(float time)
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

float GameModeAnimationViewer::GetRightFootHeightAtTime(float time)
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

void GameModeAnimationViewer::Render() const
{
	RenderGame();
}

void GameModeAnimationViewer::RenderGame() const
{
	g_theRenderer->ClearScreen(Rgba8(0, 0, 0, 255));

	Vec3 iBasis;
	Vec3 jBasis;
	Vec3 kBasis;
	m_player->m_orientationDegrees.GetAsVectors_IFwd_JLeft_KUp(iBasis, jBasis, kBasis);
	Mat44 basisTranslation = Mat44();
	basisTranslation.SetTranslation3D(m_player->m_position + (iBasis * 0.2f));
	DebugAddBasis(basisTranslation, 0, .0075f, .0005f, 1.f, 1.f);

	g_theRenderer->BeginCamera(*m_player->m_playerCamera);

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
		m_skeleton.DebugRenderSkeleton(false);
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
		m_leftLegChain.DebugAddChain();
		m_rightLegChain.DebugAddChain();
	}

	DebugRenderWorld(*m_player->m_playerCamera);


	//Draw Screen ---------------------------------------------------------------------------------------------------------------------------------------------
	g_theRenderer->BeginCamera(m_screenCamera);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->BindTexture(&g_testFont->GetTexture());
	g_theRenderer->DrawVertexArray(m_verts);
	g_theRenderer->EndCamera(m_screenCamera);
}

void GameModeAnimationViewer::Shutdown()
{
	ShutdownGame();
}

void GameModeAnimationViewer::ShutdownGame()
{
	m_worldVerts.clear();
	m_worldVertTBNs.clear();
	m_worldVertsIndexes.clear();
	m_verts.clear();
	delete m_lightBuffer;
	delete m_perFrameBuffer;
	delete m_fbxContext;
	delete m_player;
}

void GameModeAnimationViewer::LoadAllFbx()
{
	m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/SimpleBox/Box.fbx");
	//m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/Capoeira.fbx");
	m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/HipHop.fbx");
	m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/Walking.fbx");
	m_fbxContext->LoadOrGetFbxSceneData("Data/FBXs/SillyDancing.fbx");
}

std::string GameModeAnimationViewer::GetShaderDebugModeDescription()
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

std::string GameModeAnimationViewer::GetRenderDebugModeDescription()
{
	switch (m_debugRenderMode)
	{
	case 1:  return "Render Debug Mode: Wire Frame";
	case 2:  return "Render Debug Mode: No Mesh";
	default: return "Render Debug Mode: None";
	}
}
