#include "Staircase.hpp"

Staircase::Staircase()
{
}

Staircase::Staircase(int numSteps, Vec3 const& start, Vec3 const& end, float width, float height)
	: m_numSteps(numSteps)
{
	Vec3 totalDisplacement = end - start;
	float stepDepth = totalDisplacement.GetLength() / static_cast<float>(numSteps);
	Vec3 forwardDir = totalDisplacement.GetNormalized();
	Vec3 upDir = Vec3(0.f, 0.f, 1.f);
	Vec3 rightDir = CrossProduct3D(upDir, forwardDir).GetNormalized();

	// Create individual step OBBs
	for (int i = 0; i < numSteps; ++i)
	{
		// Each step extends from its base position
		Vec3 stepBasePos = start + forwardDir * ((stepDepth * static_cast<float>(i)) - stepDepth * .5f);
		stepBasePos.z += height * static_cast<float>(i);

		// Center is half a step forward and half height up from base
		Vec3 stepCenter = stepBasePos + forwardDir * (stepDepth * 0.5f);
		stepCenter.z += height * 0.5f;

		Vec3 halfDimensions(stepDepth * 0.5f, width * 0.5f, height * 0.5f);
		OBB3 stepOBB(stepCenter, forwardDir, rightDir, upDir, halfDimensions);
		m_stepOBBs.push_back(stepOBB);
	}

	// Create physics ramp OBB (encapsulates entire staircase)
	Vec3 rampCenter = (start + end) * 0.5f;
	rampCenter.z += (height * static_cast<float>(numSteps)) * 0.5f +(height * .5f);

	float rampLength = totalDisplacement.GetLength();
	float rampHeight = height * static_cast<float>(numSteps);

	forwardDir = Vec3(end.x, end.y, height * numSteps) - Vec3(start.x, start.y, 0.f);
	forwardDir = forwardDir.GetNormalized();
	Vec3 rampHalfDimensions(rampLength * 0.5f, width * 0.5f, .001f);
	Vec3 rampNormal = CrossProduct3D(forwardDir, rightDir);
	m_physicsRampOBB = OBB3(rampCenter, forwardDir, rightDir, rampNormal, rampHalfDimensions);
}

Staircase::~Staircase()
{
}

RaycastResult3D Staircase::RaycastVsVisualStairs(Vec3 const& start, Vec3 const& normalDirection, float maxDistance)
{
	RaycastResult3D closestHit;
	closestHit.m_didImpact = false;
	closestHit.m_impactDist = maxDistance;

	for (int i = 0; i < m_numSteps; ++i)
	{
		RaycastResult3D hit = RaycastVsOBB3D(start, normalDirection, maxDistance, m_stepOBBs[i]);

		if (hit.m_didImpact && hit.m_impactDist < closestHit.m_impactDist)
		{
			closestHit = hit;
		}
	}

	return closestHit;
}

RaycastResult3D Staircase::RaycastVsRamp(Vec3 const& start, Vec3 const& normalDirection, float maxDistance)
{
	return RaycastVsOBB3D(start, normalDirection, maxDistance, m_physicsRampOBB);
}

void Staircase::AddVertsToVector(std::vector<Vertex_PCU>& verts, Rgba8 const& color)
{
	for (int i = 0; i < m_numSteps; ++i)
	{
		AddVertsForOBB3D(verts, m_stepOBBs[i], color);
	}
}