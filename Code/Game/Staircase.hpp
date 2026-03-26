#pragma once
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/OBB3.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Math/RaycastUtils.hpp"
#include "Engine/Math/VertexUtils.hpp"
#include <vector>

struct Staircase
{
public:
	std::vector<OBB3>	m_stepOBBs;
	OBB3				m_physicsRampOBB;
	int					m_numSteps;

	Staircase();
	Staircase(int numSteps, Vec3 const& start, Vec3 const& end, float width, float height);
	~Staircase();

	RaycastResult3D RaycastVsVisualStairs(Vec3 const& start, Vec3 const& normalDirection, float maxDistance);
	RaycastResult3D RaycastVsRamp(Vec3 const& start, Vec3 const& normalDirection, float maxDistance);
	void			AddVertsToVector(std::vector<Vertex_PCU>& verts, Rgba8 const& color = Rgba8::WHITE);
};