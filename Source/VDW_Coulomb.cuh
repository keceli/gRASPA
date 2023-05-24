#include "data_struct.h"
#include <cuda_fp16.h> 

///////////
// MATHS //
///////////
double matrix_determinant(double* x);

void inverse_matrix(double* x, double **inverse_x);

__host__ __device__ void operator +=(double3 &a, double3 b);

__host__ __device__ void operator -=(double3 &a, double3 b);

__host__ __device__ double3 operator +(double3 a, double3 b);

__host__ __device__ double3 operator -(double3 a, double3 b);

__host__ __device__ double3 operator *(double3 a, double3 b);

__host__ __device__ double3 operator +(double3 a, double b);

__host__ __device__ double3 operator -(double3 a, double b);

__host__ __device__ double3 operator *(double3 a, double b);

__host__ __device__ double dot(double3 a, double3 b);

__host__ void operator +=(MoveEnergy& A, MoveEnergy B);
__host__ void operator -=(MoveEnergy& A, MoveEnergy B);
__host__ MoveEnergy operator +(MoveEnergy A, MoveEnergy B);
__host__ MoveEnergy operator -(MoveEnergy A, MoveEnergy B);

__host__ __device__ void WrapInBox(double3 posvec, double* Cell, double* InverseCell, bool Cubic);

void Setup_threadblock(size_t arraysize, size_t *Nblock, size_t *Nthread);

void checkCUDAError(const char *msg);
template<size_t blockSize, typename T>
T GPUReduction(T* dA, size_t N);
//#define BLOCKSIZE 1024
//#define DEFAULTTHREAD 128
inline void checkCUDAError(const char *msg)
{
    cudaError_t err = cudaGetLastError();
    if( cudaSuccess != err)
    {
        printf("CUDA Error: %s: %s.\n", msg, cudaGetErrorString(err) );
        exit(EXIT_FAILURE);
    }
}

/////////////////////////////
// Total energies from CPU //
/////////////////////////////
void VDWReal_Total_CPU(Boxsize Box, Atoms* Host_System, Atoms* System, ForceField FF, Components SystemComponents, MoveEnergy& E);

double2 setScale(double lambda);

void setScaleGPU(double lambda, double& scalingVDW, double& scalingCoulomb);

__device__ void VDW(const double* FFarg, const double rr_dot, const double scaling, double* result);

__device__ void CoulombReal(const ForceField FF, const double chargeA, const double chargeB, const double r, const double scaling, double* result, double prefactor, double alpha);

__host__ __device__ void PBC(double3& posvec, double* Cell, double* InverseCell, bool Cubic);
//__device__ void PBC(double* posvec, double* Cell, double* InverseCell, bool Cubic);

__device__ void WrapInBox(double* posvec, double* Cell, double* InverseCell, bool Cubic);

__global__ void one_thread_GPU_test(Boxsize Box, Atoms* d_a, ForceField FF, double* xxx);

/////////////////////////////////////////////
// VDW + Real Pairwise Energy Calculations //
/////////////////////////////////////////////
__global__ void Calculate_Single_Body_Energy_SEPARATE_HostGuest_VDWReal(Boxsize Box, Atoms* System, Atoms Old, Atoms New, ForceField FF, double* BlockEnergy, size_t ComponentID, size_t totalAtoms, size_t chainsize, bool* flag, size_t HG_Nblock, size_t GG_Nblock, bool Do_New, bool Do_Old, int3 NComps);

__global__ void Calculate_Multiple_Trial_Energy_SEPARATE_HostGuest_VDWReal(Boxsize Box, Atoms* System, Atoms NewMol, ForceField FF, double* Blocksum, size_t ComponentID, size_t totalAtoms, bool* flag, size_t totalthreads, size_t chainsize, size_t NblockForTrial, size_t HG_Nblock, int3 NComps, int2* ExcludeList);

__global__ void REZERO_VALS(double* vals, size_t size);

__global__ void Energy_difference_LambdaChange(Boxsize Box, Atoms* System, Atoms Mol, ForceField FF, double* BlockEnergy, size_t ComponentID, size_t totalAtoms, size_t chainsize, size_t HG_Nblock, size_t GG_Nblock, int3 NComps, bool* flag, double2 newScale);

//////////////////////
// Ewald Summations //
//////////////////////
double CPU_EwaldDifference(Boxsize& Box, Atoms& New, Atoms& Old, ForceField& FF, Components& SystemComponents, size_t SelectedComponent, bool Swap, size_t SelectedTrial);

