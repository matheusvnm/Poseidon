/* File that contains the variable declarations */
#include "poseidon.h"
#include <stdio.h>

/* First function called. It initiallizes all the functions and variables used by AURORA */
void lib_init(int metric, int start_search)
{
	int i, fd;
	char set[2];
	int numCores = sysconf(_SC_NPROCESSORS_ONLN);
	/*Initialization of RAPL */
	//lib_detect_cpu();
	lib_detect_packages();
	/*End initialization of RAPL */
	int startThreads = numCores;
	while (startThreads != 2 && startThreads != 3 && startThreads != 5)
	{
		startThreads = startThreads / 2;
	}

	/* Initialization of the variables necessary to perform the search algorithm */
	for (i = 0; i < MAX_KERNEL; i++)
	{
		libKernels[i].numThreads = numCores;
		libKernels[i].startThreads = startThreads;
		libKernels[i].numCores = numCores;
		libKernels[i].initResult = 0.0;
		libKernels[i].state = START;
		libKernels[i].metric = metric;
		libKernels[i].bestFreq = TURBO_OFF;
		libKernels[i].timeTurboOff = 0.0;
		libKernels[i].timeTurboOn = 0.0;
		libKernels[i].idSeq = -1;
		libKernels[i].totalTime = 0.0;
		libKernels[i].totalEnergy = 0.0;
		idKernels[i] = 0;
	}

	/* Start the counters for energy and time for all the application execution */
	id_actual_region = MAX_KERNEL - 1;
	lib_start_amd_msr();
	initGlobalTime = omp_get_wtime();

	if (metric == EDP)
	{
		fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
		sprintf(set, "%d", TURBO_OFF);
		write(fd, set, sizeof(set));
		close(fd);
		write_file_threshold = 0.00074;
	}
}

