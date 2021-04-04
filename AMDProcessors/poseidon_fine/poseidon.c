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
		libKernels[i].state = REPEAT;
		libKernels[i].metric = metric;
		libKernels[i].seqMetric = metric;
		libKernels[i].bestFreq = TURBO_OFF;
		libKernels[i].bestFreqSeq = TURBO_OFF;
		libKernels[i].timeTurboOff = 0.0;
		libKernels[i].timeTurboOn = 0.0;
		libKernels[i].seqState = PASS;
		idKernels[i] = 0;
	}

	/* Start the counters for energy and time for all the application execution */
	id_actual_region = MAX_KERNEL - 1;
	lib_start_amd_msr();
	initGlobalTime = omp_get_wtime();

	/* Find the cost of writing the turbo file. Also activates Turbo Core in the first iteration */
	if (metric == EDP)
	{
		sprintf(set, "%d", TURBO_OFF);
		fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
		write(fd, set, sizeof(set));
		close(fd);
		boost_status = TURBO_OFF;
		write_file_threshold = 0.00074;
	}
}

/* It defines the number of threads that will execute the actual parallel region based on the current state of the search algorithm */
int lib_resolve_num_threads(uintptr_t ptr_region)
{
	int i, fd;
	char set[2];
	double time = 0, energy = 0, result = 0;
	id_actual_region = -1;

	if (libKernels[id_previous_region].seqState != END_SEQUENTIAL && libKernels[id_previous_region].seqState != PASS)
	{
		switch (libKernels[id_previous_region].seqMetric)
		{
		case PERFORMANCE:
			result = omp_get_wtime() - initSeqTime;
			time = result;
			break;
		case EDP:
			time = omp_get_wtime() - initSeqTime;
			energy = lib_end_seq_amd_msr();
			result = time * energy;
			/* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
			if (result == 0.00000 || result < 0)
			{
				libKernels[id_previous_region].seqState = PASS;
				libKernels[id_previous_region].seqMetric = PERFORMANCE;
				libKernels[id_previous_region].bestFreqSeq = TURBO_ON;
			}
			break;
		}

		switch (libKernels[id_previous_region].seqState)
		{
		case INITIAL:
			libKernels[id_previous_region].timeSeqTurboOff = time;
			libKernels[id_previous_region].resultSeqTurboOff = result;
			libKernels[id_previous_region].bestFreqSeq = TURBO_ON;
			libKernels[id_previous_region].seqState = END_TURBO;
			break;
		case END_TURBO:
			libKernels[id_previous_region].timeSeqTurboOn = time;
			libKernels[id_previous_region].resultSeqTurboOn = result;
			libKernels[id_previous_region].seqState = END_SEQUENTIAL;
			if (libKernels[id_previous_region].resultSeqTurboOff < libKernels[id_previous_region].resultSeqTurboOn)
			{
				libKernels[id_previous_region].bestFreqSeq = TURBO_OFF;
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
	switch (libKernels[id_actual_region].state)
	{
	case END:
		if ((boost_status == TURBO_OFF && libKernels[id_actual_region].bestFreq == TURBO_ON && (libKernels[id_actual_region].timeTurboOn + write_file_threshold < libKernels[id_actual_region].timeTurboOff)) || (boost_status == TURBO_ON && libKernels[id_actual_region].bestFreq == TURBO_OFF && (libKernels[id_actual_region].timeTurboOff + write_file_threshold < libKernels[id_actual_region].timeTurboOn)))
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
			boost_status = libKernels[id_actual_region].bestFreq;
		}
		return libKernels[id_actual_region].bestThreadOn;
		break;
	case END_THREADS:
		lib_start_amd_msr();
		libKernels[id_actual_region].initResult = omp_get_wtime();
		if (boost_status != libKernels[id_actual_region].bestFreq && libKernels[id_actual_region].bestTime > write_file_threshold)
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
			boost_status = libKernels[id_actual_region].bestFreq;
		}
		return libKernels[id_actual_region].bestThreadOn;
		break;
	default:
		lib_start_amd_msr();
		libKernels[id_actual_region].initResult = omp_get_wtime();
		if (boost_status != libKernels[id_actual_region].bestFreq && libKernels[id_actual_region].bestTime > write_file_threshold)
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
			boost_status = libKernels[id_actual_region].bestFreq;
		}
		return libKernels[id_actual_region].numThreads;
	}
}

// lib_end_amd_msr();
/* It is responsible for performing the search algorithm */
void lib_end_parallel_region()
{
	double time = 0, energy = 0, result = 0;
	int fd;
	char set[2];
	if (libKernels[id_actual_region].state != END)
	{
		/* Check the metric that is being evaluated and collect the results */
		switch (libKernels[id_actual_region].metric)
		{
		case PERFORMANCE:
			//printf("case Performance\n");
			result = omp_get_wtime() - libKernels[id_actual_region].initResult;
			time = result;
			break;
		case EDP:
			//printf("case EDP\n");
			time = omp_get_wtime() - libKernels[id_actual_region].initResult;
			energy = lib_end_amd_msr();
			result = time * energy;
			/* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
			if (result == 0.00000 || result < 0)
			{
				libKernels[id_actual_region].state = REPEAT;
				libKernels[id_actual_region].metric = PERFORMANCE;
			}
			break;
		}
		switch (libKernels[id_actual_region].state)
		{
		case REPEAT:
			libKernels[id_actual_region].state = S0;
			libKernels[id_actual_region].numThreads = libKernels[id_actual_region].startThreads;
			libKernels[id_actual_region].lastThread = libKernels[id_actual_region].numThreads;
			break;
		case S0:
			libKernels[id_actual_region].bestResult = result;
			libKernels[id_actual_region].bestTime = time;
			libKernels[id_actual_region].bestThreadOn = libKernels[id_actual_region].numThreads;
			libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads * 2;
			libKernels[id_actual_region].state = S1;
			break;
		case S1:
			if (result < libKernels[id_actual_region].bestResult)
			{
				libKernels[id_actual_region].bestResult = result;
				libKernels[id_actual_region].bestTime = time;
				libKernels[id_actual_region].bestThreadOn = libKernels[id_actual_region].numThreads;
				if (libKernels[id_actual_region].numThreads * 2 <= libKernels[id_actual_region].numCores)
				{
					libKernels[id_actual_region].lastThread = libKernels[id_actual_region].numThreads;
					libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads * 2;
				}
				else
				{
					libKernels[id_actual_region].pass = libKernels[id_actual_region].lastThread / 2;
					if (libKernels[id_actual_region].pass >= 2)
					{
						libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads - libKernels[id_actual_region].pass;
						libKernels[id_actual_region].state = S2;
					}
					else
					{
						libKernels[id_actual_region].bestFreq = TURBO_ON; //testar com turbo off;
						libKernels[id_actual_region].timeTurboOff = time;
						libKernels[id_actual_region].state = END_THREADS;
					}
				}
			}
			else
			{
				if (libKernels[id_actual_region].bestThreadOn == libKernels[id_actual_region].numCores / 2)
				{
					libKernels[id_actual_region].bestFreq = TURBO_ON;
					libKernels[id_actual_region].timeTurboOff = time;
					libKernels[id_actual_region].state = END_THREADS;
				}
				else
				{
					libKernels[id_actual_region].pass = libKernels[id_actual_region].lastThread / 2;
					if (libKernels[id_actual_region].pass >= 2)
					{
						libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads + libKernels[id_actual_region].pass;
						libKernels[id_actual_region].state = S2;
					}
					else
					{
						libKernels[id_actual_region].bestFreq = TURBO_ON;
						libKernels[id_actual_region].timeTurboOff = time;
						libKernels[id_actual_region].state = END_THREADS;
					}
				}
			}
			break;
		case S2:
			if (libKernels[id_actual_region].bestResult < result)
			{
				libKernels[id_actual_region].pass = libKernels[id_actual_region].pass / 2;
				if (libKernels[id_actual_region].pass >= 2)
				{
					libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads + libKernels[id_actual_region].pass;
				}
				else
				{
					libKernels[id_actual_region].bestFreq = TURBO_ON;
					libKernels[id_actual_region].timeTurboOff = time;
					libKernels[id_actual_region].state = END_THREADS;
				}
			}
			else
			{
				libKernels[id_actual_region].bestThreadOn = libKernels[id_actual_region].numThreads;
				libKernels[id_actual_region].bestTime = time;
				libKernels[id_actual_region].bestResult = result;
				libKernels[id_actual_region].pass = libKernels[id_actual_region].pass / 2;
				if (libKernels[id_actual_region].pass >= 2)
				{
					libKernels[id_actual_region].numThreads += libKernels[id_actual_region].pass;
				}
				else
				{
					libKernels[id_actual_region].bestFreq = TURBO_ON;
					libKernels[id_actual_region].timeTurboOff = time;
					libKernels[id_actual_region].state = END_THREADS;
				}
			}
			break;
		case END_THREADS:
			libKernels[id_actual_region].state = END;
			libKernels[id_actual_region].timeTurboOn = time;
			if (libKernels[id_actual_region].bestResult < result)
				libKernels[id_actual_region].bestFreq = TURBO_OFF;

			break;
		}
	}

	switch (libKernels[id_actual_region].seqState)
	{
	case PASS:
		libKernels[id_actual_region].seqState = INITIAL;
		initSeqTime = omp_get_wtime();
		lib_start_seq_amd_msr();
		break;
	case END_TURBO:
		if (libKernels[id_actual_region].bestFreq != libKernels[id_actual_region].bestFreqSeq && write_file_threshold < libKernels[id_actual_region].timeSeqTurboOff)
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", libKernels[id_actual_region].bestFreqSeq);
			write(fd, set, sizeof(set));
			close(fd);
			boost_status = libKernels[id_actual_region].bestFreqSeq;
		}
		initSeqTime = omp_get_wtime();
		lib_start_seq_amd_msr();
		break;
	case END_SEQUENTIAL:
		if ((boost_status == TURBO_OFF && libKernels[id_actual_region].bestFreqSeq == TURBO_ON && (libKernels[id_actual_region].timeSeqTurboOn + write_file_threshold < libKernels[id_actual_region].timeSeqTurboOff)) || (boost_status == TURBO_ON && libKernels[id_actual_region].bestFreqSeq == TURBO_OFF && (libKernels[id_actual_region].timeSeqTurboOff + write_file_threshold < libKernels[id_actual_region].timeSeqTurboOn)))
		{
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", libKernels[id_actual_region].bestFreqSeq);
			write(fd, set, sizeof(set));
			close(fd);
			boost_status = libKernels[id_actual_region].bestFreqSeq;
		}
		break;
	}
	id_previous_region = id_actual_region;
}

/* It finalizes the environment of Aurora */
void lib_destructor()
{
	double time = omp_get_wtime() - initGlobalTime;
	id_actual_region = MAX_KERNEL - 1;
	double energy = lib_end_amd_msr();
	float edp = time * energy;
	printf("POSEIDON - Execution Time: %.5f seconds\n", time);
	printf("POSEIDON - Energy: %.5f joules\n", energy);
	printf("POSEIDON - EDP: %.5f\n", edp);
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
	double result = (libKernels[id_actual_region].kernelAfter[0] - libKernels[id_actual_region].kernelBefore[0]) * pow(0.5, (float)(energy_unit));
	return result;
}

void lib_start_seq_amd_msr()
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
	libKernels[id_actual_region].kernelBeforeSeq[0] = (long long)data;
}

double lib_end_seq_amd_msr()
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
	libKernels[id_previous_region].kernelAfterSeq[0] = (long long)data;
	double result = (libKernels[id_previous_region].kernelAfterSeq[0] - libKernels[id_previous_region].kernelBeforeSeq[0]) * pow(0.5, (float)(energy_unit));
	return result;
}
