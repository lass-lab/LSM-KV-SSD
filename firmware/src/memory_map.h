//////////////////////////////////////////////////////////////////////////////////
// memory_map.h for Cosmos+ OpenSSD
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
// Module Name: Static Memory Allocator
// File Name: memory_map.h
//
// Version: v1.0.0
//
// Description:
//	 - allocate DRAM address space (0x0010_0000 ~ 0x3FFF_FFFF) to each module
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#ifndef MEMORY_MAP_H_
#define MEMORY_MAP_H_

#include "data_buffer.h"
#include "address_translation.h"
#include "request_allocation.h"
#include "request_schedule.h"
#include "request_transform.h"
#include "garbage_collection.h"
#include "sstable/sstable.h"
#include "sstable/super.h"
#include "memtable/memtable.h"

#define DRAM_START_ADDR					0x00100000

#define MEMORY_SEGMENTS_START_ADDR		DRAM_START_ADDR
#define MEMORY_SEGMENTS_END_ADDR		0x001FFFFF

#define NVME_MANAGEMENT_START_ADDR		0x00200000
#define NVME_MANAGEMENT_END_ADDR		0x002FFFFF

// Uncached & Unbuffered
// For LSM-Tree Management
#define RESERVED0_START_ADDR			0x00300000

/*
 * For Get Query,
 * Bloom Filter to check probabilistically
 * Max Index Size = MemTable Index Block Size * 2^n (doubling each level)
 * Max Data Size = MemTable Data Block Block Size * 2^n (doubling each level)
 */
#define BMF_BUFFER_ADDR     (RESERVED0_START_ADDR)
#define BMF_BUFFER_SIZE		(BYTES_PER_DATA_REGION_OF_SLICE * 8)

#define SSTABLE_INDEX_BUFFER     (BMF_BUFFER_ADDR + (BYTES_PER_DATA_REGION_OF_SLICE * 8))
#define SSTABLE_INDEX_BUFFER_SIZE (sizeof(struct _SSTABLE_INDEX_NODE) * MAX_SKIPLIST_NODE * 2 * 2 * 2)
#define SSTABLE_INDEX_BUFFER_END (SSTABLE_INDEX_BUFFER + SSTABLE_INDEX_BUFFER_SIZE) 

// #define SSTABLE_DATA_BUFFER      (SSTABLE_INDEX_BUFFER_END)
#define SSTABLE_DATA_BUFFER      (((SSTABLE_INDEX_BUFFER_END + BYTES_PER_DATA_REGION_OF_SLICE - 1) / BYTES_PER_DATA_REGION_OF_SLICE) * BYTES_PER_DATA_REGION_OF_SLICE)
#define SSTABLE_DATA_BUFFER_SIZE (sizeof(struct _SSTABLE_DATA_NODE) * MAX_SKIPLIST_NODE * 2 * 2 * 2)
#define SSTABLE_DATA_BUFFER_END  (SSTABLE_DATA_BUFFER + SSTABLE_DATA_BUFFER_SIZE)

/*
 * For Compaction,
 * We need two more buffers for out-of-place update
 */
#define COMPACTION_BUFFER_ADDR (SSTABLE_DATA_BUFFER_END)

#define CP_INDEX_BUFFER1_ADDR (COMPACTION_BUFFER_ADDR)
#define CP_INDEX_BUFFER1_SIZE (sizeof(struct _SSTABLE_INDEX_NODE) * MAX_SKIPLIST_NODE * 2 * 2 * 2)

#define CP_INDEX_BUFFER2_ADDR (CP_INDEX_BUFFER1_ADDR + CP_INDEX_BUFFER1_SIZE)
#define CP_INDEX_BUFFER2_SIZE (sizeof(struct _SSTABLE_INDEX_NODE) * MAX_SKIPLIST_NODE * 2 * 2 * 2)

