/////////////////////////////////////////////////////////////////////
//                         2023 May 08                             //
//                   authors: F. Tortorici                         //
//                email: francesco.tortorici@ct.infn.it            //
//                        notes:                                   //
/////////////////////////////////////////////////////////////////////

#ifndef _FERS_EUDAQ_h
#define _FERS_EUDAQ_h

#include <vector>
#include "paramparser.h"
#include <map>

#define DTQ_STAIRCASE 10
// staircase datatype
typedef struct {
	uint16_t threshold;
	uint16_t shapingt; // enum, see FERS_Registers.h
	uint32_t dwell_time; // in seconds, divide hitcnt by this to get rate
	uint32_t chmean; // over channels, no division by time
	uint32_t HV; // 1000 * HV from HV_Get_Vbias( handle, &HV);
	uint32_t Tor_cnt;
	uint32_t Qor_cnt;
	uint32_t hitcnt[FERSLIB_MAX_NCH];
} StaircaseEvent_t;

// known types of event (for event length checks for instance in Monitor)
static const std::map<uint8_t, int> event_lengths =
{
{ DTQ_SPECT     , sizeof(SpectEvent_t)    },
{ DTQ_TIMING    , sizeof(ListEvent_t)     },
{ DTQ_COUNT     , sizeof(CountingEvent_t) },
{ DTQ_WAVE      , sizeof(WaveEvent_t)     },
{ DTQ_TSPECT    , sizeof(SpectEvent_t)    },
{ DTQ_TEST      , sizeof(TestEvent_t)     },
{ DTQ_STAIRCASE , sizeof(StaircaseEvent_t)}
};                                
                                  
                                  
//////////////////////////        
// use this to pack every kind of  event
void FERSpackevent(void* Event, int dataqualifier, std::vector<uint8_t> *vec);
//////////////////////////        
                                  
// for each kind of event: the "pack" is used by FERSpackevent,
// whereas the "unpack" is meant sto be used individually

// basic types of events 
void FERSpack_spectevent(void* Event, std::vector<uint8_t> *vec);
SpectEvent_t FERSunpack_spectevent(std::vector<uint8_t> *vec);

void FERSpack_listevent(void* Event, std::vector<uint8_t> *vec);
ListEvent_t FERSunpack_listevent(std::vector<uint8_t> *vec);

void FERSpack_tspectevent(void* Event, std::vector<uint8_t> *vec);
SpectEvent_t FERSunpack_tspectevent(std::vector<uint8_t> *vec);

void FERSpack_countevent(void* Event, std::vector<uint8_t> *vec);
CountingEvent_t FERSunpack_countevent(std::vector<uint8_t> *vec);

void FERSpack_waveevent(void* Event, std::vector<uint8_t> *vec);
WaveEvent_t FERSunpack_waveevent(std::vector<uint8_t> *vec);

void FERSpack_testevent(void* Event, std::vector<uint8_t> *vec);
TestEvent_t FERSunpack_testevent(std::vector<uint8_t> *vec);

// advanced
void FERSpack_staircaseevent(void* Event, std::vector<uint8_t> *vec);
StaircaseEvent_t FERSunpack_staircaseevent(std::vector<uint8_t> *vec);

/////////////////

// utilities used by the above methods

// fill "data" with some info
//void make_header(int handle, uint8_t x_pixel, uint8_t y_pixel, int DataQualifier, std::vector<uint8_t> *data);
void make_header(int board, int DataQualifier, std::vector<uint8_t> *data);

// reads back essential header info (see params)
// prints them w/ board ID info with EUDAQ_WARN
// returns index at which raw data starts
//int read_header(std::vector<uint8_t> *data, uint8_t *x_pixel, uint8_t *y_pixel, uint8_t *DataQualifier);
int read_header(std::vector<uint8_t> *data, int *board, uint8_t *DataQualifier);

void dump_vec(std::string title, std::vector<uint8_t> *vec, int start=0, int stop=0);

void FERSpack(int nbits, uint32_t input, std::vector<uint8_t> *vec);
uint16_t FERSunpack16(int index, std::vector<uint8_t> vec);
uint32_t FERSunpack32(int index, std::vector<uint8_t> vec);
uint64_t FERSunpack64(int index, std::vector<uint8_t> vec);
//// to test:
//  uint16_t num16 = 0xabcd;
//  uint32_t num32 = 0x12345678;
//  std::vector<uint8_t> vec;
//  FERSpack(32, num32, &vec);
//  std::printf("FERSpack   : num32 = %x test32= %x %x %x %x\n", num32, vec.at(0), vec.at(1), vec.at(2), vec.at(3));
//  FERSpack(16, num16, &vec);
//  std::printf("FERSpack   : num16 = %x test16= %x %x\n", num16, vec.at(4), vec.at(5));
//
//  uint32_t num32r = FERSunpack32( 0, vec );
//  uint16_t num16r = FERSunpack16( 4, vec );
//  std::printf("FERSunpack : num32r = %x\n", num32r);
//  std::printf("FERSunpack : num16r = %x\n", num16r);


///////////////////////  FUNCTIONS IN ALPHA STATE  /////////////////////
///////////////////////  NO DEBUG IS DONE. AT ALL! /////////////////////

// shared structure
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/types.h>
#define SHM_KEY 0x1234
#define MAXCHAR 100 // max size of chars in following struct
struct shmseg {
	int connectedboards = 0; // number of connected boards
	int nchannels[MAX_NBRD];
	int handle[MAX_NBRD]; // handle is given by FERS_OpenDevice()
	//from ini file:
	char IP[MAX_NBRD][MAXCHAR]; // IP address
	char desc[MAX_NBRD][MAXCHAR]; // for example serial number
	char location[MAX_NBRD][MAXCHAR]; // for instance "on the scope"
	char producer[MAX_NBRD][MAXCHAR]; // title of producer
	// from conf file:
	float HVbias[MAX_NBRD]; // HV bias
	char collector[MAX_NBRD][MAXCHAR]; // title of data collector
};
void initshm( int shmid );
void dumpshm( struct shmseg* shmp, int brd );
//
// IN ORDER TO ACCESS IT:
//
// 1) general note: the current board number variable, say
// int brd;
// must be private in both producer and monitor 
//
// 2) in the PRODUCER:
// put this in the global section of the file:
//
// struct shmseg *shmp;
// int shmid;
//
// 3) in the MONITOR:
// put this in the global section of the file:
//
// extern struct shmseg *shmp;
// extern int shmid;
//
// 4) this goes in DoInitialise() of both PRODUCER and MONITOR
// 
// shmid = shmget(SHM_KEY, sizeof(struct shmseg), 0644|IPC_CREAT);
// if (shmid == -1) {
//	perror("Shared memory");
// }
// shmp = (shmseg*)shmat(shmid, NULL, 0);
// if (shmp == (void *) -1) {
//	perror("Shared memory attach");
// }
//
// 5) in DoInitialise() of PRODUCER, also add
//
// initshm( shmid );
//
// 6) that's it! you can now access the content of the shared structure via pointer, for example
//
// shmp->connectedboards++;
//
//
// CLEAN UP
// in the DoTerminate of the producer (since the latter is the class that created the shared memory, it is its responsibility to also destroy it)
//
//if (shmdt(shmp) == -1) {
//	perror("shmdt");
//}


#endif
