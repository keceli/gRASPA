void Setup_RandomNumber(RandomNumber& Random, size_t SIZE);
void Copy_Atom_data_to_device(size_t NumberOfComponents, Atoms* device_System, Atoms* System);
void Update_Components_for_framework(size_t NumberOfComponents, Components& SystemComponents, Atoms* System);

void Setup_Temporary_Atoms_Structure(Atoms& TempMol, Atoms* System);  

void Initialize_Move_Statistics(Move_Statistics& MoveStats);

void Setup_Box_Temperature_Pressure(Units& Constants, Components& SystemComponents, Boxsize& device_Box);

void Prepare_ForceField(ForceField& FF, ForceField& device_FF, PseudoAtomDefinitions PseudoAtom);

void Prepare_Widom(WidomStruct& Widom, Boxsize Box, Simulations& Sims, Components SystemComponents, Atoms* System, Move_Statistics MoveStats);

void Prepare_TempSystem_On_Host(Atoms& TempSystem)
{
    size_t Allocate_size = 1024;
    TempSystem.x             = (double*) malloc(Allocate_size*sizeof(double));
    TempSystem.y             = (double*) malloc(Allocate_size*sizeof(double));
    TempSystem.z             = (double*) malloc(Allocate_size*sizeof(double));
    TempSystem.scale         = (double*) malloc(Allocate_size*sizeof(double));
    TempSystem.charge        = (double*) malloc(Allocate_size*sizeof(double));
    TempSystem.scaleCoul     = (double*) malloc(Allocate_size*sizeof(double));
    TempSystem.Type          = (size_t*) malloc(Allocate_size*sizeof(size_t));
    TempSystem.MolID         = (size_t*) malloc(Allocate_size*sizeof(size_t));
    TempSystem.size          = 0;
    TempSystem.Allocate_size = Allocate_size;
}

inline void Setup_RandomNumber(RandomNumber& Random, size_t SIZE)
{
  Random.randomsize = SIZE; Random.offset = 0;
  std::vector<double> array_random(Random.randomsize);
  for (size_t i = 0; i < Random.randomsize; i++)
  {
    array_random[i] = get_random_from_zero_to_one();
  }
  Random.host_random = Doubleconvert1DVectortoArray(array_random);
  Random.device_random = CUDA_copy_allocate_double_array(Random.host_random, Random.randomsize);
}

static inline void Setup_threadblock_MAIN(size_t arraysize, size_t *Nblock, size_t *Nthread)
{
  size_t value = arraysize;
  if(value >= DEFAULTTHREAD) value = DEFAULTTHREAD;
  double ratio = (double)arraysize/value;
  size_t blockValue = ceil(ratio);
  if(blockValue == 0) blockValue++;
  //Zhao's note: Default thread should always be 64, 128, 256, 512, ...
  // This is because we are using partial sums, if arraysize is smaller than defaultthread, we need to make sure that
  //while Nthread is dividing by 2, it does not generate ODD NUMBER (for example, 5/2 = 2, then element 5 will be ignored)//
  *Nthread = DEFAULTTHREAD;
  *Nblock = blockValue;
}

inline void Copy_Atom_data_to_device(size_t NumberOfComponents, Atoms* device_System, Atoms* System)
{
  size_t required_size = 0;
  for(size_t i = 0; i < NumberOfComponents; i++)
  {
    if(i == 0){ required_size = System[i].size;} else { required_size = System[i].Allocate_size; }
    device_System[i].x             = CUDA_copy_allocate_array(System[i].x,         required_size);
    device_System[i].y             = CUDA_copy_allocate_array(System[i].y,         required_size);
    device_System[i].z             = CUDA_copy_allocate_array(System[i].z,         required_size);
    device_System[i].scale         = CUDA_copy_allocate_array(System[i].scale,     required_size);
    device_System[i].charge        = CUDA_copy_allocate_array(System[i].charge,    required_size);
    device_System[i].scaleCoul     = CUDA_copy_allocate_array(System[i].scaleCoul, required_size);
    device_System[i].Type          = CUDA_copy_allocate_array(System[i].Type,      required_size);
    device_System[i].MolID         = CUDA_copy_allocate_array(System[i].MolID,     required_size);
    device_System[i].size          = System[i].size;
    device_System[i].Molsize       = System[i].Molsize;
    device_System[i].Allocate_size = System[i].Allocate_size;
  }
}

