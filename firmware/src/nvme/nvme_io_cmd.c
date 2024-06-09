//////////////////////////////////////////////////////////////////////////////////
// nvme_io_cmd.c for Cosmos+ OpenSSD
// Copyright (c) 2016 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Youngjin Jo <yjjo@enc.hanyang.ac.kr>
//				  Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// This file is part of Cosmos+ OpenSSD.
//
// Cosmos+ OpenSSD is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// Cosmos+ OpenSSD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Cosmos+ OpenSSD; see the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Company: ENC Lab. <http://enc.hanyang.ac.kr>
// Engineer: Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//			 Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: NVMe IO Command Handler
// File Name: nvme_io_cmd.c
//
// Version: v1.0.1
//
// Description:
//   - handles NVMe IO command
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.1
//   - header file for buffer is changed from "ia_lru_buffer.h" to "lru_buffer.h"
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////
#include "xil_printf.h"
#include "debug.h"
#include "io_access.h"

#include "nvme.h"
#include "host_lld.h"
#include "nvme_io_cmd.h"
#include "../memory_map.h"

#include "../ftl_config.h"
#include "../request_transform.h"
#include "../sstable/sstable.h"
#include "../sstable/super.h"
#include "../memtable/memtable.h"
#include "../iterator/iterator.h"

#include "xtime_l.h"

#define SEED1 0xcc9ed51
#define HASH_NUM 30

static SKIPLIST_HEAD* skiphead = (SKIPLIST_HEAD*)MEMTABLE_HEAD_ADDR;
static SUPER_LEVEL_INFO* super_level_info = (SUPER_LEVEL_INFO*)SUPER_BLOCK_ADDR;
static SUPER_SSTABLE_LIST* super_sstable_list = (SUPER_SSTABLE_LIST*)SUPER_SSTABLE_LIST_ADDR;
static SUPER_SSTABLE_INFO* super_sstable_list_level[4] = {
		(SUPER_SSTABLE_INFO*)SUPER_SSTABLE_LIST0_ADDR,
		(SUPER_SSTABLE_INFO*)SUPER_SSTABLE_LIST1_ADDR,
		(SUPER_SSTABLE_INFO*)SUPER_SSTABLE_LIST2_ADDR,
		(SUPER_SSTABLE_INFO*)SUPER_SSTABLE_LIST3_ADDR
};

const unsigned int MAX_SSTABLE_LEVEL[4] = {
	MAX_SSTABLE_LEVEL0,
	MAX_SSTABLE_LEVEL1,
	MAX_SSTABLE_LEVEL2,
	MAX_SSTABLE_LEVEL3
};

const unsigned int COMPACTION_THRESHOLD_LEVEL[4] = {
	MAX_SSTABLE_LEVEL0-1,
	MAX_SSTABLE_LEVEL1-1,
	MAX_SSTABLE_LEVEL2-1,
	MAX_SSTABLE_LEVEL3-1
};

extern AUTO_CPL_INFO auto_cpl_info[MAX_NUM_NVME_SLOT];

enum CMD_TYPE commandType[1024];
XTime st_read_log[AVAILABLE_OUNTSTANDING_REQ_COUNT];
XTime ed_read_log[AVAILABLE_OUNTSTANDING_REQ_COUNT];
struct STAT_PER_TYPE stat[LAST_CMD];

inline unsigned int GetTypefromCmdSlotTag (int cmdSlotTag) {
	return commandType[cmdSlotTag];
}

void handle_nvme_io_read(unsigned int cmdSlotTag, NVME_IO_COMMAND *nvmeIOCmd)
{
	IO_READ_COMMAND_DW12 readInfo12;
	//IO_READ_COMMAND_DW13 readInfo13;
	//IO_READ_COMMAND_DW15 readInfo15;
	unsigned int startLba[2];
	unsigned int nlb;

	readInfo12.dword = nvmeIOCmd->dword[12];
	//readInfo13.dword = nvmeIOCmd->dword[13];
	//readInfo15.dword = nvmeIOCmd->dword[15];

	startLba[0] = nvmeIOCmd->dword[10];
	startLba[1] = nvmeIOCmd->dword[11];
	nlb = readInfo12.NLB;

	ASSERT(startLba[0] < storageCapacity_L && (startLba[1] < STORAGE_CAPACITY_H || startLba[1] == 0));
	//ASSERT(nlb < MAX_NUM_OF_NLB);
	ASSERT((nvmeIOCmd->PRP1[0] & 0x3) == 0 && (nvmeIOCmd->PRP2[0] & 0x3) == 0); //error
	ASSERT(nvmeIOCmd->PRP1[1] < 0x10000 && nvmeIOCmd->PRP2[1] < 0x10000);

	ReqTransNvmeToSlice(cmdSlotTag, startLba[0], nlb, IO_NVM_READ);
}


