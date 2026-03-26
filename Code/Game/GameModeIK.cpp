#include "GameModeIK.hpp"

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
#include "Game/Player.hpp"
#include "Game/App.hpp"

extern App* g_theApp;
extern Renderer* g_theRenderer;
extern RandomNumberGenerator* g_rng;
extern InputSystem* g_theInputSystem;
extern AudioSystem* g_theAudioSystem;
extern Clock* g_theSystemClock;
extern BitmapFont* g_testFont;
extern Window* g_theWindow;
extern DevConsole* g_theDevConsole;

GameModeIK::GameModeIK()
{
	m_screenCamera = Camera();
	m_screenCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));

	m_IKDemoChain.start = m_IKrootPos;
	m_IKDemoChain.end = m_IKTarget;
	m_IKDemoChain.lenStartMid = m_IKLen0;
	m_IKDemoChain.lenMidEnd = m_IKLen1;
}

GameModeIK::~GameModeIK()
{
	delete m_gameClock;
}

void GameModeIK::Startup()
{
	m_player = new Player(this, Vec3(-1.f, 0.f, 2.f));
	m_player->m_playerCamera = new Camera();
	m_player->m_playerCamera->SetOrthographicView(Vec2(WORLD_MINS_X, WORLD_MINS_Y), Vec2(WORLD_MAXS_X, WORLD_MAXS_Y));
	m_player->m_playerCamera->SetPerspectiveView(g_theWindow->GetConfig().m_aspectRatio, 50.f, .1f, 100.f);
	m_player->m_playerCamera->SetCameraToRenderTransform(Mat44(Vec3(0.f, 0.f, 1.f), Vec3(-1.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f), Vec3(0.f, 0.f, 0.f)));

	int gridSize = 50;
	float halfWidth = .01f;
	float startingY = -(float)gridSize * .5f;

	for (int i = 0; i < gridSize; i++)
	{
		if (i == 25)
		{
			AddVertsForAABB3D(m_gridVerts, AABB3(-25.f, (startingY + i - halfWidth * 4), -halfWidth * 4, 25.f, (startingY + i + halfWidth * 4), halfWidth * 4), Rgba8(255, 0, 0, 70));
		}
		else if (i % 5 == 0)
		{
			AddVertsForAABB3D(m_gridVerts, AABB3(-25.f, (startingY + i - halfWidth * 2), -halfWidth * 2, 25.f, (startingY + i + halfWidth * 2), halfWidth * 2), Rgba8(255, 0, 0, 70));
		}
		else
		{
			AddVertsForAABB3D(m_gridVerts, AABB3(-25.f, (startingY + i - halfWidth), -halfWidth, 25.f, (startingY + i + halfWidth), halfWidth), Rgba8(150, 150, 150, 70));
		}
	}

	for (int i = 0; i < gridSize; i++)
	{
		if (i == 25)
		{
			AddVertsForAABB3D(m_gridVerts, AABB3((startingY + i - halfWidth * 4), -25.f, -halfWidth * 4, (startingY + i + halfWidth * 4), 25.f, halfWidth * 4), Rgba8(0, 255, 0, 70));
		}
		else if (i % 5 == 0)
		{
			AddVertsForAABB3D(m_gridVerts, AABB3((startingY + i - halfWidth * 2), -25.f, -halfWidth * 2, (startingY + i + halfWidth * 2), 25.f, halfWidth * 2), Rgba8(0, 255, 0, 70));
		}
		else
		{
			AddVertsForAABB3D(m_gridVerts, AABB3((startingY + i - halfWidth), -25.f, -halfWidth, (startingY + i + halfWidth), 25.f, halfWidth), Rgba8(150, 150, 150, 70));
		}
	}
}

