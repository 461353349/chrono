/*
 * SphInterface.cpp
 *
 *  Created on: Mar 2, 2015
 *      Author: Arman Pazouki
 */

#include "chrono_fsi/SphInterface.h"
#include "chrono_fsi/UtilsDeviceOperations.cuh"
#include "chrono/core/ChTransform.h"

// Chrono Vehicle Include

chrono::ChVector<> ConvertRealToChVector(Real3 p3) {
	return chrono::ChVector<>(p3.x, p3.y, p3.z);
}
chrono::ChVector<> ConvertRealToChVector(Real4 p4) {
	return ConvertRealToChVector(mR3(p4));
}

chrono::ChQuaternion<> ConvertToChQuaternion(Real4 q4) {
	return chrono::ChQuaternion<>(q4.x, q4.y, q4.z, q4.w);
}

Real3 ConvertChVectorToR3(chrono::ChVector<> v3) {
	return mR3(v3.x, v3.y, v3.z);
}

Real4 ConvertChVectorToR4(chrono::ChVector<> v3, Real m) {
	return mR4(v3.x, v3.y, v3.z, m);
}

Real4 ConvertChQuaternionToR4(chrono::ChQuaternion<> q4) {
	return mR4(q4.e0, q4.e1, q4.e2, q4.e3);
}

Real3 Rotate_By_Quaternion(Real4 q4, Real3 BCE_Pos_local) {
	chrono::ChQuaternion<> chQ = ConvertToChQuaternion(q4);
	chrono::ChVector<> dumPos = chQ.Rotate(
			ConvertRealToChVector(BCE_Pos_local));
	return ConvertChVectorToR3(dumPos);
}

