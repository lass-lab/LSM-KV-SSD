#include "iLSM.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/nvme_ioctl.h>
#include <fcntl.h>
#include <cstring>
#include <cstdint>
#include <stdlib.h>
#include <limits.h>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>

#ifdef DEBUG_iLSM
#include <iostream>
#include <cstdio>
#include <inttypes.h>
#endif

using namespace std;

const unsigned int PAGE_SIZE = 4096;       // Memory page size
const unsigned int MAX_BUFLEN = 524288;    // 512KB (MDTS)
const unsigned int NSID = 60365824;        // Check via dmesg

int iLSM::DB::Open(const std::string &dev)
{
    int err;
    err = open(dev.c_str(), O_RDONLY);
    if (err < 0)
        return -1; // fail to open
    fd_ = err;
    struct stat nvme_stat;
    err = fstat(fd_, &nvme_stat);
    if (err < 0)
        return -1;
    if (!S_ISCHR(nvme_stat.st_mode) && !S_ISBLK(nvme_stat.st_mode))
        return -1;
    
    return 0;
}

int iLSM::DB::Put(const std::string &key, const std::string &value)
{
    auto st = chrono::high_resolution_clock::now();
    int ret = _Put(key, value);
    auto ed = chrono::high_resolution_clock::now();
    chrono::nanoseconds d = ed-st;
    finishOp(iLSMOp::Put, d);
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// BandSlim ////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// Transfer Modes (KVSSD-PRP, PIGGY-FG, ADAPT-OPT)
// * Threshold of ADAPT is a configurable parameter
#define KVSSD 1
#define PIGGY 16384
#define ADAPT 127
// * Select the Transfer Mode using below macro
// #define TRANSFER_MODE KVSSD
// #define TRANSFER_MODE PIGGY
#define TRANSFER_MODE ADAPT
//////////////////////////////////////////////////////////////////////////////////////////
// * Enable the combination transfer method for PRP-based transfer
#if TRANSFER_MODE == ADAPT
        #define ADAPT_COMBI 
#endif
//////////////////////////////////////////////////////////////////////////////////////////
// * Macro function for piggybacking value (be sure to wrap up this macro with {,})
#define PIGGYBACK_VALUE(cdw, ptr, left, step) \
        memcpy(&cdw, ptr, (left < step ? left : step)); \
        ptr = (char*)ptr + step; \
        left -= step;
// * Macro function for checking value size
#define IS_LEFT(left) left > 0 && left <= 16384
//////////////////////////////////////////////////////////////////////////////////////////
int iLSM::DB::_Put(const std::string &key, const std::string &value)
{
		std::cout << std::hex << value << '\n';
    int err = 0;
    uint32_t result;
    uint32_t cdw2, cdw3, cdw8, cdw9, cdw10, cdw11, cdw12, cdw13, cdw14, cdw15; uint64_t cdw4_5, cdw6_7; 
    cdw2 = cdw3 = cdw8 = cdw9 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0; cdw4_5 = cdw6_7 = 0;

    // CDW2 CDW3 CDW14 CDW15 -> Key (we only use CDW2)
    // CDW11's 1Byte -> Key Size (we rather specify in CDW3) 
    memcpy(&cdw2, key.c_str(), 4); cdw3 = 4; 
    
    // CDW10 -> Value Size
    uint32_t value_size = value.size();
    cdw10 = value_size;
    
    // PRP Entry Base Address
    void *data = NULL;//, *data_start;
    unsigned int data_len = value_size;
    unsigned int nlb = (data_len - 1) / PAGE_SIZE;
    data_len = (nlb + 1) * PAGE_SIZE;
    if (posix_memalign(&data, PAGE_SIZE, data_len))
        return -ENOMEM;
    // data_start = data;
    memcpy(data, value.c_str(), value.size()); 

    // Select the Transfer Mode
    if (value_size > TRANSFER_MODE) { // (1) Page-Unit DMA via PRP
        cdw12 = 0 | (0xFFFF & ((value.size() - 1) / PAGE_SIZE));

#ifdef ADAPT_COMBI
        // (1-1) Combination transfer of Adaptive Value Transfer
        unsigned int prp_len = nlb * PAGE_SIZE;

        // (1-1-1) PRP-based transfer part
        err = nvme_passthru(NVME_CMD_KV_PUT, 0, 0, NSID, cdw2, cdw3,
            cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, data_len, data, result);
        cdw2 = cdw3 = cdw8 = cdw9 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0; cdw4_5 = cdw6_7 = 0;
        
        // (1-1-2) Piggyback-based transfer part
        data = (char*)data + prp_len; 
        value_size -= prp_len;
        while (prp_len != 0 && IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw2, data, value_size, 4) 
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw3, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw4_5, data, value_size, 8) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw6_7, data, value_size, 8) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw8, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw9, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw10, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw11, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw12, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw13, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw14, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw15, data, value_size, 4) }

            // BandSlim Transfer Command
            err = nvme_passthru_bandslim(NVME_CMD_KV_BANDSLIM_TRANSFER, 0, 0, NSID, 
                cdw2, cdw3, cdw4_5, cdw6_7, cdw8, cdw9, cdw10, 
                cdw11, cdw12, cdw13, cdw14, cdw15, result);

            cdw2 = cdw3 = cdw8 = cdw9 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0; cdw4_5 = cdw6_7 = 0;
        }
