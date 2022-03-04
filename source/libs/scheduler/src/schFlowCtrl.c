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

#include "schedulerInt.h"
#include "tmsg.h"
#include "query.h"
#include "catalog.h"
#include "tref.h"

void schFreeFlowCtrl(SSchLevel *pLevel) {
  if (NULL == pLevel->flowCtrl) {
    return;
  }

  SSchFlowControl *ctrl = NULL;
  void *pIter = taosHashIterate(pLevel->flowCtrl, NULL);
  while (pIter) {
    ctrl = (SSchFlowControl *)pIter;

    if (ctrl->taskList) {
      taosArrayDestroy(ctrl->taskList);
    }
    
    pIter = taosHashIterate(pLevel->flowCtrl, pIter);
  }

  taosHashCleanup(pLevel->flowCtrl);
  pLevel->flowCtrl = NULL;
}

int32_t schCheckJobNeedFlowCtrl(SSchJob *pJob, SSchLevel *pLevel) {
  if (!SCH_IS_QUERY_JOB(pJob)) {
    return TSDB_CODE_SUCCESS;
  }

  double sum = 0;
  
  for (int32_t i = 0; i < pLevel->taskNum; ++i) {
    SSchTask *pTask = taosArrayGet(pLevel->subTasks, i);

    sum += pTask->plan->execNodeStat.tableNum;
  }

  if (sum < schMgmt.cfg.maxNodeTableNum) {
    return TSDB_CODE_SUCCESS;
  }

  pLevel->flowCtrl = taosHashInit(pLevel->taskNum, taosGetDefaultHashFunction(TSDB_DATA_TYPE_BINARY), false, HASH_ENTRY_LOCK);
  if (NULL == pLevel->flowCtrl) {
    SCH_JOB_ELOG("taosHashInit %d flowCtrl failed", pLevel->taskNum);
    SCH_ERR_RET(TSDB_CODE_QRY_OUT_OF_MEMORY);
  }

  SCH_SET_JOB_NEED_FLOW_CTRL(pJob);

  return TSDB_CODE_SUCCESS;
}

int32_t schDecTaskFlowQuota(SSchJob *pJob, SSchTask *pTask) {
  SSchLevel *pLevel = pTask->level;
  SSchFlowControl *ctrl = NULL;
  int32_t code = 0;
  SEp *ep = SCH_GET_CUR_EP(&pTask->plan->execNode);
  
  ctrl = (SSchFlowControl *)taosHashGet(pLevel->flowCtrl, ep, sizeof(SEp));
  if (NULL == ctrl) {
    SCH_TASK_ELOG("taosHashGet node from flowCtrl failed, fqdn:%s, port:%d", ep->fqdn, ep->port);
    SCH_ERR_RET(TSDB_CODE_SCH_INTERNAL_ERROR);
  }
  
  SCH_LOCK(SCH_WRITE, &ctrl->lock);
  if (ctrl->execTaskNum <= 0) {
    SCH_TASK_ELOG("taosHashGet node from flowCtrl failed, fqdn:%s, port:%d", ep->fqdn, ep->port);
    SCH_ERR_JRET(TSDB_CODE_SCH_INTERNAL_ERROR);
  }

  --ctrl->execTaskNum;
  ctrl->tableNumSum -= pTask->plan->execNodeStat.tableNum;

_return:

  SCH_UNLOCK(SCH_WRITE, &ctrl->lock);

  SCH_RET(code);
}

