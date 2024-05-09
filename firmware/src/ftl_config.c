//////////////////////////////////////////////////////////////////////////////////
// ftl_config.c for Cosmos+ OpenSSD
// Copyright (c) 2017 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//				  Sangjin Lee <sjlee@enc.hanyang.ac.kr>
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
// Engineer: Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: Flash Translation Layer Configuration Manager
// File Name: ftl_config.c
//
// Version: v1.0.0
//
// Description:
//   - initialize flash translation layer
//	 - check configuration options
//	 - initialize NAND device
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#include <assert.h>
#include "xil_printf.h"
#include "memory_map.h"
#define MB (1024*1024)
#define KB (1024)

unsigned int storageCapacity_L;
V2FMCRegisters* chCtlReg[USER_CHANNELS];

void printMemoryMap()
{

	xil_printf("\n\n---------------- BUFFER START ----------------\n");
	xil_printf("DATA_BUFFER_ADDR = 0x%x \r\n", DATA_BUFFER_BASE_ADDR);
	xil_printf("DATA_BUFFER_SIZE = %u Bytes (%u.%uMB) \r\n", DATA_BUFFER_SIZE, DATA_BUFFER_SIZE/MB, (DATA_BUFFER_SIZE%MB)*100/MB);

	xil_printf("TEMPORARY_DATA_BUFFER_ADDR = 0x%x \r\n", TEMPORARY_DATA_BUFFER_BASE_ADDR);
	xil_printf("TEMPORARY_DATA_BUFFER_SIZE = %u Bytes (%uKB)\r\n", TEMPORARY_DATA_BUFFER_SIZE, TEMPORARY_DATA_BUFFER_SIZE/KB);

	xil_printf("SPARE_DATA_BUFFER_ADDR = 0x%x \r\n", SPARE_DATA_BUFFER_BASE_ADDR);
	xil_printf("SPARE_DATA_BUFFER_SIZE = %u Bytes (%uKB)\r\n", SPARE_DATA_BUFFER_SIZE, SPARE_DATA_BUFFER_SIZE/KB);

	xil_printf("TEMPORARY_SPARE_DATA_BUFFER_ADDR = 0x%x \r\n", TEMPORARY_SPARE_DATA_BUFFER_BASE_ADDR);
	xil_printf("TEMPORARY_SPARE_DATA_BUFFER_SIZE = %u Bytes (%uKB)\r\n", TEMPORARY_SPARE_DATA_BUFFER_SIZE, TEMPORARY_SPARE_DATA_BUFFER_SIZE/KB);

	xil_printf("RESERVED_DATA_BUFFER_ADDR = 0x%x \r\n", RESERVED_DATA_BUFFER_BASE_ADDR);
	xil_printf("RESERVED_DATA_BUFFER_SIZE = %u Bytes (%u.%uMB)\r\n", RESERVED_DATA_BUFFER_SIZE, RESERVED_DATA_BUFFER_SIZE/MB, (RESERVED_DATA_BUFFER_SIZE%MB)*100/MB);
	xil_printf("---------------- BUFFER END ----------------\n\n");

	xil_printf("---------------- FOR NAND REQUEST COMPLETION START ----------------\n");
	xil_printf("COMPLETE_FLAG_TABLE_ADDR = 0x%x \r\n", COMPLETE_FLAG_TABLE_ADDR);
	xil_printf("COMPLETE_FLAG_TABLE_SIZE = %u Bytes \r\n", COMPLETE_FLAG_TABLE_SIZE);

	xil_printf("STATUS_REPORT_TABLE_ADDR = 0x%x \r\n", STATUS_REPORT_TABLE_ADDR);
	xil_printf("STATUS_REPORT_TABLE_SIZE = %u Bytes \r\n", STATUS_REPORT_TABLE_SIZE);

	xil_printf("ERROR_INFO_TABLE_ADDR = 0x%x \r\n", ERROR_INFO_TABLE_ADDR);
	xil_printf("ERROR_INFO_TABLE_SIZE = %u Bytes \r\n", ERROR_INFO_TABLE_SIZE);

	xil_printf("TEMPORARY_PAY_LOAD_ADDR = 0x%x \r\n", TEMPORARY_PAY_LOAD_ADDR);
	xil_printf("TEMPORARY_PAY_LOAD_SIZE = %u Bytes (%u.%uMB)\r\n", TEMPORARY_PAY_LOAD_SIZE, TEMPORARY_PAY_LOAD_SIZE/MB, (TEMPORARY_PAY_LOAD_SIZE%MB)*100/MB);
	xil_printf("---------------- FOR NAND REQUEST COMPLETION END ----------------\n\n");

	xil_printf("---------------- MAP FOR BUFFER START ----------------\n");
	xil_printf("DATA_BUFFER_MAP_ADDR = 0x%x \r\n", DATA_BUFFER_MAP_ADDR);
	xil_printf("DATA_BUFFER_MAP_SIZE = %u Bytes (%uKB)\r\n", DATA_BUFFER_MAP_SIZE, DATA_BUFFER_MAP_SIZE/KB);

	xil_printf("DATA_BUFFFER_HASH_TABLE_ADDR = 0x%x \r\n", DATA_BUFFFER_HASH_TABLE_ADDR);
	xil_printf("DATA_BUFFFER_HASH_TABLE_SIZE = %u Bytes (%uKB)\r\n", DATA_BUFFFER_HASH_TABLE_SIZE, DATA_BUFFFER_HASH_TABLE_SIZE/KB);

	xil_printf("TEMPORARY_DATA_BUFFER_MAP_ADDR = 0x%x \r\n", TEMPORARY_DATA_BUFFER_MAP_ADDR);
	xil_printf("TEMPORARY_DATA_BUFFER_MAP_SIZE = %u Bytes \r\n", TEMPORARY_DATA_BUFFER_MAP_SIZE);
	xil_printf("---------------- MAP FOR BUFFER END ----------------\n\n");

	xil_printf("---------------- MAP FOR FTL START ----------------\n");
	xil_printf("LOGICAL_SLICE_MAP_ADDR = 0x%x \r\n", LOGICAL_SLICE_MAP_ADDR);
	xil_printf("LOGICAL_SLICE_MAP_SIZE = %u Bytes (%u.%uMB)\r\n", LOGICAL_SLICE_MAP_SIZE, LOGICAL_SLICE_MAP_SIZE/MB, (LOGICAL_SLICE_MAP_SIZE%MB)*100/MB);

	xil_printf("VIRTUAL_SLICE_MAP_ADDR = 0x%x \r\n", VIRTUAL_SLICE_MAP_ADDR);
	xil_printf("VIRTUAL_SLICE_MAP_SIZE = %u Bytes (%u.%uMB)\r\n", VIRTUAL_SLICE_MAP_SIZE, VIRTUAL_SLICE_MAP_SIZE/MB, (VIRTUAL_SLICE_MAP_SIZE%MB)*100/MB);

	xil_printf("VIRTUAL_BLOCK_MAP_ADDR = 0x%x \r\n", VIRTUAL_BLOCK_MAP_ADDR);
	xil_printf("VIRTUAL_BLOCK_MAP_SIZE = %u Bytes (%u.%uMB)\r\n", VIRTUAL_BLOCK_MAP_SIZE, VIRTUAL_BLOCK_MAP_SIZE/MB, (VIRTUAL_BLOCK_MAP_SIZE%MB)*100/MB);

	xil_printf("PHY_BLOCK_MAP_ADDR = 0x%x \r\n", PHY_BLOCK_MAP_ADDR);
	xil_printf("PHY_BLOCK_MAP_SIZE = %u Bytes (%u.%uMB)\r\n", PHY_BLOCK_MAP_SIZE, PHY_BLOCK_MAP_SIZE/MB, (PHY_BLOCK_MAP_SIZE%MB)*100/MB);

	xil_printf("BAD_BLOCK_TABLE_INFO_MAP_ADDR = 0x%x \r\n", BAD_BLOCK_TABLE_INFO_MAP_ADDR);
	xil_printf("BAD_BLOCK_TABLE_INFO_MAP_SIZE = %u Bytes \r\n", BAD_BLOCK_TABLE_INFO_MAP_SIZE);

	xil_printf("VIRTUAL_DIE_MAP_ADDR = 0x%x \r\n", VIRTUAL_DIE_MAP_ADDR);
	xil_printf("VIRTUAL_DIE_MAP_SIZE = %u Bytes \r\n", VIRTUAL_DIE_MAP_SIZE);
	xil_printf("---------------- MAP FOR FTL END ----------------\n\n");

	xil_printf("---------------- FOR GC VICTIM SELECTION START ----------------\n");
	xil_printf("GC_VICTIM_MAP_ADDR = 0x%x \r\n", GC_VICTIM_MAP_ADDR);
	xil_printf("GC_VICTIM_MAP_SIZE = %u Bytes (%uKB)\r\n", GC_VICTIM_MAP_SIZE, GC_VICTIM_MAP_SIZE/KB);
	xil_printf("---------------- FOR GC VICTIM SELECTION END ----------------\n\n");

	xil_printf("---------------- FOR REQUEST SCHEDULER START ----------------\n");
	xil_printf("REQ_POOL_ADDR = 0x%x \r\n", REQ_POOL_ADDR);
	xil_printf("REQ_POOL_SIZE = %u Bytes (%uKB)\r\n", REQ_POOL_SIZE, REQ_POOL_SIZE/KB);

	xil_printf("ROW_ADDR_DEPENDENCY_TABLE_ADDR = 0x%x \r\n", ROW_ADDR_DEPENDENCY_TABLE_ADDR);
	xil_printf("ROW_ADDR_DEPENDENCY_TABLE_SIZE = %u Bytes (%u.0MB) \r\n", ROW_ADDR_DEPENDENCY_TABLE_SIZE, ROW_ADDR_DEPENDENCY_TABLE_SIZE/MB);

	xil_printf("DIE_STATE_TABLE_ADDR = 0x%x \r\n", DIE_STATE_TABLE_ADDR);
	xil_printf("DIE_STATE_TABLE_SIZE = %u Bytes \r\n", DIE_STATE_TABLE_SIZE);

	xil_printf("RETRY_LIMIT_TABLE_ADDR = 0x%x \r\n", RETRY_LIMIT_TABLE_ADDR);
	xil_printf("RETRY_LIMIT_TABLE_SIZE = %u Bytes \r\n", RETRY_LIMIT_TABLE_SIZE);

	xil_printf("WAY_PRIORITY_TABLE_ADDR = 0x%x \r\n", WAY_PRIORITY_TABLE_ADDR);
	xil_printf("WAY_PRIORITY_TABLE_SIZE = %u Bytes \r\n", WAY_PRIORITY_TABLE_SIZE);
	xil_printf("---------------- FOR REQUEST SCHEDULER END ----------------\n");

	xil_printf("Cached Area (RESERVED1_START_ADDR): 0x%u MB\n", RESERVED1_START_ADDR / MB);
	xil_printf("----------------- For Key-Value SSD -----------------------\n");

	xil_printf("Capacity for %d Iterators %u Bytes (%u MB)\n", MAX_ITERATOR_NUM, ITER_SSTABLE_LEVEL3_DATA_BUFFER_START_ADDR + ITER_SSTABLE_LEVEL3_DATA_BUFFER_SIZE - ITER_POOL_START_ADDR, (ITER_SSTABLE_LEVEL3_DATA_BUFFER_START_ADDR+ITER_SSTABLE_LEVEL3_DATA_BUFFER_SIZE-ITER_POOL_START_ADDR)/MB);
	xil_printf("Current End Addr 0x%x, DRAM End Addr 0x%x\n", ITER_SSTABLE_LEVEL3_DATA_BUFFER_START_ADDR + ITER_SSTABLE_LEVEL3_DATA_BUFFER_SIZE, DRAM_END_ADDR);

	xil_printf("TOTAL DRAM USAGE : %u MB\n", (DATA_BUFFER_SIZE+
										TEMPORARY_DATA_BUFFER_SIZE+
										SPARE_DATA_BUFFER_SIZE+
										TEMPORARY_SPARE_DATA_BUFFER_SIZE+
										COMPLETE_FLAG_TABLE_SIZE+
										STATUS_REPORT_TABLE_SIZE+
										ERROR_INFO_TABLE_SIZE+
										DATA_BUFFER_MAP_SIZE+
										DATA_BUFFFER_HASH_TABLE_SIZE+
										TEMPORARY_DATA_BUFFER_MAP_SIZE+
										LOGICAL_SLICE_MAP_SIZE+
										VIRTUAL_SLICE_MAP_SIZE+
										VIRTUAL_BLOCK_MAP_SIZE+
										PHY_BLOCK_MAP_SIZE+
										BAD_BLOCK_TABLE_INFO_MAP_SIZE+
										VIRTUAL_DIE_MAP_SIZE+
										GC_VICTIM_MAP_SIZE+
										REQ_POOL_SIZE+
										ROW_ADDR_DEPENDENCY_TABLE_SIZE+
										DIE_STATE_TABLE_SIZE+
										RETRY_LIMIT_TABLE_SIZE+
										WAY_PRIORITY_TABLE_SIZE) / MB + 1);
}


