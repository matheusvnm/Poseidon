/* File that contains the variable declarations */
#include <stdio.h>
#include "aurora.h"

/* First function called. It initiallizes all the functions and variables used by AURORA */
void lib_init(int metric, int start_search){
        int i, fd;
        char set[2];
        int numCores = sysconf(_SC_NPROCESSORS_ONLN);
        /*Initialization of RAPL */
        lib_detect_cpu();
        lib_detect_packages();
        /*End initialization of RAPL */

        //int startThreads = numCores;
	//while(startThreads != 2 && startThreads != 3 && startThreads != 5){
	//       startThreads = startThreads/2;
	//}

        /* Initialization of the variables necessary to perform the search algorithm */
        for(i=0;i<MAX_KERNEL;i++){
                libKernels[i].numThreads = numCores;
                libKernels[i].startThreads = 2;//startThreads;
                libKernels[i].numCores = numCores;
                libKernels[i].initResult = 0.0;
                libKernels[i].state = REPEAT;
                libKernels[i].metric = metric;
                libKernels[i].seqMetric = metric;
		libKernels[i].bestFreq = TURBO_ON;
                libKernels[i].bestFreqSeq = TURBO_ON;
                libKernels[i].timeTurboOff = 0.0;
                libKernels[i].timeTurboOn = 0.0;
                libKernels[i].seqState = PASS;
                idKernels[i] = 0;
                
        }

        /* Start the counters for energy and time for all the application execution */
        id_actual_region = MAX_KERNEL-1;
        lib_start_rapl_sysfs();
        initGlobalTime = omp_get_wtime();
	
	/* Find the cost of writing the turbo file. Also activates Turbo Core in the first iteration */	
        if(metric == EDP){
	        sprintf(set, "%d", TURBO_ON);
	        fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
	        write(fd, set, sizeof(set));
	        close(fd);
                boost_status=TURBO_ON;    
                write_file_threshold = 0.000136;
        }
}


