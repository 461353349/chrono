/*
 * FSI_Integrate.cpp
 *
 *  Created on: Nov 5, 2015
 *      Author: arman
 */

#include "chrono_fsi/FSI_Integrate.h"
#include "chrono_fsi/SphInterface.h"
#include "chrono_fsi/collideSphereSphere.cuh"
#include "chrono_fsi/UtilsDeviceOperations.cuh"

//#ifdef CHRONO_OPENGL
//#undef CHRONO_OPENGL
//#endif
#ifdef CHRONO_OPENGL
#include "chrono_opengl/ChOpenGLWindow.h"
chrono::opengl::ChOpenGLWindow& gl_window =
		chrono::opengl::ChOpenGLWindow::getInstance();
#endif

// =============================================================================

void InitializeChronoGraphics(chrono::ChSystemParallelDVI& mphysicalSystem) {
	//	Real3 domainCenter = 0.5 * (paramsH.cMin + paramsH.cMax);
	//	ChVector<> CameraLocation = ChVector<>(2 * paramsH.cMax.x, 2 * paramsH.cMax.y, 2 * paramsH.cMax.z);
	//	ChVector<> CameraLookAt = ChVector<>(domainCenter.x, domainCenter.y, domainCenter.z);
	chrono::ChVector<> CameraLocation = chrono::ChVector<>(0, -10, 0);
	chrono::ChVector<> CameraLookAt = chrono::ChVector<>(0, 0, 0);

#ifdef CHRONO_OPENGL
	gl_window.Initialize(1280, 720, "HMMWV", &mphysicalSystem);
	gl_window.SetCamera(CameraLocation, CameraLookAt,
			chrono::ChVector<>(0, 0, 1));
	gl_window.SetRenderMode(chrono::opengl::WIREFRAME);

// Uncomment the following two lines for the OpenGL manager to automatically un the simulation in an infinite loop.

// gl_window.StartDrawLoop(paramsH.dT);
// return 0;
#endif
}
// =============================================================================