void printMemoryLayout () {
	
	unsigned int seg_size  = MEMORY_SEGMENTS_END_ADDR - MEMORY_SEGMENTS_START_ADDR;
	unsigned int nvme_size = NVME_MANAGEMENT_END_ADDR - NVME_MANAGEMENT_START_ADDR;
	unsigned int reserved_0_size = RESERVED0_END_ADDR - RESERVED0_START_ADDR;
	unsigned int reserved_1_size = RESERVED1_END_ADDR - RESERVED1_START_ADDR;
	unsigned int ftl_size = FTL_MANAGEMENT_END_ADDR - FTL_MANAGEMENT_START_ADDR;
				
	unsigned int bmf_size = BMF_BUFFER_SIZE;
	unsigned int sst_size = SSTABLE_DATA_BUFFER_END - SSTABLE_INDEX_BUFFER;
	unsigned int cp_buf_size = COMPACTION_BUFFER_END_ADDR - COMPACTION_BUFFER_ADDR;
	unsigned int mem_size = MEMTABLE_NODE_END_ADDR - MEMTABLE_HEAD_ADDR;
	unsigned int super_size = SUPER_SSTABLE_LIST3_END_ADDR - SUPER_BLOCK_ADDR;

	unsigned int databuf_size = DATA_BUFFER_SIZE;
	unsigned int sparebuf_size = SPARE_DATA_BUFFER_SIZE;
	unsigned int iterator_size = DIRECT_DMA_BUFFER_ADDR + DIRECT_DMA_BUFFER_SIZE - ITER_POOL_START_ADDR;
	
	unsigned int avail_0_size = RESERVED0_END_ADDR - SUPER_SSTABLE_LIST3_END_ADDR;
	unsigned int avail_1_size = DRAM_END_ADDR - DIRECT_DMA_BUFFER_ADDR + DIRECT_DMA_BUFFER_SIZE;
	unsigned int avail_map_size = 0x17000000 - RESERVED_DATA_BUFFER_BASE_ADDR + RESERVED_DATA_BUFFER_SIZE;

	xil_printf("\n\n---------------- DRAM START ----------------\n");
	xil_printf("MEMORY_SEGEMENTS_SIZE : %u B(%u.%uMB)\r\n", seg_size, seg_size / MB, (seg_size % MB) * 100 / MB);
	xil_printf("NVME_MANAGEMENT_SIZE : %u B(%u.%uMB)\r\n", nvme_size, nvme_size / MB, (nvme_size % MB) * 100 / MB);
	
	xil_printf("\n\n---------------- RESERVED 0 AREA (%u.%uMB) ----------------\n", reserved_0_size / MB, (reserved_0_size % MB) * 100 / MB);
	xil_printf("BLOOM_FILTER_BUFFER_SIZE : %u B(%u.%uMB)\r\n", bmf_size, bmf_size / MB, (bmf_size % MB) * 100 / MB);
	xil_printf("SSTABLE_BUFFER_SIZE : %u B(%u.%uMB)\r\n", sst_size, sst_size / MB, (sst_size % MB) * 100 / MB);
	xil_printf("COMPACTION_BUFFFER_SIZE : %u B (%u.%uMB)\r\n", cp_buf_size, cp_buf_size / MB, (cp_buf_size % MB) * 100 / MB);
	xil_printf("MEMTABLE_BUFFER_SIZE : %u B (%u.%uMB)\r\n", mem_size, mem_size / MB, (mem_size % MB) * 100 / MB);
	xil_printf("SUPER_BLOCK_SIZE : %xu B (%u.%uMB)\r\n", super_size, super_size / MB, (super_size % MB) * 100 / MB);
	xil_printf("AVAILABLE_AREA_0_SIZE : %u B (%u.%uMB)\r\n", avail_0_size, avail_0_size / MB, (avail_0_size % MB) * 100 / MB);

	xil_printf("\n\n---------------- FTL MANAGEMENT ----------------\n");
	xil_printf("FTL_MANAGEMENT_SIZE : %u B (%u.%uMB)\r\n", ftl_size, ftl_size / MB, (ftl_size % MB) * 100 / MB);
	xil_printf("AVAILABLE_MAP_SIZE : %u B (%u.%uMB)\r\n", avail_map_size, avail_map_size / MB, (avail_map_size % MB) * 100 / MB);
	xil_printf("DATA_BUFFER_SIZE : %u B (%u.%uMB)\r\n", databuf_size, databuf_size / MB, (databuf_size % MB) * 100 / MB);
	xil_printf("SPARE_BUFFER_SIZE : %u B (%u.%uMB)\r\n", sparebuf_size, sparebuf_size / MB, (sparebuf_size % MB) * 100 / MB);

	xil_printf("\n\n---------------- RESERVED 1 AREA (%u.%uMB) ----------------\n", reserved_1_size / MB, (reserved_1_size % MB) * 100 / MB);
	xil_printf("ITERATOR_SIZE : %u B (%u.%uMB)\r\n", iterator_size, iterator_size / MB, (iterator_size % MB) * 100 / MB);
	xil_printf("AVAILABLE_AREA_1_SIZE : %u B (%u.%uMB)\r\n", avail_1_size, avail_1_size / MB, (avail_1_size % MB) * 100 / MB);
}

