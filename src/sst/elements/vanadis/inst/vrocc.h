#ifndef _H_VANADIS_ROCC_INST
#define _H_VANADIS_ROCC_INST

#include "inst/vinst.h"
#include "inst/vrocc.h"

#include <cstdio>

namespace SST {
namespace Vanadis {

class VanadisRoCCInstruction : public VanadisInstruction
{

public:
    VanadisRoCCInstruction(
        const uint64_t addr, const uint32_t hw_thr, const VanadisDecoderOptions* isa_opts, 
        const uint16_t rs1, const int16_t rs2, const uint16_t rd, const bool xd, const bool xs1, 
        const bool xs2, uint32_t func_code7, uint8_t accelerator_id) :
        VanadisInstruction(addr, hw_thr, isa_opts, 2, 1, 2, 1, 0, 0, 0, 0)
    {

        isa_int_regs_in[0] = rs1;
        isa_int_regs_in[1] = rs2;
        isa_int_regs_out[0] = rd;
        
        func7 = func_code7;
    }

    VanadisRoCCInstruction* clone() { return new VanadisRoCCInstruction(*this); }

    virtual VanadisFunctionalUnitType getInstFuncType() const { return INST_ROCC; }

    virtual const char* getInstCode() const { return "RoCC"; }

    virtual void printToBuffer(char* buffer, size_t buffer_size)
    {
        snprintf(buffer, buffer_size, "RoCC");
    }

    virtual void execute(SST::Output* output, VanadisRegisterFile* regFile) {
        markExecuted(); 
    }

    virtual void getRegisterValues(SST::Output* output, VanadisRegisterFile* regFile, uint64_t* rs1, uint64_t* rs2) {
        (*rs1) = regFile->getIntReg<uint64_t>(phys_int_regs_in[0]);
		(*rs2) = regFile->getIntReg<uint64_t>(phys_int_regs_in[1]);
    }

    uint8_t func7;
};

} // namespace Vanadis
} // namespace SST

#endif
