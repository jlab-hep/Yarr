#ifndef FEI4
#define FEI4

// #################################
// # Author: Timon Heim
// # Email: timon.heim at cern.ch
// # Project: Yarr
// # Description: FE-I4 Library
// # Comment: FEI4 Base class
// ################################

#include <iostream>

#include "TxCore.h"
#include "Fei4Cmd.h"
#include "Fei4Cfg.h"

enum MASK_STAGE {
    MASK_1  = 0xFFFFFFFF,
    MASK_2  = 0x55555555,
    MASK_4  = 0x11111111,
    MASK_8  = 0x01010101,
    MASK_16 = 0x00010001,
    MASK_32 = 0x00000001,
    MASK_NONE    = 0x00000000
};

enum DC_MODE {
    SINGLE_DC = 0x0,
    QUAD_DC = 0x1,
    OCTA_DC = 0x2,
    ALL_DC = 0x3
};

class Fei4 : public Fei4Cfg, public Fei4Cmd {
    public:
        Fei4(TxCore *arg_core, unsigned chipId);

        void configure();
        void configurePixels(unsigned lsb=0, unsigned msb=Fei4PixelCfg::n_Bits);

        void setRunMode(bool mode=true) {
            runMode(chipId, mode);
        }

        void initMask(enum MASK_STAGE mask);
        void initMask(uint32_t mask);
        void shiftMask();
        void loadIntoShiftReg(unsigned pixel_latch);
        void loadIntoPixel(unsigned pixel_latch);
        void shiftByOne();

        TxCore* getTxCore() {
            return core;
        }

        template<typename T, unsigned mOffset, unsigned bOffset, unsigned mask, bool msbRight>
            void writeRegister(Field<T, mOffset, bOffset, mask, msbRight> Fei4GlobalCfg::*ref, uint16_t cfgBits){
                setValue(ref, cfgBits);
                writeRegister(ref);
            }

        template<typename T, unsigned mOffset, unsigned bOffset, unsigned mask, bool msbRight>
            void writeRegister(Field<T, mOffset, bOffset, mask, msbRight> Fei4GlobalCfg::*ref){
                wrRegister(chipId, getAddr(ref), cfg[getAddr(ref)]);
            }
        
        template<typename T, unsigned mOffset, unsigned bOffset, unsigned mask, bool msbRight>
            T readRegister(Field<T, mOffset, bOffset, mask, msbRight> Fei4GlobalCfg::*ref){
                return getValue(ref);
            }
    private:
        unsigned chipId;
};

#endif
