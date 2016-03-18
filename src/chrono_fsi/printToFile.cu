/*
 * printToFile.cu
 *
 *  Created on: Mar 2, 2015
 *      Author: Arman Pazouki
 */
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <fstream>
#include <thrust/reduce.h>
#include "chrono_fsi/printToFile.cuh"
#include "chrono_fsi/custom_cutil_math.h"
#include "chrono_fsi/SPHCudaUtils.h"
using namespace std;

//*******************************************************************************************************************************
void PrintCartesianData_MidLine(const thrust::host_vector<Real4>& rho_Pres_CartH,
                                const thrust::host_vector<Real4>& vel_VelMag_CartH,
                                const int3& cartesianGridDims,
                                const SimParams& paramsH) {
  int3 gridCenter = mI3(cartesianGridDims.x / 2, cartesianGridDims.y / 2, cartesianGridDims.z / 2);
  stringstream midLineProfile;
  for (int k = 0; k < cartesianGridDims.z; k++) {
    // Assuming flow in x Direction, walls on Z direction, periodic on y direction
    int index = (cartesianGridDims.x * cartesianGridDims.y) * k + cartesianGridDims.x * gridCenter.y + gridCenter.x;
    Real3 v = mR3(vel_VelMag_CartH[index]);
    Real3 rp = mR3(rho_Pres_CartH[index]);
    //		midLineProfile << v.x << ", " << v.y << ", " << v.z << ", " << length(v) << ", " << rp.x << ", " << rp.y
    //<<
    // endl;
    midLineProfile << v.x << ", ";
  }
  midLineProfile << endl;
  static int count = 0;
  ofstream midLineData;
  if (count == 0) {
    midLineData.open("MidLineData.txt");
  } else {
    midLineData.open("MidLineData.txt", ios::app);
  }
  count++;
  midLineData << midLineProfile.str();
  midLineData.close();
}

//*******************************************************************************************************************************
void PrintToFile_SPH(const thrust::device_vector<Real3>& posRadD,
                     const thrust::device_vector<Real3>& velMasD,
                     const thrust::device_vector<Real4>& rhoPresMuD,
                     const thrust::host_vector<int4>& referenceArray,

                     const SimParams paramsH,
                     const Real realTime,
                     int tStep,
                     int stepSave,
                     const std::string& out_dir) {
  thrust::host_vector<Real3> posRadH = posRadD;
  thrust::host_vector<Real3> velMasH = velMasD;
  thrust::host_vector<Real4> rhoPresMuH = rhoPresMuD;

  int tStepsPovFiles = stepSave;  // 25;//1000;//2000;
  if (tStep % tStepsPovFiles == 0) {
    //#ifdef _WIN32
    //			system("mkdir povFiles");
    //#else
    //			system("mkdir -p povFiles");
    //#endif
    if (tStep / tStepsPovFiles == 0) {
      const string rmCmd = string("rm ") + out_dir + string("/*.csv");
      system(rmCmd.c_str());
    }
    char fileCounter[5];
    int dumNumChar = sprintf(fileCounter, "%d", int(tStep / tStepsPovFiles));

    //*****************************************************
    const string nameFluid = out_dir + string("/fluid") + string(fileCounter) + string(".csv");

    ofstream fileNameFluidParticles;
    fileNameFluidParticles.open(nameFluid);
    stringstream ssFluidParticles;
    ssFluidParticles << "x, y, z, vx, vy, vz, rho, p, mu, type\n";
    for (int i = referenceArray[0].x; i < referenceArray[0].y; i++) {
      Real3 pos = posRadH[i];
      Real3 vel = velMasH[i];
      Real4 rP = rhoPresMuH[i];
      Real velMag = length(vel);
      ssFluidParticles << pos.x << ", " << pos.y << ", " << pos.z << ", " << vel.x << ", " << vel.y << ", " << vel.z
                       << ", " << rP.x << ", " << rP.y << ", " << rP.z << ", " << rP.w << ", " << endl;
    }
    fileNameFluidParticles << ssFluidParticles.str();
    fileNameFluidParticles.close();
    //*****************************************************
    const string nameBoundary = out_dir + string("/boundary") + string(fileCounter) + string(".csv");

    //    ofstream fileNameBoundaries;
    //    fileNameBoundaries.open(nameBoundary);
    //    stringstream ssBoundary;
    //    for (int i = referenceArray[1].x; i < referenceArray[1].y; i++) {
    //      Real3 pos = posRadH[i];
    //      Real3 vel = velMasH[i];
    //      Real4 rP = rhoPresMuH[i];
    //      Real velMag = length(vel);
    //      ssBoundary << pos.x << ", " << pos.y << ", " << pos.z << ", " << vel.x << ", " << vel.y << ", " << vel.z <<
    //      ", "
    //                 << velMag << ",
    //                              "<< rP.x<<",
    //          "<< rP.y<<", "<< rP.w<<", "<<endl;
    //    }
    //    fileNameBoundaries << ssBoundary.str();
    //    fileNameBoundaries.close();
    //*****************************************************
    const string nameFluidBoundaries = out_dir + string("/fluid_boundary") + string(fileCounter) + string(".csv");

    ofstream fileNameFluidBoundaries;
    fileNameFluidBoundaries.open(nameFluidBoundaries);
    stringstream ssFluidBoundaryParticles;
    //		ssFluidBoundaryParticles.precision(20);
    ssFluidBoundaryParticles << "x, y, z, vx, vy, vz, rho, p, mu, type\n";

    for (int i = referenceArray[0].x; i < referenceArray[1].y; i++) {
      Real3 pos = posRadH[i];
      Real3 vel = velMasH[i];
      Real4 rP = rhoPresMuH[i];
      Real velMag = length(vel);
      // if (pos.y > .0002 && pos.y < .0008)
      ssFluidBoundaryParticles << pos.x << ", " << pos.y << ", " << pos.z << ", " << vel.x << ", " << vel.y << ", "
                               << vel.z << ", " << rP.x << ", " << rP.y << ", " << rP.z << ", " << rP.w << endl;
    }
    fileNameFluidBoundaries << ssFluidBoundaryParticles.str();
    fileNameFluidBoundaries.close();
    //*****************************************************
    const string nameBCE = out_dir + string("/BCE") + string(fileCounter) + string(".csv");

    ofstream fileNameBCE;
    fileNameBCE.open(nameBCE);
    stringstream ssBCE;
    //		ssFluidBoundaryParticles.precision(20);
    ssBCE << "x, y, z, vx, vy, vz, rho, p, mu, type\n";

    int refSize = referenceArray.size();
    if (refSize > 2) {
      for (int i = referenceArray[2].x; i < referenceArray[refSize - 1].y; i++) {
        Real3 pos = posRadH[i];
        Real3 vel = velMasH[i];
        Real4 rP = rhoPresMuH[i];
        Real velMag = length(vel);
        // if (pos.y > .0002 && pos.y < .0008)
        ssBCE << pos.x << ", " << pos.y << ", " << pos.z << ", " << vel.x << ", " << vel.y << ", " << vel.z << ", "
              << velMag << ", " << rP.x << ", " << rP.y << ", " << rP.z << ", " << rP.w << endl;
      }
    }
    fileNameBCE << ssBCE.str();
    fileNameBCE.close();
    //*****************************************************
  }
  posRadH.clear();
  velMasH.clear();
  rhoPresMuH.clear();
}

