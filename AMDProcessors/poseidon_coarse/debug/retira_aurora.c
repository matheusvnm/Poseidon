/* File that contains the variable declarations */
#include "aurora.h"
#include <stdio.h>

/* First function called. It initiallizes all the functions and variables used by AURORA */
void aurora_init(int metric, int start_search)
{
	int i, fd;
	char set[2];
	int numCores = sysconf(_SC_NPROCESSORS_ONLN);
	/*Initialization of RAPL */
	//aurora_detect_cpu();
	aurora_detect_packages();
	/*End initialization of RAPL */
	int startThreads = numCores;
	while(startThreads != 2 && startThreads != 3 && startThreads != 5){
	       startThreads = startThreads/2;
	}

	/* Initialization of the variables necessary to perform the search algorithm */
	for (i = 0; i < MAX_KERNEL; i++)
	{
		auroraKernels[i].numThreads = numCores;
		auroraKernels[i].startThreads = startThreads;
		auroraKernels[i].numCores = numCores;
		auroraKernels[i].initResult = 0.0;
		auroraKernels[i].state = START;
    auroraKernels[i].auroraMetric = metric;
		auroraKernels[i].timeTurboOff = 0.0;
		auroraKernels[i].timeTurboOn = 0.0;
		auroraKernels[i].idSeq = -1;
		auroraKernels[i].totalTime = 0.0;
		auroraKernels[i].totalEnergy = 0.0;
		idKernels[i] = 0;
	}

	/* Start the counters for energy and time for all the application execution */
	id_actual_region = MAX_KERNEL - 1;
	aurora_start_amd_msr();
	initGlobalTime = omp_get_wtime();
}

