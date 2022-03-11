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

#include <gtest/gtest.h>
#include <taoserror.h>
#include <tglobal.h>
#include <iostream>

#include <metaDef.h>
#include <tmsg.h>
#include <tsdbDef.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(testCase, tSmaEncodeDecodeTest) {
  // encode
  STSma tSma = {0};
  tSma.version = 0;
  tSma.intervalUnit = TD_TIME_UNIT_DAY;
  tSma.interval = 1;
  tSma.slidingUnit = TD_TIME_UNIT_HOUR;
  tSma.sliding = 0;
  tstrncpy(tSma.indexName, "sma_index_test", TSDB_INDEX_NAME_LEN);
  tstrncpy(tSma.timezone, "Asia/Shanghai", TD_TIMEZONE_LEN);
  tSma.tableUid = 1234567890;
  tSma.nFuncColIds = 5;
  tSma.funcColIds = (SFuncColIds *)calloc(tSma.nFuncColIds, sizeof(SFuncColIds));
  ASSERT(tSma.funcColIds != NULL);
  for (int32_t n = 0; n < tSma.nFuncColIds; ++n) {
    SFuncColIds *funcColIds = tSma.funcColIds + n;
    funcColIds->funcId = n;
    funcColIds->nColIds = 10;
    funcColIds->colIds = (col_id_t *)calloc(funcColIds->nColIds, sizeof(col_id_t));
    ASSERT(funcColIds->colIds != NULL);
    for (int32_t i = 0; i < funcColIds->nColIds; ++i) {
      *(funcColIds->colIds + i) = (i + PRIMARYKEY_TIMESTAMP_COL_ID);
    }
  }

  STSmaWrapper tSmaWrapper = {.number = 1, .tSma = &tSma};
  uint32_t     bufLen = tEncodeTSmaWrapper(NULL, &tSmaWrapper);

  void *buf = calloc(bufLen, 1);
  assert(buf != NULL);

  STSmaWrapper *pSW = (STSmaWrapper *)buf;
  uint32_t      len = tEncodeTSmaWrapper(&buf, &tSmaWrapper);

  EXPECT_EQ(len, bufLen);

  // decode
  STSmaWrapper dstTSmaWrapper = {0};
  void *       result = tDecodeTSmaWrapper(pSW, &dstTSmaWrapper);
  assert(result != NULL);

  EXPECT_EQ(tSmaWrapper.number, dstTSmaWrapper.number);

  for (int i = 0; i < tSmaWrapper.number; ++i) {
    STSma *pSma = tSmaWrapper.tSma + i;
    STSma *qSma = dstTSmaWrapper.tSma + i;

    EXPECT_EQ(pSma->version, qSma->version);
    EXPECT_EQ(pSma->intervalUnit, qSma->intervalUnit);
    EXPECT_EQ(pSma->slidingUnit, qSma->slidingUnit);
    EXPECT_STRCASEEQ(pSma->indexName, qSma->indexName);
    EXPECT_STRCASEEQ(pSma->timezone, qSma->timezone);
    EXPECT_EQ(pSma->nFuncColIds, qSma->nFuncColIds);
    EXPECT_EQ(pSma->tableUid, qSma->tableUid);
    EXPECT_EQ(pSma->interval, qSma->interval);
    EXPECT_EQ(pSma->sliding, qSma->sliding);
    EXPECT_EQ(pSma->tagsFilterLen, qSma->tagsFilterLen);
    EXPECT_STRCASEEQ(pSma->tagsFilter, qSma->tagsFilter);
    for (uint32_t j = 0; j < pSma->nFuncColIds; ++j) {
      SFuncColIds *pFuncColIds = pSma->funcColIds + j;
      SFuncColIds *qFuncColIds = qSma->funcColIds + j;
      EXPECT_EQ(pFuncColIds->funcId, qFuncColIds->funcId);
      EXPECT_EQ(pFuncColIds->nColIds, qFuncColIds->nColIds);
      for (uint32_t k = 0; k < pFuncColIds->nColIds; ++k) {
        EXPECT_EQ(*(pFuncColIds->colIds + k), *(qFuncColIds->colIds + k));
      }
    }
  }

  // resource release
  tdDestroyTSma(&tSma);
  tdDestroyTSmaWrapper(&dstTSmaWrapper);
}

