/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DATA_SINK_MGT_H
#define _DATA_SINK_MGT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "os.h"
#include "executorimpl.h"

#define DS_CAPACITY_ENOUGH 1
#define DS_CAPACITY_FULL   2
#define DS_NEED_SCHEDULE   3

struct SDataSink;
struct SSDataBlock;

typedef struct SDataSinkMgtCfg {
  uint32_t maxDataBlockNum;
  uint32_t maxDataBlockNumPerQuery;
} SDataSinkMgtCfg;

int32_t dsDataSinkMgtInit(SDataSinkMgtCfg *cfg);

typedef void* DataSinkHandle;

typedef struct SDataResult {
  SQueryCostInfo profile;
  const SSDataBlock* pData;
  SHashObj* pTableRetrieveTsMap;
} SDataResult;

/**
 * Create a subplan's datasinker handle for all later operations. 
 * @param pDataSink
 * @param pHandle output
 * @return error code
 */
int32_t dsCreateDataSinker(const struct SDataSink *pDataSink, DataSinkHandle* pHandle);

/**
 * Put the result set returned by the executor into datasinker.
 * @param handle
 * @param pRes
 * @return error code
 */
int32_t dsPutDataBlock(DataSinkHandle handle, const SDataResult* pRes);

/**
 * Get the length of the data returned by the next call to dsGetDataBlock.
 * @param handle
 * @return data length
 */
int32_t dsGetDataLength(DataSinkHandle handle);

/**
 * Get data, the caller needs to allocate data memory.
 * @param handle
 * @param pData output
 * @param pLen output
 * @return error code
 */
int32_t dsGetDataBlock(DataSinkHandle handle, char* pData, int32_t* pLen);

/**
 * Get the datasinker state, after each dsPutDataBlock and dsGetDataBlock call.
 * @param handle
 * @return datasinker status
 */
int32_t dsGetStatus(DataSinkHandle handle);

/**
 * After dsGetStatus returns DS_NEED_SCHEDULE, the caller need to put this into the work queue.
 * @param ahandle
 * @param pItem
 */
void dsScheduleProcess(void* ahandle, void* pItem);

/**
 * Destroy the datasinker handle.
 * @param handle
 */
void dsDestroyDataSinker(DataSinkHandle handle);

#ifdef __cplusplus
}
#endif

#endif /*_DATA_SINK_MGT_H*/