Real3 R3_LocalToGlobal(Real3 p3LF, chrono::ChVector<> pos,
		chrono::ChQuaternion<> rot) {
	chrono::ChVector<> p3GF = chrono::ChTransform<>::TransformLocalToParent(
			ConvertRealToChVector(p3LF), pos, rot);
	return ConvertChVectorToR3(p3GF);
}
//------------------------------------------------------------------------------------
// version 1.0 SPH-FSI. You may delete it
void AddSphDataToChSystem(chrono::ChSystemParallelDVI& mphysicalSystem,
		int& startIndexSph, const thrust::host_vector<Real3>& posRadH,
		const thrust::host_vector<Real3>& velMasH, const SimParams& paramsH,
		const NumberOfObjects& numObjects, int collisionFamilly) {
	Real rad = 0.5 * paramsH.MULT_INITSPACE * paramsH.HSML;
	// NOTE: mass properties and shapes are all for sphere
	double volume = chrono::utils::CalcSphereVolume(rad);
	chrono::ChVector<> gyration =
			chrono::utils::CalcSphereGyration(rad).Get_Diag();
	double density = paramsH.rho0;
	double mass = paramsH.markerMass;  // density * volume;
	double muFriction = 0;

	// int fId = 0; //fluid id

	// Create a common material
	chrono::ChSharedPtr<chrono::ChMaterialSurface> mat_g(
			new chrono::ChMaterialSurface);
	mat_g->SetFriction(muFriction);
	mat_g->SetCohesion(0);
	mat_g->SetCompliance(0.0);
	mat_g->SetComplianceT(0.0);
	mat_g->SetDampingF(0.2);

	const chrono::ChQuaternion<> rot = chrono::ChQuaternion<>(1, 0, 0, 0);

	startIndexSph = mphysicalSystem.Get_bodylist()->size();
	// openmp does not work here
	for (int i = 0; i < numObjects.numFluidMarkers; i++) {
		Real3 p3 = posRadH[i];
		Real3 vM3 = velMasH[i];
		chrono::ChVector<> pos = ConvertRealToChVector(p3);
		chrono::ChVector<> vel = ConvertRealToChVector(vM3);
		chrono::ChSharedBodyPtr body;
		body = chrono::ChSharedBodyPtr(
				new chrono::ChBody(
						new chrono::collision::ChCollisionModelParallel));
		body->SetMaterialSurface(mat_g);
		// body->SetIdentifier(fId);
		body->SetPos(pos);
		body->SetRot(rot);
		body->SetCollide(true);
		body->SetBodyFixed(false);
		body->SetMass(mass);
		body->SetInertiaXX(mass * gyration);

		body->GetCollisionModel()->ClearModel();

		// add collision geometry
		//	body->GetCollisionModel()->AddEllipsoid(size.x, size.y, size.z, pos, rot);
		//
		//	// add asset (for visualization)
		//	ChSharedPtr<ChEllipsoidShape> ellipsoid(new ChEllipsoidShape);
		//	ellipsoid->GetEllipsoidGeometry().rad = size;
		//	ellipsoid->Pos = pos;
		//	ellipsoid->Rot = rot;
		//
		//	body->GetAssets().push_back(ellipsoid);

		//	chrono::utils::AddCapsuleGeometry(body.get_ptr(), size.x, size.y);		// X
		//	chrono::utils::AddCylinderGeometry(body.get_ptr(), size.x, size.y);		// O
		//	chrono::utils::AddConeGeometry(body.get_ptr(), size.x, size.y); 		// X
		//	chrono::utils::AddBoxGeometry(body.get_ptr(), size);					// O
		chrono::utils::AddSphereGeometry(body.get_ptr(), rad);  // O
		//	chrono::utils::AddEllipsoidGeometry(body.get_ptr(), size);					// X

		body->GetCollisionModel()->SetFamily(collisionFamilly);
		body->GetCollisionModel()->SetFamilyMaskNoCollisionWithFamily(
				collisionFamilly);

		body->GetCollisionModel()->BuildModel();
		mphysicalSystem.AddBody(body);
	}
}
//------------------------------------------------------------------------------------
// Arman : Delete later
void AddHydroForce(chrono::ChSystemParallelDVI& mphysicalSystem,
		int& startIndexSph, const NumberOfObjects& numObjects) {
// openmp does not work here
#pragma omp parallel for
	for (int i = 0; i < numObjects.numFluidMarkers; i++) {
		char forceTag[] = "hydrodynamics_force";
		auto mBody = mphysicalSystem.Get_bodylist()->at(i + startIndexSph);
		chrono::ChSharedPtr<chrono::ChForce> hydroForce = mBody->SearchForce(
				forceTag);
		if (hydroForce.IsNull()) {
			hydroForce = chrono::ChSharedPtr<chrono::ChForce>(
					new chrono::ChForce);
			hydroForce->SetMode(FTYPE_FORCE); // no need for this. It is the default option.
			mBody->AddForce(hydroForce);
			// ** or: hydroForce = ChSharedPtr<ChForce>(new ChForce());
			hydroForce->SetName(forceTag);
		}
	}
}
//------------------------------------------------------------------------------------
// Arman : Delete later
void UpdateSphDataInChSystem(chrono::ChSystemParallelDVI& mphysicalSystem,
		const thrust::host_vector<Real3>& posRadH,
		const thrust::host_vector<Real3>& velMasH,
		const NumberOfObjects& numObjects, int startIndexSph) {
#pragma omp parallel for
	for (int i = 0; i < numObjects.numFluidMarkers; i++) {
		Real3 p3 = posRadH[i];
		Real3 vM3 = velMasH[i];
		chrono::ChVector<> pos = ConvertRealToChVector(p3);
		chrono::ChVector<> vel = ConvertRealToChVector(vM3);

		auto mBody = mphysicalSystem.Get_bodylist()->at(i + startIndexSph);
		mBody->SetPos(pos);
		mBody->SetPos_dt(vel);
	}
}
//------------------------------------------------------------------------------------
// Arman : Delete later
void AddChSystemForcesToSphForces(
		thrust::host_vector<Real4>& derivVelRhoChronoH,
		const thrust::host_vector<Real3>& velMasH2,
		chrono::ChSystemParallelDVI& mphysicalSystem,
		const NumberOfObjects& numObjects, int startIndexSph, Real dT) {
#pragma omp parallel for
	for (int i = 0; i < numObjects.numFluidMarkers; i++) {
		auto mBody = mphysicalSystem.Get_bodylist()->at(i + startIndexSph);
		chrono::ChVector<> v = mBody->GetPos_dt();
		Real3 a3 = (mR3(v.x, v.y, v.z) - velMasH2[i]) / dT;  // f = m * a
		derivVelRhoChronoH[i] += mR4(a3, 0); // note, gravity force is also coming from rigid system
	}
}
//------------------------------------------------------------------------------------