#else
        // (1-2) Naive NVMe PRP-based transfer
        err = nvme_passthru(NVME_CMD_KV_PUT, 0, 0, NSID, cdw2, cdw3,
            cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, data_len, data, result);
#endif	
    }
    else { // (2) Piggyback-based transfer
	// * We assume there's no value bigger than 16KB (for simple PoC)
        if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw4_5, data, value_size, 8) }
        if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw6_7, data, value_size, 8) }
        if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw8, data, value_size, 4) }
        if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw9, data, value_size, 4) }
        if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw11, data, value_size, 4) }
        if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw12, data, value_size, 4) }
        if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw13, data, value_size, 4) }
        
        // BandSlim Write Command
        err = nvme_passthru_bandslim(NVME_CMD_KV_BANDSLIM_WRITE, 0, 0, NSID, 
            cdw2, cdw3, cdw4_5, cdw6_7, cdw8, cdw9, cdw10, 
            cdw11, cdw12, cdw13, cdw14, cdw15, result);

        cdw2 = cdw3 = cdw8 = cdw9 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0; cdw4_5 = cdw6_7 = 0;
	
        // * Key (CDW2 CDW3 CDW14 CDW15) and Value Size (CDW10) are also used here
        // * Known Limitation: synchronous NVMe command submission is forced...
        while (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw2, data, value_size, 4) 
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw3, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw4_5, data, value_size, 8) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw6_7, data, value_size, 8) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw8, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw9, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw10, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw11, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw12, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw13, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw14, data, value_size, 4) }
            if (IS_LEFT(value_size)) { PIGGYBACK_VALUE(cdw15, data, value_size, 4) }

            // BandSlim Transfer Command
            err = nvme_passthru_bandslim(NVME_CMD_KV_BANDSLIM_TRANSFER, 0, 0, NSID, 
                cdw2, cdw3, cdw4_5, cdw6_7, cdw8, cdw9, cdw10, 
                cdw11, cdw12, cdw13, cdw14, cdw15, result);
            
            cdw2 = cdw3 = cdw8 = cdw9 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0; cdw4_5 = cdw6_7 = 0;
	}
    }
    //free(data_start);
    
    if (err < 0 || result != 0)
        return -1;
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// BandSlim ////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

#ifndef GET_FOR_SEEK_AND_NEXT_ILSM
int iLSM::DB::Seek(const unsigned int iter_id, const std::string &key, std::string &value)
{
    auto st = chrono::high_resolution_clock::now();
    int ret = _Seek(iter_id, key, value);
    auto ed = chrono::high_resolution_clock::now();
    chrono::nanoseconds d = ed-st;
    // s[cnt++] = d;
    finishOp(iLSMOp::Seek, d);
    return ret;
}
#else

int iLSM::DB::Seek(const unsigned int iter_id, const std::string &key, std::string &value)
{
    auto st = chrono::high_resolution_clock::now();
    int ret;
    memcpy((void*) &iter[iter_id].key, (void*) key.c_str(), 4); 
    unsigned int i = iter[iter_id].key;
    while (1) {
        std::string k((char *)&i, 4); 
        numGetofSeek++;
        ret = _Get(k, value);
        if (ret != -2) // No Such Key
            break;
        else
            i++;
    }

    iter[iter_id].key = i;

    auto ed = chrono::high_resolution_clock::now();
    chrono::nanoseconds d = ed-st;
    finishOp(iLSMOp::Seek, d);
    return ret;
}

#endif

int iLSM::DB::_Seek(const unsigned int iter_id, const std::string &key, std::string &value)
{
    void *data = NULL;
    unsigned int data_len = MAX_BUFLEN;
    unsigned int nlb = (MAX_BUFLEN-1) / PAGE_SIZE;
    if (posix_memalign(&data, PAGE_SIZE, data_len)) {
        return -ENOMEM;
    }
    memset(data, 0, data_len);
    int err;
    uint32_t result;
    uint32_t cdw2, cdw3, cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
    cdw2 = cdw3 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0;
    //    memcpy(&cdw10, key.c_str()+4, 4);
    //    memcpy(&cdw11, key.c_str(), 4);
    memcpy(&cdw10, key.c_str(), 4);
    cdw12 = 0 | (0xFFFF & nlb);
    cdw13 = iter_id;
    err = nvme_passthru(NVME_CMD_KV_ITER_SEEK, 0, 0, NSID, cdw2, cdw3, 
            cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, 
            data_len, data, result);

    if (err < 0) {
        // ioctl fail
#ifdef DEBUG_iLSM
        perror("ilsm seek");
#endif
        free(data);
        return -1;
    }

    if (err == 0x7C1) {
        // no such key
        free(data);
        return -2;
    }

    if (result > 0) { // value length
        value = std::string(static_cast<const char*>(data), (int)4);
    }  else {
        value = std::string();
    }
    free(data);
    return result;
}

