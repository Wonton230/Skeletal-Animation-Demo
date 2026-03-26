#pragma once
#include "GameCommon.hpp"

// Forward declarations
class Game;
class Camera;
struct EulerAngles;
struct Vec3;
struct Mat44;

class Player
{
public:
	// Constructor/Destructor
	Player(Game* owner, Vec3 const& startPos);
	~Player();

	// Methods
	void Update();
	void Render() const;
	Mat44 GetModelToWorldTransform() const;

	// Member variables
	Game*		 m_game = nullptr;
	Vec3         m_position = Vec3(0.f, 0.f, 0.f);
	Vec3         m_velocity = Vec3(0.f, 0.f, 0.f);
	EulerAngles  m_orientationDegrees = EulerAngles(0.f, 0.f, 0.f);
	EulerAngles  m_angularVelocity = EulerAngles(0.f, 0.f, 0.f);
	Rgba8        m_color = Rgba8::WHITE;

	float        m_speed = 2.f;
	float        m_lookSpeed = 0.125f;
	Camera*		 m_playerCamera = nullptr;
};