void InitFTL()
{
	
	// xil_printf("[ CheckConfigRestriction(); ] \r\n");
	CheckConfigRestriction();
	// xil_printf("[ InitChCtlReg(); ] \r\n");
	InitChCtlReg();
	// xil_printf("[ InitReqPool(); ] \r\n");
	InitReqPool();
	// xil_printf("[ InitDependencyTable(); ] \r\n");
	InitDependencyTable();
	// xil_printf("[ InitReqScheduler(); ] \r\n");
	InitReqScheduler();
	// xil_printf("[ InitNandArray(); ] \r\n");
	InitNandArray();
	// xil_printf("[ InitAddressMap(); ] \r\n");
	InitAddressMap();
	// xil_printf("[ InitDataBuf(); ] \r\n");
	InitDataBuf();
	// xil_printf("[ InitGcVictimMap(); ] \r\n");
	InitGcVictimMap();

	storageCapacity_L = (MB_PER_SSD - (MB_PER_MIN_FREE_BLOCK_SPACE + mbPerbadBlockSpace + MB_PER_OVER_PROVISION_BLOCK_SPACE)) * ((1024*1024) / BYTES_PER_NVME_BLOCK);

	xil_printf("[ storage capacity %d MB ]\r\n", storageCapacity_L / ((1024*1024) / BYTES_PER_NVME_BLOCK));
	xil_printf("[ ftl configuration complete. ]\r\n");

	// printMemoryMap();
	printMemoryLayout();
}