#ifndef GET_FOR_SEEK_AND_NEXT_ILSM
int iLSM::DB::Next(const unsigned int iter_id, std::string &value)
{
    auto st = chrono::high_resolution_clock::now();
    int ret = _Next(iter_id, value);
    auto ed = chrono::high_resolution_clock::now();
    chrono::nanoseconds d = ed-st;
    finishOp(iLSMOp::Next, d);
    return ret;
}
#else
int iLSM::DB::Next(const unsigned int iter_id, std::string &value)
{
    auto st = chrono::high_resolution_clock::now();
    int ret;
    unsigned int i = iter[iter_id].key;
    i++;
    while (1) {
        std::string k((char *)&i, 4); 
        numGetofNext++;
        ret = _Get(k, value);
        if (ret != -2) // No Such Key
            break;
        else
            i++;
    }
    iter[iter_id].key = i;

    auto ed = chrono::high_resolution_clock::now();
    chrono::nanoseconds d = ed-st;
    finishOp(iLSMOp::Next, d);
    return ret;
}
#endif

int iLSM::DB::_Next(const unsigned int iter_id, std::string &value)
{
    void *data = NULL;
    unsigned int data_len = MAX_BUFLEN;
    unsigned int nlb = (MAX_BUFLEN-1) / PAGE_SIZE;
    std::string key="1234";
    if (posix_memalign(&data, PAGE_SIZE, data_len)) {
        return -ENOMEM;
    }
    memset(data, 0, data_len);
    int err;
    uint32_t result;
    uint32_t cdw2, cdw3, cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
    cdw2 = cdw3 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0;
    //    memcpy(&cdw10, key.c_str()+4, 4);
    //    memcpy(&cdw11, key.c_str(), 4);
    cdw10 = 0; // key
    cdw12 = 0 | (0xFFFF & nlb);
    cdw13 = iter_id;
    err = nvme_passthru(NVME_CMD_KV_ITER_NEXT, 0, 0, NSID, cdw2, cdw3, 
            cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, 
            data_len, data, result);

    if (err < 0) {
        // ioctl fail
#ifdef DEBUG_iLSM
        perror("ilsm next");
#endif
        free(data);
        return -1;
    }

    if (err == 0x7C1) {
        // no such key
        free(data);
        return -2;
    }

    if (result > 0) { // value length
        value = std::string(static_cast<const char*>(data), (int)4);
    }  else {
        value = std::string();
    }

    free(data);
    return result;
}

int iLSM::DB::Get(const std::string &key, std::string &value)
{
    auto st = chrono::high_resolution_clock::now();
    int ret = _Get(key, value);
    auto ed = chrono::high_resolution_clock::now();
    chrono::nanoseconds d = ed-st;
    finishOp(iLSMOp::Get, d);
    return ret;
}

int iLSM::DB::_Get(const std::string &key, std::string &value)
{
    void *data = NULL; // void *temp = NULL;
    unsigned int data_len = MAX_BUFLEN; unsigned int nlb = (MAX_BUFLEN-1) / PAGE_SIZE;
    // unsigned int lba, index, lba, tuple_offset, tuple_value_len, tuple_value_offset;
    
    if (posix_memalign(&data, PAGE_SIZE, data_len)) {
        return -ENOMEM;
    }
    memset(data, 0, data_len);
   
    int err;
    uint32_t result;  
    uint32_t cdw2, cdw3, cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
    cdw2 = cdw3 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0;
    //    memcpy(&cdw10, key.c_str()+4, 4);
    //    memcpy(&cdw11, key.c_str(), 4);
    memcpy(&cdw10, key.c_str(), 4);

    cdw12 = 0 | (0xFFFF & nlb);
    err = nvme_passthru(NVME_CMD_KV_GET, 0, 0, NSID, cdw2, cdw3,
            cdw10, cdw11, cdw12, cdw13, cdw14, cdw15,
            data_len, data, result);

    if (err < 0) {
        // ioctl fail
#ifdef DEBUG
        perror("ilsm get");
#endif
        free(data);
        return -1;
    }

    if (err == 0x7C1) {
        // no such key
        free(data);
        return -2;
    }
    else {
        value = std::string();
    }

    free(data);
    return result;
}

