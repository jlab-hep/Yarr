#include "RxCore.h"

RxCore::RxCore(SpecController *arg_spec) {
    spec = arg_spec;
    verbose = false;
}

void RxCore::setRxEnable(uint32_t value) {
    if (verbose)
        std::cout << __PRETTY_FUNCTION__ << " : Value 0x" << std::hex << value << std::dec << std::endl;
    spec->writeSingle(RX_ADDR | RX_ENABLE, value);
}

void RxCore::maskRxEnable(uint32_t value, uint32_t mask) {
    uint32_t tmp = spec->readSingle(RX_ADDR | RX_ENABLE);
    tmp &= ~mask;
    value |= tmp;
    if (verbose)
        std::cout << __PRETTY_FUNCTION__ << " : Value 0x" << std::hex << value << std::dec << std::endl;
    spec->writeSingle(RX_ADDR | RX_ENABLE, value);
}

RawData* RxCore::readData() {
    uint32_t dma_addr = getStartAddr();
    uint32_t dma_count = getDataCount();
    if (dma_count > 0 && dma_count < (251*256)) {
        uint32_t real_dma_count = dma_count;
        if (dma_count < 32) dma_count = 32;
        if (verbose)
            std::cout << __PRETTY_FUNCTION__ << " : Addr 0x" << std::hex <<
                dma_addr << " ,Count " << std::dec << real_dma_count << std::endl;
        uint32_t *buf = new uint32_t[dma_count];
        for (unsigned i=0; i<dma_count; i++) buf[i] = 0;
        if (spec->readDma(dma_addr, buf, real_dma_count)) {
            std::cout << __PRETTY_FUNCTION__ << std::hex << "0x" << dma_addr << " 0x" << dma_count << std::dec << std::endl;
            exit(1);
            return NULL;
        }
        return new RawData(dma_addr, buf, real_dma_count);
    } else {
        return NULL;
    }
}

uint32_t RxCore::getDataRate() {
    return spec->readSingle(RX_BRIDGE | RX_DATA_RATE);
}

uint32_t RxCore::getStartAddr() {
    return spec->readSingle(RX_BRIDGE | RX_START_ADDR);
}

uint32_t RxCore::getDataCount() {
    return spec->readSingle(RX_BRIDGE | RX_DATA_COUNT);
}