void ClearArraysH(
		thrust::host_vector<Real3>& posRadH, // do not set the size here since you are using push back later
		thrust::host_vector<Real3>& velMasH,
		thrust::host_vector<Real4>& rhoPresMuH) {
	posRadH.clear();
	velMasH.clear();
	rhoPresMuH.clear();
}
//------------------------------------------------------------------------------------

void ClearArraysH(
		thrust::host_vector<Real3>& posRadH, // do not set the size here since you are using push back later
		thrust::host_vector<Real3>& velMasH,
		thrust::host_vector<Real4>& rhoPresMuH,
		thrust::host_vector<uint>& bodyIndex,
		thrust::host_vector<::int4>& referenceArray) {
	ClearArraysH(posRadH, velMasH, rhoPresMuH);
	bodyIndex.clear();
	referenceArray.clear();
}
//------------------------------------------------------------------------------------

void CopyD2H(thrust::host_vector<Real4>& derivVelRhoChronoH,
		const thrust::device_vector<Real4>& derivVelRhoD) {
	//	  assert(derivVelRhoChronoH.size() == derivVelRhoD.size() && "Error! size mismatch host and device");
	if (derivVelRhoChronoH.size() != derivVelRhoD.size()) {
		printf("\n\n\n\n Error! size mismatch host and device \n\n\n\n");
	}
	thrust::copy(derivVelRhoD.begin(), derivVelRhoD.end(),
			derivVelRhoChronoH.begin());
}

//------------------------------------------------------------------------------------
void CountNumContactsPerSph(thrust::host_vector<short int>& numContactsOnAllSph,
		const chrono::ChSystemParallelDVI& mphysicalSystem,
		const NumberOfObjects& numObjects, int startIndexSph) {
	int numContacts =
			mphysicalSystem.data_manager->host_data.bids_rigid_rigid.size();
	//#pragma omp parallel for // it is very wrong to do it in parallel. race condition will occur
	for (int i = 0; i < numContacts; i++) {
		chrono::int2 ids =
				mphysicalSystem.data_manager->host_data.bids_rigid_rigid[i];
		if (ids.x > startIndexSph)
			numContactsOnAllSph[ids.x - startIndexSph] += 1;
		if (ids.y > startIndexSph)
			numContactsOnAllSph[ids.y - startIndexSph] += 1;
	}
}

//------------------------------------------------------------------------------------
void CopyForceSphToChSystem(chrono::ChSystemParallelDVI& mphysicalSystem,
		const NumberOfObjects& numObjects, int startIndexSph,
		const thrust::device_vector<Real4>& derivVelRhoD,
		const thrust::host_vector<short int>& numContactsOnAllSph,
		Real sphMass) {
#pragma omp parallel for
	for (int i = 0; i < numObjects.numFluidMarkers; i++) {
		char forceTag[] = "hydrodynamics_force";
		auto mBody = mphysicalSystem.Get_bodylist()->at(i + startIndexSph);
		chrono::ChSharedPtr<chrono::ChForce> hydroForce = mBody->SearchForce(
				forceTag);
		//		if (!hydroForce.IsNull())
		//			hydroForce->SetMforce(0);
		//
		//		if (numContactsOnAllSph[i] == 0) continue;
		//    assert(!hydroForce.IsNull() && "Error! sph marker does not have hyroforce tag in ChSystem");
		if (hydroForce.IsNull()) {
			printf(
					"\n\n\n\n Error! sph marker does not have hyroforce tag in ChSystem \n\n\n\n");
		}

		Real4 mDerivVelRho = derivVelRhoD[i];
		Real3 forceSphMarker = mR3(mDerivVelRho) * sphMass;
		chrono::ChVector<> f3 = ConvertRealToChVector(forceSphMarker);
		hydroForce->SetMforce(f3.Length());
		f3.Normalize();
		hydroForce->SetDir(f3);
	}
}