inline void Update_Components_for_framework(size_t NumberOfComponents, Components& SystemComponents, Atoms* System)
{
  SystemComponents.Total_Components = NumberOfComponents; //Framework + 1 adsorbate
  SystemComponents.TotalNumberOfMolecules = 1; //If there is a framework, framework is counted as a molecule//
  SystemComponents.NumberOfFrameworks = 1; //Just one framework
  SystemComponents.MoleculeName.push_back("MOF"); //Name of the framework
  SystemComponents.Moleculesize.push_back(System[0].size);
  SystemComponents.Allocate_size.push_back(System[0].size);
  SystemComponents.NumberOfMolecule_for_Component.push_back(1);
  SystemComponents.MolFraction.push_back(1.0);
  SystemComponents.IdealRosenbluthWeight.push_back(1.0);
  SystemComponents.FugacityCoeff.push_back(1.0);
  SystemComponents.Tc.push_back(0.0);        //Tc for framework is set to zero
  SystemComponents.Pc.push_back(0.0);        //Pc for framework is set to zero
  SystemComponents.Accentric.push_back(0.0); //Accentric factor for framework is set to zero
  //Zhao's note: for now, assume the framework is rigid//
  SystemComponents.rigid.push_back(true);
  SystemComponents.hasfractionalMolecule.push_back(false); //No fractional molecule for the framework//
  SystemComponents.NumberOfCreateMolecules.push_back(0); //Create zero molecules for the framework//
  LAMBDA lambda;
  lambda.newBin = 0; lambda.delta = static_cast<double>(1.0/(lambda.binsize)); lambda.WangLandauScalingFactor = 0.0; //Zhao's note: in raspa3, delta is 1/(nbin - 1)
  lambda.FractionalMoleculeID = 0;
  SystemComponents.Lambda.push_back(lambda);

  TMMC tmmc;
  SystemComponents.Tmmc.push_back(tmmc); //Just use default values for tmmc for the framework, it will do nothing//
  //Add PseudoAtoms from the Framework to the total PseudoAtoms array//
  SystemComponents.UpdatePseudoAtoms(INSERTION, 0);
}

inline void Setup_Temporary_Atoms_Structure(Atoms& TempMol, Atoms* System)
{
  //Set up MolArrays//
  size_t Allocate_size_Temporary=1024; //Assign 1024 empty slots for the temporary structures//
  //OLD//
  TempMol.x         = CUDA_allocate_array<double> (Allocate_size_Temporary);
  TempMol.y         = CUDA_allocate_array<double> (Allocate_size_Temporary);
  TempMol.z         = CUDA_allocate_array<double> (Allocate_size_Temporary);
  TempMol.scale     = CUDA_allocate_array<double> (Allocate_size_Temporary);
  TempMol.charge    = CUDA_allocate_array<double> (Allocate_size_Temporary);
  TempMol.scaleCoul = CUDA_allocate_array<double> (Allocate_size_Temporary);
  TempMol.Type      = CUDA_allocate_array<size_t> (Allocate_size_Temporary);
  TempMol.MolID     = CUDA_allocate_array<size_t> (Allocate_size_Temporary);
  TempMol.size      = 0;
  TempMol.Molsize   = 0;
  TempMol.Allocate_size = Allocate_size_Temporary;
}

inline void Initialize_Move_Statistics(Move_Statistics& MoveStats)
{
  MoveStats.TranslationProb = 0.0; MoveStats.RotationProb = 0.0; MoveStats.WidomProb = 0.0; MoveStats.SwapProb = 0.0; MoveStats.ReinsertionProb = 0.0; MoveStats.CBCFProb = 0.0;
  MoveStats.TranslationAccepted = 0; MoveStats.TranslationTotal      = 0; MoveStats.TranslationAccRatio = 0.0;
  MoveStats.RotationAccepted    = 0; MoveStats.RotationTotal         = 0; MoveStats.RotationAccRatio    = 0.0;
  MoveStats.InsertionAccepted   = 0; MoveStats.InsertionTotal        = 0; 
  MoveStats.DeletionAccepted    = 0; MoveStats.DeletionTotal         = 0;
  MoveStats.ReinsertionAccepted = 0; MoveStats.ReinsertionTotal      = 0;
  MoveStats.CBCFAccepted        = 0; MoveStats.CBCFTotal             = 0; 
  MoveStats.CBCFInsertionTotal  = 0; MoveStats.CBCFInsertionAccepted = 0;
  MoveStats.CBCFLambdaTotal     = 0; MoveStats.CBCFLambdaAccepted    = 0;
  MoveStats.CBCFDeletionTotal   = 0; MoveStats.CBCFDeletionAccepted  = 0;
}

inline void Setup_Box_Temperature_Pressure(Units& Constants, Components& SystemComponents, Boxsize& device_Box)
{
  SystemComponents.Beta = 1.0/(Constants.BoltzmannConstant/(Constants.MassUnit*pow(Constants.LengthUnit,2)/pow(Constants.TimeUnit,2))*SystemComponents.Temperature);
  //Convert pressure from pascal
  device_Box.Pressure/=(Constants.MassUnit/(Constants.LengthUnit*pow(Constants.TimeUnit,2)));
  printf("------------------- SIMULATION BOX PARAMETERS -----------------\n");
  printf("Pressure:        %.5f\n", device_Box.Pressure);
  printf("Box Volume:      %.5f\n", device_Box.Volume);
  printf("Box Beta:        %.5f\n", SystemComponents.Beta);
  printf("Box Temperature: %.5f\n", SystemComponents.Temperature);
  printf("---------------------------------------------------------------\n");
}

