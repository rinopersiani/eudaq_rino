/////////////////////////////////////////////////////////////////////
//                         2022 Jun 20                             //
//                   author: R. Persianiy                          //
//                email: rinopersiani@gmail.com                    //
//                        notes:                                   //
/////////////////////////////////////////////////////////////////////


#include "eudaq/Producer.hh"
#include <iostream>
#include <fstream>
#include <ratio>
#include <chrono>
#include <thread>
#include <random>
#include "stdlib.h"
#include "JanusC.h"
#include "Statistics.h"
#include "FERS_Registers.h"
#include "FERSutils.h"
#include "paramparser.h"
#include "FERSlib.h"
#include "configure.h"
#ifndef _WIN32
#include <sys/file.h>
#endif
//----------DOC-MARK-----BEG*DEC-----DOC-MARK----------


  Config_t WDcfg;		// struct with all parameters
  RunVars_t RunVars;            // struct containing run time variables
  int SockConsole = 0;
  char ErrorMsg[250];

class FERSProducer : public eudaq::Producer {
  public:
  FERSProducer(const std::string & name, const std::string & runcontrol);
  void DoInitialise() override;
  void DoConfigure() override;
  void DoStartRun() override;
  void DoStopRun() override;
  void DoTerminate() override;
  void DoReset() override;
  void RunLoop() override;
  
  static const uint32_t m_id_factory = eudaq::cstr2hash("FERSProducer");

  
private:
  bool m_flag_ts;
  bool m_flag_tg;
  uint32_t m_plane_id;
  FILE* m_file_lock;
  FILE* conf_file;
  std::chrono::milliseconds m_ms_busy;
  bool m_exit_of_run;
  std::string fers_ip_address;  // IP address of the board
  int handle;		 	// Board handle

  float vmon, imon, vbias, imax;
};
//----------DOC-MARK-----END*DEC-----DOC-MARK----------
//----------DOC-MARK-----BEG*CON-----DOC-MARK----------
namespace{
  auto dummy0 = eudaq::Factory<eudaq::Producer>::
    Register<FERSProducer, const std::string&, const std::string&>(FERSProducer::m_id_factory);
}
//----------DOC-MARK-----END*REG-----DOC-MARK----------

FERSProducer::FERSProducer(const std::string & name, const std::string & runcontrol)
  :eudaq::Producer(name, runcontrol), m_file_lock(0), m_exit_of_run(false){
  //conf_file = NULL;
}

//----------DOC-MARK-----BEG*INI-----DOC-MARK----------
void FERSProducer::DoInitialise(){
  auto ini = GetInitConfiguration();
  std::string lock_path = ini->Get("FERS_DEV_LOCK_PATH", "ferslockfile.txt");
  m_file_lock = fopen(lock_path.c_str(), "a");
#ifndef _WIN32
  if(flock(fileno(m_file_lock), LOCK_EX|LOCK_NB)){ //fail
    EUDAQ_THROW("unable to lock the lockfile: "+lock_path );
  }
#endif

  fers_ip_address = ini->Get("FERS_IP_ADDRESS", "1.0.0.0");
  memset(&handle, -1, sizeof(handle));
  char ip_address[20];
  char connection_path[40];
  strcpy(ip_address, fers_ip_address.c_str());
  sprintf(connection_path,"eth:%s",ip_address);
  // std::cout <<"----3333---- "<<connection_path<<std::endl;
  int ret  = FERS_OpenDevice(connection_path, &handle);
  //FERS_ReadBoardInfo() // return=0 e riempie la struct FERS_BoardInfo
  // std::cout <<"-------- ret= "<<ret<<std::endl;
  if(ret == 0)
    {
    std::cout <<"Connected to: "<< connection_path<<std::endl;
    ret = FERS_ReadBoardInfo(*handle, FERS_BoardInfo[0]); //FERS_BoardInfo[BoardIndex]);
		if ((ret != 0) && (ret != FERSLIB_ERR_INVALID_BIC)) return ret;
    }
  else
    EUDAQ_THROW("unable to connect to fers with ip address: "+ fers_ip_address);
  
  // std::cout <<" ------- RINO ----------   "<<fers_ip_address<<std::endl;  
}