//------------------------------------------------------------------------------------

void CopyCustomChSystemPosVel2HostThrust(thrust::host_vector<Real3>& posRadH,
		thrust::host_vector<Real3>& velMasH,
		chrono::ChSystemParallelDVI& mphysicalSystem,
		const NumberOfObjects& numObjects, int startIndexSph,
		const thrust::host_vector<short int>& numContactsOnAllSph) {
#pragma omp parallel for
	for (int i = 0; i < numObjects.numFluidMarkers; i++) {
		if (numContactsOnAllSph[i] == 0)
			continue;
		auto mBody = mphysicalSystem.Get_bodylist()->at(i + startIndexSph);
		chrono::ChVector<> pos = mBody->GetPos();
		posRadH[i] = mR3(pos.x, pos.y, pos.z);
		chrono::ChVector<> vel = mBody->GetPos_dt();
		velMasH[i] = mR3(vel.x, vel.y, vel.z);
	}
}
//------------------------------------------------------------------------------------

void CopyH2DPosVel(thrust::device_vector<Real3>& posRadD,
		thrust::device_vector<Real3>& velMasD,
		const thrust::host_vector<Real3>& posRadH,
		const thrust::host_vector<Real3>& velMasH) {
	//  assert(posRadH.size() == posRadD.size() && "Error! size mismatch host and device");
	if (posRadH.size() != posRadD.size()) {
		printf("\n\n\n\n Error! size mismatch host and device \n\n\n\n");
	}
	thrust::copy(posRadH.begin(), posRadH.end(), posRadD.begin());
	thrust::copy(velMasH.begin(), velMasH.end(), velMasD.begin());
}

//------------------------------------------------------------------------------------

void CopyD2HPosVel(thrust::host_vector<Real3>& posRadH,
		thrust::host_vector<Real3>& velMasH,
		const thrust::device_vector<Real3>& posRadD,
		const thrust::device_vector<Real3>& velMasD) {
	//  assert(posRadH.size() == posRadD.size() && "Error! size mismatch host and device");
	if (posRadH.size() != posRadD.size()) {
		printf("\n\n\n\n Error! size mismatch host and device \n\n\n\n");
	}
	thrust::copy(posRadD.begin(), posRadD.end(), posRadH.begin());
	thrust::copy(velMasD.begin(), velMasD.end(), velMasH.begin());
}

//------------------------------------------------------------------------------------

void CopyH2D(thrust::device_vector<Real4>& derivVelRhoD,
		const thrust::host_vector<Real4>& derivVelRhoChronoH) {
	//  assert(derivVelRhoChronoH.size() == derivVelRhoD.size() && "Error! size mismatch host and device");
	if (derivVelRhoChronoH.size() != derivVelRhoD.size()) {
		printf("\n\n\n\n Error! size mismatch host and device \n\n\n\n");
	}
	thrust::copy(derivVelRhoChronoH.begin(), derivVelRhoChronoH.end(),
			derivVelRhoD.begin());
}
//------------------------------------------------------------------------------------