inline void Prepare_ForceField(ForceField& FF, ForceField& device_FF, PseudoAtomDefinitions PseudoAtom)
{
  // COPY DATA TO DEVICE POINTER //
  //device_FF.FFParams      = CUDA_copy_allocate_double_array(FF.FFParams, 5);
  device_FF.OverlapCriteria = FF.OverlapCriteria;
  device_FF.CutOffVDW       = FF.CutOffVDW;
  device_FF.CutOffCoul      = FF.CutOffCoul;
  //device_FF.Prefactor       = FF.Prefactor;
  //device_FF.Alpha           = FF.Alpha;

  device_FF.epsilon         = CUDA_copy_allocate_double_array(FF.epsilon, FF.size*FF.size);
  device_FF.sigma           = CUDA_copy_allocate_double_array(FF.sigma, FF.size*FF.size);
  device_FF.z               = CUDA_copy_allocate_double_array(FF.z, FF.size*FF.size);
  device_FF.shift           = CUDA_copy_allocate_double_array(FF.shift, FF.size*FF.size);
  device_FF.FFType          = CUDA_copy_allocate_int_array(FF.FFType, FF.size*FF.size);
  device_FF.noCharges       = FF.noCharges;
  device_FF.size            = FF.size;
  device_FF.VDWRealBias     = FF.VDWRealBias;
  //Formulate Component statistics on the host
  //ForceFieldParser(FF, PseudoAtom);
  //PseudoAtomParser(FF, PseudoAtom);
}

inline void Prepare_Widom(WidomStruct& Widom, Boxsize Box, Simulations& Sims, Components SystemComponents, Atoms* System, size_t NumberOfBlocks)
{
  //Zhao's note: NumberWidomTrials is for first bead. NumberWidomTrialsOrientations is for the rest, here we consider single component, not mixture //

  size_t MaxTrialsize = max(Widom.NumberWidomTrials, Widom.NumberWidomTrialsOrientations*(SystemComponents.Moleculesize[1]-1));

  //Zhao's note: The previous way yields a size for blocksum that can be smaller than the number of kpoints
  //This is a problem when you need to do parallel Ewald summation for the whole system//
  //Might be good to add a flag or so//
  //size_t MaxResultsize = MaxTrialsize*(System[0].Allocate_size+System[1].Allocate_size);
  size_t MaxAllocatesize = max(System[0].Allocate_size, System[1].Allocate_size);
  size_t MaxResultsize = MaxTrialsize * SystemComponents.Total_Components * MaxAllocatesize * 5; //For Volume move, it really needs a lot of blocks//


  printf("----------------- MEMORY ALLOCAION STATUS -----------------\n");
  //Compare Allocate sizes//
  printf("System allocate_sizes are: %zu, %zu\n", System[0].Allocate_size, System[1].Allocate_size); 
  printf("Component allocate_sizes are: %zu, %zu\n", SystemComponents.Allocate_size[0], SystemComponents.Allocate_size[1]);

  Sims.flag        = (bool*)malloc(MaxTrialsize * sizeof(bool));
  cudaMallocHost(&Sims.device_flag,          MaxTrialsize * sizeof(bool));

  cudaMallocHost(&Sims.Blocksum,             (MaxResultsize/DEFAULTTHREAD + 1)*sizeof(double));
  //cudaMalloc(&Sims.Blocksum,             (MaxResultsize/DEFAULTTHREAD + 1)*sizeof(double));

  printf("Allocated Blocksum size: %zu\n", (MaxResultsize/DEFAULTTHREAD + 1));
 
  //cudaMalloc(&Sims.Blocksum,             (MaxResultsize/DEFAULTTHREAD + 1)*sizeof(double));
  Sims.Nblocks = MaxResultsize/DEFAULTTHREAD + 1;

  printf("Allocated %zu doubles for Blocksums\n", MaxResultsize/DEFAULTTHREAD + 1);

  std::vector<double> MaxRotation    = {30.0/(180/3.1415), 30.0/(180/3.1415), 30.0/(180/3.1415)};
  Sims.MaxTranslation.x = Box.Cell[0]*0.1; Sims.MaxTranslation.y = Box.Cell[4]*0.1; Sims.MaxTranslation.z = Box.Cell[8]*0.1;
  Sims.MaxRotation.x = 30.0/(180/3.1415);  Sims.MaxRotation.y = 30.0/(180/3.1415);  Sims.MaxRotation.z = 30.0/(180/3.1415);

  Sims.start_position = 0;
  //Sims.Nblocks = 0;
  Sims.TotalAtoms = 0;
  Sims.AcceptedFlag = false;

  Widom.WidomFirstBeadAllocatesize = MaxResultsize/DEFAULTTHREAD;
  printf("------------------------------------------------------------\n");
}

