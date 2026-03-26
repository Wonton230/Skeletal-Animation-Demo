#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Input/XboxController.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "App.hpp"
#include "Game.hpp"
#include "Game/GameModeAttract.hpp"
#include "Game/GameModeAnimationViewer.hpp"
#include "Game/GameModeIK.hpp"
#include "Game/GameModeThirdPerson.hpp"

App*					g_theApp		 = nullptr;
Renderer*				g_theRenderer	 = nullptr;
RandomNumberGenerator*	g_rng			 = nullptr;
extern InputSystem*		g_theInputSystem;
AudioSystem*			g_theAudioSystem = nullptr;
Window*					g_theWindow		 = nullptr;
BitmapFont*				g_testFont = nullptr;
extern NamedStrings*	g_gameConfigBlackboard;
extern DevConsole*		g_theDevConsole;
extern Clock*			g_theSystemClock;

bool EventSystemConsoleOpenerLog(EventArgs& args)
{
	UNUSED(args);
	g_theDevConsole->AddText(DevConsole::DEVC_INFO_MINOR, "Event called: Console Open");
	return true;
}

App::App()
{
	m_Game = nullptr;
}

App::~App()
{
}

void App::Startup()
{
	InputConfig inputConfig;
	g_theInputSystem = new InputSystem(inputConfig);

	WindowConfig windowConfig;
	windowConfig.m_inputSystem = g_theInputSystem;
	windowConfig.m_aspectRatio = 2.f;
	windowConfig.m_windowTitle = "FBX Loader";
	g_theWindow = new Window(windowConfig);
	g_theWindow->Startup();

	RenderConfig renderConfig;
	renderConfig.m_window = g_theWindow;
	g_theRenderer = new Renderer(renderConfig);
	g_theRenderer->Startup();

	DevConsoleConfig consoleConfig;
	consoleConfig.m_renderer = g_theRenderer;
	consoleConfig.m_camera = nullptr;
	g_testFont = g_theRenderer->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont.png");
	consoleConfig.font = g_testFont;
	g_theDevConsole = new DevConsole(consoleConfig);

	//Input starts up after devconsole to block inputs 
	g_theDevConsole->Startup();
	g_theInputSystem->Startup();

	g_theEventSystem->SubscribeEventCallbackFunction("quit", App::Event_Quit);
	g_theDevConsole->AddText(g_theDevConsole->DEVC_INFO_MAJOR, "Controls:\n~ - Open/Close Dev Console\nW - Forward\nA - Left\nS - Backward\nD - Right\nE - Up\nQ - Down\nShift - Move Faster\nMouse - Look Around\nESC - Quit\nI - Cycle Shader Mode Back\nO - Cycle Shader Mode Forward\n[ - Cycle Render Mode Back\n] - Cycle Render Mode Forward\nL - Toggle skinned bind pose\nP - Pause animation\nArrow Keys - Move IK target along XY plane\nZ/X - Move IK target up and down\nF6/F7 - Cycle Game Modes (Animation viewer and IK demo)");

	AudioConfig audioConfig;
	g_theAudioSystem = new AudioSystem(audioConfig);
	g_theAudioSystem->Startup();

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_theRenderer;
	DebugRenderSystemStartup(debugRenderConfig);

	g_rng = new RandomNumberGenerator();

	m_Game = CreateNewGameFromMode(GameMode::ATTRACT);
	m_Game->Startup();
}

void App::Shutdown()
{
	m_isQuitting = true;

	m_Game->Shutdown();
	m_Game = nullptr;

	g_theAudioSystem->Shutdown();
	delete g_theAudioSystem;
	g_theAudioSystem = nullptr;

	g_theEventSystem->Shutdown();
	delete g_theEventSystem;
	g_theEventSystem = nullptr;

	g_theDevConsole->Shutdown();
	delete g_theDevConsole;
	g_theDevConsole = nullptr;

	g_theRenderer->Shutdown();
	delete g_theRenderer;
	g_theRenderer = nullptr;

	g_theWindow->Shutdown();
	delete g_theWindow;
	g_theWindow = nullptr;

	g_theInputSystem->Shutdown();
	delete g_theInputSystem;
	g_theInputSystem = nullptr;
}

void App::RunFrame()
{
	BeginFrame();
	Update();
	Render();
	HandleDeveloperInput();
	EndFrame();
}