void CopySys2D(thrust::device_vector<Real3>& posRadD,
		chrono::ChSystemParallelDVI& mphysicalSystem,
		const NumberOfObjects& numObjects, int startIndexSph) {
	thrust::host_vector<Real3> posRadH(numObjects.numFluidMarkers);
#pragma omp parallel for
	for (int i = 0; i < numObjects.numFluidMarkers; i++) {
		auto mBody = mphysicalSystem.Get_bodylist()->at(i + startIndexSph);
		chrono::ChVector<> p = mBody->GetPos();
		posRadH[i] = mR3(p.x, p.y, p.z);
	}
	thrust::copy(posRadH.begin(), posRadH.end(), posRadD.begin());
}
//------------------------------------------------------------------------------------

void CopyD2H(
		thrust::host_vector<Real3>& posRadH, // do not set the size here since you are using push back later
		thrust::host_vector<Real3>& velMasH,
		thrust::host_vector<Real4>& rhoPresMuH,
		const thrust::device_vector<Real3>& posRadD,
		const thrust::device_vector<Real3>& velMasD,
		const thrust::device_vector<Real4>& rhoPresMuD) {
	//  assert(posRadH.size() == posRadD.size() && "Error! size mismatch host and device");
	if (posRadH.size() != posRadD.size()) {
		printf("\n\n\n\n Error! size mismatch host and device \n\n\n\n");
	}
	thrust::copy(posRadD.begin(), posRadD.end(), posRadH.begin());
	thrust::copy(velMasD.begin(), velMasD.end(), velMasH.begin());
	thrust::copy(rhoPresMuD.begin(), rhoPresMuD.end(), rhoPresMuH.begin());
}

//------------------------------------------------------------------------------------
// FSI_Bodies_Index_H[i] is the the index of the i_th sph represented rigid body in ChSystem
void Add_Rigid_ForceTorques_To_ChSystem(
		chrono::ChSystemParallelDVI& mphysicalSystem,
		const thrust::device_vector<Real3>& rigid_FSI_ForcesD,
		const thrust::device_vector<Real3>& rigid_FSI_TorquesD,
		const std::vector<chrono::ChSharedPtr<chrono::ChBody> >& FSI_Bodies) {
	int numRigids = FSI_Bodies.size();
	//#pragma omp parallel for // Arman: you can bring it back later, when you have a lot of bodies
	for (int i = 0; i < numRigids; i++) {
		chrono::ChSharedPtr<chrono::ChBody> bodyPtr = FSI_Bodies[i];

//		// --------------------------------
//		// Add forces to bodies: Version 1
//		// --------------------------------
//
//		bodyPtr->Empty_forces_accumulators();
//		Real3 mforce = rigid_FSI_ForcesD[i];
//
//		printf("\n\n\n\n\n\n\n rigid forces %e %e %e \n", mforce.x, mforce.y,
//				mforce.z);
//		std::cout << "body name: " << bodyPtr->GetName() << "\n\n\n\n\n";
//		bodyPtr->Empty_forces_accumulators();
//
//		bodyPtr->Accumulate_force(ConvertRealToChVector(mforce),
//				bodyPtr->GetPos(), false);
//
//		Real3 mtorque = rigid_FSI_TorquesD[i];
//		bodyPtr->Accumulate_torque(ConvertRealToChVector(mtorque), false);


		// --------------------------------
		// Add forces to bodies: Version 2
		// --------------------------------

		//	string forceTag("hydrodynamics_force");
		char forceTag[] = "fsi_force";
		char torqueTag[] = "fsi_torque";
		chrono::ChSharedPtr<chrono::ChForce> hydroForce = bodyPtr->SearchForce(
				forceTag);
		chrono::ChSharedPtr<chrono::ChForce> hydroTorque = bodyPtr->SearchForce(
				torqueTag);

		if (hydroForce.IsNull()) {
			hydroForce = chrono::ChSharedPtr<chrono::ChForce>(new chrono::ChForce);
			hydroTorque = chrono::ChSharedPtr<chrono::ChForce>(new chrono::ChForce);

			hydroForce->SetMode(FTYPE_FORCE);
			hydroTorque->SetMode(FTYPE_TORQUE);

			hydroForce->SetName(forceTag);
			hydroTorque->SetName(torqueTag);

			bodyPtr->AddForce(hydroForce);
			bodyPtr->AddForce(hydroTorque);
		}

		chrono::ChVector<> mforce = ConvertRealToChVector(rigid_FSI_ForcesD[i]);
		chrono::ChVector<> mtorque = ConvertRealToChVector(rigid_FSI_TorquesD[i]);

		hydroForce->SetVpoint(bodyPtr->GetPos());
		hydroForce->SetMforce(mforce.Length());
		mforce.Normalize();
		hydroForce->SetDir(mforce);

		hydroTorque->SetMforce(mtorque.Length());
		mtorque.Normalize();
		hydroTorque->SetDir(mtorque);
	}
}