//*******************************************************************************************************************************

void PrintToFile(const thrust::device_vector<Real3>& posRadD,
                 const thrust::device_vector<Real3>& velMasD,
                 const thrust::device_vector<Real4>& rhoPresMuD,
                 const thrust::host_vector<int4>& referenceArray,
                 const SimParams paramsH,
                 Real realTime,
                 int tStep,
                 int stepSave,
                 const string& out_dir) {
  // print fluid stuff
  PrintToFile_SPH(posRadD, velMasD, rhoPresMuD, referenceArray, paramsH, realTime, tStep, stepSave, out_dir);
}
//*******************************************************************************************************************************
// to be implemented
void PrintToFileCartesian() {
  //  // ######## the commented sections need to be fixed. you need cartesian data by calling SphSystemGpu.MapSPH_ToGrid
  //  ////////-+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//comcom
  //  ofstream fileNameCartesianTotal;
  //  thrust::host_vector<Real4> rho_Pres_CartH(1);
  //  thrust::host_vector<Real4> vel_VelMag_CartH(1);
  //  Real resolution = 2 * paramsH.HSML;
  //  int3 cartesianGridDims;
  //  int tStepCartesianTotal = 1000000;
  //  int tStepCartesianSlice = 100000;
  //  int tStepPoiseuilleProf = 1000;  // tStepCartesianSlice;
  //
  //  int stepCalcCartesian = min(tStepCartesianTotal, tStepCartesianSlice);
  //  stepCalcCartesian = min(stepCalcCartesian, tStepPoiseuilleProf);
  //
  //  if (tStep % stepCalcCartesian == 0) {
  //    MapSPH_ToGrid(resolution, cartesianGridDims, rho_Pres_CartH, vel_VelMag_CartH, posRadD, velMasD, rhoPresMuD,
  //                  referenceArray[referenceArray.size() - 1].y, paramsH);
  //  }
  //  if (tStep % tStepCartesianTotal == 0) {
  //    if (tStep / tStepCartesianTotal == 0) {
  //      fileNameCartesianTotal.open("dataCartesianTotal.txt");
  //      fileNameCartesianTotal << "variables = \"x\", \"y\", \"z\", \"Vx\", \"Vy\", \"Vz\", \"Velocity
  //          Magnitude\", \"Rho\", \"Pressure\"\n";
  //    } else {
  //      fileNameCartesianTotal.open("dataCartesianTotal.txt", ios::app);
  //    }
  //    fileNameCartesianTotal << "zone I = " << cartesianGridDims.x << ", J = " << cartesianGridDims.y
  //                           << ", K =
  //                              "<<cartesianGridDims.z<<endl;
  //        stringstream ssCartesianTotal;
  //    for (int k = 0; k < cartesianGridDims.z; k++) {
  //      for (int j = 0; j < cartesianGridDims.y; j++) {
  //        for (int i = 0; i < cartesianGridDims.x; i++) {
  //          int index = i + j * cartesianGridDims.x + k * cartesianGridDims.x * cartesianGridDims.y;
  //          Real3 gridNodeLoc = resolution * mR3(i, j, k) + paramsH.worldOrigin;
  //          ssCartesianTotal << gridNodeLoc.x << ", " << gridNodeLoc.y << ", " << gridNodeLoc.z
  //                           << ",
  //                              "<<
  //                              vel_VelMag_CartH[index]
  //                                  .x
  //                           << ", " << vel_VelMag_CartH[index].y
  //                           << ",
  //                              "<<
  //                              vel_VelMag_CartH[index]
  //                                  .z
  //                           << ", " << vel_VelMag_CartH[index].w << ", " << rho_Pres_CartH[index].x << ", "
  //                           << rho_Pres_CartH[index].y << endl;
  //        }
  //      }
  //    }
  //    fileNameCartesianTotal << ssCartesianTotal.str();
  //    fileNameCartesianTotal.close();
  //  }
  //  ////////-+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //comcom
  //  ofstream fileNameCartesianMidplane;
  //  if (tStep % tStepCartesianSlice == 0) {
  //    if (tStep / tStepCartesianSlice == 0) {
  //      fileNameCartesianMidplane.open("dataCartesianMidplane.txt");
  //      fileNameCartesianMidplane << "variables = \"x\", \"z\", \"Vx\", \"Vy\", \"Vz\", \"Velocity
  //          Magnitude\",
  //	\"Rho\", \"Pressure\"\n";
  //    } else {
  //      fileNameCartesianMidplane.open("dataCartesianMidplane.txt", ios::app);
  //    }
  //    fileNameCartesianMidplane << "zone I = " << cartesianGridDims.x << ", J = " << cartesianGridDims.z << "\n";
  //    int j = cartesianGridDims.y / 2;
  //    stringstream ssCartesianMidplane;
  //    for (int k = 0; k < cartesianGridDims.z; k++) {
  //      for (int i = 0; i < cartesianGridDims.x; i++) {
  //        int index = i + j * cartesianGridDims.x + k * cartesianGridDims.x * cartesianGridDims.y;
  //        Real3 gridNodeLoc = resolution * mR3(i, j, k) + paramsH.worldOrigin;
  //        ssCartesianMidplane << gridNodeLoc.x << ", " << gridNodeLoc.z << ", " << vel_VelMag_CartH[index].x
  //                            << ",
  //                               "<<
  //                               vel_VelMag_CartH[index]
  //                                   .y
  //                            << ", " << vel_VelMag_CartH[index].z << ", " << vel_VelMag_CartH[index].w << ", "
  //                            << rho_Pres_CartH[index].x << ", " << rho_Pres_CartH[index].y << endl;
  //      }
  //    }
  //    fileNameCartesianMidplane << ssCartesianMidplane.str();
  //    fileNameCartesianMidplane.close();
  //  }
  //  rho_Pres_CartH.clear();
  //  ////////-+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++comcom
  //  ofstream fileVelocityProfPoiseuille;
  //  if (tStep % tStepPoiseuilleProf == 0) {
  //    if (tStep / tStepPoiseuilleProf == 0) {
  //      fileVelocityProfPoiseuille.open("dataVelProfile.txt");
  //      fileVelocityProfPoiseuille << "variables = \"Z(m)\", \"Vx(m/s)\"\n";
  //
  //    } else {
  //      fileVelocityProfPoiseuille.open("dataVelProfile.txt", ios::app);
  //    }
  //    fileVelocityProfPoiseuille << "zone T=\"t = " << realTime << "\"" endl;
  //    stringstream ssVelocityProfPoiseuille;
  //    int j = cartesianGridDims.y / 2;
  //    int i = cartesianGridDims.x / 2;
  //    for (int k = 0; k < cartesianGridDims.z; k++) {
  //      int index = i + j * cartesianGridDims.x + k * cartesianGridDims.x * cartesianGridDims.y;
  //      Real3 gridNodeLoc = resolution * mR3(i, j, k) + paramsH.worldOrigin;
  //      if (gridNodeLoc.z > 1 * paramsH.sizeScale && gridNodeLoc.z < 2 * paramsH.sizeScale) {
  //        ssVelocityProfPoiseuille << gridNodeLoc.z << ", " << vel_VelMag_CartH[index].x << endl;
  //      }
  //    }
  //    fileVelocityProfPoiseuille << ssVelocityProfPoiseuille.str();
  //    fileVelocityProfPoiseuille.close();
  //  }
  //  vel_VelMag_CartH.clear();
  //  //////////-+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++comcom
}