inline void Allocate_Copy_Ewald_Vector(Boxsize& device_Box, Components SystemComponents)
{
  printf("******   Allocating Ewald WaveVectors (INITIAL STAGE ONLY)   ******\n");
  //Zhao's note: This only works if the box size is not changed, eik_xy might not be useful if box size is not changed//
  size_t eikx_size     = SystemComponents.eik_x.size() * 2;
  size_t eiky_size     = SystemComponents.eik_y.size() * 2; //added times 2 for box volume move//
  size_t eikz_size     = SystemComponents.eik_z.size() * 2;
  printf("Allocated %zu %zu %zu space for eikxyz\n", eikx_size, eiky_size, eikz_size);
  //size_t eikxy_size    = SystemComponents.eik_xy.size();
  size_t storedEiksize = SystemComponents.storedEik.size() * 2; //added times 2 for box volume move//
  cudaMalloc(&device_Box.eik_x,     eikx_size     * sizeof(Complex));
  cudaMalloc(&device_Box.eik_y,     eiky_size     * sizeof(Complex));
  cudaMalloc(&device_Box.eik_z,     eikz_size     * sizeof(Complex));
  //cudaMalloc(&device_Box.eik_xy,    eikxy_size    * sizeof(Complex));
  cudaMalloc(&device_Box.storedEik,    storedEiksize * sizeof(Complex));
  cudaMalloc(&device_Box.totalEik,     storedEiksize * sizeof(Complex));
  cudaMalloc(&device_Box.FrameworkEik, storedEiksize * sizeof(Complex));

  Complex storedEik[storedEiksize]; //Temporary Complex struct on the host//
  Complex FrameworkEik[storedEiksize];
  //for(size_t i = 0; i < SystemComponents.storedEik.size(); i++)
  for(size_t i = 0; i < storedEiksize; i++)
  {
    if(i < SystemComponents.storedEik.size())
    {
      storedEik[i].real = SystemComponents.storedEik[i].real();
      storedEik[i].imag = SystemComponents.storedEik[i].imag();
      
      FrameworkEik[i].real = SystemComponents.FrameworkEik[i].real();
      FrameworkEik[i].imag = SystemComponents.FrameworkEik[i].imag();
    }
    else
    {
      storedEik[i].real    = 0.0; storedEik[i].imag    = 0.0;
      FrameworkEik[i].real = 0.0; FrameworkEik[i].imag = 0.0;
    }
    if(i < 10) printf("Wave Vector %zu is %.5f %.5f\n", i, storedEik[i].real, storedEik[i].imag);
  }
  cudaMemcpy(device_Box.storedEik,    storedEik,    storedEiksize * sizeof(Complex), cudaMemcpyHostToDevice); checkCUDAError("error copying Complex");
  cudaMemcpy(device_Box.FrameworkEik, FrameworkEik, storedEiksize * sizeof(Complex), cudaMemcpyHostToDevice); checkCUDAError("error copying Complex");
  printf("****** DONE Allocating Ewald WaveVectors (INITIAL STAGE ONLY) ******\n");
}

