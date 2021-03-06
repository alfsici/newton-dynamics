/* Copyright (c) <2003-2016> <Julio Jerez, Newton Game Dynamics>
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
*
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
*
* 3. This notice may not be removed or altered from any source distribution.
*/

#include "dgNewtonCpuStdafx.h"
#include "dgNewtonCpu.h"


// This is an example of an exported function.
dgWorldPlugin* GetPlugin(dgMemoryAllocator* const allocator)
{
	static dgNewtonCpu module(allocator);
	return &module;
}


dgNewtonCpu::dgNewtonCpu(dgMemoryAllocator* const allocator)
	:dgWorldPlugin(allocator)
	,m_avxBody(allocator)
	,m_bodyArray(NULL)
	,m_jointArray(NULL)
	,m_cluster(NULL)
	,m_timestep(dgFloat32 (0.0f))
{
}

dgNewtonCpu::~dgNewtonCpu()
{
}

const char* dgNewtonCpu::GetId() const
{
#ifdef _DEBUG
	return "newtonCpu_d";
#else
	return "newtonCpu";
#endif
}

void dgNewtonCpu::CalculateJointForces(const dgBodyCluster& cluster, dgBodyInfo* const bodyArray, dgJointInfo* const jointArray, dgFloat32 timestep)
{
	m_timestep = timestep;
	m_cluster= &cluster;

	m_jointArray = jointArray;

/*
	syncData.m_bodyLocks = dgAlloca(dgInt32, bodyIndex);
	memset(syncData.m_bodyLocks, 0, bodyIndex * sizeof(dgInt32));

	dgJacobian* const internalForces = &world->m_solverMemory.m_internalForcesBuffer[0];
	internalForces[0].m_linear = dgVector::m_zero;
	internalForces[0].m_angular = dgVector::m_zero;

	const dgInt32 rkSubSteps = 4;
	syncData.m_timestep = timestep;
	syncData.m_invTimestep = (timestep > dgFloat32(0.0f)) ? dgFloat32(1.0f) / timestep : dgFloat32(0.0f);
	syncData.m_invStepRK = (dgFloat32(1.0f) / dgFloat32(rkSubSteps));
	syncData.m_timestepRK = syncData.m_timestep * syncData.m_invStepRK;
	syncData.m_invTimestepRK = syncData.m_invTimestep * dgFloat32(rkSubSteps);
	syncData.m_rkSubSteps = rkSubSteps;
	syncData.m_passes = world->m_solverMode;
	syncData.m_passes = 16;

	syncData.m_bodyCount = bodyIndex;
	syncData.m_jointCount = jointsCount;
	syncData.m_bodyArray = bodyArray;
	syncData.m_jointsArray = jointArray;
	syncData.m_atomicIndex = 0;

	syncData.m_clusterCount = clustersCount;
	syncData.m_clusterArray = clusterArray;

	syncData.m_cluster = &cluster;
	syncData.m_weight = dgAlloca(dgFloat32, cluster.m_bodyCount * 2);
*/

	m_avxBody.Reserve(cluster.m_bodyCount);
	const dgInt32 bodyCount = m_avxBody.m_count * 8;
	m_bodyArray = dgAlloca (dgBodyInfo, bodyCount);
	memcpy (m_bodyArray, bodyArray, cluster.m_bodyCount * sizeof (dgBodyInfo));
	for (dgInt32 i = cluster.m_bodyCount; i < bodyCount; i ++) {
		m_bodyArray[i] = m_bodyArray[0];
	}
	
	float* const weights = &m_avxBody.m_weight.m_val[0].m_f[0];
	memset (weights, 0, bodyCount * sizeof (float));
	for (dgInt32 i = 0; i < cluster.m_jointCount; i++) {
		dgJointInfo* const jointInfo = &m_jointArray[i];
		const dgInt32 m0 = jointInfo->m_m0;
		const dgInt32 m1 = jointInfo->m_m1;
		weights[m0] += dgFloat32(1.0f);
		weights[m1] += dgFloat32(1.0f);
	}

	float* const invWeights = &m_avxBody.m_invWeigh.m_val[0].m_f[0];
	for (dgInt32 i = 0; i < bodyCount; i++) {
		const dgFloat32 w = weights[i] ? weights[i] : 1.0f;
		weights[i] = w;
		invWeights[i] = 1.0f / w;
	}

	InityBodyArray();

}


void dgNewtonCpu::InityBodyArray()
{
//	dgParallelSolverSyncData* const syncData = (dgParallelSolverSyncData*)context;
//	dgWorld* const world = (dgWorld*)worldContext;
//	dgInt32* const atomicIndex = &syncData->m_atomicIndex;
//	dgBodyInfo* const bodyArray = syncData->m_bodyArray;
//	dgJacobian* const internalForces = &world->m_solverMemory.m_internalForcesBuffer[0];

	const dgInt32 bodyCount = m_avxBody.m_count;
	for (dgInt32 i = 0; i < bodyCount; i ++) {
		dgDynamicBody* body[8];
		for (dgInt32 j = 0; j < 8; j++) {
			body[j] = (dgDynamicBody*)m_bodyArray[i * 8 + j].m_body;
		}

		m_avxBody.GetMatrix(i, body);
		m_avxBody.GetVeloc(i, body);
		m_avxBody.GetOmega(i, body);
		m_avxBody.GetDampingCoef(i, body, m_timestep);
	}

	dgAvxFloat timestep (m_timestep);
	dgAvxBody& avxBody = m_avxBody;
	for (dgInt32 i = 0; i < bodyCount; i ++) {
//		dgDynamicBody* const body = (dgDynamicBody*)m_bodyArray[i].m_body;
//		if (!body->m_equilibrium) {
//			body->AddDampingAcceleration(m_timestep);
//			body->CalcInvInertiaMatrix();
//		}
		avxBody.AddDampingAcceleration(i, timestep);

/*
		// re use these variables for temp storage 
		body->m_accel = body->m_veloc;
		body->m_alpha = body->m_omega;
		internalForces[i].m_linear = dgVector::m_zero;
		internalForces[i].m_angular = dgVector::m_zero;
*/
	}

}
