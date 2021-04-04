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
		auroraKernels[i].state = REPEAT;
                auroraKernels[i].auroraMetric = metric;
		auroraKernels[i].seqMetric = metric;
		auroraKernels[i].bestFreq = TURBO_OFF;
                auroraKernels[i].bestFreqSeq = TURBO_OFF;
		auroraKernels[i].timeTurboOff = 0.0;
		auroraKernels[i].timeTurboOn = 0.0;
		auroraKernels[i].totalTime = 0.0;
		auroraKernels[i].totalEnergy = 0.0;
		auroraKernels[i].totalTimeSeq = 0.0;
		auroraKernels[i].totalEnergySeq = 0.0;
                auroraKernels[i].seqState = PASS;
		idKernels[i] = 0;
	}

	/* Start the counters for energy and time for all the application execution */
	id_actual_region = MAX_KERNEL - 1;
	aurora_start_amd_msr();
	initGlobalTime = omp_get_wtime();

	/* Find the cost of writing the turbo file. Also activates Turbo Core in the first iteration */
	if (metric == EDP)
	{
		sprintf(set, "%d", TURBO_OFF);
		fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
		write(fd, set, sizeof(set));
		close(fd);      	
        	write_file_threshold = 0.00074;
	}
}	
	

/* It defines the number of threads that will execute the actual parallel region based on the current state of the search algorithm */
int aurora_resolve_num_threads(uintptr_t ptr_region)
{
	int i, fd;
	char set[2];
	double time=0, energy=0, result=0;
	id_actual_region = -1;

	if(auroraKernels[id_previous_region].seqState == END_SEQUENTIAL){
		time = omp_get_wtime() - initSeqTime;
                auroraKernels[id_previous_region].totalTimeSeq += time;
		energy = aurora_end_amd_msr();
		auroraKernels[id_previous_region].totalEnergySeq += energy;
	}

	
        
        if(auroraKernels[id_previous_region].seqState != END_SEQUENTIAL && auroraKernels[id_previous_region].seqState != PASS){
                switch(auroraKernels[id_previous_region].seqMetric){
                                case PERFORMANCE:
                                        result = omp_get_wtime() - initSeqTime;
                                        time = result;
					auroraKernels[id_previous_region].totalTimeSeq += time;
                                        break;
                                case EDP:
                                        time = omp_get_wtime() - initSeqTime;
					auroraKernels[id_previous_region].totalTimeSeq += time;
                                        energy = aurora_end_amd_msr();
					auroraKernels[id_previous_region].totalEnergySeq += energy;
                                        result = time * energy;
                                        /* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
                                        if(result == 0.00000 || result < 0){
                                                auroraKernels[id_previous_region].seqState = PASS;
                                                auroraKernels[id_previous_region].seqMetric = PERFORMANCE;
                                                auroraKernels[id_previous_region].bestFreqSeq = TURBO_ON;
                                        }
                                        break;
                        }

                switch (auroraKernels[id_previous_region].seqState)
                {
                        case INITIAL:
                                auroraKernels[id_previous_region].timeSeqTurboOff = time;
                                auroraKernels[id_previous_region].resultSeqTurboOff = result;
                                auroraKernels[id_previous_region].bestFreqSeq = TURBO_ON;
                                auroraKernels[id_previous_region].seqState = END_TURBO;
                                break;
                        case END_TURBO:
                                auroraKernels[id_previous_region].timeSeqTurboOn = time;
                                auroraKernels[id_previous_region].resultSeqTurboOn = result;
                                auroraKernels[id_previous_region].seqState = END_SEQUENTIAL;
                                if(auroraKernels[id_previous_region].resultSeqTurboOff < auroraKernels[id_previous_region].resultSeqTurboOn){
                                        auroraKernels[id_previous_region].bestFreqSeq = TURBO_OFF;
                                }
                                break;
                }
        }


	/* Find the actual parallel region */
	for (i = 0; i < totalKernels; i++)
	{
		if (idKernels[i] == ptr_region)
		{
			id_actual_region = i;
			break;
		}
	}

	/* If a new parallel region is discovered */
	if (id_actual_region == -1)
	{
		idKernels[totalKernels] = ptr_region;
		id_actual_region = totalKernels;
		totalKernels++;
	}
	
	/* Check the state of the search algorithm. */
	switch (auroraKernels[id_actual_region].state)
	{
	case END:
		aurora_start_amd_msr();
		auroraKernels[id_actual_region].initResult = omp_get_wtime();
                if((auroraKernels[id_previous_region].bestFreqSeq == TURBO_OFF && auroraKernels[id_actual_region].bestFreq == TURBO_ON && (auroraKernels[id_actual_region].timeTurboOn + write_file_threshold < auroraKernels[id_actual_region].timeTurboOff)) || (auroraKernels[id_previous_region].bestFreqSeq == TURBO_ON && auroraKernels[id_actual_region].bestFreq == TURBO_OFF && (auroraKernels[id_actual_region].timeTurboOff + write_file_threshold < auroraKernels[id_actual_region].timeTurboOn))){
                        fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", auroraKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
                }
		return auroraKernels[id_actual_region].bestThreadOn;
		break;
	case END_THREADS:
		aurora_start_amd_msr();
		auroraKernels[id_actual_region].initResult = omp_get_wtime();
		if (auroraKernels[id_actual_region].bestTime > write_file_threshold)
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", auroraKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
		}
		return auroraKernels[id_actual_region].bestThreadOn;
		break;
	default:
		aurora_start_amd_msr();
		auroraKernels[id_actual_region].initResult = omp_get_wtime();
		if (auroraKernels[id_actual_region].bestTime > write_file_threshold)
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", auroraKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
		}
		return auroraKernels[id_actual_region].numThreads;
	}
}