//------------------------------------------------------------------------------------
// FSI_Bodies_Index_H[i] is the the index of the i_th sph represented rigid body in ChSystem
void Copy_External_To_ChSystem(chrono::ChSystemParallelDVI& mphysicalSystem,
		const thrust::host_vector<Real3>& pos_ChSystemBackupH,
		const thrust::host_vector<Real3>& vel_ChSystemBackupH,
		const thrust::host_vector<Real3>& acc_ChSystemBackupH,
		const thrust::host_vector<Real4>& quat_ChSystemBackupH,
		const thrust::host_vector<Real3>& omegaVelGRF_ChSystemBackupH,
		const thrust::host_vector<Real3>& omegaAccGRF_ChSystemBackupH) {
	int numBodies = mphysicalSystem.Get_bodylist()->size();
	if (pos_ChSystemBackupH.size() != numBodies) {
		throw std::runtime_error ("Size of the external data does not match the ChSystem !\n");
	}
	//#pragma omp parallel for // Arman: you can bring it back later, when you have a lot of bodies
	for (int i = 0; i < numBodies; i++) {
		auto mBody = mphysicalSystem.Get_bodylist()->at(i);
		mBody->SetPos(ConvertRealToChVector(pos_ChSystemBackupH[i]));
		mBody->SetPos_dt(ConvertRealToChVector(vel_ChSystemBackupH[i]));
		mBody->SetPos_dtdt(ConvertRealToChVector(acc_ChSystemBackupH[i]));

		mBody->SetRot(ConvertToChQuaternion(quat_ChSystemBackupH[i]));
		mBody->SetWvel_par(ConvertRealToChVector(omegaVelGRF_ChSystemBackupH[i]));
		chrono::ChVector<> acc = ConvertRealToChVector(omegaAccGRF_ChSystemBackupH[i]);
		mBody->SetWacc_par(acc);
	}
}
//------------------------------------------------------------------------------------
void Copy_ChSystem_to_External(thrust::host_vector<Real3>& pos_ChSystemBackupH,
		thrust::host_vector<Real3>& vel_ChSystemBackupH,
		thrust::host_vector<Real3>& acc_ChSystemBackupH,
		thrust::host_vector<Real4>& quat_ChSystemBackupH,
		thrust::host_vector<Real3>& omegaVelGRF_ChSystemBackupH,
		thrust::host_vector<Real3>& omegaAccGRF_ChSystemBackupH,
		chrono::ChSystemParallelDVI& mphysicalSystem) {
	int numBodies = mphysicalSystem.Get_bodylist()->size();
	pos_ChSystemBackupH.resize(numBodies);
	vel_ChSystemBackupH.resize(numBodies);
	acc_ChSystemBackupH.resize(numBodies);
	quat_ChSystemBackupH.resize(numBodies);
	omegaVelGRF_ChSystemBackupH.resize(numBodies);
	omegaAccGRF_ChSystemBackupH.resize(numBodies);
	//#pragma omp parallel for // Arman: you can bring it back later, when you have a lot of bodies
	for (int i = 0; i < numBodies; i++) {
		auto mBody = mphysicalSystem.Get_bodylist()->at(i);
		pos_ChSystemBackupH[i] = ConvertChVectorToR3(mBody->GetPos());
		vel_ChSystemBackupH[i] = ConvertChVectorToR3(mBody->GetPos_dt());
		acc_ChSystemBackupH[i] = ConvertChVectorToR3(mBody->GetPos_dtdt());

		quat_ChSystemBackupH[i] = ConvertChQuaternionToR4(mBody->GetRot());
		omegaVelGRF_ChSystemBackupH[i] = ConvertChVectorToR3(mBody->GetWvel_par());
		omegaAccGRF_ChSystemBackupH[i] = ConvertChVectorToR3(mBody->GetWacc_par());
	}
}