//----------DOC-MARK-----BEG*CONF-----DOC-MARK----------
void FERSProducer::DoConfigure(){
  auto conf = GetConfiguration();
  conf->Print(std::cout);
  int ret = -1000;
  int conf_verbose = conf->Get("FERS_CONF_VERBOSE",0);
  std::string fers_conf_dir = conf->Get("FERS_CONF_DIR", ".");
  std::string fers_conf_file = conf->Get("FERS_CONF_FILE", "configuration_0.txt");
  std::string conf_filename = fers_conf_dir+fers_conf_file;



  HV_Get_Vmon(handle, &vmon);
  HV_Get_Imon(handle, &imon);
  printf("ecco i valori di Vmon e Imon prima della configurazione: %f \t %f \n",vmon,imon);
  
  if(conf_verbose>=1)
    std::cout <<"Opening configuration file: "<<conf_filename<<std::endl;
  conf_file = fopen(conf_filename.c_str(),"r");
  if(conf_file == NULL)
    EUDAQ_THROW("unable to open configuration file: " + (std::string)(conf_filename));
  else {
    ret = ParseConfigFile(conf_file, &WDcfg, 1);
    if(ret == 0)
      std::cout <<"Board properly configured."<<std::endl;
    fclose(conf_file);
  }

  // if()
  RunVars.StaircaseCfg[SCPARAM_MIN] = 200;
  RunVars.StaircaseCfg[SCPARAM_MAX] = 300;
  RunVars.StaircaseCfg[SCPARAM_STEP] = 1;
  RunVars.StaircaseCfg[SCPARAM_DWELL] = 500;

  ConfigureFERS(handle,0);
  
  HV_Get_Vmon(handle, &vmon);
  HV_Get_Imon(handle, &imon);
  printf("ecco i valori di Vmon e Imon dopo la configurazione: %f \t %f \n",vmon,imon);
  HV_Get_Vbias(handle, &vbias);
  HV_Get_Imax(handle, &imax);
  printf("ecco vbias e imax: %f \t %f \n",vbias,imax);
  
  m_plane_id = conf->Get("EX0_PLANE_ID", 0);
  m_ms_busy = std::chrono::milliseconds(conf->Get("EX0_DURATION_BUSY_MS", 1000));
  m_flag_ts = conf->Get("EX0_ENABLE_TIMESTAMP", 0);
  m_flag_tg = conf->Get("EX0_ENABLE_TRIGERNUMBER", 0);
  if(!m_flag_ts && !m_flag_tg){
    EUDAQ_WARN("Both Timestamp and TriggerNumber are disabled. Now, Timestamp is enabled by default");
    m_flag_ts = false;
    m_flag_tg = true;
  }
}

//----------DOC-MARK-----BEG*RUN-----DOC-MARK----------
void FERSProducer::DoStartRun(){
  m_exit_of_run = false;
  // here the hardware is told to startup

  HV_Get_Vmon(handle, &vmon);
  HV_Get_Imon(handle, &imon);
  printf("ecco i valori di Vmon e Imon allo start run: %f \t %f \n",vmon,imon);
  printf("----- ORA ACCENDO L'HV ----- \n");
  Sleep(100);
  HV_Set_OnOff(handle, 1);
  HV_Get_Vmon(handle, &vmon);
  HV_Get_Imon(handle, &imon);
  printf("ecco i valori di Vmon e Imon allo start run: %f \t %f \n",vmon,imon);
}

