#include "tarmac_plugin.h"

//************************************************************************************************
// constructor, destructor, init-method...
//************************************************************************************************

tarmac_plugin::tarmac_plugin() {
  output_file = "my_test.tarmac";
  icount = 0;
}

tarmac_plugin::~tarmac_plugin() {
  outfile.close();
}

bool tarmac_plugin::Init(State *cpus, int num_cores, std::string cmdline_options) {
  if (cmdline_options.size() > 0) {
    output_file = cmdline_options;
    if (output_file.find("tarmac") == std::string::npos)
      output_file += ".tarmac";
  }
  
  std::cout << "tarmac output file: '" << output_file << "'" << std::endl;

  outfile.open(output_file);

  for (int i = 0; i < num_cores; i++) {
    core_clocks.push_back(0);
  }
  
  // output initial state for each cpu if 'enhanced' tarmac...

  return true;
}

//************************************************************************************************
// C functions to use to get/discard instance of this class...
//************************************************************************************************

extern "C" void *get_freerun_handle() {
  return (void *) new tarmac_plugin;
};

extern "C" void discard_freerun_handle(void *the_handle) {
  delete (tarmac_plugin *) the_handle;
};

//************************************************************************************************
// some formatting utils...
//************************************************************************************************

std::string hex_b(unsigned int t) {
  char tbuf[1024];
  sprintf(tbuf,"%x",t);
  return string(tbuf);
}

std::string hex_u(unsigned int t) {
  char tbuf[1024];
  sprintf(tbuf,"%08x",t);
  return string(tbuf);
}

std::string hex_ull(unsigned long long t) {
  char tbuf[1024];
  sprintf(tbuf,"%016llx",t);
  return string(tbuf);
}

std::string my_tolower(std::string src) {
  char tbuf[1024];
  for (unsigned int i = 0; i < src.size(); i++) {
    tbuf[i] = tolower(src[i]);
    tbuf[i+1] = '\0';
  }
  return tbuf;
}

//************************************************************************************************
// PostStep - called after each instruction has been simulated...
//************************************************************************************************

a64simUserSpace::FreeRun::outcome tarmac_plugin::PostStep(State *cpu, Memory *memory, Packet *ipacket, STEP_RESULT step_result) {
  if (step_result != NO_SIM_ERROR)
    return SUCCESS;
  
  icount += 1;

  // output instruction record...

  unsigned int core_id = cpu->GetID();
  unsigned int start_clock = core_clocks[core_id];
  
  outfile << start_clock << " clk " << core_id << " IT (" << icount << ") "
	  << hex_ull(ipacket->PC.Value()) << " " << hex_u(ipacket->Opcode) << std::dec << " A el"
	  << ipacket->Pstate.EL() << " " << (cpu->SCR_EL3.NS()==1 ? "NS" : "S") << " : " << ipacket->Disassembly();

  core_clocks[cpu->GetID()] = cpu->Clock();
  
  // output source registers if 'enhanced' tarmac...

  // output memory reads if 'enhanced' tarmac...

  // output pstate if its changed...

  if (ipacket->NextPstate.Value() != ipacket->Pstate.Value()) {
    if (ipacket->NextPstate.SP() != ipacket->Pstate.SP())
      outfile << start_clock << " clk " << core_id << " R sp " << hex_b(ipacket->NextPstate.SP()) << " : register write\n";
    if (ipacket->NextPstate.CurrentEL() != ipacket->Pstate.CurrentEL())
      outfile << start_clock << " clk " << core_id << " R currentel " << hex_b(ipacket->NextPstate.CurrentEL()) << " : register write\n";
    if (ipacket->NextPstate.NZCV() != ipacket->Pstate.NZCV())
      outfile << start_clock << " clk " << core_id << " R nzcv " << hex_b(ipacket->NextPstate.NZCV()<<28) << " : register write\n";
    if (ipacket->NextPstate.DAIF() != ipacket->Pstate.DAIF())
      outfile << start_clock << " clk " << core_id << " R daif " << hex_b(ipacket->NextPstate.DAIF()) << " : register write\n";
  }
  
  // output destination registers...

  // do asimd last...
  
  for (vector<struct RegDep>::iterator i = ipacket->destRegDep.begin(); i != ipacket->destRegDep.end(); i++) {
     int type = (*i).TYPE();
     int rid = (*i).ID();
     unsigned long long rval = (*i).RVAL();
     switch(type) {
       case DEP_GP:     outfile << start_clock << " clk " << core_id << " R r" << rid << " " << hex_ull(rval) << " : register write\n";
                        break;
       case DEP_SP:     outfile << start_clock << " clk " << core_id << " R sp_el" << rid << " " << hex_ull(rval) << " : register write\n";
                        break;
       case DEP_SPR:    outfile << start_clock << " clk " << core_id << " R " << my_tolower(cpu->SystemRegisterName(rid)) << " " << hex_ull(rval)
				<< " : spr register write\n";
                        break;
       default: break;
     }
  }

  for (vector<struct RegDep>::iterator i = ipacket->destRegDep.begin(); i != ipacket->destRegDep.end(); i++) {
     int type = (*i).TYPE();
     int rid = (*i).ID();
     unsigned long long rval = (*i).RVAL();
     unsigned long long rval_hi = (*i).RVAL_HI();
     switch(type) {
       case DEP_FP_SP:
       case DEP_FP_DP:
       case DEP_ASIMD:  // float/vector reg...
	                if (rval_hi != 0)
	                  outfile << start_clock << " clk " << core_id << " R q" << rid << " " << hex_ull(rval_hi) << ":" << hex_ull(rval)
				  << " : register write\n";
	                else
	                  outfile << start_clock << " clk " << core_id << " R d" << rid << " " << hex_ull(rval) << " : register write\n";
                        break;
       default: break;
     }
  }

  if (ipacket->Next_FPSR.Value() != ipacket->FPSR.Value()) {
    outfile << start_clock << " clk " << core_id << " R fpsr " << hex_u(ipacket->Next_FPSR.Value()) << " : register write\n";
  }
  
  // output memory writes...
  
  for (vector<MemoryAccess>::iterator i = ipacket->mOpsMemory.begin(); i != ipacket->mOpsMemory.end(); i++) {
     if ((*i).direction==FOR_WRITE) {
        std::string mval = "";
        for (int j = 0; j < (*i).size; j++) {
           char tbuf[128];
           sprintf(tbuf,"%02x",(unsigned char) (*i).membuffer[j]);
           mval = mval + tbuf;
        }
	std::string excl   = (*i).exclusive ? "X" : "";
	std::string unpriv = (*i).type==UNPRIVILEGED ? "L" : "";
	std::string attribs = excl + unpriv;
        outfile << start_clock << " clk " << core_id << " MW" << (*i).size << attribs << " "
		<< std::hex << (*i).address << std::dec << " " << mval << "\n";
     }
  }

  return SUCCESS;
}