#define CP_DATA_BUFFER1_ADDR  (CP_INDEX_BUFFER2_ADDR + CP_INDEX_BUFFER2_SIZE)
#define CP_DATA_BUFFER1_SIZE  (sizeof(struct _SSTABLE_DATA_NODE) * MAX_SKIPLIST_NODE * 2 * 2 * 2)

#define CP_DATA_BUFFER2_ADDR  (CP_DATA_BUFFER1_ADDR + CP_DATA_BUFFER1_SIZE)
#define CP_DATA_BUFFER2_SIZE  (sizeof(struct _SSTABLE_DATA_NODE) * MAX_SKIPLIST_NODE * 2 * 2 * 2)

#define COMPACTION_BUFFER_END_ADDR (CP_DATA_BUFFER2_ADDR + CP_DATA_BUFFER2_SIZE)

/*
 * For MemTable (with sigle thread CPU, no need for immutable MemTable),
 * SkipList Head = Metadata for MemTable and Pointers to Nodes 
 * SkipList Node = <Key, Value Offset, Value Length>
 */
#define MEMTABLE_HEAD_ADDR  (COMPACTION_BUFFER_END_ADDR) 
#define MEMTABLE_NODE_ADDR  (MEMTABLE_HEAD_ADDR + sizeof(struct _SKIPLIST_HEAD))
#define MEMTABLE_NODE_SIZE  (sizeof(struct _SKIPLIST_NODE) * MAX_SKIPLIST_NODE)
#define MEMTABLE_NODE_END_ADDR   (MEMTABLE_NODE_ADDR + MEMTABLE_NODE_SIZE)

/*
 * For Super Block,
 * In order to locate all the LPN of SSTables,
 * it should have some data strucutres like MANIFEST in LevelDB
 * Super SSTable does the role
 */
#define SUPER_BLOCK_ADDR	(MEMTABLE_NODE_END_ADDR)
#define SUPER_SSTABLE_LIST_ADDR  (SUPER_BLOCK_ADDR + sizeof(struct _SUPER_LEVEL_INFO))
#define SUPER_SSTABLE_LIST0_ADDR  (SUPER_SSTABLE_LIST_ADDR + (4 * sizeof(struct _SUPER_SSTABLE_LIST)))
#define SUPER_SSTABLE_LIST1_ADDR  (SUPER_SSTABLE_LIST0_ADDR + (MAX_SSTABLE_LEVEL0 * sizeof(struct _SUPER_SSTABLE_INFO)))
#define SUPER_SSTABLE_LIST2_ADDR  (SUPER_SSTABLE_LIST1_ADDR + (MAX_SSTABLE_LEVEL1 * sizeof(struct _SUPER_SSTABLE_INFO)))
#define SUPER_SSTABLE_LIST3_ADDR  (SUPER_SSTABLE_LIST2_ADDR + (MAX_SSTABLE_LEVEL2 * sizeof(struct _SUPER_SSTABLE_INFO)))
#define SUPER_SSTABLE_LIST3_END_ADDR  (SUPER_SSTABLE_LIST3_ADDR + (MAX_SSTABLE_LEVEL3 * sizeof(struct _SUPER_SSTABLE_INFO)))

#define RESERVED0_END_ADDR				0x0FFFFFFF

#define FTL_MANAGEMENT_START_ADDR		0x10000000

// for data buffer
#define DATA_BUFFER_BASE_ADDR 					0x10000000
#define DATA_BUFFER_SIZE						(AVAILABLE_DATA_BUFFER_ENTRY_COUNT * BYTES_PER_DATA_REGION_OF_SLICE)

#define TEMPORARY_DATA_BUFFER_BASE_ADDR			(DATA_BUFFER_BASE_ADDR + DATA_BUFFER_SIZE)
#define TEMPORARY_DATA_BUFFER_SIZE				(AVAILABLE_TEMPORARY_DATA_BUFFER_ENTRY_COUNT * BYTES_PER_DATA_REGION_OF_SLICE)