/* It defines the number of threads that will execute the actual region based on the current state of the search algorithm */
int aurora_resolve_num_threads(uintptr_t ptr_region){
	int i;
	double result=0, time=0, energy=0;
	
	//matheus, para tirar dado de tempo e energia, descomentar abaixo.
	if(auoraKernels[id_previous_region].state = END){
		auroraKernels[id_previous_region].totalTime += omp_get_wtime() - auroraKernels[id_previous_region].initResult;
		energy = aurora_end_amd_msr();
		auroraKernels[id_previous_region].totalEnergy += energy;		
	}
	
	id_actual_region = -1;
	for (i = 0; i < totalKernels; i++){
        	if (idKernels[i] == ptr_region){
            		id_actual_region = i;
            		break;
        	}
    	}
	/* If a new region of interest is found */
    	if (id_actual_region == -1){
            	idKernels[totalKernels] = ptr_region;
            	id_actual_region = totalKernels;
            	//auroraKernels[id_actual_region].idSeq = id_actual_region + 1;
            	totalKernels++;
    	}


	//it means that there is no previous region. So it will start the learning algorithm with R1
	if(id_previous_region == -1){
	        auroraKernels[id_actual_region].state = REPEAT;
	        id_previous_region = id_actual_region;
	        auroraKernels[id_actual_region].initResult = omp_get_wtime();
	        aurora_start_amd_msr();
	        return auroraKernels[id_actual_region].numCores;
	}
	//this is not the first region in the code.
	else{
		if(auroraKernels[id_previous_region].state != END && auroraKernels[id_actual_region].state != START){
			switch(auroraKernels[id_previous_region].auroraMetric){
				case PERFORMANCE:
					result = omp_get_wtime() - auroraKernels[id_previous_region].initResult;
					time = result;
					auroraKernels[id_previous_region].totalTime += time;
					break;
				case EDP:
					time = omp_get_wtime() - auroraKernels[id_previous_region].initResult;
					auroraKernels[id_previous_region].totalTime +=time;
					energy = aurora_end_amd_msr();
					auroraKernels[id_previous_region].totalEnergy += energy;
					result = time * energy;
					if(result == 0.00000000 || result < 0){
	                            		auroraKernels[id_previous_region].state = REPEAT;
	                            		auroraKernels[id_previous_region].auroraMetric = PERFORMANCE;
	                        	}
	                        	break;
			}
			switch(auroraKernels[id_previous_region].state){
		case REPEAT:
                                auroraKernels[id_previous_region].state = S0;
                                auroraKernels[id_previous_region].numThreads = auroraKernels[id_previous_region].startThreads; 
                                auroraKernels[id_previous_region].lastThread = auroraKernels[id_previous_region].numThreads;
                                break;
                        case S0:
                                auroraKernels[id_previous_region].bestResult = result;
                                auroraKernels[id_previous_region].bestThread = auroraKernels[id_previous_region].numThreads;
                                auroraKernels[id_previous_region].numThreads = auroraKernels[id_previous_region].bestThread*2; 
                                auroraKernels[id_previous_region].state = S1;
                                break;
                        case S1:
                                if(result < auroraKernels[id_previous_region].bestResult){ //comparing S0 to REPEAT
                                        auroraKernels[id_previous_region].bestResult = result;
                                        auroraKernels[id_previous_region].bestThread = auroraKernels[id_previous_region].numThreads;
                                        /* if there are opportunities for improvements, then double the number of threads */
                                        if(auroraKernels[id_previous_region].numThreads * 2 <= auroraKernels[id_previous_region].numCores){
                                                auroraKernels[id_previous_region].lastThread = auroraKernels[id_previous_region].numThreads;
                                                auroraKernels[id_previous_region].numThreads = auroraKernels[id_previous_region].bestThread*2;
                                                auroraKernels[id_previous_region].state = S1;
                                        }else{
                                                /* It means that the best number so far is equal to the number of cores */
                                                /* Then, it will realize a guided search near to this number */
                                                auroraKernels[id_previous_region].pass = auroraKernels[id_previous_region].lastThread/2;                                                
                                                auroraKernels[id_previous_region].numThreads = auroraKernels[id_previous_region].numThreads  - auroraKernels[id_previous_region].pass;
                                                if(auroraKernels[id_previous_region].pass == 1)
                                                        auroraKernels[id_previous_region].state = S3;        
						else
	                                                auroraKernels[id_previous_region].state = S2;
                                        }
                                }else{ 
                                        /* Thread scalability stopped */
                                        /* Find the interval of threads that provided this result. */
                                        /* if the best number of threads so far is equal to the number of cores, then go to.. */
                                        if(auroraKernels[id_previous_region].bestThread == auroraKernels[id_previous_region].numCores/2){
                                                auroraKernels[id_previous_region].pass = auroraKernels[id_previous_region].lastThread/2;                                                
                                                auroraKernels[id_previous_region].numThreads = auroraKernels[id_previous_region].numThreads  - auroraKernels[id_previous_region].pass;
                                                if(auroraKernels[id_previous_region].pass == 1)
                                                        auroraKernels[id_previous_region].state = S3;        
        					else
		                                        auroraKernels[id_previous_region].state = S2;
                                        }else{
                                                auroraKernels[id_previous_region].pass = auroraKernels[id_previous_region].lastThread/2;                                                
                                                auroraKernels[id_previous_region].numThreads = auroraKernels[id_previous_region].numThreads  + auroraKernels[id_previous_region].pass;
                                                if(auroraKernels[id_previous_region].pass == 1)
                                                        auroraKernels[id_previous_region].state = S3;        
                                                else
														auroraKernels[id_previous_region].state = S2;  
                                        }

                                }
                                break;
                        case S2:        
                                if(auroraKernels[id_previous_region].bestResult < result){
                                        auroraKernels[id_previous_region].pass = auroraKernels[id_previous_region].pass/2;
                                        auroraKernels[id_previous_region].numThreads = auroraKernels[id_previous_region].numThreads + auroraKernels[id_previous_region].pass;
                                        if(auroraKernels[id_previous_region].pass == 1)
                                                auroraKernels[id_previous_region].state = S3;
                                        else
                                                auroraKernels[id_previous_region].state = S2;
                                }else{
                                        auroraKernels[id_previous_region].bestThread = auroraKernels[id_previous_region].numThreads;
                                        auroraKernels[id_previous_region].bestResult = result;
                                        auroraKernels[id_previous_region].pass = auroraKernels[id_previous_region].pass/2;
                                        auroraKernels[id_previous_region].numThreads = auroraKernels[id_previous_region].numThreads + auroraKernels[id_previous_region].pass;
                                        if(auroraKernels[id_previous_region].pass == 1)
                                                auroraKernels[id_previous_region].state = S3;
                                        else
                                                auroraKernels[id_previous_region].state = S2;
                                }
                                break;                        
                        case S3: //The last comparison to define the best number of threads
                                if(result < auroraKernels[id_previous_region].bestResult){
                                        auroraKernels[id_previous_region].bestThread = auroraKernels[id_previous_region].numThreads;
                                        auroraKernels[id_previous_region].state = END;
                                }else{
                                        auroraKernels[id_previous_region].state = END;
                                }
                                break;
				
                }
	}

	//sets the configuration for the current region.
	switch(auroraKernels[id_actual_region].state){
		case END:
			      		auroraKernels[id_actual_region].initResult = omp_get_wtime();
            		aurora_start_amd_msr();
        	    	id_previous_region = id_actual_region;
        	    	return auroraKernels[id_actual_region].bestThread;
	  case START:
          		  auroraKernels[id_actual_region].state = REPEAT;
           		  auroraKernels[id_actual_region].initResult = omp_get_wtime();
           		  aurora_start_amd_msr();
            		id_previous_region = id_actual_region;
            		return auroraKernels[id_actual_region].numCores;
    default:
            		auroraKernels[id_actual_region].initResult = omp_get_wtime();
            		aurora_start_amd_msr();
            		id_previous_region = id_actual_region;
            		return auroraKernels[id_actual_region].numThreads;
	}
    

}

/* It finalizes the environment of Aurora */
void aurora_destructor()
{
	double time = omp_get_wtime() - initGlobalTime;
	id_actual_region = MAX_KERNEL - 1;
	double energy = aurora_end_amd_msr();
        float edp = time * energy;
	printf("Poseidon - Execution Time: %.5f seconds\n", time);
	printf("Poseidon - Energy: %.5f joules\n", energy);
	printf("Poseidon - EDP: %.5f\n", edp);

	for(int i=0; i<totalKernels; i++){
		printf("%d %d %d %lf %lf\n", i, auroraKernels[i].bestThread, auroraKernels[i].bestFreq, auroraKernels[i].totalTime, auroraKernels[i].totalEnergy);
	}
}

void aurora_detect_packages()
{

	char filename[STRING_BUFFER];
	FILE *fff;
	int package;
	int i;

	for (i = 0; i < MAX_PACKAGES; i++)
		package_map[i] = -1;

	for (i = 0; i < MAX_CPUS; i++)
	{
		sprintf(filename, "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", i);
		fff = fopen(filename, "r");
		if (fff == NULL)
			break;
		fscanf(fff, "%d", &package);
		fclose(fff);

		if (package_map[package] == -1)
		{
			auroraTotalPackages++;
			package_map[package] = i;
		}
	}
}

void aurora_start_amd_msr()
{
	char msr_filename[STRING_BUFFER];
	int fd;
	sprintf(msr_filename, "/dev/cpu/0/msr");
	fd = open(msr_filename, O_RDONLY);
	if (fd < 0)
	{
		if (errno == ENXIO)
		{
			fprintf(stderr, "rdmsr: No CPU 0\n");
			exit(2);
		}
		else if (errno == EIO)
		{
			fprintf(stderr, "rdmsr: CPU 0 doesn't support MSRs\n");
			exit(3);
		}
		else
		{
			perror("rdmsr:open");
			fprintf(stderr, "Trying to open %s\n", msr_filename);
			exit(127);
		}
	}
	uint64_t data;
	pread(fd, &data, sizeof data, AMD_MSR_PACKAGE_ENERGY);
	//auroraKernels[id_actual_region].kernelBefore[0] = read_msr(fd, AMD_MSR_PACKAGE_ENERGY);
	close(fd);
	auroraKernels[id_actual_region].kernelBefore[0] = (long long)data;
}

double aurora_end_amd_msr()
{
	char msr_filename[STRING_BUFFER];
	int fd;
	sprintf(msr_filename, "/dev/cpu/0/msr");
	fd = open(msr_filename, O_RDONLY);
	uint64_t data;
	pread(fd, &data, sizeof data, AMD_MSR_PWR_UNIT);
	int core_energy_units = (long long)data;
	unsigned int energy_unit = (core_energy_units & AMD_ENERGY_UNIT_MASK) >> 8;
	pread(fd, &data, sizeof data, AMD_MSR_PACKAGE_ENERGY);
	auroraKernels[id_actual_region].kernelAfter[0] = (long long)data;
	close(fd);
	double result = (auroraKernels[id_actual_region].kernelAfter[0] - auroraKernels[id_actual_region].kernelBefore[0]) * pow(0.5, (float)(energy_unit));
	return result;
}