double GPU_EwaldDifference_Reinsertion(Boxsize& Box, Atoms*& d_a, Atoms& Old, double3* temp, ForceField& FF, double* Blocksum, Components& SystemComponents, size_t SelectedComponent, size_t UpdateLocation);

double GPU_EwaldDifference_IdentitySwap(Boxsize& Box, Atoms*& d_a, Atoms& Old, double3* temp, ForceField& FF, double* Blocksum, Components& SystemComponents, size_t OLDComponent, size_t NEWComponent, size_t UpdateLocation);

void Update_Ewald_Vector(Boxsize& Box, bool CPU, Components& SystemComponents);

double GPU_EwaldDifference_General(Boxsize& Box, Atoms*& d_a, Atoms& New, Atoms& Old, ForceField& FF, double* Blocksum, Components& SystemComponents, size_t SelectedComponent, int MoveType, size_t Location, double2 proposed_scale);

double GPU_EwaldDifference_LambdaChange(Boxsize& Box, Atoms*& d_a, Atoms& Old, ForceField& FF, double* Blocksum, Components& SystemComponents, size_t SelectedComponent, double2 oldScale, double2 newScale, int MoveType);

void Skip_Ewald(Boxsize& Box);

__global__ void TotalVDWCoul(Boxsize Box, Atoms* System, ForceField FF, double* Blocksum, bool* flag, size_t totalthreads, size_t Host_threads, size_t NAds, size_t NFrameworkAtomsPerThread, bool HostHost, bool UseOffset);

void Setup_threadblock_EW(size_t arraysize, size_t *Nblock, size_t *Nthread);

__global__ void Double3_CacheCheck(double* Array, Complex* Vector, size_t totsize);

__global__ void Setup_Ewald_Vector(Boxsize Box, Complex* eik_x, Complex* eik_y, Complex* eik_z, Atoms* System, size_t numberOfAtoms, size_t NumberOfComponents, bool UseOffSet);

void CPU_GPU_EwaldTotalEnergy(Boxsize& Box, Boxsize& device_Box, Atoms* System, Atoms* d_a, ForceField FF, ForceField device_FF, Components& SystemComponents, MoveEnergy& E);

void Calculate_Exclusion_Energy_Rigid(Boxsize& Box, Atoms* System, ForceField FF, Components& SystemComponents);

void Check_WaveVector_CPUGPU(Boxsize& Box, Components& SystemComponents);

////////////////////
// Total energies //
////////////////////

__global__ void TotalEwald(Atoms* d_a, Boxsize Box, double* BlockSum, Complex* eik_x, Complex* eik_y, Complex* eik_z, Complex* Eik, size_t totAtom, size_t Ncomp, size_t NAtomPerThread, size_t residueAtoms);

MoveEnergy Ewald_TotalEnergy(Simulations& Sim, Components& SystemComponents, bool UseOffSet);

MoveEnergy Total_VDW_Coulomb_Energy(Simulations& Sim, ForceField FF, size_t totMol, size_t Host_threads, size_t Guest_threads, size_t NFrameworkAtomsPerThread, bool ConsiderHostHost, bool UseOffset);

//////////////////////
// Tail Corrections //
//////////////////////
double TotalTailCorrection(Components& SystemComponents, size_t FFsize, double Volume);

double TailCorrectionDifference(Components& SystemComponents, size_t SelectedComponent, size_t FFsize, double Volume, int MoveType);

double TailCorrectionIdentitySwap(Components& SystemComponents, size_t NEWComponent, size_t OLDComponent, size_t FFsize, double Volume);

///////////////////////////////////
// Deep Potential For Host-Guest //
///////////////////////////////////
void Prepare_DNN_InitialPositions(Atoms*& d_a, Atoms& New, Atoms& Old, Components& SystemComponents, size_t SelectedComponent, int MoveType, size_t Location);

void Prepare_DNN_InitialPositions_Reinsertion(Atoms*& d_a, Atoms& Old, double3* temp, Components& SystemComponents, size_t SelectedComponent, size_t Location);

void Read_DNN_Model(Components& SystemComponents);
double DNN_Evaluate(Components& SystemComponents, std::vector<double>& Feature);
void Prepare_FeatureMatrix(Simulations& Sim, Components& SystemComponents, Atoms* HostSystem, Boxsize Box);
//void Predict_From_FeatureMatrix(Simulations& Sim, Components& SystemComponents);
double Predict_From_FeatureMatrix_Total(Simulations& Sim, Components& SystemComponents);
double Predict_From_FeatureMatrix_Move(Simulations& Sim, Components& SystemComponents, int DNN_CalcType);

void WriteOutliers(Components& SystemComponents, Simulations& Sim, int MoveType, MoveEnergy E, double Correction);