// aurora_end_amd_msr();
/* It is responsible for performing the search algorithm */
void aurora_end_parallel_region(){
        double time=0, energy=0, result=0;
	int fd;
	char set[2];

	if(auroraKernels[id_actual_region].state == END){
		time = omp_get_wtime() - auroraKernels[id_actual_region].initResult;
                auroraKernels[id_actual_region].totalTime += time;
		energy = aurora_end_amd_msr();
		auroraKernels[id_actual_region].totalEnergy += energy;
	}


        if(auroraKernels[id_actual_region].state !=END){
                /* Check the auroraMetric that is being evaluated and collect the results */
                switch(auroraKernels[id_actual_region].auroraMetric){
                        case PERFORMANCE:
				//printf("case Performance\n");
                                result = omp_get_wtime() - auroraKernels[id_actual_region].initResult;
				time = result;
				auroraKernels[id_actual_region].totalTime += time;
                                break;
                        case EDP:
				//printf("case EDP\n");
                                time = omp_get_wtime() - auroraKernels[id_actual_region].initResult;
				auroraKernels[id_actual_region].totalTime += time;
                                energy = aurora_end_amd_msr();
				auroraKernels[id_actual_region].totalEnergy += energy;
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
				auroraKernels[id_actual_region].lastThread = auroraKernels[id_actual_region].numThreads; 
				break;
			case S0:
				auroraKernels[id_actual_region].bestResult = result;
				auroraKernels[id_actual_region].bestTime = time;
				auroraKernels[id_actual_region].bestThreadOn = auroraKernels[id_actual_region].numThreads;
				auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads*2;
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
					}
					else{
						auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].lastThread/2;
						if(auroraKernels[id_actual_region].pass >= 2){
							auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads - auroraKernels[id_actual_region].pass;
							auroraKernels[id_actual_region].state = S2;
						}else{
							auroraKernels[id_actual_region].bestFreq = TURBO_ON; //testar com turbo off;
							auroraKernels[id_actual_region].timeTurboOff = time;
							auroraKernels[id_actual_region].state = END_THREADS;
						}

					}
				}else{
					if(auroraKernels[id_actual_region].bestThreadOn == auroraKernels[id_actual_region].numCores/2){
							auroraKernels[id_actual_region].bestFreq = TURBO_ON;
							auroraKernels[id_actual_region].timeTurboOff = time;
							auroraKernels[id_actual_region].state = END_THREADS;
					}else{
						auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].lastThread/2;
						if(auroraKernels[id_actual_region].pass >= 2){
							auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads + auroraKernels[id_actual_region].pass;
							auroraKernels[id_actual_region].state = S2;
						}else{
							auroraKernels[id_actual_region].bestFreq = TURBO_ON;
							auroraKernels[id_actual_region].timeTurboOff = time;
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
					}
					else{
						auroraKernels[id_actual_region].bestFreq = TURBO_ON;
						auroraKernels[id_actual_region].timeTurboOff = time;
						auroraKernels[id_actual_region].state = END_THREADS;
					}
				}else{
					auroraKernels[id_actual_region].bestThreadOn = auroraKernels[id_actual_region].numThreads;
					auroraKernels[id_actual_region].bestTime = time;
					auroraKernels[id_actual_region].bestResult = result;
					auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].pass/2;
					if(auroraKernels[id_actual_region].pass >= 2){
						auroraKernels[id_actual_region].numThreads += auroraKernels[id_actual_region].pass;
					}else{
						auroraKernels[id_actual_region].bestFreq = TURBO_ON;
						auroraKernels[id_actual_region].timeTurboOff = time;
						auroraKernels[id_actual_region].state = END_THREADS;
					}
				}
				break;
			case END_THREADS:
				auroraKernels[id_actual_region].state = END;
				auroraKernels[id_actual_region].timeTurboOn = time;
				if(auroraKernels[id_actual_region].bestResult < result)
					auroraKernels[id_actual_region].bestFreq = TURBO_OFF;

                       		break;
		}

        }

	switch(auroraKernels[id_actual_region].seqState){
                case PASS:
			auroraKernels[id_actual_region].seqState = INITIAL;
			if (auroraKernels[id_actual_region].bestFreq != auroraKernels[id_actual_region].bestFreqSeq){
                		fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
        			sprintf(set, "%d", auroraKernels[id_actual_region].bestFreqSeq);
	        		write(fd, set, sizeof(set));
	        		close(fd);
			}
			initSeqTime = omp_get_wtime();
			aurora_start_amd_msr();
			break;
		case END_TURBO:
			if (auroraKernels[id_actual_region].bestFreq != auroraKernels[id_actual_region].bestFreqSeq && write_file_threshold < auroraKernels[id_actual_region].timeSeqTurboOff){
                		fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
        			sprintf(set, "%d", auroraKernels[id_actual_region].bestFreqSeq);
	        		write(fd, set, sizeof(set));
	        		close(fd);
			}
			initSeqTime = omp_get_wtime();
			aurora_start_amd_msr();
			break;
		case END_SEQUENTIAL:
			if((auroraKernels[id_actual_region].bestFreq == TURBO_OFF && auroraKernels[id_actual_region].bestFreqSeq == TURBO_ON && (auroraKernels[id_actual_region].timeSeqTurboOn + write_file_threshold < auroraKernels[id_actual_region].timeSeqTurboOff)) || (auroraKernels[id_previous_region].bestFreqSeq == TURBO_ON && auroraKernels[id_actual_region].bestFreq == TURBO_OFF && (auroraKernels[id_actual_region].timeSeqTurboOff + write_file_threshold < auroraKernels[id_actual_region].timeSeqTurboOn))){
                        	fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
				sprintf(set, "%d", auroraKernels[id_actual_region].bestFreqSeq);
				write(fd, set, sizeof(set));
				close(fd);
                	}
			initSeqTime = omp_get_wtime();
			aurora_start_amd_msr();
                        break;
		
	}
	id_previous_region = id_actual_region;
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
		printf("%d %d %d %lf %lf\n", i, auroraKernels[i].bestThreadOn, auroraKernels[i].bestFreq, auroraKernels[i].totalTime, auroraKernels[i].totalEnergy);
		printf("%d %d %d %lf %lf\n", i+1, 1 , auroraKernels[i].bestFreqSeq, auroraKernels[i].totalTime, auroraKernels[i].totalEnergy);
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