TEST(testCase, tSma_DB_Put_Get_Del_Test) {
  const char *   smaIndexName1 = "sma_index_test_1";
  const char *   smaIndexName2 = "sma_index_test_2";
  const char *   timeZone = "Asia/Shanghai";
  const char *   tagsFilter = "I'm tags filter";
  const char *   smaTestDir = "./smaTest";
  const uint64_t tbUid = 1234567890;
  const uint32_t nCntTSma = 2;
  // encode
  STSma tSma = {0};
  tSma.version = 0;
  tSma.intervalUnit = TD_TIME_UNIT_DAY;
  tSma.interval = 1;
  tSma.slidingUnit = TD_TIME_UNIT_HOUR;
  tSma.sliding = 0;
  tstrncpy(tSma.indexName, smaIndexName1, TSDB_INDEX_NAME_LEN);
  tstrncpy(tSma.timezone, timeZone, TD_TIMEZONE_LEN);
  tSma.tableUid = tbUid;
  tSma.nFuncColIds = 5;
  tSma.funcColIds = (SFuncColIds *)calloc(tSma.nFuncColIds, sizeof(SFuncColIds));
  ASSERT(tSma.funcColIds != NULL);
  for (int32_t n = 0; n < tSma.nFuncColIds; ++n) {
    SFuncColIds *funcColIds = tSma.funcColIds + n;
    funcColIds->funcId = n;
    funcColIds->nColIds = 10;
    funcColIds->colIds = (col_id_t *)calloc(funcColIds->nColIds, sizeof(col_id_t));
    ASSERT(funcColIds->colIds != NULL);
    for (int32_t i = 0; i < funcColIds->nColIds; ++i) {
      *(funcColIds->colIds + i) = (i + PRIMARYKEY_TIMESTAMP_COL_ID);
    }
  }
  tSma.tagsFilterLen = strlen(tagsFilter);
  tSma.tagsFilter = (char *)calloc(tSma.tagsFilterLen + 1, 1);
  tstrncpy(tSma.tagsFilter, tagsFilter, tSma.tagsFilterLen + 1);

  SMeta *         pMeta = NULL;
  STSma *         pSmaCfg = &tSma;
  const SMetaCfg *pMetaCfg = &defaultMetaOptions;

  taosRemoveDir(smaTestDir);

  pMeta = metaOpen(smaTestDir, pMetaCfg, NULL);
  assert(pMeta != NULL);
  // save index 1
  metaSaveSmaToDB(pMeta, pSmaCfg);

  tstrncpy(pSmaCfg->indexName, smaIndexName2, TSDB_INDEX_NAME_LEN);
  pSmaCfg->version = 1;
  pSmaCfg->intervalUnit = TD_TIME_UNIT_HOUR;
  pSmaCfg->interval = 1;
  pSmaCfg->slidingUnit = TD_TIME_UNIT_MINUTE;
  pSmaCfg->sliding = 5;

  // save index 2
  metaSaveSmaToDB(pMeta, pSmaCfg);

  // get value by indexName
  STSma *qSmaCfg = NULL;
  qSmaCfg = metaGetSmaInfoByName(pMeta, smaIndexName1);
  assert(qSmaCfg != NULL);
  printf("name1 = %s\n", qSmaCfg->indexName);
  printf("timezone1 = %s\n", qSmaCfg->timezone);
  printf("tagsFilter1 = %s\n", qSmaCfg->tagsFilter != NULL ? qSmaCfg->tagsFilter : "");
  EXPECT_STRCASEEQ(qSmaCfg->indexName, smaIndexName1);
  EXPECT_EQ(qSmaCfg->tableUid, tSma.tableUid);
  tdDestroyTSma(qSmaCfg);
  tfree(qSmaCfg);

  qSmaCfg = metaGetSmaInfoByName(pMeta, smaIndexName2);
  assert(qSmaCfg != NULL);
  printf("name2 = %s\n", qSmaCfg->indexName);
  printf("timezone2 = %s\n", qSmaCfg->timezone);
  printf("tagsFilter2 = %s\n", qSmaCfg->tagsFilter != NULL ? qSmaCfg->tagsFilter : "");
  EXPECT_STRCASEEQ(qSmaCfg->indexName, smaIndexName2);
  EXPECT_EQ(qSmaCfg->interval, tSma.interval);
  tdDestroyTSma(qSmaCfg);
  tfree(qSmaCfg);

  // get index name by table uid
  SMSmaCursor *pSmaCur = metaOpenSmaCursor(pMeta, tbUid);
  assert(pSmaCur != NULL);
  uint32_t indexCnt = 0;
  while (1) {
    const char *indexName = metaSmaCursorNext(pSmaCur);
    if (indexName == NULL) {
      break;
    }
    printf("indexName = %s\n", indexName);
    ++indexCnt;
  }
  EXPECT_EQ(indexCnt, nCntTSma);
  metaCloseSmaCurosr(pSmaCur);

  // get wrapper by table uid
  STSmaWrapper *pSW = metaGetSmaInfoByUid(pMeta, tbUid);
  assert(pSW != NULL);
  EXPECT_EQ(pSW->number, nCntTSma);
  EXPECT_STRCASEEQ(pSW->tSma->indexName, smaIndexName1);
  EXPECT_STRCASEEQ(pSW->tSma->timezone, timeZone);
  EXPECT_STRCASEEQ(pSW->tSma->tagsFilter, tagsFilter);
  EXPECT_EQ(pSW->tSma->tableUid, tSma.tableUid);
  EXPECT_STRCASEEQ((pSW->tSma + 1)->indexName, smaIndexName2);
  EXPECT_STRCASEEQ((pSW->tSma + 1)->timezone, timeZone);
  EXPECT_STRCASEEQ((pSW->tSma + 1)->tagsFilter, tagsFilter);
  EXPECT_EQ((pSW->tSma + 1)->tableUid, tSma.tableUid);

  tdDestroyTSmaWrapper(pSW);
  tfree(pSW);

  // get all sma table uids
  SArray *pUids = metaGetSmaTbUids(pMeta, false);
  assert(pUids != NULL);
  for (uint32_t i = 0; i < taosArrayGetSize(pUids); ++i) {
    printf("metaGetSmaTbUids: uid[%" PRIu32 "] = %" PRIi64 "\n", i, *(tb_uid_t *)taosArrayGet(pUids, i));
    // printf("metaGetSmaTbUids: index[%" PRIu32 "] = %s", i, (char *)taosArrayGet(pUids, i));
  }
  EXPECT_EQ(taosArrayGetSize(pUids), 1);
  taosArrayDestroy(pUids);

  // resource release
  metaRemoveSmaFromDb(pMeta, smaIndexName1);
  metaRemoveSmaFromDb(pMeta, smaIndexName2);

  tdDestroyTSma(&tSma);
  metaClose(pMeta);
}

