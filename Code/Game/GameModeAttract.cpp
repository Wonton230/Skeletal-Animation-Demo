#include "GameModeAttract.hpp"
#include "App.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"

extern App* g_theApp;
extern Renderer* g_theRenderer;
extern RandomNumberGenerator* g_rng;
extern InputSystem* g_theInputSystem;
extern AudioSystem* g_theAudioSystem;
extern Clock* g_theSystemClock;
extern BitmapFont* g_testFont;
extern Window* g_theWindow;
extern DevConsole* g_theDevConsole;

GameModeAttract::GameModeAttract()
{
	m_worldCamera = Camera();
	m_worldCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(WORLD_SIZE_X, WORLD_SIZE_Y));

	m_screenCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
	m_gameClock = new Clock(*g_theSystemClock);
}

GameModeAttract::~GameModeAttract()
{
	delete m_gameClock;
}

void GameModeAttract::Startup()
{
}

void GameModeAttract::Update()
{
	m_elapsedTime += static_cast<float>(m_gameClock->GetDeltaSeconds());
	m_ringSize = CosDegrees(m_elapsedTime * 50) * 500;

	HandleInputGame();
}

void GameModeAttract::HandleInputGame()
{
	if (g_theInputSystem->WasKeyJustPressed(' '))
	{
		g_theApp->m_currentGameMode = GameMode::ANIMATION;
		g_theApp->StartGameFromCurrentMode();
	}
}

void GameModeAttract::Render() const
{
	g_theRenderer->BeginCamera(m_screenCamera);

	g_theRenderer->ClearScreen(Rgba8(0, 0, 0, 255));
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetStatesIfChanged();

	g_theRenderer->BindTexture(nullptr);
	DrawDebugRing(Vec2(SCREEN_CENTER_X, SCREEN_CENTER_Y), m_ringSize, 10, Rgba8(0, 200, 150, 255));
	g_theRenderer->BindTexture(nullptr);

	g_theRenderer->EndCamera(m_screenCamera);
}

void GameModeAttract::Shutdown()
{
}