#ifndef GET_FOR_SEEK_AND_NEXT_ILSM
int iLSM::DB::CreateIter(unsigned int &iter_id)
{
    auto st = chrono::high_resolution_clock::now();
    int ret = _CreateIter(iter_id);
    auto ed = chrono::high_resolution_clock::now();
    chrono::nanoseconds d = ed-st;
    finishOp(iLSMOp::CreateIter, d);
    return ret;
}
#else
int iLSM::DB::CreateIter(unsigned int &iter_id)
{
    auto st = chrono::high_resolution_clock::now();
    int ret = _CreateIter(iter_id);

    if (ret >= 0)
        iter[iter_id].key = 0;
    
    auto ed = chrono::high_resolution_clock::now();
    chrono::nanoseconds d = ed-st;
    finishOp(iLSMOp::CreateIter, d);
    return ret;
}
#endif

int iLSM::DB::_CreateIter(unsigned int &iter_id)
{
    void *data = NULL;
    unsigned int data_len = 0;
    int err;
    uint32_t result;
    uint32_t cdw2, cdw3, cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
    cdw2 = cdw3 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0;
    //    memcpy(&cdw10, key.c_str()+4, 4);
    err = nvme_passthru(NVME_CMD_KV_ITER_CREATE_ITER, 0, 0, NSID, cdw2, cdw3, 
            cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, 
            data_len, data, result);
//    fprintf(stderr, "iLSM::DB::CreateIter => err=%d, result=%d\n", err, result);
    if (err < 0) {
        // ioctl fail
#ifdef DEBUG_iLSM
        perror("ilsm create iter");
#endif
        return -1;
    } 
    
    if (err == 0x7C1) {
        // no such key
        return -2;
    }
    
    iter_id = result;
    return 0;
}

#ifndef GET_FOR_SEEK_AND_NEXT_ILSM
int iLSM::DB::DestroyIter(const unsigned int iter_id)
{
    auto st = chrono::high_resolution_clock::now();
    int ret = _DestroyIter(iter_id);
    auto ed = chrono::high_resolution_clock::now();
    chrono::nanoseconds d = ed-st;
    finishOp(iLSMOp::DestroyIter, d);
    return ret;
}
#else
int iLSM::DB::DestroyIter(const unsigned int iter_id)
{
    auto st = chrono::high_resolution_clock::now();
    int ret = _DestroyIter(iter_id);

    if (ret >= 0)
       iter[iter_id].key = 0; 

    auto ed = chrono::high_resolution_clock::now();
    chrono::nanoseconds d = ed-st;
    finishOp(iLSMOp::DestroyIter, d);
    return ret;
}
#endif

int iLSM::DB::_DestroyIter(const unsigned int iter_id)
{
    void *data = NULL;
    unsigned int data_len = 0;
    int err;
    uint32_t result;
    uint32_t cdw2, cdw3, cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
    cdw2 = cdw3 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0;
    //    memcpy(&cdw10, key.c_str()+4, 4);
    //    memcpy(&cdw11, key.c_str(), 4);
    cdw13 = iter_id;
    err = nvme_passthru(NVME_CMD_KV_ITER_DESTROY_ITER, 0, 0, NSID, cdw2, cdw3, 
            cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, 
            data_len, data, result);
    if (err < 0) {
        // ioctl fail
#ifdef DEBUG_iLSM
        perror("ilsm destroy iter");
#endif
        return -1;
    }

    if (err == 0x7C1) {
        // no such key
        return -2;
    }

    return result;
}