#define SPARE_DATA_BUFFER_BASE_ADDR				(TEMPORARY_DATA_BUFFER_BASE_ADDR + TEMPORARY_DATA_BUFFER_SIZE)
#define SPARE_DATA_BUFFER_SIZE                  (AVAILABLE_DATA_BUFFER_ENTRY_COUNT * BYTES_PER_SPARE_REGION_OF_SLICE)

#define TEMPORARY_SPARE_DATA_BUFFER_BASE_ADDR	(SPARE_DATA_BUFFER_BASE_ADDR + SPARE_DATA_BUFFER_SIZE)
#define TEMPORARY_SPARE_DATA_BUFFER_SIZE        (AVAILABLE_TEMPORARY_DATA_BUFFER_ENTRY_COUNT * BYTES_PER_SPARE_REGION_OF_SLICE)

#define RESERVED_DATA_BUFFER_BASE_ADDR 			(TEMPORARY_SPARE_DATA_BUFFER_BASE_ADDR + TEMPORARY_SPARE_DATA_BUFFER_SIZE)
#define RESERVED_DATA_BUFFER_SIZE               (COMPLETE_FLAG_TABLE_ADDR - RESERVED_DATA_BUFFER_BASE_ADDR)

//for nand request completion
#define COMPLETE_FLAG_TABLE_ADDR			0x17000000
#define COMPLETE_FLAG_TABLE_SIZE            (sizeof(COMPLETE_FLAG_TABLE))

#define STATUS_REPORT_TABLE_ADDR			(COMPLETE_FLAG_TABLE_ADDR + COMPLETE_FLAG_TABLE_SIZE)
#define STATUS_REPORT_TABLE_SIZE            (sizeof(STATUS_REPORT_TABLE))

#define ERROR_INFO_TABLE_ADDR				(STATUS_REPORT_TABLE_ADDR + STATUS_REPORT_TABLE_SIZE)
#define ERROR_INFO_TABLE_SIZE               (sizeof(ERROR_INFO_TABLE))

#define TEMPORARY_PAY_LOAD_ADDR				(ERROR_INFO_TABLE_ADDR + ERROR_INFO_TABLE_SIZE)
#define TEMPORARY_PAY_LOAD_SIZE             (DATA_BUFFER_MAP_ADDR - TEMPORARY_PAY_LOAD_ADDR)

// cached & buffered
// for buffers
#define DATA_BUFFER_MAP_ADDR		 		0x18000000
#define DATA_BUFFER_MAP_SIZE                (sizeof(DATA_BUF_MAP))

#define DATA_BUFFFER_HASH_TABLE_ADDR		(DATA_BUFFER_MAP_ADDR + DATA_BUFFER_MAP_SIZE)
#define DATA_BUFFFER_HASH_TABLE_SIZE        (sizeof(DATA_BUF_HASH_TABLE))

#define TEMPORARY_DATA_BUFFER_MAP_ADDR 		(DATA_BUFFFER_HASH_TABLE_ADDR + DATA_BUFFFER_HASH_TABLE_SIZE)
#define TEMPORARY_DATA_BUFFER_MAP_SIZE 		(sizeof(TEMPORARY_DATA_BUF_MAP))