int DoStepChronoSystem(chrono::ChSystemParallelDVI& mphysicalSystem,
		chrono::vehicle::ChWheeledVehicleAssembly* mVehicle, Real dT,
		double mTime, double time_hold_vehicle, bool haveVehicle) {
	if (haveVehicle) {
		// Release the vehicle chassis at the end of the hold time.

		if (mVehicle->GetVehicle()->GetChassis()->GetBodyFixed()
				&& mTime > time_hold_vehicle) {
			mVehicle->GetVehicle()->GetChassis()->SetBodyFixed(false);
			for (int i = 0; i < 2 * mVehicle->GetVehicle()->GetNumberAxles();
					i++) {
				mVehicle->GetVehicle()->GetWheelBody(i)->SetBodyFixed(false);
			}
		}

		// Update vehicle
		mVehicle->Update(mTime);
	}

#ifdef CHRONO_OPENGL
	if (gl_window.Active()) {
		gl_window.DoStepDynamics(dT);
		gl_window.Render();
	}
#else
	mphysicalSystem.DoStepDynamics(dT);
#endif
	return 1;
}
//------------------------------------------------------------------------------------
void DoStepDynamics_FSI(chrono::ChSystemParallelDVI& mphysicalSystem,
		chrono::vehicle::ChWheeledVehicleAssembly* mVehicle,
		thrust::device_vector<Real3>& posRadD,
		thrust::device_vector<Real3>& velMasD,
		thrust::device_vector<Real3>& vel_XSPH_D,
		thrust::device_vector<Real4>& rhoPresMuD,

		thrust::device_vector<Real3>& posRadD2,
		thrust::device_vector<Real3>& velMasD2,
		thrust::device_vector<Real4>& rhoPresMuD2,

		thrust::device_vector<Real4>& derivVelRhoD,
		thrust::device_vector<uint>& rigidIdentifierD,
		const thrust::device_vector<Real3>& rigidSPH_MeshPos_LRF_D,

		thrust::device_vector<Real3>& posRigid_fsiBodies_D,
		thrust::device_vector<Real4>& velMassRigid_fsiBodies_D,
		thrust::device_vector<Real3>& accRigid_fsiBodies_D,
		thrust::device_vector<Real4>& q_fsiBodies_D,
		thrust::device_vector<Real3>& omegaVelLRF_fsiBodies_D,
		thrust::device_vector<Real3>& omegaAccLRF_fsiBodies_D,


		thrust::device_vector<Real3>& posRigid_fsiBodies_D2,
		thrust::device_vector<Real4>& velMassRigid_fsiBodies_D2,
		thrust::device_vector<Real3>& accRigid_fsiBodies_D2,
		thrust::device_vector<Real4>& q_fsiBodies_D2,
		thrust::device_vector<Real3>& omegaVelLRF_fsiBodies_D2,
		thrust::device_vector<Real3>& omegaAccLRF_fsiBodies_D2,

		thrust::host_vector<Real3>& pos_ChSystemBackupH,
		thrust::host_vector<Real3>& vel_ChSystemBackupH,
		thrust::host_vector<Real3>& acc_ChSystemBackupH,
		thrust::host_vector<Real4>& quat_ChSystemBackupH,
		thrust::host_vector<Real3>& omegaVelGRF_ChSystemBackupH,
		thrust::host_vector<Real3>& omegaAccGRF_ChSystemBackupH,

		thrust::host_vector<Real3>& posRigid_fsiBodies_dummyH,
		thrust::host_vector<Real4>& velMassRigid_fsiBodies_dummyH,
		thrust::host_vector<Real3>& accRigid_fsiBodies_dummyH,
		thrust::host_vector<Real4>& q_fsiBodies_dummyH,
		thrust::host_vector<Real3>& omegaVelLRF_fsiBodies_dummyH,
		thrust::host_vector<Real3>& omegaAccLRF_fsiBodies_dummyH,

		thrust::device_vector<Real3>& rigid_FSI_ForcesD,
		thrust::device_vector<Real3>& rigid_FSI_TorquesD,

		thrust::device_vector<uint>& bodyIndexD,
		std::vector<chrono::ChSharedPtr<chrono::ChBody> >& FSI_Bodies,
		const thrust::host_vector<int4>& referenceArray,
		const NumberOfObjects& numObjects, const SimParams& paramsH,
		double mTime, double time_hold_vehicle, int tStep,
		bool haveVehicle) {
	printf("step: %d\n", tStep);
	chrono::ChTimerParallel doStep_timer;
	doStep_timer.AddTimer("half_step_dynamic_fsi_12");
	doStep_timer.AddTimer("fsi_copy_force_fluid2ChSystem_12");
	doStep_timer.AddTimer("stepDynamic_mbd_12");
	doStep_timer.AddTimer("fsi_copy_posVel_ChSystem2fluid_12");
	doStep_timer.AddTimer("update_marker_pos_12");

	Copy_ChSystem_to_External(pos_ChSystemBackupH, vel_ChSystemBackupH, acc_ChSystemBackupH,
			quat_ChSystemBackupH, omegaVelGRF_ChSystemBackupH, omegaAccGRF_ChSystemBackupH,
			mphysicalSystem);
	//**********************************
	//----------------------------
	//--------- start fluid ------
	//----------------------------
	InitSystem(paramsH, numObjects);
	// ** initialize host mid step data
	thrust::copy(posRadD.begin(), posRadD.end(), posRadD2.begin());
	thrust::copy(velMasD.begin(), velMasD.end(), velMasD2.begin());
	thrust::copy(rhoPresMuD.begin(), rhoPresMuD.end(), rhoPresMuD2.begin());

	FillMyThrust4(derivVelRhoD, mR4(0));

	//**********************************
	// ******************
	// ******************
	// ******************
	// ******************
	// ****************** RK2: 1/2

	doStep_timer.start("half_step_dynamic_fsi_12");
	// //assumes ...D2 is a copy of ...D

	IntegrateSPH(derivVelRhoD, posRadD2, velMasD2, rhoPresMuD2, posRadD,
			velMasD, vel_XSPH_D, rhoPresMuD, bodyIndexD, referenceArray,
			q_fsiBodies_D, accRigid_fsiBodies_D, omegaVelLRF_fsiBodies_D, omegaAccLRF_fsiBodies_D, rigidSPH_MeshPos_LRF_D, rigidIdentifierD,
			numObjects, paramsH, 0.5 * paramsH.dT);

	Rigid_Forces_Torques(rigid_FSI_ForcesD, rigid_FSI_TorquesD, posRadD,
			posRigid_fsiBodies_D, derivVelRhoD, rigidIdentifierD, numObjects);

	doStep_timer.stop("half_step_dynamic_fsi_12");

	doStep_timer.start("fsi_copy_force_fluid2ChSystem_12");
	Add_Rigid_ForceTorques_To_ChSystem(mphysicalSystem, rigid_FSI_ForcesD,
			rigid_FSI_TorquesD, FSI_Bodies);
	doStep_timer.stop("fsi_copy_force_fluid2ChSystem_12");
	//----------------------------
	//--------- end fluid ------
	//----------------------------

	doStep_timer.start("stepDynamic_mbd_12");
	mTime += 0.5 * paramsH.dT;
	DoStepChronoSystem(mphysicalSystem, mVehicle, 0.5 * paramsH.dT, mTime,
			time_hold_vehicle, haveVehicle); // Keep only this if you are just interested in the rigid sys

	doStep_timer.stop("stepDynamic_mbd_12");

	//----------------------------
	//--------- start fluid ------
	//----------------------------

	doStep_timer.start("fsi_copy_posVel_ChSystem2fluid_12");
	Copy_fsiBodies_ChSystem_to_FluidSystem(
			posRigid_fsiBodies_D2, velMassRigid_fsiBodies_D2, accRigid_fsiBodies_D2,
			q_fsiBodies_D2, omegaVelLRF_fsiBodies_D2, omegaAccLRF_fsiBodies_D2,
			posRigid_fsiBodies_dummyH, velMassRigid_fsiBodies_dummyH, accRigid_fsiBodies_dummyH,
			q_fsiBodies_dummyH, omegaVelLRF_fsiBodies_dummyH, omegaAccLRF_fsiBodies_dummyH,
			FSI_Bodies, mphysicalSystem);
	doStep_timer.stop("fsi_copy_posVel_ChSystem2fluid_12");

	doStep_timer.start("update_marker_pos_12");
	UpdateRigidMarkersPositionVelocity(posRadD2, velMasD2, rigidSPH_MeshPos_LRF_D,
			rigidIdentifierD, posRigid_fsiBodies_D2, q_fsiBodies_D2,
			velMassRigid_fsiBodies_D2, omegaVelLRF_fsiBodies_D2, numObjects);
	doStep_timer.stop("update_marker_pos_12");
	// ******************
	// ******************
	// ******************
	// ******************
	// ****************** RK2: 2/2
	FillMyThrust4(derivVelRhoD, mR4(0));

	// //assumes ...D2 is a copy of ...D
	IntegrateSPH(derivVelRhoD, posRadD, velMasD, rhoPresMuD, posRadD2, velMasD2,
			vel_XSPH_D, rhoPresMuD2, bodyIndexD, referenceArray,
			q_fsiBodies_D2, accRigid_fsiBodies_D2, omegaVelLRF_fsiBodies_D2, omegaAccLRF_fsiBodies_D2, rigidSPH_MeshPos_LRF_D, rigidIdentifierD,
			numObjects, paramsH, paramsH.dT);

	Rigid_Forces_Torques(rigid_FSI_ForcesD, rigid_FSI_TorquesD, posRadD2,
			posRigid_fsiBodies_D2, derivVelRhoD, rigidIdentifierD, numObjects);
	Add_Rigid_ForceTorques_To_ChSystem(mphysicalSystem, rigid_FSI_ForcesD,
			rigid_FSI_TorquesD, FSI_Bodies);  // Arman: take care of this
											  //----------------------------
											  //--------- end fluid ------
											  //----------------------------

	mTime -= 0.5 * paramsH.dT;

	// Arman: do it so that you don't need gpu when you don't have fluid
	Copy_External_To_ChSystem(mphysicalSystem,
			pos_ChSystemBackupH, vel_ChSystemBackupH, acc_ChSystemBackupH,
			quat_ChSystemBackupH, omegaVelGRF_ChSystemBackupH, omegaAccGRF_ChSystemBackupH);

	mTime += paramsH.dT;

	DoStepChronoSystem(mphysicalSystem, mVehicle, 1.0 * paramsH.dT, mTime,
			time_hold_vehicle, haveVehicle);

	//----------------------------
	//--------- start fluid ------
	//----------------------------
	Copy_fsiBodies_ChSystem_to_FluidSystem(
			posRigid_fsiBodies_D, velMassRigid_fsiBodies_D, accRigid_fsiBodies_D,
			q_fsiBodies_D, omegaVelLRF_fsiBodies_D, omegaAccLRF_fsiBodies_D,
			posRigid_fsiBodies_dummyH, velMassRigid_fsiBodies_dummyH, accRigid_fsiBodies_dummyH,
			q_fsiBodies_dummyH, omegaVelLRF_fsiBodies_dummyH, omegaAccLRF_fsiBodies_dummyH,
			FSI_Bodies, mphysicalSystem);
	UpdateRigidMarkersPositionVelocity(posRadD, velMasD, rigidSPH_MeshPos_LRF_D,
			rigidIdentifierD, posRigid_fsiBodies_D, q_fsiBodies_D,
			velMassRigid_fsiBodies_D, omegaVelLRF_fsiBodies_D, numObjects);

	if ((tStep % 10 == 0) && (paramsH.densityReinit != 0)) {
		DensityReinitialization(posRadD, velMasD, rhoPresMuD,
				numObjects.numAllMarkers, paramsH.gridSize);
	}
	//----------------------------
	//--------- end fluid ------
	//----------------------------
	doStep_timer.PrintReport();

	// ****************** End RK2
}

