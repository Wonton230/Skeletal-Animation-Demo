#include "Game.hpp"
#include "App.hpp"
#include "GameCommon.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Clock.hpp"

Game::Game()
{
	m_gameClock = new Clock(*g_theSystemClock);
}