inline void Check_Simulation_Energy(Boxsize& Box, Atoms* System, ForceField FF, ForceField device_FF, Components& SystemComponents, int SIMULATIONSTAGE, size_t Numsim, Simulations& Sim, SystemEnergies& Energy)
{
  std::string STAGE; 
  switch(SIMULATIONSTAGE)
  {
    case INITIAL:
    { STAGE = "INITIAL"; break;}
    case CREATEMOL:
    { STAGE = "CREATE_MOLECULE"; break;}
    case FINAL:
    { STAGE = "FINAL"; break;}
  }
  printf("======================== CALCULATING %s STAGE ENERGY ========================\n", STAGE.c_str());
  MoveEnergy ENERGY;
  Atoms device_System[SystemComponents.Total_Components];
  cudaMemcpy(device_System, Sim.d_a, SystemComponents.Total_Components * sizeof(Atoms), cudaMemcpyDeviceToHost);
  cudaMemcpy(Box.Cell,        Sim.Box.Cell,        9 * sizeof(double), cudaMemcpyDeviceToHost);
  cudaMemcpy(Box.InverseCell, Sim.Box.InverseCell, 9 * sizeof(double), cudaMemcpyDeviceToHost); 
  //Update every value that can be changed during a volume move//
  Box.Volume = Sim.Box.Volume;
  Box.ReciprocalCutOff = Sim.Box.ReciprocalCutOff;
  Box.Cubic = Sim.Box.Cubic;
  Box.kmax  = Sim.Box.kmax;

  double start = omp_get_wtime();
  VDWReal_Total_CPU(Box, System, device_System, FF, SystemComponents, ENERGY);
  double end = omp_get_wtime();
  double CPUSerialTime = end - start;
         start = omp_get_wtime();
  double* xxx; xxx = (double*) malloc(sizeof(double)*2);
  double* device_xxx; device_xxx = CUDA_copy_allocate_double_array(xxx, 2);
  //Zhao's note: if the serial GPU energy test is too slow, comment it out//
  //one_thread_GPU_test<<<1,1>>>(Sim.Box, Sim.d_a, device_FF, device_xxx);
  cudaMemcpy(xxx, device_xxx, sizeof(double), cudaMemcpyDeviceToHost);
         end = omp_get_wtime();
  cudaDeviceSynchronize();
  
  double SerialGPUTime = end - start;
  //For total energy, divide the parallelization into several parts//
  //For framework, every thread treats the interaction between one framework atom with an adsorbate molecule//
  //For adsorbate/adsorbate, every thread treats one adsorbate molecule with an adsorbate molecule//
  start = omp_get_wtime();
  size_t Host_threads  = 0;
  size_t Guest_threads = 0;
  size_t NFrameworkAtomsPerThread = 4;
  size_t NAdsorbate = 0;
  size_t Nblock = 0; size_t Nthread = 0;
  for(size_t i = 1; i < SystemComponents.Total_Components; i++) NAdsorbate += SystemComponents.NumberOfMolecule_for_Component[i];
  Host_threads  = SystemComponents.Moleculesize[0] / NFrameworkAtomsPerThread; //Per adsorbate molecule//
  if(SystemComponents.Moleculesize[0] % NFrameworkAtomsPerThread != 0) Host_threads ++;
  Host_threads *= NAdsorbate; //Total = Host_thread_per_molecule * number of Adsorbate molecule
  Guest_threads = NAdsorbate * (NAdsorbate-1)/2;
  double totE = 0.0;
  if(Host_threads + Guest_threads > 0)
  {
    bool   ConsiderHostHost = false;
    bool   UseOffset        = false;
    totE = Total_VDW_Coulomb_Energy(Sim, device_FF, NAdsorbate, Host_threads, Guest_threads, NFrameworkAtomsPerThread, ConsiderHostHost, UseOffset);
  }
  end = omp_get_wtime();

  //Do Parallel Total Ewald//
  double TotEwald     = 0.0;
  double CPUEwaldTime = 0.0;
  double GPUEwaldTime = 0.0;

  if(!device_FF.noCharges)
  {
    cudaDeviceSynchronize();
    double EwStart = omp_get_wtime();
    CPU_GPU_EwaldTotalEnergy(Box, Sim.Box, System, Sim.d_a, FF, device_FF, SystemComponents, ENERGY);
    ENERGY.EwaldE -= SystemComponents.FrameworkEwald;

    double EwEnd  = omp_get_wtime();
    //Zhao's note: if it is in the initial stage, calculate the intra and self exclusion energy for ewald summation//
    if(SIMULATIONSTAGE == INITIAL) Calculate_Exclusion_Energy_Rigid(Box, System, FF, SystemComponents);
    CPUEwaldTime = EwEnd - EwStart;

    cudaDeviceSynchronize();
    //Zhao's note: if doing initial energy, initialize and copy host Ewald to device// 
    if(SIMULATIONSTAGE == INITIAL) Allocate_Copy_Ewald_Vector(Sim.Box, SystemComponents);
    Check_WaveVector_CPUGPU(Sim.Box, SystemComponents); //Check WaveVector on the CPU and GPU//
    cudaDeviceSynchronize();
    EwStart = omp_get_wtime();
    bool UseOffset = false;
    TotEwald  = Ewald_TotalEnergy(Sim, SystemComponents, UseOffset);
    TotEwald -= SystemComponents.FrameworkEwald;
    cudaDeviceSynchronize();
    EwEnd = omp_get_wtime();
    GPUEwaldTime = EwEnd - EwStart;
  }

  //Calculate Tail Correction Energy//
  ENERGY.TailE = TotalTailCorrection(SystemComponents, FF.size, Sim.Box.Volume);

  ENERGY.DNN_E = Predict_From_FeatureMatrix_Total(Sim, SystemComponents);

  if(SIMULATIONSTAGE == INITIAL)
  {
    printf("INITIAL STATE of sim %zu: \nVDW + Real (CPU): %.5f[Host-Guest], %.5f[Guest-Guest] (%.5f secs)\nVDW + Real (1 thread GPU): %.5f (%.5f secs)\nVDW + Real (Parallel GPU): %.5f (%.5f secs)\n", Numsim, ENERGY.HGVDWReal, ENERGY.GGVDWReal, CPUSerialTime, xxx[0], SerialGPUTime, totE, end - start);
    if(!device_FF.noCharges)
    {
      printf("Ewald (CPU): %.5f (%.5f secs), Ewald (Parallel GPU): %.5f (%.5f secs)\n", ENERGY.EwaldE, CPUEwaldTime, TotEwald, GPUEwaldTime);
    }
    printf("Tail Correction Energy (CPU Only): %.10f\n", ENERGY.TailE);
    if(SystemComponents.UseDNNforHostGuest)
    {
      double correction = ENERGY.DNN_Correction();
      printf("DNN Energy (Model Prediction): %.5f, DNN Drift: %.5f\n", ENERGY.DNN_E, correction);
    }
    SystemComponents.Initial_Energy = ENERGY;
  }
  else if(SIMULATIONSTAGE == CREATEMOL)
  {
    printf("AFTER CREATING MOLECULES of sim %zu: \nVDW + Real (CPU): %.5f[Host-Guest], %.5f[Guest-Guest] (%.5f secs)\nVDW + Real (1 thread GPU): %.5f (%.5f secs)\nVDW + Real (Parallel GPU): %.5f (%.5f secs)\n", Numsim, ENERGY.HGVDWReal, ENERGY.GGVDWReal, CPUSerialTime, xxx[0], SerialGPUTime, totE, end - start);
    if(!device_FF.noCharges)
    {
      printf("Ewald (CPU): %.5f (%.5f secs), Ewald (Parallel GPU): %.5f (%.5f secs)\n", ENERGY.EwaldE, CPUEwaldTime, TotEwald, GPUEwaldTime);
    }
    printf("Tail Correction Energy (CPU Only): %.10f\n", ENERGY.TailE);
    if(SystemComponents.UseDNNforHostGuest)
    {
      double correction = ENERGY.DNN_Correction();
      printf("DNN Energy (Model Prediction): %.5f, DNN Drift: %.5f\n", ENERGY.DNN_E, correction);
    }
    SystemComponents.CreateMol_Energy = ENERGY;
    //Record delta energies, and rezero delta energies after creating molecules//
    SystemComponents.CreateMol_deltaVDWReal = SystemComponents.deltaVDWReal; SystemComponents.CreateMol_deltaEwald = SystemComponents.deltaEwald;
    SystemComponents.CreateMol_deltaTailE   = SystemComponents.deltaTailE;   SystemComponents.CreateMol_deltaDNN   = SystemComponents.deltaDNN;
    SystemComponents.deltaVDWReal = 0.0;
    SystemComponents.deltaEwald   = 0.0;
    SystemComponents.deltaTailE   = 0.0;
    SystemComponents.deltaDNN     = 0.0;
    Energy.CreateMol_running_energy = Energy.running_energy;
    Energy.running_energy = 0.0;
  }
  else
  { 
    printf("FINAL STATE of sim %zu: \nVDW + Real (CPU): %.5f[Host-Guest], %.5f[Guest-Guest] (%.5f secs)\nVDW + Real (1 thread GPU): %.5f (%.5f secs)\nVDW + Real (Parallel GPU): %.5f (%.5f secs)\n", Numsim, ENERGY.HGVDWReal, ENERGY.GGVDWReal, CPUSerialTime, xxx[0], SerialGPUTime, totE, end - start);
    if(!device_FF.noCharges)
    {
      printf("Ewald (CPU): %.5f (%.5f secs), Ewald (Parallel GPU): %.5f (%.5f secs)\n", ENERGY.EwaldE, CPUEwaldTime, TotEwald, GPUEwaldTime);
    }
    printf("Tail Correction Energy (CPU Only): %.10f\n", ENERGY.TailE);
    if(SystemComponents.UseDNNforHostGuest)
    {
      double correction = ENERGY.DNN_Correction();
      printf("DNN Energy (Model Prediction): %.5f, DNN Drift: %.5f\n", ENERGY.DNN_E, correction);
    }
    SystemComponents.Final_Energy = ENERGY;
  }
  printf("====================== DONE CALCULATING %s STAGE ENERGY ======================\n", STAGE.c_str());
}

