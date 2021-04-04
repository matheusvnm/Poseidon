#include "libgomp.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <unistd.h>
#include <stdint.h>

/*define RAPL Environment*/

#define CPU_SANDYBRIDGE         42
#define CPU_SANDYBRIDGE_EP      45
#define CPU_IVYBRIDGE           58
#define CPU_IVYBRIDGE_EP        62
#define CPU_HASWELL             60      // 70 too?
#define CPU_HASWELL_EP          63
#define CPU_HASWELL_1           69
#define CPU_BROADWELL           61      // 71 too?
#define CPU_BROADWELL_EP        79
#define CPU_BROADWELL_DE        86
#define CPU_SKYLAKE             78      
#define CPU_SKYLAKE_1           94
#define NUM_RAPL_DOMAINS        4
#define MAX_CPUS                128 //1024
#define MAX_PACKAGES            4 //16



/*define AURORA environment*/

#define MAX_KERNEL              61 // Matheus: O que é isso?
#define MAX_THREADS             32
#define PERFORMANCE             0
#define EDP                     2
#define TURBO_ON                1
#define TURBO_OFF               0


#define END                     110
#define END_THREADS             109
#define END_TURBO               108
#define S0                      100
#define S1                      101
#define S2                      102
#define S3                      103
#define S4                      104
#define S5                      105
#define REPEAT                  106

#define INITIAL                 107
#define END_SEQUENTIAL          111
#define PASS                    112



/*global variables*/

static int package_map[MAX_PACKAGES];
static int total_packages=0, total_cores=0;
char rapl_domain_names[NUM_RAPL_DOMAINS][30]= {"energy-cores", "energy-gpu", "energy-pkg", "energy-ram"};
char event_names[MAX_PACKAGES][NUM_RAPL_DOMAINS][256];
char filenames[MAX_PACKAGES][NUM_RAPL_DOMAINS][256];
char packname[MAX_PACKAGES][256];
char tempfile[256];
int valid[MAX_PACKAGES][NUM_RAPL_DOMAINS];
double initGlobalTime = 0.0;
double write_file_threshold=0.0;
double initSeqTime;
unsigned long int idKernels[MAX_KERNEL];
short int id_actual_region=0;
short int id_previous_region=0;
short int totalKernels=0;

typedef struct{
        short int numThreads;
        short int numCores;
        short int bestThread;
        short int bestThreadOn;
	short int startThreads;
        short int metric;
        short int seqMetric;
        short int state;
        short int seqState;
        int bestFreq;
        int bestFreqSeq;
	short int pass;
	short int lastThread;
        double bestResult, bestTime, initResult, totalTime, totalEnergy, totalTimeSeq, totalEnergySeq;
        double timeTurboOff, timeTurboOn, timeSeqTurboOn, timeSeqTurboOff, resultSeqTurboOn, resultSeqTurboOff;
        long long kernelBefore[MAX_PACKAGES][NUM_RAPL_DOMAINS];
        long long kernelAfter[MAX_PACKAGES][NUM_RAPL_DOMAINS];
}typeFrame;

typeFrame libKernels[MAX_KERNEL];