//------------------------------------------------------------------------------------
// FSI_Bodies_Index_H[i] is the the index of the i_th sph represented rigid body in ChSystem
void Copy_fsiBodies_ChSystem_to_FluidSystem(
		thrust::device_vector<Real3>& posRigid_fsiBodies_D,
		thrust::device_vector<Real4>& velMassRigid_fsiBodies_D,
		thrust::device_vector<Real3>& accRigid_fsiBodies_D,
		thrust::device_vector<Real4>& q_fsiBodies_D,
		thrust::device_vector<Real3>& rigidOmegaLRF_fsiBodies_D,
		thrust::device_vector<Real3>& omegaAccLRF_fsiBodies_D,

		thrust::host_vector<Real3>& posRigid_fsiBodies_H,
		thrust::host_vector<Real4>& velMassRigid_fsiBodies_H,
		thrust::host_vector<Real3>& accRigid_fsiBodies_H,
		thrust::host_vector<Real4>& q_fsiBodies_H,
		thrust::host_vector<Real3>& rigidOmegaLRF_fsiBodies_H,
		thrust::host_vector<Real3>& omegaAccLRF_fsiBodies_H,
		const std::vector<chrono::ChSharedPtr<chrono::ChBody> >& FSI_Bodies,
		chrono::ChSystemParallelDVI& mphysicalSystem) {
	int num_fsiBodies_Rigids = FSI_Bodies.size();
	if (posRigid_fsiBodies_D.size() != num_fsiBodies_Rigids
			|| posRigid_fsiBodies_H.size() != num_fsiBodies_Rigids) {
		throw std::runtime_error ("number of fsi bodies that are tracked does not match the array size !\n");
	}
	//#pragma omp parallel for // Arman: you can bring it back later, when you have a lot of bodies
	for (int i = 0; i < num_fsiBodies_Rigids; i++) {
		chrono::ChSharedPtr<chrono::ChBody> bodyPtr = FSI_Bodies[i];
		posRigid_fsiBodies_H[i] = ConvertChVectorToR3(bodyPtr->GetPos());
		velMassRigid_fsiBodies_H[i] = ConvertChVectorToR4(bodyPtr->GetPos_dt(), bodyPtr->GetMass());
		accRigid_fsiBodies_H[i] = ConvertChVectorToR3(bodyPtr->GetPos_dtdt());

		q_fsiBodies_H[i] = ConvertChQuaternionToR4(bodyPtr->GetRot());
		rigidOmegaLRF_fsiBodies_H[i] = ConvertChVectorToR3(bodyPtr->GetWvel_loc());
		omegaAccLRF_fsiBodies_H[i] = ConvertChVectorToR3(bodyPtr->GetWacc_loc());
	}

	CopyFluidDataH2D(
		posRigid_fsiBodies_D,
		velMassRigid_fsiBodies_D,
		accRigid_fsiBodies_D,
		q_fsiBodies_D,
		rigidOmegaLRF_fsiBodies_D,
		omegaAccLRF_fsiBodies_D,
		posRigid_fsiBodies_H,
		velMassRigid_fsiBodies_H,
		accRigid_fsiBodies_H,
		q_fsiBodies_H,
		rigidOmegaLRF_fsiBodies_H,
		omegaAccLRF_fsiBodies_H);
}