// for map tables
#define LOGICAL_SLICE_MAP_ADDR				(TEMPORARY_DATA_BUFFER_MAP_ADDR + TEMPORARY_DATA_BUFFER_MAP_SIZE)
#define LOGICAL_SLICE_MAP_SIZE              (sizeof(LOGICAL_SLICE_MAP))
#define VIRTUAL_SLICE_MAP_ADDR				(LOGICAL_SLICE_MAP_ADDR + LOGICAL_SLICE_MAP_SIZE)
#define VIRTUAL_SLICE_MAP_SIZE              (sizeof(VIRTUAL_SLICE_MAP))
#define VIRTUAL_BLOCK_MAP_ADDR				(VIRTUAL_SLICE_MAP_ADDR + VIRTUAL_SLICE_MAP_SIZE)
#define VIRTUAL_BLOCK_MAP_SIZE              (sizeof(VIRTUAL_BLOCK_MAP))
#define PHY_BLOCK_MAP_ADDR					(VIRTUAL_BLOCK_MAP_ADDR + VIRTUAL_BLOCK_MAP_SIZE)
#define PHY_BLOCK_MAP_SIZE                  (sizeof(PHY_BLOCK_MAP))
#define BAD_BLOCK_TABLE_INFO_MAP_ADDR		(PHY_BLOCK_MAP_ADDR + PHY_BLOCK_MAP_SIZE)
#define BAD_BLOCK_TABLE_INFO_MAP_SIZE       (sizeof(BAD_BLOCK_TABLE_INFO_MAP))
#define VIRTUAL_DIE_MAP_ADDR				(BAD_BLOCK_TABLE_INFO_MAP_ADDR + BAD_BLOCK_TABLE_INFO_MAP_SIZE)
#define VIRTUAL_DIE_MAP_SIZE                (sizeof(VIRTUAL_DIE_MAP))

// for GC victim selection
#define GC_VICTIM_MAP_ADDR					(VIRTUAL_DIE_MAP_ADDR + VIRTUAL_DIE_MAP_SIZE)
#define GC_VICTIM_MAP_SIZE                  (sizeof(GC_VICTIM_MAP))

// for request pool
#define REQ_POOL_ADDR						(GC_VICTIM_MAP_ADDR + GC_VICTIM_MAP_SIZE)
#define REQ_POOL_SIZE                       (sizeof(REQ_POOL))

// for dependency table
#define ROW_ADDR_DEPENDENCY_TABLE_ADDR		(REQ_POOL_ADDR + REQ_POOL_SIZE)
#define ROW_ADDR_DEPENDENCY_TABLE_SIZE      (sizeof(ROW_ADDR_DEPENDENCY_TABLE))

// for request scheduler
#define DIE_STATE_TABLE_ADDR				(ROW_ADDR_DEPENDENCY_TABLE_ADDR + ROW_ADDR_DEPENDENCY_TABLE_SIZE)
#define DIE_STATE_TABLE_SIZE                (sizeof(DIE_STATE_TABLE))

#define RETRY_LIMIT_TABLE_ADDR				(DIE_STATE_TABLE_ADDR + DIE_STATE_TABLE_SIZE)
#define RETRY_LIMIT_TABLE_SIZE              (sizeof(RETRY_LIMIT_TABLE))

#define WAY_PRIORITY_TABLE_ADDR 			(RETRY_LIMIT_TABLE_ADDR + RETRY_LIMIT_TABLE_SIZE)
#define WAY_PRIORITY_TABLE_SIZE             (sizeof(WAY_PRIORITY_TABLE))

#define FTL_MANAGEMENT_END_ADDR				((WAY_PRIORITY_TABLE_ADDR + WAY_PRIORITY_TABLE_SIZE) - 1)

#define RESERVED1_START_ADDR				(FTL_MANAGEMENT_END_ADDR + 1)

#define TRASH_CAN_ADDR                                (RESERVED1_START_ADDR)
#define TRASH_CAN_SIZE                                (BYTES_PER_SPARE_REGION_OF_SLICE)

#define DIRECT_DMA_BUFFER_ADDR                        (TRASH_CAN_ADDR + TRASH_CAN_SIZE)
#define DIRECT_DMA_BUFFER_SIZE                        (BYTES_PER_SECTOR * MAX_NUM_NVME_SLOT)

#define RESERVED1_SIZE                                RESERVED1_END_ADDR - RESERVED1_START_ADDR

#define DRAM_END_ADDR				      0x3FFFFFFF

#endif /* MEMORY_MAP_H_ */