void handle_nvme_io_write(unsigned int cmdSlotTag, NVME_IO_COMMAND *nvmeIOCmd)
{
	IO_READ_COMMAND_DW12 writeInfo12;
	//IO_READ_COMMAND_DW13 writeInfo13;
	//IO_READ_COMMAND_DW15 writeInfo15;
	unsigned int startLba[2];
	unsigned int nlb;

	writeInfo12.dword = nvmeIOCmd->dword[12];
	//writeInfo13.dword = nvmeIOCmd->dword[13];
	//writeInfo15.dword = nvmeIOCmd->dword[15];

	//if(writeInfo12.FUA == 1)
	//	xil_printf("write FUA\r\n");

	startLba[0] = nvmeIOCmd->dword[10];
	startLba[1] = nvmeIOCmd->dword[11];
	nlb = writeInfo12.NLB;

	ASSERT(startLba[0] < storageCapacity_L && (startLba[1] < STORAGE_CAPACITY_H || startLba[1] == 0));
	//ASSERT(nlb < MAX_NUM_OF_NLB);
	ASSERT((nvmeIOCmd->PRP1[0] & 0xF) == 0 && (nvmeIOCmd->PRP2[0] & 0xF) == 0);
	ASSERT(nvmeIOCmd->PRP1[1] < 0x10000 && nvmeIOCmd->PRP2[1] < 0x10000);

	ReqTransNvmeToSlice(cmdSlotTag, startLba[0], nlb, IO_NVM_WRITE);
}

void handle_nvme_io_cmd(NVME_COMMAND *nvmeCmd)
{
	NVME_IO_COMMAND *nvmeIOCmd;
	NVME_COMPLETION nvmeCPL;
	unsigned int opc;
	nvmeIOCmd = (NVME_IO_COMMAND*)nvmeCmd->cmdDword;
	// xil_printf("OPC = 0x%X\r\n", nvmeIOCmd->OPC);
	// xil_printf("PRP1[63:32] = 0x%X, PRP1[31:0] = 0x%X\r\n", nvmeIOCmd->PRP1[1], nvmeIOCmd->PRP1[0]);
	// xil_printf("PRP2[63:32] = 0x%X, PRP2[31:0] = 0x%X\r\n", nvmeIOCmd->PRP2[1], nvmeIOCmd->PRP2[0]);
	// xil_printf("dword10 = 0x%X\r\n", nvmeIOCmd->dword10);
	// xil_printf("dword11 = 0x%X\r\n", nvmeIOCmd->dword11);
	// xil_printf("dword12 = 0x%X\r\n", nvmeIOCmd->dword12);

	opc = (unsigned int)nvmeIOCmd->OPC;

	switch(opc)
	{
		case IO_NVM_FLUSH:
		{
		//	xil_printf("IO Flush Command\r\n");
			nvmeCPL.dword[0] = 0;
			nvmeCPL.specific = 0x0;
			set_auto_nvme_cpl(nvmeCmd->cmdSlotTag, nvmeCPL.specific, nvmeCPL.statusFieldWord);
			break;
		}
		case IO_NVM_WRITE:
		{
//			xil_printf("IO Write Command\r\n");
			handle_nvme_io_write(nvmeCmd->cmdSlotTag, nvmeIOCmd);
			break;
		}
		case IO_NVM_READ:
		{
//			xil_printf("IO Read Command\r\n");
			handle_nvme_io_read(nvmeCmd->cmdSlotTag, nvmeIOCmd);
			break;
		}
		
		/*
		 * Key-Value Commands
		 * Point Query : KV_PUT, KV_GET, KV_DELETE
		 * Range Query : ITER_CREATE, KV_SEEK, KV_NEXT, ITER_DELETE 
		 */
		case IO_NVM_KV_PUT:
		{
			// xil_printf("KV Put Command \r\n");
			// commandType[nvmeCmd->cmdSlotTag] = KV_PUT;
			// stat[KV_PUT].c++;
			handle_nvme_io_kv_put(nvmeCmd->cmdSlotTag, nvmeIOCmd);
			
			unsigned int retry;
			do {
				retry = MaybeDoCompaction();
			} while(retry == 1);
			
			break;
		}
		case IO_NVM_KV_GET:
		{
			commandType[nvmeCmd->cmdSlotTag] = KV_GET;
			stat[KV_GET].c++;

			XTime st, ed;
			XTime_GetTime(&st);
			handle_nvme_io_kv_get(nvmeCmd->qID, nvmeIOCmd->CID, nvmeCmd->cmdSlotTag, nvmeIOCmd);
			XTime_GetTime(&ed);		
			stat[KV_GET].TOTAL_TIME += ed-st;
			break;
		}
		case IO_NVM_KV_DELETE:
		{
			// xil_printf("KV Delete Command \r\n");
			// handle_nvme_io_kv_delete(nvmeCmd->cmdSlotTag, nvmeIOCmd);
			break;
		}
		default:
		{
			xil_printf("Not Support IO Command OPC: %X\r\n", opc);
			ASSERT(0);
			break;
		}
	}
}