int iLSM::DB::nvme_passthru(uint8_t opcode,
        uint8_t flags, uint16_t rsvd,
        uint32_t nsid, uint32_t cdw2, uint32_t cdw3, uint32_t cdw10, uint32_t cdw11,
        uint32_t cdw12, uint32_t cdw13, uint32_t cdw14, uint32_t cdw15,
        uint32_t data_len, void *data, uint32_t &result)
{
    auto st = chrono::high_resolution_clock::now();
    struct nvme_passthru_cmd cmd = {
        .opcode		= opcode,
        .flags		= flags,
        .rsvd1		= rsvd,
        .nsid		= nsid,
        .cdw2		= cdw2,
        .cdw3		= cdw3,
        .metadata	= (uint64_t)(uintptr_t) NULL,
        .addr		= (uint64_t)(uintptr_t) data,
        .metadata_len	= 0,
        .data_len	= data_len,
        .cdw10		= cdw10,
        .cdw11		= cdw11,
        .cdw12		= cdw12,
        .cdw13		= cdw13,
        .cdw14		= cdw14,
        .cdw15		= cdw15,
        .timeout_ms	= 0,
        .result		= 0,
    };
    int err;
#ifdef DEBUG_iLSM
    {/*
        fprintf(stderr, "-- iLSM::DB::nvme_passthru --\n");
        fprintf(stderr, "opcode       : %02x\n", cmd.opcode);
        fprintf(stderr, "flags        : %02x\n", cmd.flags);
        fprintf(stderr, "rsvd1        : %04x\n", cmd.rsvd1);
        fprintf(stderr, "nsid         : %08x\n", cmd.nsid);
        fprintf(stderr, "cdw2         : %08x\n", cmd.cdw2);
        fprintf(stderr, "cdw3         : %08x\n", cmd.cdw3);
        fprintf(stderr, "data_len     : %08x\n", cmd.data_len);
        fprintf(stderr, "metadata_len : %08x\n", cmd.metadata_len);
        fprintf(stderr, "addr         : %llx\n", cmd.addr);
        fprintf(stderr, "metadata     : %llx\n", cmd.metadata);
        fprintf(stderr, "cdw10        : %08x\n", cmd.cdw10);
        fprintf(stderr, "cdw11        : %08x\n", cmd.cdw11);
        fprintf(stderr, "cdw12        : %08x\n", cmd.cdw12);
        fprintf(stderr, "cdw13        : %08x\n", cmd.cdw13);
        fprintf(stderr, "cdw14        : %08x\n", cmd.cdw14);
        fprintf(stderr, "cdw15        : %08x\n", cmd.cdw15);
        fprintf(stderr, "timeout_ms   : %08x\n", cmd.timeout_ms); */
    }
#endif
    {
#ifdef THREAD_SAFE_ILSM
        lock_guard<mutex> l(ioctl_mtx);
#endif
        err = ioctl(fd_, NVME_IOCTL_IO_CMD, &cmd);
    }

    if ((!err && opcode < NVME_CMD_KV_LAST) || (opcode == NVME_CMD_KV_GET)) {
        result = cmd.result; 
        auto ed = chrono::high_resolution_clock::now();
        chrono::nanoseconds d = ed-st;
        finishPassthru(static_cast<enum NvmeOpcode>(opcode), d);
    }
    if (opcode == NVME_CMD_KV_LAST)
        result = cmd.result;  // For reporting #ofSectors

    return err;
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// BandSlim ////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
int iLSM::DB::nvme_passthru_bandslim(
        uint8_t opcode, uint8_t flags, uint16_t rsvd, uint32_t nsid, 
        uint32_t cdw2, uint32_t cdw3, uint64_t cdw4_5, uint64_t cdw6_7, 
	uint32_t cdw8, uint32_t cdw9, uint32_t cdw10, uint32_t cdw11,
        uint32_t cdw12, uint32_t cdw13, uint32_t cdw14, uint32_t cdw15,
        uint32_t &result)
{
    // Make a comment on the right below line if you've modified the NVMe driver 
    cdw4_5 = cdw6_7 = 0;
    auto st = chrono::high_resolution_clock::now();
    struct nvme_passthru_cmd cmd = {
        .opcode		= opcode,
        .flags		= flags,
        .rsvd1		= rsvd,
        .nsid		= nsid,
        .cdw2		= cdw2,
        .cdw3		= cdw3,
        .metadata	= cdw4_5,
        .addr		= cdw6_7,
        .metadata_len	= cdw8,
        .data_len	= cdw9,
        .cdw10		= cdw10,
        .cdw11		= cdw11,
        .cdw12		= cdw12,
        .cdw13		= cdw13,
        .cdw14		= cdw14,
        .cdw15		= cdw15,
        .timeout_ms	= 0,
        .result		= 0,
    };
    int err;
    {
#ifdef THREAD_SAFE_ILSM
        lock_guard<mutex> l(ioctl_mtx);
#endif
        err = ioctl(fd_, NVME_IOCTL_IO_CMD, &cmd);
    }
    if ((!err && opcode < NVME_CMD_KV_LAST) || (opcode == NVME_CMD_KV_GET) ||
        (opcode == NVME_CMD_KV_BANDSLIM_WRITE) || (opcode == NVME_CMD_KV_BANDSLIM_TRANSFER)) {
        result = cmd.result; 
        auto ed = chrono::high_resolution_clock::now();
        chrono::nanoseconds d = ed-st;
        finishPassthru(static_cast<enum NvmeOpcode>(opcode), d);
    }
    if (opcode == NVME_CMD_KV_LAST)
        result = cmd.result;  // For reporting #ofSectors

    return err;
}
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// BandSlim ////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

void iLSM::DB::finishOp(const enum iLSMOp op, chrono::nanoseconds &d)
{
#ifdef THREAD_SAFE_ILSM
    lock_guard<mutex> l(op_stat_mtx);
#endif
    int idx = static_cast<int>(op);
    op_stat.t[idx] += d;
    op_stat.c[idx] ++;
}

void iLSM::DB::finishPassthru(const enum NvmeOpcode opcode, chrono::nanoseconds &d)
{
#ifdef THREAD_SAFE_ILSM
    lock_guard<mutex> l(passthru_stat_mtx);
#endif
    int idx = opcode - NvmeOpcode::NVME_CMD_KV_PUT;
    passthru_stat.t[idx] += d;
    passthru_stat.c[idx] ++;
}

string iLSM::DB::Report()
{
#ifdef THREAD_SAFE_ILSM
    lock_guard<mutex> l(report_mtx);
#endif
    string msg;
    for (int i = 0; i < static_cast<int>(iLSMOp::LAST); i++) {
        
        if (!op_stat.t[i].count())
            continue;

        switch(static_cast<enum iLSMOp>(i)) {
            case iLSMOp::Put:
                msg += "[Put] ";
                break;
            case iLSMOp::Get:
                msg += "[Get] ";
                break;
            case iLSMOp::CreateIter:
                msg += "[CreateIter] ";
                break;
            case iLSMOp::Seek:
                msg += "[Seek] ";
                break;
            case iLSMOp::Next:
                msg += "[Next] ";
                break;
            case iLSMOp::DestroyIter:
                msg += "[DestroyIter] ";
                break;
            default:
                msg += "[????] ";
        }
        double total, avg;
        total = (double) op_stat.t[i].count() / 1000;
        avg = total / op_stat.c[i];
        msg += "Elapse Time " + to_string (total) + " us / " + to_string(op_stat.c[i]) + " = Average " + to_string(avg) + " us \n";
    }
    for (int i = 0; i < (static_cast<int>(NvmeOpcode::NVME_CMD_KV_LAST) - 0xA0); i++) {

        if (!passthru_stat.t[i].count())
            continue;

        switch(static_cast<enum NvmeOpcode>(i + 0xA0)) {
            case NVME_CMD_KV_PUT:
                msg += "[NVME_CMD_KV_PUT] ";
                break;
            //////////////////////////////////////////////////////////////////////////////////////
            ////////////////////////////////////// BandSlim //////////////////////////////////////
            //////////////////////////////////////////////////////////////////////////////////////
            case NVME_CMD_KV_BANDSLIM_WRITE:
                msg += "[NVME_CMD_KV_BANDSLIM_WRITE] ";
                break;
            case NVME_CMD_KV_BANDSLIM_TRANSFER:
                msg += "[NVME_CMD_KV_BANDSLIM_TRANSFER] ";
                break;
            //////////////////////////////////////////////////////////////////////////////////////
            ////////////////////////////////////// BandSlim //////////////////////////////////////
            //////////////////////////////////////////////////////////////////////////////////////
            case NVME_CMD_KV_GET:
                msg += "[NVME_CMD_KV_GET] ";
                break;
            case NVME_CMD_KV_DELETE:
                msg += "[NVME_CMD_KV_DELETE] ";
                break;
            case NVME_CMD_KV_ITER_CREATE_ITER:
                msg += "[NVME_CMD_KV_ITER_CREATE_ITER] ";
                break;
            case NVME_CMD_KV_ITER_SEEK:
                msg += "[NVME_CMD_KV_ITER_SEEK] ";
                break;
            case NVME_CMD_KV_ITER_NEXT:
                msg += "[NVME_CMD_KV_ITER_NEXT] ";
                break;
            case NVME_CMD_KV_ITER_DESTROY_ITER:
                msg += "[NVME_CMD_KV_ITER_DESTROY_ITER] ";
                break;
            default:
                msg += "[???] ";
        }

        double total, avg;
        total = (double) passthru_stat.t[i].count() / 1000;
        avg = total / passthru_stat.c[i];
        msg += "Elapse Time " + to_string (total) + " us / " + to_string(passthru_stat.c[i]) + " = Average " + to_string(avg) + " us \n";

    }
 
    {
        void *data = NULL;
        unsigned int data_len = 0;
        int err;
        uint32_t result;
        uint32_t cdw2, cdw3, cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
        cdw2 = cdw3 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0;
        err = nvme_passthru(NVME_CMD_KV_LAST, 0, 0, NSID, cdw2, cdw3, 
            cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, 
                data_len, data, result);
        if (err < 0) {
            // ioctl fail
            perror("ilsm report failed");
        }
        // fprintf(stderr, "Total used sectors (space amplification): %u\n", result);
    }
    return msg;
}


//////////////////////////////////////////////////////////////////////////////// BulkDB
// #define NO_SGVL_LEVEL1                  // Turn on if you want no L1-SGVL
// #define SMALL_PUT_FLAG     9999          // PUT for a small value
// #define BIG_GET_FLAG       0x7C0         // GET for a big value
//
///* < Write-back Cache with Converging Dual-Stack > */
// * [ val1, val2, val3, ..., tup3, tup2, tup1, meta ]
// * val: a value with a variable-length size (word aligned)
// * tup: a tuple = (key(16B), value_size(8B), offset_ptr(8B))
// * meta: a metadata block = (#_of_pairs(16B), min_key(16B), max_key(16B))
// const unsigned int TUPLE_SIZE = 32;      // 32B
// const unsigned int META_SIZE = 64;       // 48B -> 64B (for NVM features)
// unsigned int wc_min_key = UINT_MAX;      // Minimum key inserted to write-back cache
// unsigned int wc_max_key = 0;             // Maximum key inserted to write-back cache
//
/////////////////////////////////////////////////////////////////////////////////// KVCXD
// #define DEBUG_BULKDB
// const unsigned int CACHE_SIZE = 16*1024; // 16KB
// void *write_cache = NULL;                // Pointer to the write-back cache
// unsigned int wc_left = CACHE_SIZE;       // Space left in write-back cache
// unsigned int wc_inserted = 0;            // # of pairs inserted to write-back cache
// unsigned int wc_value_log = 0;           // Starting address of value to be inserted
//
///* Initialize the host-side batch for CXL.mem emulation */
//void iLSM::DB::wc_init(void) {
//    if (posix_memalign(&write_cache, PAGE_SIZE, CACHE_SIZE)) {
//        cout << "[KVCXD] CXL.mem emulation -> host-side batch construction error!\n";
//        exit(-1);
//    }
//    wc_left = CACHE_SIZE;
//    wc_inserted = 0;
//    wc_value_log = 0;
//#ifdef DEBUG_BULKDB
//    cout << "[KVCXD] CXL.mem emulation -> host-side batch construction done.\n";
//#endif
//}
//
/* Thresholds based on transfer mode */
//#define DUAL_MODE 4096
//#define CXL_MEM_ONLY CACHE_SIZE
//#define CXL_IO_ONLY 1
//
/* Intentional delays for sleeping 200ns (CXL driver & controller emulation) */
//long long delay_ns = 200;
//auto delay_duration = std::chrono::nanoseconds(delay_ns);
//
//#define CXL_PACKING
/* Shared buffer based on CXL.mem emulation -> simple host-side batching */
//int iLSM::DB::wc_insert(const std::string &value, uint32_t *cdw14) {
//    unsigned int value_len = value.size();
//    unsigned int value_len_align = ((value_len + 3) / 4) * 4;         // Word-alignment
//    
//    if (wc_left == 0 || value_len_align > wc_left)       // Original code for evaluation
//        wc_flush();
//
//#ifdef CXL_PACKING
//    wc_left -= value_len_align;
//#else 
//    wc_left -= 4096;
//#endif
//    *cdw14 = wc_value_log;             // Offset for current key-value pair
//    wc_value_log += value_len_align;
//    wc_inserted++;
//
//    return 0;        // Host-side batch processing completed
//}
//
//// #define NAND_IO_DISABLE
///* Flush the host-side batch as a NAND page, and then initialize it */
//void iLSM::DB::wc_flush(void) {
//#ifndef NAND_IO_DISABLE
//    unsigned int nlb = (CACHE_SIZE-1) / PAGE_SIZE; int err;
//    uint32_t result, cdw2, cdw3, cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
//
//    // Write (flush) to the device. It will trigger 4 unit DMAs!
//    cdw2 = cdw3 = cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0;
//    cdw12 = 0 | (0xFFFF & nlb); cdw13 = CACHE_SIZE;
//    err = nvme_passthru(NVME_CMD_KV_FLUSH, 0, 0, NSID, cdw2, cdw3,
//        cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, CACHE_SIZE, write_cache, result);
//    // err = nvme_passthru(NVME_CMD_KV_FLUSH, 0, 0, NSID, cdw2, cdw3,
//    //     cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, 0, NULL, result);
//#endif
//    // Initialize the write-back cache (but the space is identical)
//    wc_left = CACHE_SIZE;
//    wc_inserted = 0;
//    wc_value_log = 0;
//#ifndef NAND_IO_DISABLE
//    if (err < 0 || result != 0) {
//        cout << "[KVCXD] CXL.mem emulation -> host-side batch flush error!\n";
//        exit(-1);
//    }
//#endif
//}
/////////////////////////////////////////////////////////////////////////////////// KVCXD
//
///* Write-back functionality for data consistency */
//int iLSM::DB::wc_search(unsigned int target_key, std::string &value) {
//    unsigned int i, tuple_key, tuple_offset, tuple_value_len, tuple_value_offset;
//
//    // Qualifying the given key at the first stage
//    if (target_key < wc_min_key || target_key > wc_max_key) {
//#ifdef DEBUG_BULKDB
//        std::cout << "[BulkDB] Write-back cache search: unqualified (not found)\n";
//#endif
//        return 0;
//    }
//    // Iterate the stack of tuples in the write-back cache, starting from the top
//    for (i = wc_inserted; i >= 1; i--) {    // Search fresher one first for consistency
//        tuple_offset = CACHE_SIZE - META_SIZE - TUPLE_SIZE * i;
//
//        // Get the key field of current tuple
//        tuple_value_len = *(unsigned int *)((char *)write_cache + tuple_offset + 16);
//        // Check whether the key is invalidated or not
//        if (tuple_value_len == 0) continue;  // If invalid, then skip it
//
//        tuple_key = *(unsigned int *)((char *)write_cache + tuple_offset);
//#ifdef DEBUG_BULKDB
//        std::cout << "[BulkDB] Write-back cache search: current tuple's key = "
//            << hex<<tuple_key<<dec << "\n";
//#endif
//        // If key found, then return the value immediately
//        if (target_key == tuple_key) {
//            tuple_value_offset = *((unsigned int *)((char *)write_cache + tuple_offset + 24));
//
//            // Retrieve the value from the converging dual-stack via the given index
//            value = std::string(static_cast<const char*>((char *)write_cache + tuple_value_offset),
//                (int)tuple_value_len);
//#ifdef DEBUG_BULKDB
//            std::cout << "[BulkDB] Write-back cache search: found value = " << value << "\n";
//#endif
//            return 1;   // Found flag
//        }
//    }
//#ifdef DEBUG_BULKDB
//    std::cout << "[BulkDB] Write-back cache search: value isn't found in write cache!\n";
//#endif
//    return 0;   // Not found!
//}
//
///* Invalidating the key in WC when there's a PUT operation for big pairs */
//void iLSM::DB::wc_invalidate(unsigned int target_key) {
//    unsigned int i, tuple_key, tuple_offset, validate_value = 0x0, prev_value_len;
//
//    // Qualifying the given key at the first stage
//    if (target_key < wc_min_key || target_key > wc_max_key) {
//#ifdef DEBUG_BULKDB
//        std::cout << "[BulkDB] Write-back invalidate search: unqualified (not found)\n";
//#endif
//        return;
//    }
//    // Iterate the stack of tuples in the write-back cache, starting from the top
//    for (i = wc_inserted; i >= 1; i--) {    // Search fresher one first for consistency
//        tuple_offset = CACHE_SIZE - META_SIZE - TUPLE_SIZE * i;
//
//        // Get the key field of current tuple
//        tuple_key = *(unsigned int *)((char *)write_cache + tuple_offset);
//       // If key found, then check whether to invalidate or not
//        if (target_key == tuple_key) {
//            prev_value_len = *(unsigned int *)((char *)write_cache + tuple_offset + 16);
//            if (prev_value_len == validate_value) break;  // If it's the lastly invalidated ver
//
//            // Invalidate the entry with making the length as zero (it's naive, but
//            // more implementation isn't necessary for performance evaluation)
//            memcpy((void *)((char *)write_cache + tuple_offset + 16), &validate_value, 4);
//#ifdef DEBUG_BULKDB
//            std::cout << "[BulkDB] Write-back invalidate search: invalidated (" << tuple_key << ")\n";
//#endif
//        }
//    }
//}
//
///* < Read Cache for small values > */
//std::unordered_map<int, void *> read_cache_small;
//
///* Cache the given page (for small value) */
//void iLSM::DB::rc_small_cache(unsigned int lba, void *data) {
//    void *rc_small_slot = NULL;
//    if (posix_memalign(&rc_small_slot, CACHE_SIZE, CACHE_SIZE)) {
//        std::cout << "[BulkDB] Read cache slot allocation error! (small)\n";
//        exit(-1);
//    }
//    memcpy(rc_small_slot, data, CACHE_SIZE);
//    read_cache_small[lba] = rc_small_slot;
//#ifdef DEBUG_BULKDB
//    std::cout << "[BulkDB] Read cache slot has been allocated (lba:" << lba << ")\n";
//#endif
//}
//
///* Find the cached page, and return it (for small value) */
//void *iLSM::DB::rc_small_get(unsigned int lba) {
//    auto it = read_cache_small.find(lba);
//    if (it != read_cache_small.end()) {
//#ifdef DEBUG_BULKDB
//        std::cout << "[BulkDB] Read cache slot has been found (lba:" << lba << ")\n";
//#endif
//        return it->second;
//    }
//    else return NULL;
//}
//
///* < Read Cache for big values > */
//std::unordered_map<std::string, void *> read_cache_big;
//
///* Cache the given page (for big value) */
//void iLSM::DB::rc_big_cache(std::string key, void *data, unsigned int value_len) {
//    void *rc_big_slot = NULL;
//    unsigned int nlb = (value_len - 1) / PAGE_SIZE;
//    value_len = (nlb + 1) * PAGE_SIZE;
//    if (posix_memalign(&rc_big_slot, CACHE_SIZE, value_len)) {
//        std::cout << "[BulkDB] Read cache slot allocation error! (big)\n";
//        exit(-1);
//    }
//    memcpy(rc_big_slot, data, value_len);
//    read_cache_big[key] = rc_big_slot;
//#ifdef DEBUG_BULKDB
//    std::cout << "[BulkDB] Read cache slot has been allocated (key:" << key << ")\n";
//#endif
//}
//
///* Find the cached page, and return it (for big value) */
//void *iLSM::DB::rc_big_get(std::string key) {
//    auto it = read_cache_big.find(key);
//    if (it != read_cache_big.end()) {
//#ifdef DEBUG_BULKDB
//        std::cout << "[BulkDB] Read cache slot has been found (key:" << key << ")\n";
//#endif
//        return it->second;
//    }
//    else return NULL;
//}
////////////////////////////////////////////////////////////////////////////////// BulkDB
