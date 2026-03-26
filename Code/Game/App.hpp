#pragma once
#include "Engine/Core/EventSystem.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"

class Game;
class Clock;
enum GameMode;

class App
{
public:
	App();
	~App();
	void Startup();
	void Shutdown();
	void RunFrame();

	bool IsQuitting() const { return m_isQuitting;  }
	void HandleDeveloperInput();
	void StartGameFromCurrentMode();
	void QuitToAttract();
	bool HandleQuitRequested();

	static bool Event_Quit(EventArgs& args);

	GameMode m_currentGameMode;
	bool	m_drawDebug = false;
	bool	m_isPaused = false;
	bool	m_isSlowMo = false;
	bool	m_isQuitting = false;

private:
	Game* CreateNewGameFromMode(GameMode mode);
	void BeginFrame();
	void Update();
	void Render() const;
	void EndFrame();

	float	m_timeSinceLastFrameStart = 0;
	float	m_deltaSeconds = 0;
	Game*	m_Game;
};