void GameModeIK::Update()
{
	m_screenVerts.clear();

	if (m_frameCounter >= m_framesbetweenFPSUpdate)
	{
		float deltaSeconds = (float)g_theSystemClock->GetDeltaSeconds();
		m_frameCounter = 0;
		m_fps = 1.f / deltaSeconds;
	}
	else
	{
		m_frameCounter++;
	}

	std::string fpsString = Stringf("Current FPS: %.2f", m_fps);
	g_testFont->AddVertsForText2D(m_screenVerts, Vec2(0.f, SCREEN_SIZE_Y - (SCREEN_SIZE_Y * .04f)), SCREEN_SIZE_Y * .02f, fpsString, Rgba8::WHITE, .7f);
	m_player->Update();
	HandleInputGame();

	//IK----------------------------------------------------------------------------------------------------------------
	switch (m_IKMode)
	{
	case FREE:
		m_IKDemoChain = FABRIKSolveChain2(m_IKDemoChain, m_IKTarget);
		break;
	case SWING_CONSTRAINT:
		m_IKDemoChain = FABRIKSolveChain2SwingConstraint(m_IKDemoChain, m_IKTarget, Vec3(1, 0, 0).GetNormalized());
		break;
	case SWING_ANGLE_CONSTRAINT:
		m_IKDemoChain = FABRIKSolveChain2SwingAngleConstraint(m_IKDemoChain, m_IKTarget, Vec3(1, 0, 0).GetNormalized(), FloatRange(80.f, 160.f));
		break;
	case ARM_CONSTRAINT:
		m_IKDemoChain = FABRIKSolveChain2Arm(m_IKDemoChain, m_IKTarget, Vec3(0,-1,0), 60.f, FloatRange(80.f, 160.f));
		break;
	}
	
}

void GameModeIK::HandleInputGame()
{
	float deltaSeconds = (float)g_theSystemClock->GetDeltaSeconds();
	Vec3 forward;
	Vec3 left;
	Vec3 up;
	m_player->m_orientationDegrees.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

	if (g_theInputSystem->IsKeyDown(KEYCODE_UPARROW))
	{
		m_IKTarget = m_IKTarget + (forward.GetFlattenedNormalXY() * m_IKTargetSpeed * deltaSeconds);
	}
	else if (g_theInputSystem->IsKeyDown(KEYCODE_DOWNARROW))
	{
		m_IKTarget = m_IKTarget + (-forward.GetFlattenedNormalXY() * m_IKTargetSpeed * deltaSeconds);
	}
	if (g_theInputSystem->IsKeyDown(KEYCODE_LEFTARROW))
	{
		m_IKTarget = m_IKTarget + (left.GetFlattenedNormalXY() * m_IKTargetSpeed * deltaSeconds);
	}
	else if (g_theInputSystem->IsKeyDown(KEYCODE_RIGHTARROW))
	{
		m_IKTarget = m_IKTarget + (-left.GetFlattenedNormalXY() * m_IKTargetSpeed * deltaSeconds);
	}

	if (g_theInputSystem->IsKeyDown('Z'))
	{
		m_IKTarget = m_IKTarget + (Vec3(0,0,1) * m_IKTargetSpeed * deltaSeconds);
	}
	else if (g_theInputSystem->IsKeyDown('X'))
	{
		m_IKTarget = m_IKTarget + (Vec3(0,0,-1) * m_IKTargetSpeed * deltaSeconds);
	}

	if (g_theInputSystem->WasKeyJustPressed('P'))
	{
		if ((IKMode)(m_IKMode + 1) == NUM_MODES)
		{
			m_IKMode = (IKMode)0;
		}
		else
		{
			m_IKMode = (IKMode)((int)m_IKMode + 1);
		}
	}
}

void GameModeIK::Render() const
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

	//Draw grid ---------------------------------------------------------------------------------------------------------------------------------------------
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(m_gridVerts);

	//Debug Draw IK
	DebugAddWorldSphere(m_IKDemoChain.start, .015f, 0.f, Rgba8::YELLOW);
	DebugAddWorldArrow(m_IKDemoChain.start, m_IKDemoChain.mid, .007f, 0.f, Rgba8::BLUE);
	DebugAddWorldSphere(m_IKDemoChain.mid, .015f, 0.f, Rgba8::ORANGE);
	DebugAddWorldArrow(m_IKDemoChain.mid, m_IKDemoChain.end, .007f, 0.f, Rgba8::BLUE);
	DebugAddWorldSphere(m_IKDemoChain.end, .015f, 0.f, Rgba8::RED);

	DebugAddWorldWireSphere(m_IKTarget, .02f, 0.f, Rgba8::WHITE);

	DebugRenderWorld(*m_player->m_playerCamera);

	//Draw Screen ---------------------------------------------------------------------------------------------------------------------------------------------
	g_theRenderer->BeginCamera(m_screenCamera);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->BindTexture(&g_testFont->GetTexture());
	g_theRenderer->DrawVertexArray(m_screenVerts);
	g_theRenderer->EndCamera(m_screenCamera);
}

void GameModeIK::Shutdown()
{
	m_gridVerts.clear();
	m_screenVerts.clear();
	delete m_player;
}