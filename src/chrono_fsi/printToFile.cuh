/*
 * printToFile.cuh
 *
 *  Created on: Mar 2, 2015
 *      Author: Arman Pazouki
 */
#ifndef PRINTTOFILE_CUH
#define PRINTTOFILE_CUH
#include "chrono_fsi/custom_cutil_math.h"
#include "chrono_fsi/SPHCudaUtils.h"
#include "chrono_fsi/MyStructs.cuh"
#include <thrust/host_vector.h>
#include <thrust/device_vector.h>

//--------------------------------------------------------------------------------------------------------------------------------
void PrintCartesianData_MidLine(const thrust::host_vector<Real4>& rho_Pres_CartH,
                                const thrust::host_vector<Real4>& vel_VelMag_CartH,
                                const int3& cartesianGridDims,
                                const SimParams& paramsH);
//--------------------------------------------------------------------------------------------------------------------------------
void PrintToFile(const thrust::device_vector<Real3>& posRadD,
                 const thrust::device_vector<Real3>& velMasD,
                 const thrust::device_vector<Real4>& rhoPresMuD,
                 const thrust::host_vector<int4>& referenceArray,
                 const SimParams paramsH,
                 Real realTime,
                 int tStep,
                 int stepSave,
                 const std::string& out_dir);

#endif
