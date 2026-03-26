#pragma once
#include "Game/Game.hpp"

class GameModeAttract : public Game
{
public:
	GameModeAttract();
	~GameModeAttract();

	void Startup();

	void Update();
	void HandleInputGame();

	void Render() const;

	void Shutdown();

	std::vector<Vertex_PCU>		m_verts;				//Screen verts

private:
	float m_ringSize = 50.f;
	float m_elapsedTime = 0.f;

	int	  m_frameCounter = 0;
	int	  m_framesbetweenFPSUpdate = 100;
	float m_fps = 0.f;

	int	  m_debugRenderMode = 0;
	int	  m_debugShaderMode = 0;
};