void InitChCtlReg()
{
	if(USER_CHANNELS < 1)
		assert(!"[WARNING] Configuration Error: Channel [WARNING]");
	xil_printf("USER_CHANNELS = %d\n", USER_CHANNELS);
	
	chCtlReg[0] = (V2FMCRegisters*) NSC_0_BASEADDR;

	if(USER_CHANNELS > 1)
		chCtlReg[1] = (V2FMCRegisters*) NSC_1_BASEADDR;

	if(USER_CHANNELS > 2)
		chCtlReg[2] = (V2FMCRegisters*) NSC_2_BASEADDR;

	if(USER_CHANNELS > 3)
		chCtlReg[3] = (V2FMCRegisters*) NSC_3_BASEADDR;

	if(USER_CHANNELS > 4)
		chCtlReg[4] = (V2FMCRegisters*) NSC_4_BASEADDR;

	if(USER_CHANNELS > 5)
		chCtlReg[5] = (V2FMCRegisters*) NSC_5_BASEADDR;

	if(USER_CHANNELS > 6)
		chCtlReg[6] = (V2FMCRegisters*) NSC_6_BASEADDR;

	if(USER_CHANNELS > 7)
		chCtlReg[7] = (V2FMCRegisters*) NSC_7_BASEADDR;
}



