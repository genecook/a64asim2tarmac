#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include "a64sim.h"
#include "a64simUserSpace.h"
  
class tarmac_plugin : a64simUserSpace::FreeRun {
 public:
  tarmac_plugin();
  ~tarmac_plugin();
  
  bool Init(State *cpus, int num_cores, std::string cmdline_options);
 
  FreeRun::outcome PostStep(State *cpu, Memory *memory, Packet *ipacket, STEP_RESULT step_result);

 private:
  std::string output_file;
  ofstream outfile;
  int icount;

  std::vector<unsigned int> core_clocks;
};