/* It defines the number of threads that will execute the actual parallel region based on the current state of the search algorithm */
int lib_resolve_num_threads(uintptr_t ptr_region){
        int i, fd;
	char set[2];
        double time=0, energy=0, result=0;
        id_actual_region = -1;

        
        if(libKernels[id_previous_region].seqState != END_SEQUENTIAL && libKernels[id_previous_region].seqState != PASS){
                switch(libKernels[id_previous_region].seqMetric){
                                case PERFORMANCE:
                                        result = omp_get_wtime() - initSeqTime;
                                        time = result;
                                        break;
                                case EDP:
                                        time = omp_get_wtime() - initSeqTime;
                                        energy = lib_end_rapl_sysfs();
                                        result = time * energy;
                                        /* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
                                        if(result == 0.00000 || result < 0){
                                                libKernels[id_previous_region].seqState = PASS;
                                                libKernels[id_previous_region].seqMetric = PERFORMANCE;
                                                libKernels[id_previous_region].bestFreqSeq = TURBO_ON;
                                        }
                                        break;
                        }

                switch (libKernels[id_previous_region].seqState)
                {
                        case INITIAL:
                                libKernels[id_previous_region].timeSeqTurboOn = time;
                                libKernels[id_previous_region].resultSeqTurboOn = result;
                                libKernels[id_previous_region].bestFreqSeq = TURBO_OFF;
                                libKernels[id_previous_region].seqState = END_TURBO;
                                break;
                        case END_TURBO:
                                libKernels[id_previous_region].timeSeqTurboOff = time;
                                libKernels[id_previous_region].resultSeqTurboOff = result;
                                libKernels[id_previous_region].seqState = END_SEQUENTIAL;
                                if(libKernels[id_previous_region].resultSeqTurboOff > libKernels[id_previous_region].resultSeqTurboOn){
                                        libKernels[id_previous_region].bestFreqSeq = TURBO_ON;
                                }
                                break;
                }
        }


        /* Find the actual parallel region */
        for(i=0;i<totalKernels;i++){
                if(idKernels[i] == ptr_region){
                        id_actual_region = i;  
                        break;
                }
        }

        /* If a new parallel region is discovered */
        if(id_actual_region == -1){
                idKernels[totalKernels] = ptr_region;
                id_actual_region = totalKernels; 
                totalKernels++;                     
        }        
         

        /* Check the state of the search algorithm. */
        switch(libKernels[id_actual_region].state){
	        case END:
			if((boost_status == TURBO_OFF && libKernels[id_actual_region].bestFreq == TURBO_ON && (libKernels[id_actual_region].timeTurboOn + write_file_threshold < libKernels[id_actual_region].timeTurboOff)) || (boost_status == TURBO_ON && libKernels[id_actual_region].bestFreq == TURBO_OFF && (libKernels[id_actual_region].timeTurboOff + write_file_threshold < libKernels[id_actual_region].timeTurboOn))){
                                fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
                                sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
                                write(fd, set, sizeof(set));
                                close(fd);
                                boost_status=libKernels[id_actual_region].bestFreq;
                        }
               		return libKernels[id_actual_region].bestThread;
			break;
		case END_THREADS:
                        lib_start_rapl_sysfs();
                        libKernels[id_actual_region].initResult = omp_get_wtime();
			if(boost_status != libKernels[id_actual_region].bestFreq && libKernels[id_actual_region].bestTime > write_file_threshold){ //0.1
				fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
				sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
				write(fd, set, sizeof(set));
				close(fd);
                                boost_status=libKernels[id_actual_region].bestFreq;
			}
               		return libKernels[id_actual_region].bestThread;
			break;
                default:
                        lib_start_rapl_sysfs();
                        libKernels[id_actual_region].initResult = omp_get_wtime();
			if(boost_status != libKernels[id_actual_region].bestFreq && libKernels[id_actual_region].bestTime > write_file_threshold){ //0.1
				fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
				sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
				write(fd, set, sizeof(set));
				close(fd);
                                boost_status=libKernels[id_actual_region].bestFreq;
			}
                        return libKernels[id_actual_region].numThreads; 
        }      
}


/* It is responsible for performing the search algorithm */
void lib_end_parallel_region(){
        double time=0, energy=0, result=0;
	int fd;
	char set[2];
        if(libKernels[id_actual_region].state !=END){
                /* Check the metric that is being evaluated and collect the results */
                switch(libKernels[id_actual_region].metric){
                        case PERFORMANCE:
                                result = omp_get_wtime() - libKernels[id_actual_region].initResult;
				time = result;
                                break;
                        case EDP:
                                time = omp_get_wtime() - libKernels[id_actual_region].initResult;
                                energy = lib_end_rapl_sysfs();
                                result = time * energy;
                                /* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
                                if(result == 0.00000 || result < 0){
                                        libKernels[id_actual_region].state = REPEAT;
                                        libKernels[id_actual_region].metric = PERFORMANCE;
                                }
                                break;
                }
                switch(libKernels[id_actual_region].state){
			case REPEAT:
                                printf("REPEAT - Região %d, Turbo State %d, Número de Threads %d, Resultado Atual %lf, Melhor Resultado %lf\n", id_actual_region, boost_status, libKernels[id_actual_region].numThreads, result, libKernels[id_actual_region].bestResult);
				libKernels[id_actual_region].state = S0;
				libKernels[id_actual_region].numThreads = libKernels[id_actual_region].startThreads;
				libKernels[id_actual_region].lastThread = libKernels[id_actual_region].numThreads; 
				break;
			case S0:
                                printf("S0 - Região %d, Turbo State %d, Número de Threads %d, Resultado Atual %lf, Melhor Resultado %lf\n", id_actual_region, boost_status, libKernels[id_actual_region].numThreads, result, libKernels[id_actual_region].bestResult);
				libKernels[id_actual_region].bestResult = result;
				libKernels[id_actual_region].bestTime = time;
				libKernels[id_actual_region].bestThread = libKernels[id_actual_region].numThreads;
				libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads*2;
				libKernels[id_actual_region].state = S1;
				break;
			case S1:
                                printf("S1 - Região %d, Turbo State %d, Número de Threads %d, Resultado Atual %lf, Melhor Resultado %lf\n", id_actual_region, boost_status, libKernels[id_actual_region].numThreads, result, libKernels[id_actual_region].bestResult);
				if(result < libKernels[id_actual_region].bestResult){
					libKernels[id_actual_region].bestResult = result;
					libKernels[id_actual_region].bestTime = time;
					libKernels[id_actual_region].bestThread = libKernels[id_actual_region].numThreads;
					if(libKernels[id_actual_region].numThreads * 2 <= libKernels[id_actual_region].numCores){
						libKernels[id_actual_region].lastThread = libKernels[id_actual_region].numThreads;
						libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads*2;
					}
					else{
						libKernels[id_actual_region].pass = libKernels[id_actual_region].lastThread/2;
						if(libKernels[id_actual_region].pass >= 2){
							libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads - libKernels[id_actual_region].pass;
							libKernels[id_actual_region].state = S2;
						}else{
							libKernels[id_actual_region].bestFreq = TURBO_OFF; //testar com turbo off;
                                                        libKernels[id_actual_region].timeTurboOn = time;
							libKernels[id_actual_region].state = END_THREADS;
						}

					}
				}else{
					if(libKernels[id_actual_region].bestThread == libKernels[id_actual_region].numCores/2){
							libKernels[id_actual_region].bestFreq = TURBO_OFF;
                                                        libKernels[id_actual_region].timeTurboOn = time;
							libKernels[id_actual_region].state = END_THREADS;
					}else{
						libKernels[id_actual_region].pass = libKernels[id_actual_region].lastThread/2;
						if(libKernels[id_actual_region].pass >= 2){
							libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads + libKernels[id_actual_region].pass;
							libKernels[id_actual_region].state = S2;
						}else{
							libKernels[id_actual_region].bestFreq = TURBO_OFF;
                                                        libKernels[id_actual_region].timeTurboOn = time;
							libKernels[id_actual_region].state = END_THREADS;
						}
					}
				}
				break;
			case S2:
                                printf("S2 - Região %d, Turbo State %d, Número de Threads %d, Resultado Atual %lf, Melhor Resultado %lf\n", id_actual_region, boost_status, libKernels[id_actual_region].numThreads, result, libKernels[id_actual_region].bestResult);
				if(libKernels[id_actual_region].bestResult < result){
					libKernels[id_actual_region].pass = libKernels[id_actual_region].pass/2;
					if(libKernels[id_actual_region].pass >= 2){
						libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads + libKernels[id_actual_region].pass;
					}
					else{
						libKernels[id_actual_region].bestFreq = TURBO_OFF;
                                                libKernels[id_actual_region].timeTurboOn = time;
						libKernels[id_actual_region].state = END_THREADS;
					}
				}else{
					libKernels[id_actual_region].bestThread = libKernels[id_actual_region].numThreads;
					libKernels[id_actual_region].bestTime = time;
					libKernels[id_actual_region].bestResult = result;
					libKernels[id_actual_region].pass = libKernels[id_actual_region].pass/2;
					if(libKernels[id_actual_region].pass >= 2){
						libKernels[id_actual_region].numThreads += libKernels[id_actual_region].pass;
					}else{
						libKernels[id_actual_region].bestFreq = TURBO_OFF;
                                                libKernels[id_actual_region].timeTurboOn = time;
						libKernels[id_actual_region].state = END_THREADS;
					}
				}
				break;
			case END_THREADS:
                                printf("END_THREADS - Região %d, Turbo State %d, Número de Threads %d, Resultado Atual %lf, Melhor Resultado %lf\n", id_actual_region, boost_status, libKernels[id_actual_region].numThreads, result, libKernels[id_actual_region].bestResult);
				libKernels[id_actual_region].state = END;
                                libKernels[id_actual_region].timeTurboOff = time;
				if(libKernels[id_actual_region].bestResult < result)
					libKernels[id_actual_region].bestFreq = TURBO_ON;
                       		break;
		}
	 
        }


        switch(libKernels[id_actual_region].seqState){
                case PASS:
			libKernels[id_actual_region].seqState = INITIAL;
                        /* Matheus: A ideia do código abaixo seria o caso de dar um erro no registrador de energia e reiniciar a busca só que com performance. 
                        Neste caso teria que haver o código abaixo para consegui reiniciar a busca com a métrica performance e ativando o turbo.
                        */
                        //if (boost_status != libKernels[id_actual_region].bestFreqSeq){
                	//	fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
        		//	sprintf(set, "%d", libKernels[id_actual_region].bestFreqSeq);
	        	//	write(fd, set, sizeof(set));
	        	//	close(fd);
                        //      boost_status=libKernels[id_actual_region].bestFreqSeq;
			//}
			initSeqTime = omp_get_wtime();
                        lib_start_rapl_sysfs();
                        break;
		case END_TURBO:
                		fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
        			sprintf(set, "%d", libKernels[id_actual_region].bestFreqSeq);
	        		write(fd, set, sizeof(set));
	        		close(fd);
                                boost_status=libKernels[id_actual_region].bestFreqSeq;
			}
			initSeqTime = omp_get_wtime();
                        lib_start_rapl_sysfs();
			break;
		case END_SEQUENTIAL:
                        printf("SEQUENTIAL - END_SEQUENTIAL - Região %d, Turbo State %d, Resultado TURBO_OFF %lf, Resultado TURBO_ON %lf\n", id_actual_region, libKernels[id_actual_region].resultSeqTurboOff, libKernels[id_actual_region].resultSeqTurboOn);
			if((boost_status == TURBO_OFF && libKernels[id_actual_region].bestFreqSeq == TURBO_ON && (libKernels[id_actual_region].timeSeqTurboOn + write_file_threshold < libKernels[id_actual_region].timeSeqTurboOff)) || (boost_status == TURBO_ON && libKernels[id_actual_region].bestFreqSeq == TURBO_OFF && (libKernels[id_actual_region].timeSeqTurboOff + write_file_threshold < libKernels[id_actual_region].timeSeqTurboOn))){
                        	fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
				sprintf(set, "%d", libKernels[id_actual_region].bestFreqSeq);
				write(fd, set, sizeof(set));
				close(fd);
                                boost_status=libKernels[id_actual_region].bestFreqSeq;
                	}
                        break;
	}
	id_previous_region = id_actual_region;
}


/* It finalizes the environment of Aurora */
void lib_destructor(){
        float time = omp_get_wtime() - initGlobalTime;
        id_actual_region = MAX_KERNEL-1;
        float energy = lib_end_rapl_sysfs();
        float edp = time * energy;
        printf("Poseidon - Execution Time: %.5f seconds\n", time);
        printf("Poseidon - Energy: %.5f joules\n",energy);
        printf("Poseidon - EDP: %.5f\n", edp);
}

/* Function used by the Intel RAPL to detect the CPU Architecture*/
void lib_detect_cpu(){
        FILE *fff;
        int family,model=-1;
        char buffer[BUFSIZ],*result;
        char vendor[BUFSIZ];
        fff=fopen("/proc/cpuinfo","r");
        while(1) {
                result=fgets(buffer,BUFSIZ,fff);
                if (result==NULL)
                        break;
                if (!strncmp(result,"vendor_id",8)) {
                        sscanf(result,"%*s%*s%s",vendor);
                        if (strncmp(vendor,"GenuineIntel",12)) {
                                printf("%s not an Intel chip\n",vendor);
                        }
                }
                if (!strncmp(result,"cpu family",10)) {
                        sscanf(result,"%*s%*s%*s%d",&family);
                        if (family!=6) {
                                printf("Wrong CPU family %d\n",family);
                        }
                }
                if (!strncmp(result,"model",5)) {
                        sscanf(result,"%*s%*s%d",&model);
                }
        }
        fclose(fff);
}

/* Function used by the Intel RAPL to detect the number of cores and CPU sockets*/
void lib_detect_packages(){
        char filename[BUFSIZ];
        FILE *fff;
        int package;
        int i;
        for(i=0;i<MAX_PACKAGES;i++)
                package_map[i]=-1;
        for(i=0;i<MAX_CPUS;i++) {
                sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id",i);
                fff=fopen(filename,"r");
                if (fff==NULL)
                        break;
                fscanf(fff,"%d",&package);
                fclose(fff);
                if (package_map[package]==-1) {
                        total_packages++;
                        package_map[package]=i;
                }
        }
        total_cores=i;
}

/* Function used by the Intel RAPL to store the actual value of the hardware counter*/
void lib_start_rapl_sysfs(){
        int i,j;
        FILE *fff;
        for(j=0;j<total_packages;j++) {
                i=0;
                sprintf(packname[j],"/sys/class/powercap/intel-rapl/intel-rapl:%d",j);
                sprintf(tempfile,"%s/name",packname[j]);
                fff=fopen(tempfile,"r");
                if (fff==NULL) {
                        fprintf(stderr,"\tCould not open %s\n",tempfile);
                        exit(0);
                }
                fscanf(fff,"%s",event_names[j][i]);
                valid[j][i]=1;
                fclose(fff);
                sprintf(filenames[j][i],"%s/energy_uj",packname[j]);

                /* Handle subdomains */
                for(i=1;i<NUM_RAPL_DOMAINS;i++){
                        sprintf(tempfile,"%s/intel-rapl:%d:%d/name", packname[j],j,i-1);
                        fff=fopen(tempfile,"r");
                        if (fff==NULL) {
                                //fprintf(stderr,"\tCould not open %s\n",tempfile);
                                valid[j][i]=0;
                                continue;
                        }
                        valid[j][i]=1;
                        fscanf(fff,"%s",event_names[j][i]);
                        fclose(fff);
                        sprintf(filenames[j][i],"%s/intel-rapl:%d:%d/energy_uj", packname[j],j,i-1);
                }
        }
 /* Gather before values */
        for(j=0;j<total_packages;j++) {
                for(i=0;i<NUM_RAPL_DOMAINS;i++) {
                        if(valid[j][i]) {
                                fff=fopen(filenames[j][i],"r");
                                if (fff==NULL) {
                                        fprintf(stderr,"\tError opening %s!\n",filenames[j][i]);
                                }
                                else {
                                        fscanf(fff,"%lld",&libKernels[id_actual_region].kernelBefore[j][i]);
                                        fclose(fff);
                                }
                        }
                }
        }
}

/* Function used by the Intel RAPL to load the value of the hardware counter and returns the energy consumption*/
double lib_end_rapl_sysfs(){
        int i, j;
        FILE *fff;
        double total=0;
        for(j=0;j<total_packages;j++) {
                for(i=0;i<NUM_RAPL_DOMAINS;i++) {
                        if (valid[j][i]) {
                                fff=fopen(filenames[j][i],"r");
                        if (fff==NULL) {
                                fprintf(stderr,"\tError opening %s!\n",filenames[j][i]);
                        }
                        else {
                                fscanf(fff,"%lld",&libKernels[id_actual_region].kernelAfter[j][i]);
                                fclose(fff);
                        }
                }
                }
        }
        for(j=0;j<total_packages;j++) {
                for(i=0;i<NUM_RAPL_DOMAINS;i++) {
                        if(valid[j][i]){
                                if(strcmp(event_names[j][i],"core")!=0 && strcmp(event_names[j][i],"uncore")!=0){
                                        total += (((double)libKernels[id_actual_region].kernelAfter[j][i]-(double)libKernels[id_actual_region].kernelBefore[j][i])/1000000.0);
                                }
                        }
                }
        }
        return total;
}