#if 0
TEST(testCase, tSmaInsertTest) {
  STSma      tSma = {0};
  STSmaData *pSmaData = NULL;
  STsdb      tsdb = {0};

  // init
  tSma.intervalUnit = TD_TIME_UNIT_DAY;
  tSma.interval = 1;
  tSma.numOfFuncIds = 5;  // sum/min/max/avg/last

  int32_t blockSize = tSma.numOfFuncIds * sizeof(int64_t);
  int32_t numOfColIds = 3;
  int32_t numOfBlocks = 10;

  int32_t dataLen = numOfColIds * numOfBlocks * blockSize;

  pSmaData = (STSmaData *)malloc(sizeof(STSmaData) + dataLen);
  ASSERT_EQ(pSmaData != NULL, true);
  pSmaData->tableUid = 3232329230;
  pSmaData->numOfColIds = numOfColIds;
  pSmaData->numOfBlocks = numOfBlocks;
  pSmaData->dataLen = dataLen;
  pSmaData->tsWindow.skey = 1640000000;
  pSmaData->tsWindow.ekey = 1645788649;
  pSmaData->colIds = (col_id_t *)malloc(sizeof(col_id_t) * numOfColIds);
  ASSERT_EQ(pSmaData->colIds != NULL, true);

  for (int32_t i = 0; i < numOfColIds; ++i) {
    *(pSmaData->colIds + i) = (i + PRIMARYKEY_TIMESTAMP_COL_ID);
  }

  // execute
  EXPECT_EQ(tsdbInsertTSmaData(&tsdb, &tSma, pSmaData), TSDB_CODE_SUCCESS);

  // release
  tdDestroySmaData(pSmaData);
}
#endif

#pragma GCC diagnostic pop