//----------DOC-MARK-----BEG*STOP-----DOC-MARK----------
void FERSProducer::DoStopRun(){
  m_exit_of_run = true;
  HV_Get_Vmon(handle, &vmon);
  HV_Get_Imon(handle, &imon);
  printf("ecco i valori di Vmon e Imon allo stop run: %f \t %f \n",vmon,imon);
  printf("----- ORA SPENGO L'HV ----- \n");
  Sleep(100);
  HV_Set_OnOff(handle, 0);
  HV_Get_Vmon(handle, &vmon);
  HV_Get_Imon(handle, &imon);
  printf("ecco i valori di Vmon e Imon allo stop run: %f \t %f \n",vmon,imon);
}

//----------DOC-MARK-----BEG*RST-----DOC-MARK----------
void FERSProducer::DoReset(){
  m_exit_of_run = true;
  if(m_file_lock){
#ifndef _WIN32
    flock(fileno(m_file_lock), LOCK_UN);
#endif
    fclose(m_file_lock);
    m_file_lock = 0;
  }
  m_ms_busy = std::chrono::milliseconds();
  m_exit_of_run = false;
}

//----------DOC-MARK-----BEG*TER-----DOC-MARK----------
void FERSProducer::DoTerminate(){
  m_exit_of_run = true;
  if(m_file_lock){
    fclose(m_file_lock);
    m_file_lock = 0;
  }
  
  HV_Get_Vmon(handle, &vmon);
  HV_Get_Imon(handle, &imon);
  printf("ecco i valori di Vmon e Imon al terminate run: %f \t %f \n",vmon,imon);
  printf("----- ORA SPENGO NUOVAMENTE L'HV ----- \n");
  Sleep(100);
  HV_Set_OnOff(handle, 0);
  HV_Get_Vmon(handle, &vmon);
  HV_Get_Imon(handle, &imon);
  printf("ecco i valori di Vmon e Imon al terminate run: %f \t %f \n",vmon,imon);
}

//----------DOC-MARK-----BEG*LOOP-----DOC-MARK----------
void FERSProducer::RunLoop(){

  
  
  //ScanThreshold(handle);
  //ConfigureFERS(handle, CFG_HARD);
  
  auto tp_start_run = std::chrono::steady_clock::now();
  uint32_t trigger_n = 0;
  uint8_t x_pixel = 16;
  uint8_t y_pixel = 16;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> position(0, x_pixel*y_pixel-1);
  std::uniform_int_distribution<uint32_t> signal(0, 255);
  while(!m_exit_of_run){
    sleep(0.001);
    HV_Get_Vmon(handle, &vmon);
    HV_Get_Imon(handle, &imon);
    printf("Vmon e Imon nel run: %f \t %f \n",vmon,imon);
    
    auto ev = eudaq::Event::MakeUnique("FERSRaw");
    ev->SetTag("Plane ID", std::to_string(m_plane_id));
    auto tp_trigger = std::chrono::steady_clock::now();
    auto tp_end_of_busy = tp_trigger + m_ms_busy;
    if(m_flag_ts){
      std::chrono::nanoseconds du_ts_beg_ns(tp_trigger - tp_start_run);
      std::chrono::nanoseconds du_ts_end_ns(tp_end_of_busy - tp_start_run);
      ev->SetTimestamp(du_ts_beg_ns.count(), du_ts_end_ns.count());
    }
    if(m_flag_tg)
      ev->SetTriggerN(trigger_n);

    std::vector<uint8_t> hit(x_pixel*y_pixel, 0);
    hit[position(gen)] = signal(gen);
    std::vector<uint8_t> data;
    data.push_back(x_pixel);
    data.push_back(y_pixel);
    data.insert(data.end(), hit.begin(), hit.end());
    
    uint32_t block_id = m_plane_id;
    ev->AddBlock(block_id, data);
    SendEvent(std::move(ev));
    trigger_n++;
    std::this_thread::sleep_until(tp_end_of_busy);
  }
}
//----------DOC-MARK-----END*IMP-----DOC-MARK----------
