#include "Player.hpp"
#include "Game/Game.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Renderer/Camera.hpp"

extern Renderer* g_theRenderer;
extern InputSystem* g_theInputSystem;
extern Window* g_theWindow;

Player::Player(Game* owner, Vec3 const& startPos)
{
	m_position = startPos;
	m_game = owner;
}

Player::~Player()
{
	m_game = nullptr;
	delete m_playerCamera;
}

void Player::Update()
{
	float deltaSeconds = (float)g_theSystemClock->GetDeltaSeconds();
	Vec3 forward;
	Vec3 left;
	Vec3 up;
	m_orientationDegrees.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

	//Shift to run
	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_SHIFT))
	{
		m_speed = m_speed * 10.f;
	}
	if (g_theInputSystem->WasKeyJustReleased(KEYCODE_SHIFT))
	{
		m_speed = m_speed * 0.1f;
	}
	//Movement
	if (g_theInputSystem->IsKeyDown('W'))
	{
		m_position = m_position + (forward * m_speed * deltaSeconds);
	}
	if (g_theInputSystem->IsKeyDown('S'))
	{
		m_position = m_position - (forward * m_speed * deltaSeconds);
	}
	if (g_theInputSystem->IsKeyDown('A'))
	{
		m_position = m_position + (left * m_speed * deltaSeconds);
	}
	if (g_theInputSystem->IsKeyDown('D'))
	{
		m_position = m_position - (left * m_speed * deltaSeconds);
	}
	if (g_theInputSystem->IsKeyDown('E'))
	{
		m_position = m_position + (Vec3(0.f, 0.f, 1.f) * m_speed * deltaSeconds);
	}
	if (g_theInputSystem->IsKeyDown('Q'))
	{
		m_position = m_position - (Vec3(0.f, 0.f, 1.f) * m_speed * deltaSeconds);
	}

	//Orientation
	float deltaY = g_theInputSystem->GetCursorClientDelta().y;
	if (deltaY > 0)
	{
		deltaY = 0;
	}
	m_orientationDegrees.m_yawDegrees -= m_lookSpeed * g_theInputSystem->GetCursorClientDelta().x;
	m_orientationDegrees.m_pitchDegrees = GetClamped(m_orientationDegrees.m_pitchDegrees + m_lookSpeed * g_theInputSystem->GetCursorClientDelta().y, -85.f, 85.f);

	if (g_theInputSystem->WasKeyJustPressed('R'))
	{
		m_orientationDegrees = EulerAngles(0.f, 0.f, 0.f);
		m_position = Vec3(0.f, 0.f, 0.f);
	}

	m_playerCamera->SetPosition(m_position);
	m_playerCamera->SetOrientation(m_orientationDegrees);
	m_playerCamera->SetPerspectiveView(g_theWindow->GetConfig().m_aspectRatio, 60.f, .1f, 100.f);
}

void Player::Render() const
{
}

Mat44 Player::GetModelToWorldTransform() const
{
	Mat44 modelToWorld = Mat44::MakeTranslation3D(m_position);
	modelToWorld.Append(m_orientationDegrees.GetAsMatrix_IFwd_JLeft_KUp());
	return modelToWorld;
}