/* It defines the number of threads that will execute the actual region based on the current state of the search algorithm */
int lib_resolve_num_threads(uintptr_t ptr_region)
{
	int i, fd;
	char set[2];
	double result = 0, time = 0, energy = 0;

	//Retirar dados de energia por região
	/*	if(libKernels[id_previous_region].state = EDP){
		libKernels[id_previous_region].totalTime += omp_get_wtime() - libKernels[id_previous_region].initResult;
		energy = lib_end_amd_msr();
		libKernels[id_previous_region].totalEnergy += energy;		
	} */

	id_actual_region = -1;
	for (i = 0; i < totalKernels; i++)
	{
		if (idKernels[i] == ptr_region)
		{
			id_actual_region = i;
			break;
		}
	}
	/* If a new region of interest is found */
	if (id_actual_region == -1)
	{
		idKernels[totalKernels] = ptr_region;
		id_actual_region = totalKernels;
		//libKernels[id_actual_region].idSeq = id_actual_region + 1;
		totalKernels++;
	}

	if (id_previous_region == -1)
	{
		libKernels[id_actual_region].state = REPEAT;
		id_previous_region = id_actual_region;
		libKernels[id_actual_region].initResult = omp_get_wtime();
		lib_start_amd_msr();
		return libKernels[id_actual_region].numCores;
	}
	else
	{
		if (libKernels[id_previous_region].state != END && libKernels[id_actual_region].state != START)
		{
			switch (libKernels[id_previous_region].metric)
			{
			case PERFORMANCE:
				result = omp_get_wtime() - libKernels[id_previous_region].initResult;
				time = result;
				libKernels[id_previous_region].totalTime += time;
				break;
			case EDP:
				time = omp_get_wtime() - libKernels[id_previous_region].initResult;
				libKernels[id_previous_region].totalTime += time;
				energy = lib_end_amd_msr();
				libKernels[id_previous_region].totalEnergy += energy;
				result = time * energy;
				if (result == 0.00000000 || result < 0)
				{
					libKernels[id_previous_region].state = REPEAT;
					libKernels[id_previous_region].metric = PERFORMANCE;
				}
				break;
			}
			switch (libKernels[id_previous_region].state)
			{
			case REPEAT:
				libKernels[id_previous_region].state = S0;
				libKernels[id_previous_region].numThreads = libKernels[id_previous_region].startThreads;
				libKernels[id_previous_region].lastThread = libKernels[id_previous_region].numThreads;
				break;
			case S0:
				libKernels[id_previous_region].bestResult = result;
				libKernels[id_previous_region].bestTime = time;
				libKernels[id_previous_region].bestThread = libKernels[id_previous_region].numThreads;
				libKernels[id_previous_region].numThreads = libKernels[id_previous_region].numThreads * 2;
				libKernels[id_previous_region].state = S1;
				break;
			case S1:
				if (result < libKernels[id_previous_region].bestResult)
				{
					libKernels[id_previous_region].bestResult = result;
					libKernels[id_previous_region].bestTime = time;
					libKernels[id_previous_region].bestThread = libKernels[id_previous_region].numThreads;
					if (libKernels[id_previous_region].numThreads * 2 <= libKernels[id_previous_region].numCores)
					{
						libKernels[id_previous_region].lastThread = libKernels[id_previous_region].numThreads;
						libKernels[id_previous_region].numThreads = libKernels[id_previous_region].numThreads * 2;
					}
					else
					{
						libKernels[id_previous_region].pass = libKernels[id_previous_region].lastThread / 2;
						if (libKernels[id_previous_region].pass >= 2)
						{
							libKernels[id_previous_region].numThreads = libKernels[id_previous_region].numThreads - libKernels[id_previous_region].pass;
							libKernels[id_previous_region].state = S2;
						}
						else
						{
							libKernels[id_previous_region].bestFreq = TURBO_ON; //testar com turbo off;
							libKernels[id_previous_region].timeTurboOff = time;
							libKernels[id_previous_region].state = END_THREADS;
						}
					}
				}
				else
				{
					if (libKernels[id_previous_region].bestThread == libKernels[id_previous_region].numCores / 2)
					{
						libKernels[id_previous_region].bestFreq = TURBO_ON;
						libKernels[id_previous_region].timeTurboOff = time;
						libKernels[id_previous_region].state = END_THREADS;
					}
					else
					{
						libKernels[id_previous_region].pass = libKernels[id_previous_region].lastThread / 2;
						if (libKernels[id_previous_region].pass >= 2)
						{
							libKernels[id_previous_region].numThreads = libKernels[id_previous_region].numThreads + libKernels[id_previous_region].pass;
							libKernels[id_previous_region].state = S2;
						}
						else
						{
							libKernels[id_previous_region].bestFreq = TURBO_ON;
							libKernels[id_previous_region].timeTurboOff = time;
							libKernels[id_previous_region].state = END_THREADS;
						}
					}
				}
				break;
			case S2:
				if (libKernels[id_previous_region].bestResult < result)
				{
					libKernels[id_previous_region].pass = libKernels[id_previous_region].pass / 2;
					if (libKernels[id_previous_region].pass >= 2)
					{
						libKernels[id_previous_region].numThreads = libKernels[id_previous_region].numThreads + libKernels[id_previous_region].pass;
					}
					else
					{
						libKernels[id_previous_region].bestFreq = TURBO_ON;
						libKernels[id_previous_region].timeTurboOff = time;
						libKernels[id_previous_region].state = END_THREADS;
					}
				}
				else
				{
					libKernels[id_previous_region].bestThread = libKernels[id_previous_region].numThreads;
					libKernels[id_previous_region].bestTime = time;
					libKernels[id_previous_region].bestResult = result;
					libKernels[id_previous_region].pass = libKernels[id_previous_region].pass / 2;
					if (libKernels[id_previous_region].pass >= 2)
					{
						libKernels[id_previous_region].numThreads += libKernels[id_previous_region].pass;
					}
					else
					{
						libKernels[id_previous_region].bestFreq = TURBO_ON;
						libKernels[id_previous_region].timeTurboOff = time;
						libKernels[id_previous_region].state = END_THREADS;
					}
				}
				break;
			case END_THREADS:
				libKernels[id_previous_region].state = END;
				libKernels[id_previous_region].timeTurboOn = time;
				//Arthur: tive que fazer isso para garantir que se fosse turbo off, ele voltasse para o off...
				if (libKernels[id_previous_region].bestResult < result)
				{
					libKernels[id_previous_region].bestFreq = TURBO_OFF;
					fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
					sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
					write(fd, set, sizeof(set));
					close(fd);
				}
				break;
			}
		}
	}

	// Sets the configuration for the current region.
	switch (libKernels[id_actual_region].state)
	{
	case END:
		if ((libKernels[id_previous_region].bestFreq == TURBO_OFF && libKernels[id_actual_region].bestFreq == TURBO_ON && (libKernels[id_actual_region].timeTurboOn + write_file_threshold < libKernels[id_actual_region].timeTurboOff)) || (libKernels[id_previous_region].bestFreq == TURBO_ON && libKernels[id_actual_region].bestFreq == TURBO_OFF && (libKernels[id_actual_region].timeTurboOff + write_file_threshold < libKernels[id_actual_region].timeTurboOn)))
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
		}
		///Retirar dados de energia por região
		/*	libKernels[id_actual_region].initResult = omp_get_wtime();
			lib_start_amd_msr();
		*/
		id_previous_region = id_actual_region;
		return libKernels[id_actual_region].bestThread;
	case END_THREADS:
		if (libKernels[id_actual_region].bestFreq != libKernels[id_previous_region].bestFreq && libKernels[id_actual_region].timeTurboOff > write_file_threshold)
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
		}
		libKernels[id_actual_region].initResult = omp_get_wtime();
		lib_start_amd_msr();
		id_previous_region = id_actual_region;
		return libKernels[id_actual_region].bestThread;
	case START:
		libKernels[id_actual_region].state = REPEAT;
		if (libKernels[id_actual_region].bestFreq != libKernels[id_previous_region].bestFreq)
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
		}
		libKernels[id_actual_region].initResult = omp_get_wtime();
		lib_start_amd_msr();
		id_previous_region = id_actual_region;
		return libKernels[id_actual_region].numCores;
	default:
		if (libKernels[id_actual_region].bestFreq != libKernels[id_previous_region].bestFreq)
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
		}
		libKernels[id_actual_region].initResult = omp_get_wtime();
		lib_start_amd_msr();
		id_previous_region = id_actual_region;
		return libKernels[id_actual_region].numThreads;
	}
}