inline void Copy_AtomData_from_Device(Atoms* System, Atoms* Host_System, Atoms* d_a, Components& SystemComponents)
{
  cudaMemcpy(System, d_a, SystemComponents.Total_Components * sizeof(Atoms), cudaMemcpyDeviceToHost);
  //printval<<<1,1>>>(System[0]);
  //printvald_a<<<1,1>>>(d_a);
  for(size_t ijk=0; ijk < SystemComponents.Total_Components; ijk++)
  {
    // if the host allocate_size is different from the device, allocate more space on the host
    Host_System[ijk].x         = (double*) malloc(System[ijk].Allocate_size*sizeof(double));
    Host_System[ijk].y         = (double*) malloc(System[ijk].Allocate_size*sizeof(double));
    Host_System[ijk].z         = (double*) malloc(System[ijk].Allocate_size*sizeof(double));
    Host_System[ijk].scale     = (double*) malloc(System[ijk].Allocate_size*sizeof(double));
    Host_System[ijk].charge    = (double*) malloc(System[ijk].Allocate_size*sizeof(double));
    Host_System[ijk].scaleCoul = (double*) malloc(System[ijk].Allocate_size*sizeof(double));
    Host_System[ijk].Type      = (size_t*) malloc(System[ijk].Allocate_size*sizeof(size_t));
    Host_System[ijk].MolID     = (size_t*) malloc(System[ijk].Allocate_size*sizeof(size_t));
    Host_System[ijk].size      = System[ijk].size;
    Host_System[ijk].Allocate_size = System[ijk].Allocate_size;

    cudaMemcpy(Host_System[ijk].x, System[ijk].x, sizeof(double)*System[ijk].Allocate_size, cudaMemcpyDeviceToHost);
    cudaMemcpy(Host_System[ijk].y, System[ijk].y, sizeof(double)*System[ijk].Allocate_size, cudaMemcpyDeviceToHost);
    cudaMemcpy(Host_System[ijk].z, System[ijk].z, sizeof(double)*System[ijk].Allocate_size, cudaMemcpyDeviceToHost);
    cudaMemcpy(Host_System[ijk].scale, System[ijk].scale, sizeof(double)*System[ijk].Allocate_size, cudaMemcpyDeviceToHost);
    cudaMemcpy(Host_System[ijk].charge, System[ijk].charge, sizeof(double)*System[ijk].Allocate_size, cudaMemcpyDeviceToHost);
    cudaMemcpy(Host_System[ijk].scaleCoul, System[ijk].scaleCoul, sizeof(double)*System[ijk].Allocate_size, cudaMemcpyDeviceToHost);
    cudaMemcpy(Host_System[ijk].Type, System[ijk].Type, sizeof(size_t)*System[ijk].Allocate_size, cudaMemcpyDeviceToHost);
    cudaMemcpy(Host_System[ijk].MolID, System[ijk].MolID, sizeof(size_t)*System[ijk].Allocate_size, cudaMemcpyDeviceToHost);
    Host_System[ijk].size = System[ijk].size;
  }
}