Game* App::CreateNewGameFromMode(GameMode mode)
{
	Game* temp;
	switch (mode)
	{
	case GameMode::ATTRACT:
		temp = new GameModeAttract();
		return temp;

	case GameMode::ANIMATION:
		temp = new GameModeAnimationViewer();
		return temp;

	case GameMode::FABRIK:
		temp = new GameModeIK();
		return temp;

	case GameMode::PLAYGROUND:
		temp = new GameModeThirdPerson();
		return temp;

	default:
		return nullptr;
	}
}

void App::BeginFrame()
{
	//calls begin frame on all engine systems
	g_theInputSystem->BeginFrame();
	g_theAudioSystem->BeginFrame();
	g_theWindow->BeginFrame();
	g_theRenderer->BeginFrame();
	g_theDevConsole->BeginFrame();
	g_theEventSystem->BeginFrame();

	DebugRenderBeginFrame();
	g_theSystemClock->TickSystemClock();
}

void App::Update()
{	
	m_Game->Update();
}

void App::Render() const
{
	m_Game->Render();
	g_theDevConsole->Render(AABB2(Vec2(0.0f,0.0f),Vec2(SCREEN_SIZE_X,SCREEN_SIZE_Y)), g_theRenderer);

	DebugRenderScreen(m_Game->m_screenCamera);
}

void App::EndFrame()
{
	g_theInputSystem->EndFrame();
	g_theAudioSystem->EndFrame();
	g_theWindow->EndFrame();
	g_theRenderer->EndFrame();
	g_theDevConsole->EndFrame();
	g_theEventSystem->EndFrame();

	DebugRenderEndFrame();
}

void App::HandleDeveloperInput()
{
	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_TILDE))
	{
		g_theDevConsole->ToggleMode(DevConsoleMode::FULL);
	}

	if (g_theDevConsole->IsOpen() || m_currentGameMode == GameMode::ATTRACT || !g_theWindow->IsActiveWindow())
	{
		g_theInputSystem->SetCursorMode(CursorMode::POINTER);
	}
	else
	{
		g_theInputSystem->SetCursorMode(CursorMode::FPS);
	}

	if (g_theDevConsole->IsOpen())
	{
		g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	}
	else
	{
		g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	}

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_ESC))
	{
		if (m_currentGameMode == GameMode::ATTRACT)
		{
			FireEvent("quit");
		}
		else
		{
			QuitToAttract();
		}
	}

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_F6) && m_currentGameMode != ATTRACT) //F6
	{
		if (m_currentGameMode == ANIMATION)
		{
			m_Game->Shutdown();
			delete m_Game;
			m_currentGameMode = (GameMode)((int)(GameMode::COUNT)-1);
			m_Game = CreateNewGameFromMode(m_currentGameMode);
			m_Game->Startup();
		}
		else
		{
			m_Game->Shutdown();
			delete m_Game;
			m_currentGameMode = (GameMode)((int)(m_currentGameMode)-1);
			m_Game = CreateNewGameFromMode(m_currentGameMode);
			m_Game->Startup();
		}
	}

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_F7) && m_currentGameMode != ATTRACT) //F7
	{
		if (m_currentGameMode == (GameMode)((int)(GameMode::COUNT)-1))
		{
			m_Game->Shutdown();
			delete m_Game;
			m_currentGameMode = ANIMATION;
			m_Game = CreateNewGameFromMode(m_currentGameMode);
			m_Game->Startup();
		}
		else
		{
			m_Game->Shutdown();
			delete m_Game;
			m_currentGameMode = (GameMode)((int)(m_currentGameMode)+1);
			m_Game = CreateNewGameFromMode(m_currentGameMode);
			m_Game->Startup();
		}
	}
}

void App::StartGameFromCurrentMode()
{
	m_Game->Shutdown();
	delete m_Game;
	m_Game = CreateNewGameFromMode(m_currentGameMode);
	m_Game->Startup();
}

void App::QuitToAttract()
{
	m_Game->Shutdown();
	delete m_Game;
	m_currentGameMode = ATTRACT;
	m_Game = CreateNewGameFromMode(m_currentGameMode);
	m_Game->Startup();
}

bool App::HandleQuitRequested()
{
	m_isQuitting = true;
	return m_isQuitting;
}

bool App::Event_Quit(EventArgs& args)
{
	UNUSED(args);
	g_theApp->HandleQuitRequested();
	return true;
}