void InitNandArray()
{
	unsigned int chNo, wayNo, reqSlotTag;

	for(chNo=0; chNo<USER_CHANNELS; ++chNo)
		for(wayNo=0; wayNo<USER_WAYS; ++wayNo)
		{
			reqSlotTag = GetFromFreeReqQ();

			reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
			reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_RESET;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_PHY_ORG;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_NONE;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_TOTAL;
			reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalCh = chNo;
			reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalWay = wayNo;
			reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalBlock = 0;	//dummy
			reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalPage = 0;	//dummy
			reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq = REQ_SLOT_TAG_NONE;

			SelectLowLevelReqQ(reqSlotTag);

			reqSlotTag = GetFromFreeReqQ();

			reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
			reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_SET_FEATURE;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr =  REQ_OPT_NAND_ADDR_PHY_ORG;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_NONE;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_TOTAL;
			reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalCh = chNo;
			reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalWay = wayNo;
			reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalBlock = 0;	//dummy
			reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalPage = 0;	//dummy
			reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq = REQ_SLOT_TAG_NONE;

			SelectLowLevelReqQ(reqSlotTag);
		}

	SyncAllLowLevelReqDone();

	xil_printf("[ NAND device reset complete. ]\r\n");
}


void CheckConfigRestriction()
{
	if(USER_CHANNELS > NSC_MAX_CHANNELS)
		assert(!"[WARNING] Configuration Error: Channel [WARNING]");
	if(USER_WAYS > NSC_MAX_WAYS)
		assert(!"[WARNING] Configuration Error: WAY [WARNING]");
	if(USER_BLOCKS_PER_LUN > MAIN_BLOCKS_PER_LUN)
		assert(!"[WARNING] Configuration Error: BLOCK [WARNING]");
	if((BITS_PER_FLASH_CELL != SLC_MODE) && (BITS_PER_FLASH_CELL != MLC_MODE))
		assert(!"[WARNING] Configuration Error: BIT_PER_FLASH_CELL [WARNING]");

	if(RESERVED_DATA_BUFFER_BASE_ADDR + 0x00200000 > COMPLETE_FLAG_TABLE_ADDR)
		assert(!"[WARNING] Configuration Error: Data buffer size is too large to be allocated to predefined range [WARNING]");
	if(TEMPORARY_PAY_LOAD_ADDR + 0x00001000 > DATA_BUFFER_MAP_ADDR)
		assert(!"[WARNING] Configuration Error: Metadata for NAND request completion process is too large to be allocated to predefined range [WARNING]");
	if(FTL_MANAGEMENT_END_ADDR > DRAM_END_ADDR)
		assert(!"[WARNING] Configuration Error: Metadata of FTL is too large to be allocated to DRAM [WARNING]");
}