/* It finalizes the environment of Aurora */
void lib_destructor()
{
	double time = omp_get_wtime() - initGlobalTime;
	id_actual_region = MAX_KERNEL - 1;
	double energy = lib_end_amd_msr();
	float edp = time * energy;
	printf("Poseidon - Execution Time: %.5f seconds\n", time);
	printf("Poseidon - Energy: %.5f joules\n", energy);
	printf("Poseidon - EDP: %.5f\n", edp);

	//for (int i = 0; i < totalKernels; i++)
	//{
	//	printf("%d %d %d %lf %lf\n", i, libKernels[i].bestThread, libKernels[i].bestFreq, libKernels[i].totalTime, libKernels[i].totalEnergy);
	//}
}

void lib_detect_packages()
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
			libTotalPackages++;
			package_map[package] = i;
		}
	}
}

void lib_start_amd_msr()
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
	//libKernels[id_actual_region].kernelBefore[0] = read_msr(fd, AMD_MSR_PACKAGE_ENERGY);
	close(fd);
	libKernels[id_actual_region].kernelBefore[0] = (long long)data;
}

double lib_end_amd_msr()
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
	libKernels[id_actual_region].kernelAfter[0] = (long long)data;
	close(fd);
	double result = (libKernels[id_actual_region].kernelAfter[0] - libKernels[id_actual_region].kernelBefore[0]) * pow(0.5, (float)(energy_unit));
	return result;
}