int32_t schCheckIncTaskFlowQuota(SSchJob *pJob, SSchTask *pTask, bool *enough) {
  SSchLevel *pLevel = pTask->level;
  int32_t code = 0;
  SSchFlowControl *ctrl = NULL;
  
  do {
    ctrl = (SSchFlowControl *)taosHashGet(pLevel->flowCtrl, SCH_GET_CUR_EP(&pTask->plan->execNode), sizeof(SEp));
    if (NULL == ctrl) {
      SSchFlowControl nctrl = {.tableNumSum = pTask->plan->execNodeStat.tableNum, .execTaskNum = 1};

      code = taosHashPut(pLevel->flowCtrl, SCH_GET_CUR_EP(&pTask->plan->execNode), sizeof(SEp), &nctrl, sizeof(nctrl));
      if (code) {
        if (HASH_NODE_EXIST(code)) {
          continue;
        }
        
        SCH_TASK_ELOG("taosHashPut flowCtrl failed, size:%d", (int32_t)sizeof(nctrl));
        SCH_ERR_RET(TSDB_CODE_QRY_OUT_OF_MEMORY);
      }

      *enough = true;
      return TSDB_CODE_SUCCESS;
    }

    SCH_LOCK(SCH_WRITE, &ctrl->lock);
    
    if (0 == ctrl->execTaskNum) {
      ctrl->tableNumSum = pTask->plan->execNodeStat.tableNum;
      ++ctrl->execTaskNum;
      
      *enough = true;
      break;
    }
    
    double sum = pTask->plan->execNodeStat.tableNum + ctrl->tableNumSum;
    
    if (sum <= schMgmt.cfg.maxNodeTableNum) {
      ctrl->tableNumSum = sum;
      ++ctrl->execTaskNum;
      
      *enough = true;
      break;
    }

    if (NULL == ctrl->taskList) {
      ctrl->taskList = taosArrayInit(pLevel->taskNum, POINTER_BYTES);
      if (NULL == ctrl->taskList) {
        SCH_TASK_ELOG("taosArrayInit taskList failed, size:%d", (int32_t)pLevel->taskNum);
        SCH_ERR_JRET(TSDB_CODE_QRY_OUT_OF_MEMORY);
      }
    }

    if (NULL == taosArrayPush(ctrl->taskList, &pTask)) {
      SCH_TASK_ELOG("taosArrayPush to taskList failed, size:%d", (int32_t)taosArrayGetSize(ctrl->taskList));
      SCH_ERR_JRET(TSDB_CODE_QRY_OUT_OF_MEMORY);
    }

    *enough = false;
    
    break;
  } while (true);

_return:

  SCH_UNLOCK(SCH_WRITE, &ctrl->lock);
  
  SCH_RET(code);
}

int32_t schLaunchTasksInFlowCtrlListImpl(SSchJob *pJob, SSchFlowControl *ctrl) {
  do {
    SCH_LOCK(SCH_WRITE, &ctrl->lock);
    
    if (NULL == ctrl->taskList || taosArrayGetSize(ctrl->taskList) <= 0) {
      SCH_UNLOCK(SCH_WRITE, &ctrl->lock);
      return TSDB_CODE_SUCCESS;
    }

    SSchTask *pTask = *(SSchTask **)taosArrayPop(ctrl->taskList);
    
    SCH_UNLOCK(SCH_WRITE, &ctrl->lock);

    bool enough = false;  
    SCH_ERR_RET(schCheckIncTaskFlowQuota(pJob, pTask, &enough));

    if (enough) {
      SCH_ERR_RET(schLaunchTaskImpl(pJob, pTask));
      continue;
    }

    break;
  } while (true);

  return TSDB_CODE_SUCCESS;
}


int32_t schLaunchTasksInFlowCtrlList(SSchJob *pJob, SSchTask *pTask) {
  if (!SCH_TASK_NEED_FLOW_CTRL(pJob, pTask)) {
    return TSDB_CODE_SUCCESS;
  }

  SCH_ERR_RET(schDecTaskFlowQuota(pJob, pTask));

  SSchLevel *pLevel = pTask->level;
  SEp *ep = SCH_GET_CUR_EP(&pTask->plan->execNode);
  
  SSchFlowControl *ctrl = (SSchFlowControl *)taosHashGet(pLevel->flowCtrl, ep, sizeof(SEp));
  if (NULL == ctrl) {
    SCH_TASK_ELOG("taosHashGet node from flowCtrl failed, fqdn:%s, port:%d", ep->fqdn, ep->port);
    SCH_ERR_RET(TSDB_CODE_SCH_INTERNAL_ERROR);
  }
  
  SCH_ERR_RET(schLaunchTasksInFlowCtrlListImpl(pJob, ctrl));
  
}