//------------------------------------------------------------------------------------
void DoStepDynamics_ChronoRK2(chrono::ChSystemParallelDVI& mphysicalSystem,
		chrono::vehicle::ChWheeledVehicleAssembly* mVehicle,

		thrust::host_vector<Real3>& pos_ChSystemBackupH,
		thrust::host_vector<Real3>& vel_ChSystemBackupH,
		thrust::host_vector<Real3>& acc_ChSystemBackupH,
		thrust::host_vector<Real4>& quat_ChSystemBackupH,
		thrust::host_vector<Real3>& omegaVelGRF_ChSystemBackupH,
		thrust::host_vector<Real3>& omegaAccGRF_ChSystemBackupH,

		const SimParams& paramsH, double mTime, double time_hold_vehicle,
		bool haveVehicle) {
	chrono::ChTimerParallel doStep_timer;
	doStep_timer.AddTimer("stepDynamic_mbd_12");

	Copy_ChSystem_to_External(pos_ChSystemBackupH, vel_ChSystemBackupH, acc_ChSystemBackupH,
			quat_ChSystemBackupH, omegaVelGRF_ChSystemBackupH, omegaAccGRF_ChSystemBackupH,
			mphysicalSystem);
	//**********************************
	doStep_timer.start("stepDynamic_mbd_12");
	mTime += 0.5 * paramsH.dT;
	DoStepChronoSystem(mphysicalSystem, mVehicle, 0.5 * paramsH.dT, mTime,
			time_hold_vehicle, haveVehicle); // Keep only this if you are just interested in the rigid sys

	doStep_timer.stop("stepDynamic_mbd_12");

	mTime -= 0.5 * paramsH.dT;

	// Arman: do it so that you don't need gpu when you don't have fluid
	Copy_External_To_ChSystem(mphysicalSystem,
			pos_ChSystemBackupH, vel_ChSystemBackupH, acc_ChSystemBackupH,
			quat_ChSystemBackupH, omegaVelGRF_ChSystemBackupH, omegaAccGRF_ChSystemBackupH);

	mTime += paramsH.dT;

	DoStepChronoSystem(mphysicalSystem, mVehicle, 1.0 * paramsH.dT, mTime,
			time_hold_vehicle, haveVehicle);

	doStep_timer.PrintReport();

	// ****************** End RK2
}