///Zhao's note: this section is confusing. Try changing names and adding values just for CreateMol!//
inline void ENERGY_SUMMARY(std::vector<SystemEnergies>& Energy, std::vector<Components>& SystemComponents, Units& Constants)
{
  size_t NumberOfSimulations = SystemComponents.size();
  for(size_t i = 0; i < NumberOfSimulations; i++)
  {
    MoveEnergy InitE           = SystemComponents[i].Initial_Energy;
    MoveEnergy CreateMolE      = SystemComponents[i].CreateMol_Energy;
    MoveEnergy FinalE          = SystemComponents[i].Final_Energy;
    MoveEnergy CreateMolDeltaE = SystemComponents[i].CreateMoldeltaE;
    MoveEnergy DeltaE          = SystemComponents[i].deltaE;
    //double CreateMol_diff      = (E.InitialEnergy   + E.Init_running_energy) - (E.FinalEwaldE + E.FinalVDWReal + E.FinalTailE + E.FinalDNN);
    double diff = (CreateMolE.total() + DeltaE.total()) - (FinalE.total());
    printf("======================== ENERGY SUMMARY (Simulation %zu) =========================\n", i);
    printf("Initial VDW + Real (Before Creating Molecule): %.5f [Host-Guest] %.5f [Guest-Guest]\nInitial VDW + Real (After Creating Molecule): %.5f [Host-Guest] %.5f [Guest-Guest]\nInitial Ewald (Before Creating Molecule): %.5f [Host-Guest] %.5f [Guest-Guest]\nInitial Ewald (After Creating Molecule): %.5f[Host-Guest] %.5f [Guest-Guest]\n", InitE.HGVDWReal, InitE.GGVDWReal, CreateMolE.HGVDWReal, CreateMolE.GGVDWReal, CreateMolE.HGEwaldE, CreateMolE.EwaldE);
    printf("Initial DNN Energy (Before Creating Molecule): %.5f\nInitial DNN Energy (After Creating Molecule): %.5f\n", InitE.DNN_E, InitE.DNN_E);
    printf("Total Initial Energy (Before Creating Molecule): %.5f\n", InitE.total());
    printf("Total Initial Energy (After Creating Molecule):  %.5f\n", CreateMolE.total());
   
    printf("Creating Molecule: Running Energy (VDW + Real): %.5f [Host-Guest] %.5f [Guest-Guest]\nRunning Ewald: %.5f[Host-Guest] %.5f [Guest-Guest]\nRunning DNN: %.5f\nRunning Energy (Total): %.5f\n", CreateMolDeltaE.HGVDWReal, CreateMolDeltaE.GGVDWReal, CreateMolDeltaE.HGEwaldE, CreateMolDeltaE.EwaldE, CreateMolDeltaE.DNN_E, CreateMolDeltaE.total());
    printf("After Creating Molecule: Running Energy (VDW + Real): %.5f [Host-Guest] %.5f [Guest-Guest]\nRunning Ewald: %.5f[Host-Guest] %.5f [Guest-Guest]\nRunning DNN: %.5f\nRunning Energy (Total): %.5f\n", DeltaE.HGVDWReal, DeltaE.GGVDWReal, DeltaE.HGEwaldE, DeltaE.EwaldE, DeltaE.DNN_E, DeltaE.total());
    printf("Final VDW + Real Energy (GPU): %.5f [Host-Guest] %.5f [Guest-Guest]\n",  DeltaE.HGVDWReal + CreateMolE.HGVDWReal, DeltaE.GGVDWReal + CreateMolE.GGVDWReal);
    printf("Final Ewald Energy      (GPU): %.5f [Host-Guest] %.5f [Guest-Guest]\n", DeltaE.HGEwaldE + CreateMolE.HGEwaldE, DeltaE.EwaldE + CreateMolE.EwaldE);
    printf("Final Tail Correction Energy (Running): %.5f\n", DeltaE.TailE + CreateMolE.TailE);
    printf("Final Tail Correction Energy (End):     %.5f\n", FinalE.TailE);
    printf("Final DNN Energy (Running): %.5f, Final DNN Energy (End): %.5f\n", DeltaE.DNN_E + CreateMolE.DNN_E, FinalE.DNN_E);
    printf("Final Total Energy: %.5f\n", FinalE.total());
    printf("CPU VDW + Real Difference: %.5f [Host-Guest] %.5f [Guest-Guest]\n", FinalE.HGVDWReal - InitE.HGVDWReal, FinalE.GGVDWReal - InitE.GGVDWReal);
    printf("CPU Ewald Difference: %.5f[Host-Guest] %.5f [Guest-Guest]\n", FinalE.HGEwaldE - InitE.HGEwaldE, FinalE.EwaldE - InitE.EwaldE);
    printf("Tail Correction Difference: %.5f\n", FinalE.TailE - InitE.TailE);
    if(SystemComponents[i].UseDNNforHostGuest)
    {
      printf("Stored Host-Guest VDW + Real (Running): %.5f \n", DeltaE.storedHGVDWReal + CreateMolDeltaE.storedHGVDWReal);
      printf("Stored Host-Guest Ewald      (Running): %.5f \n", DeltaE.storedHGEwaldE  + CreateMolDeltaE.storedHGEwaldE);
      printf("Stored Host-Guest Total      (Running): *** %.5f ***\n", DeltaE.storedHGEwaldE  + CreateMolDeltaE.storedHGEwaldE + DeltaE.storedHGVDWReal + CreateMolDeltaE.storedHGVDWReal);
      printf("DNN Difference: *** %.5f ***\n", FinalE.DNN_E - InitE.DNN_E);
    }
    printf("Final Total Energy (Running Energy from Simulation, GPU): %.5f (%.5f [K])\n", DeltaE.total() + CreateMolE.total(), (DeltaE.total() + CreateMolE.total()) * Constants.energy_to_kelvin);
    printf("Final Total Energy (Recalculated by Energy Check  , CPU): %.5f (%.5f [K])\n", FinalE.total(), FinalE.total() * Constants.energy_to_kelvin);
    printf("Drift in Energy: %.5f\n", diff);
    printf("Drift in VDW + Real:  %.5f [Host-Guest] %.5f [Guest-Guest]\n", DeltaE.HGVDWReal + CreateMolE.HGVDWReal - FinalE.HGVDWReal, DeltaE.GGVDWReal + CreateMolE.GGVDWReal - FinalE.GGVDWReal);
    printf("Drift in Ewald:       %.5f [Host-Guest] %.5f [Guest-Guest]\n", DeltaE.HGEwaldE + CreateMolE.HGEwaldE - FinalE.HGEwaldE, DeltaE.EwaldE + CreateMolE.EwaldE - FinalE.EwaldE);
    printf("Drift in Tail Energy: %.5f\n", DeltaE.TailE + CreateMolE.TailE - FinalE.TailE);
    printf("Drift in DNN Energy:  %.5f\n", DeltaE.DNN_E + CreateMolE.DNN_E - FinalE.DNN_E);
    
    printf("================================================================================\n");

    printf("DNN Rejection Summary:\nTranslation+Rotation: %zu\nReinsertion: %zu\nInsertion: %zu\nDeletion: %zu\nSingleSwap: %zu\n", SystemComponents[i].TranslationRotationDNNReject, SystemComponents[i].ReinsertionDNNReject, SystemComponents[i].InsertionDNNReject, SystemComponents[i].DeletionDNNReject, SystemComponents[i].SingleSwapDNNReject);
    printf("DNN Drift Summary:\nTranslation+Rotation: %.5f\nReinsertion: %.5f\nInsertion: %.5f\nDeletion: %.5f\nSingleSwap: %.5f\n", SystemComponents[i].SingleMoveDNNDrift, SystemComponents[i].ReinsertionDNNDrift, SystemComponents[i].InsertionDNNDrift, SystemComponents[i].DeletionDNNDrift, SystemComponents[i].SingleSwapDNNDrift);
  }
}

