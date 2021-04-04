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

	/* Initialization of the variables necessary to perform the search algorithm */
	for (i = 0; i < MAX_KERNEL; i++)
	{
		auroraKernels[i].numThreads = numCores;
		auroraKernels[i].startThreads = 2;
		auroraKernels[i].numCores = numCores;
		auroraKernels[i].initResult = 0.0;
		auroraKernels[i].state = REPEAT;
                auroraKernels[i].auroraMetric = metric;
		auroraKernels[i].bestFreq = TURBO_ON;
		auroraKernels[i].timeTurboOff = 0.0;
		auroraKernels[i].timeTurboOn = 0.0;
		auroraKernels[i].idSeq = -1;
		idKernels[i] = 0;
	}

	/* Start the counters for energy and time for all the application execution */
	id_actual_region = MAX_KERNEL - 1;
	aurora_start_amd_msr();
	initGlobalTime = omp_get_wtime();
	
	fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
        sprintf(set, "%d", TURBO_ON);
	write(fd, set, sizeof(set));
	close(fd);
        write_file_threshold = 0.00074;
}

/* It defines the number of threads that will execute the actual region based on the current state of the search algorithm */
int aurora_resolve_num_threads(uintptr_t ptr_region){
        double time=0, energy=0, result=0; 
        int i, fd;
	int var_thread=0;
	char set[2];
	id_actual_region = -1;

        /* Find the actual parallel region */
	for (i = 0; i < totalKernels; i++){
		if (idKernels[i] == ptr_region){
			id_actual_region = i;
			break;
		}
	}

	/* If a new parallel region is discovered */
	if (id_actual_region == -1){
		idKernels[totalKernels] = ptr_region;
		id_actual_region = totalKernels;
                auroraKernels[id_actual_region].idSeq = id_actual_region + 1;
		totalKernels++;
	}

        /* Informs the actual parallel region which was the previous parallel region and Informs the previous parallel region which is the next parallel region*/
	auroraKernels[id_actual_region].idParAnt = id_previous_region;
	auroraKernels[id_previous_region].idParPos = id_actual_region;


        //if(auroraKernels[id_actual_region].state != END && auroraKernels[id_actual_region].state != REPEAT){
	if(auroraKernels[id_actual_region].state != END){
                /* Check the auroraMetric that is being evaluated and collect the results */
                switch(auroraKernels[id_actual_region].auroraMetric){
                        case PERFORMANCE:
                                result = omp_get_wtime() - auroraKernels[id_actual_region].initResult;
				time = result;
                                break;
                        case EDP:
                                time = omp_get_wtime() - auroraKernels[id_actual_region].initResult;
                                energy = aurora_end_amd_msr();
                                result = time * energy;
                                /* If the result is negative, it means some problem while reading of the hardware counter. Then, the auroraMetric changes to performance */
                                if(result == 0.00000 || result < 0){
                                        auroraKernels[id_actual_region].state = REPEAT;
                                        auroraKernels[id_actual_region].auroraMetric = PERFORMANCE;
                                }
                                break;
                } 
                switch(auroraKernels[id_actual_region].state){
			case REPEAT:
				auroraKernels[id_actual_region].state = S0;
				auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].startThreads;
				var_thread = auroraKernels[id_actual_region].numThreads;
				auroraKernels[id_actual_region].lastThread = auroraKernels[id_actual_region].numThreads; 
				break;
			case S0:
				auroraKernels[id_actual_region].bestResult = result;
				auroraKernels[id_actual_region].bestTime = time;
				auroraKernels[id_actual_region].bestThreadOn = auroraKernels[id_actual_region].numThreads;
				auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads*2;
				var_thread = auroraKernels[id_actual_region].numThreads;
				auroraKernels[id_actual_region].state = S1;
				break;
			case S1:
				if(result < auroraKernels[id_actual_region].bestResult){
					auroraKernels[id_actual_region].bestResult = result;
					auroraKernels[id_actual_region].bestTime = time;
					auroraKernels[id_actual_region].bestThreadOn = auroraKernels[id_actual_region].numThreads;
					if(auroraKernels[id_actual_region].numThreads * 2 <= auroraKernels[id_actual_region].numCores){
						auroraKernels[id_actual_region].lastThread = auroraKernels[id_actual_region].numThreads;
						auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads*2;
						var_thread = auroraKernels[id_actual_region].numThreads;
					}
					else{
						auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].lastThread/2;
						if(auroraKernels[id_actual_region].pass >= 2){
							auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads - auroraKernels[id_actual_region].pass;
							var_thread = auroraKernels[id_actual_region].numThreads;
							auroraKernels[id_actual_region].state = S2;
						}else{
							auroraKernels[id_actual_region].bestFreq = TURBO_OFF; //testar com turbo off;
							auroraKernels[id_actual_region].timeTurboOn = time;
							auroraKernels[id_actual_region].state = END_THREADS;
						}

					}
				}else{
					if(auroraKernels[id_actual_region].bestThreadOn == auroraKernels[id_actual_region].numCores/2){
							auroraKernels[id_actual_region].bestFreq = TURBO_OFF;
							auroraKernels[id_actual_region].timeTurboOn = time;
							auroraKernels[id_actual_region].state = END_THREADS;
					}else{
						auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].lastThread/2;
						if(auroraKernels[id_actual_region].pass >= 2){
							auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads + auroraKernels[id_actual_region].pass;
							var_thread = auroraKernels[id_actual_region].numThreads; 
							auroraKernels[id_actual_region].state = S2;
						}else{
							auroraKernels[id_actual_region].bestFreq = TURBO_OFF;
							auroraKernels[id_actual_region].timeTurboOn = time;
							auroraKernels[id_actual_region].state = END_THREADS;
						}
					}
				}
				break;
			case S2:
				if(auroraKernels[id_actual_region].bestResult < result){
					auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].pass/2;
					if(auroraKernels[id_actual_region].pass >= 2){
						auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads + auroraKernels[id_actual_region].pass;
						var_thread = auroraKernels[id_actual_region].numThreads;
					}
					else{
						auroraKernels[id_actual_region].bestFreq = TURBO_OFF;
						auroraKernels[id_actual_region].timeTurboOn = time;
						auroraKernels[id_actual_region].state =  END_THREADS;
					}
				}else{
					auroraKernels[id_actual_region].bestThreadOn = auroraKernels[id_actual_region].numThreads;
					auroraKernels[id_actual_region].bestTime = time;
					auroraKernels[id_actual_region].bestResult = result;
					auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].pass/2;
					if(auroraKernels[id_actual_region].pass >= 2){
						auroraKernels[id_actual_region].numThreads += auroraKernels[id_actual_region].pass;
						var_thread = auroraKernels[id_actual_region].numThreads;
						
					}else{
						auroraKernels[id_actual_region].bestFreq = TURBO_OFF;
						auroraKernels[id_actual_region].timeTurboOn = time;
						auroraKernels[id_actual_region].state = END_THREADS;
					}
				}
				break; 
                        case END_THREADS:
				auroraKernels[id_actual_region].state = END;
				auroraKernels[id_actual_region].timeTurboOff = time;
				if(auroraKernels[id_actual_region].bestResult < result){
					auroraKernels[id_actual_region].bestFreq = TURBO_ON;
                        	}
                        	var_thread = auroraKernels[id_actual_region].bestThreadOn;
                        	break;
        	}
		
		if(auroraKernels[id_actual_region].state != END_THREADS && auroraKernels[id_actual_region].bestFreq != auroraKernels[id_previous_region].bestFreq && auroraKernels[id_actual_region].bestResult > write_file_threshold){
                	fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
                        sprintf(set, "%d", auroraKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
                }
		id_previous_region = id_actual_region;
		return var_thread;
	}else{
		if((auroraKernels[id_previous_region].bestFreq == TURBO_OFF && auroraKernels[id_actual_region].bestFreq == TURBO_ON && (auroraKernels[id_actual_region].timeTurboOn + write_file_threshold < auroraKernels[id_actual_region].timeTurboOff)) || (auroraKernels[id_previous_region].bestFreq == TURBO_ON && auroraKernels[id_actual_region].bestFreq == TURBO_OFF && (auroraKernels[id_actual_region].timeTurboOff + write_file_threshold < auroraKernels[id_actual_region].timeTurboOn))){
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", auroraKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
                }
		id_previous_region = id_actual_region;
		return auroraKernels[id_actual_region].bestThreadOn;
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
	double result = (auroraKernels[id_actual_region].kernelAfter[0] - auroraKernels[id_actual_region].kernelBefore[0]) * pow(0.5, (float)(energy_unit));
	return result;
}