inline void GenerateRestartMovies(int Cycle, std::vector<Components>& SystemComponents, Simulations*& Sims, ForceField& FF, std::vector<Boxsize>& Box, PseudoAtomDefinitions& PseudoAtom)
{
  
  size_t NumberOfSimulations = SystemComponents.size();
  for(size_t i = 0; i < NumberOfSimulations; i++)
  {
    printf("System %zu\n", i);
    Atoms device_System[SystemComponents[i].Total_Components];
    Copy_AtomData_from_Device(device_System, SystemComponents[i].HostSystem, Sims[i].d_a, SystemComponents[i]);

    create_movie_file(Cycle, SystemComponents[i].HostSystem, SystemComponents[i], FF, Box[i], PseudoAtom.Name, i);
    create_Restart_file(Cycle, SystemComponents[i].HostSystem, SystemComponents[i], FF, Box[i], PseudoAtom.Name, Sims[i].MaxTranslation, Sims[i].MaxRotation, i);
    Write_All_Adsorbate_data(Cycle, SystemComponents[i].HostSystem, SystemComponents[i], FF, Box[i], PseudoAtom.Name, i);
    Write_Lambda(Cycle, SystemComponents[i], i);
    Write_TMMC(Cycle, SystemComponents[i], i);
    //Print Number of Pseudo Atoms//
    for(size_t j = 0; j < SystemComponents[i].NumberOfPseudoAtoms.size(); j++) printf("PseudoAtom Type: %s[%zu], #: %zu\n", PseudoAtom.Name[j].c_str(), j, SystemComponents[i].NumberOfPseudoAtoms[j]);
  }
}
