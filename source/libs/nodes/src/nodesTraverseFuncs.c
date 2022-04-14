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

#include "querynodes.h"

typedef enum ETraversalOrder {
  TRAVERSAL_PREORDER = 1,
  TRAVERSAL_INORDER,
  TRAVERSAL_POSTORDER,
} ETraversalOrder;

static EDealRes walkList(SNodeList* pNodeList, ETraversalOrder order, FNodeWalker walker, void* pContext);

static EDealRes walkNode(SNode* pNode, ETraversalOrder order, FNodeWalker walker, void* pContext) {
  if (NULL == pNode) {
    return DEAL_RES_CONTINUE;
  }

  EDealRes res = DEAL_RES_CONTINUE;

  if (TRAVERSAL_PREORDER == order) {
    res = walker(pNode, pContext);
    if (DEAL_RES_CONTINUE != res) {
      return res;
    }
  }

  switch (nodeType(pNode)) {
    case QUERY_NODE_COLUMN:
    case QUERY_NODE_VALUE:
    case QUERY_NODE_LIMIT:
      // these node types with no subnodes
      break;
    case QUERY_NODE_OPERATOR: {
      SOperatorNode* pOpNode = (SOperatorNode*)pNode;
      res = walkNode(pOpNode->pLeft, order, walker, pContext);
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = walkNode(pOpNode->pRight, order, walker, pContext);
      }
      break;
    }
    case QUERY_NODE_LOGIC_CONDITION:
      res = walkList(((SLogicConditionNode*)pNode)->pParameterList, order, walker, pContext);
      break;
    case QUERY_NODE_FUNCTION:
      res = walkList(((SFunctionNode*)pNode)->pParameterList, order, walker, pContext);
      break;
    case QUERY_NODE_REAL_TABLE:
    case QUERY_NODE_TEMP_TABLE:
      break; // todo
    case QUERY_NODE_JOIN_TABLE: {
      SJoinTableNode* pJoinTableNode = (SJoinTableNode*)pNode;
      res = walkNode(pJoinTableNode->pLeft, order, walker, pContext);
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = walkNode(pJoinTableNode->pRight, order, walker, pContext);
      }
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = walkNode(pJoinTableNode->pOnCond, order, walker, pContext);
      }
      break;
    }
    case QUERY_NODE_GROUPING_SET:
      res = walkList(((SGroupingSetNode*)pNode)->pParameterList, order, walker, pContext);
      break;
    case QUERY_NODE_ORDER_BY_EXPR:
      res = walkNode(((SOrderByExprNode*)pNode)->pExpr, order, walker, pContext);
      break;
    case QUERY_NODE_STATE_WINDOW: {
      SStateWindowNode* pState = (SStateWindowNode*)pNode;
      res = walkNode(pState->pExpr, order, walker, pContext);
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = walkNode(pState->pCol, order, walker, pContext);
      }
      break;
    }
    case QUERY_NODE_SESSION_WINDOW: {
      SSessionWindowNode* pSession = (SSessionWindowNode*)pNode;
      res = walkNode(pSession->pCol, order, walker, pContext);
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = walkNode(pSession->pGap, order, walker, pContext);
      }
      break;
    }
    case QUERY_NODE_INTERVAL_WINDOW: {
      SIntervalWindowNode* pInterval = (SIntervalWindowNode*)pNode;
      res = walkNode(pInterval->pInterval, order, walker, pContext);
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = walkNode(pInterval->pOffset, order, walker, pContext);
      }
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = walkNode(pInterval->pSliding, order, walker, pContext);
      }
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = walkNode(pInterval->pFill, order, walker, pContext);
      }
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = walkNode(pInterval->pCol, order, walker, pContext);
      }
      break;
    }
    case QUERY_NODE_NODE_LIST:
      res = walkList(((SNodeListNode*)pNode)->pNodeList, order, walker, pContext);
      break;
    case QUERY_NODE_FILL:
      res = walkNode(((SFillNode*)pNode)->pValues, order, walker, pContext);
      break;
    case QUERY_NODE_RAW_EXPR:
      res = walkNode(((SRawExprNode*)pNode)->pNode, order, walker, pContext);
      break;
    case QUERY_NODE_TARGET:
      res = walkNode(((STargetNode*)pNode)->pExpr, order, walker, pContext);
      break;
    default:
      break;
  }

  if (DEAL_RES_ERROR != res && DEAL_RES_END != res && TRAVERSAL_POSTORDER == order) {
    res = walker(pNode, pContext);
  }

  return res;
}

static EDealRes walkList(SNodeList* pNodeList, ETraversalOrder order, FNodeWalker walker, void* pContext) {
  SNode* node;
  FOREACH(node, pNodeList) {
    EDealRes res = walkNode(node, order, walker, pContext);
    if (DEAL_RES_ERROR == res || DEAL_RES_END == res) {
      return res;
    }
  }
  return DEAL_RES_CONTINUE;
}

void nodesWalkExpr(SNodeptr pNode, FNodeWalker walker, void* pContext) {
  (void)walkNode(pNode, TRAVERSAL_PREORDER, walker, pContext);
}

void nodesWalkExprs(SNodeList* pNodeList, FNodeWalker walker, void* pContext) {
  (void)walkList(pNodeList, TRAVERSAL_PREORDER, walker, pContext);
}

void nodesWalkExprPostOrder(SNodeptr pNode, FNodeWalker walker, void* pContext) {
  (void)walkNode(pNode, TRAVERSAL_POSTORDER, walker, pContext);
}

void nodesWalkExprsPostOrder(SNodeList* pList, FNodeWalker walker, void* pContext) {
  (void)walkList(pList, TRAVERSAL_POSTORDER, walker, pContext);
}

static EDealRes rewriteList(SNodeList* pNodeList, ETraversalOrder order, FNodeRewriter rewriter, void* pContext);

static EDealRes rewriteNode(SNode** pRawNode, ETraversalOrder order, FNodeRewriter rewriter, void* pContext) {
  if (NULL == pRawNode || NULL == *pRawNode) {
    return DEAL_RES_CONTINUE;
  }

  EDealRes res = DEAL_RES_CONTINUE;

  if (TRAVERSAL_PREORDER == order) {
    res = rewriter(pRawNode, pContext);
    if (DEAL_RES_CONTINUE != res) {
      return res;
    }
  }

  SNode* pNode = *pRawNode;
  switch (nodeType(pNode)) {
    case QUERY_NODE_COLUMN:
    case QUERY_NODE_VALUE:
    case QUERY_NODE_LIMIT:
      // these node types with no subnodes
      break;
    case QUERY_NODE_OPERATOR: {
      SOperatorNode* pOpNode = (SOperatorNode*)pNode;
      res = rewriteNode(&(pOpNode->pLeft), order, rewriter, pContext);
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = rewriteNode(&(pOpNode->pRight), order, rewriter, pContext);
      }
      break;
    }
    case QUERY_NODE_LOGIC_CONDITION:
      res = rewriteList(((SLogicConditionNode*)pNode)->pParameterList, order, rewriter, pContext);
      break;
    case QUERY_NODE_FUNCTION:
      res = rewriteList(((SFunctionNode*)pNode)->pParameterList, order, rewriter, pContext);
      break;
    case QUERY_NODE_REAL_TABLE:
    case QUERY_NODE_TEMP_TABLE:
      break; // todo
    case QUERY_NODE_JOIN_TABLE: {
      SJoinTableNode* pJoinTableNode = (SJoinTableNode*)pNode;
      res = rewriteNode(&(pJoinTableNode->pLeft), order, rewriter, pContext);
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = rewriteNode(&(pJoinTableNode->pRight), order, rewriter, pContext);
      }
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = rewriteNode(&(pJoinTableNode->pOnCond), order, rewriter, pContext);
      }
      break;
    }
    case QUERY_NODE_GROUPING_SET:
      res = rewriteList(((SGroupingSetNode*)pNode)->pParameterList, order, rewriter, pContext);
      break;
    case QUERY_NODE_ORDER_BY_EXPR:
      res = rewriteNode(&(((SOrderByExprNode*)pNode)->pExpr), order, rewriter, pContext);
      break;
    case QUERY_NODE_STATE_WINDOW: {
      SStateWindowNode* pState = (SStateWindowNode*)pNode;
      res = rewriteNode(&pState->pExpr, order, rewriter, pContext);
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = rewriteNode(&pState->pCol, order, rewriter, pContext);
      }
      break;
    }
    case QUERY_NODE_SESSION_WINDOW: {
      SSessionWindowNode* pSession = (SSessionWindowNode*)pNode;
      res = rewriteNode(&pSession->pCol, order, rewriter, pContext);
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = rewriteNode(&pSession->pGap, order, rewriter, pContext);
      }
      break;
    }
    case QUERY_NODE_INTERVAL_WINDOW: {
      SIntervalWindowNode* pInterval = (SIntervalWindowNode*)pNode;
      res = rewriteNode(&(pInterval->pInterval), order, rewriter, pContext);
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = rewriteNode(&(pInterval->pOffset), order, rewriter, pContext);
      }
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = rewriteNode(&(pInterval->pSliding), order, rewriter, pContext);
      }
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = rewriteNode(&(pInterval->pFill), order, rewriter, pContext);
      }
      if (DEAL_RES_ERROR != res && DEAL_RES_END != res) {
        res = rewriteNode(&(pInterval->pCol), order, rewriter, pContext);
      }
      break;
    }
    case QUERY_NODE_NODE_LIST:
      res = rewriteList(((SNodeListNode*)pNode)->pNodeList, order, rewriter, pContext);
      break;
    case QUERY_NODE_FILL:
      res = rewriteNode(&(((SFillNode*)pNode)->pValues), order, rewriter, pContext);
      break;
    case QUERY_NODE_RAW_EXPR:
      res = rewriteNode(&(((SRawExprNode*)pNode)->pNode), order, rewriter, pContext);
      break;
    case QUERY_NODE_TARGET:
      res = rewriteNode(&(((STargetNode*)pNode)->pExpr), order, rewriter, pContext);
      break;
    default:
      break;
  }

  if (DEAL_RES_ERROR != res && DEAL_RES_END != res && TRAVERSAL_POSTORDER == order) {
    res = rewriter(pRawNode, pContext);
  }

  return res;
}

static EDealRes rewriteList(SNodeList* pNodeList, ETraversalOrder order, FNodeRewriter rewriter, void* pContext) {
  SNode** pNode;
  FOREACH_FOR_REWRITE(pNode, pNodeList) {
    EDealRes res = rewriteNode(pNode, order, rewriter, pContext);
    if (DEAL_RES_ERROR == res || DEAL_RES_END == res) {
      return res;
    }
  }
  return DEAL_RES_CONTINUE;
}

void nodesRewriteExpr(SNode** pNode, FNodeRewriter rewriter, void* pContext) {
  (void)rewriteNode(pNode, TRAVERSAL_PREORDER, rewriter, pContext);
}

void nodesRewriteExprs(SNodeList* pList, FNodeRewriter rewriter, void* pContext) {
  (void)rewriteList(pList, TRAVERSAL_PREORDER, rewriter, pContext);
}

void nodesRewriteExprPostOrder(SNode** pNode, FNodeRewriter rewriter, void* pContext) {
  (void)rewriteNode(pNode, TRAVERSAL_POSTORDER, rewriter, pContext);
}

void nodesRewriteExprsPostOrder(SNodeList* pList, FNodeRewriter rewriter, void* pContext) {
  (void)rewriteList(pList, TRAVERSAL_POSTORDER, rewriter, pContext);
}

void nodesWalkSelectStmt(SSelectStmt* pSelect, ESqlClause clause, FNodeWalker walker, void* pContext) {
  if (NULL == pSelect) {
    return;
  }

  switch (clause) {
    case SQL_CLAUSE_FROM:
      nodesWalkExpr(pSelect->pFromTable, walker, pContext);
      nodesWalkExpr(pSelect->pWhere, walker, pContext);
    case SQL_CLAUSE_WHERE:
      nodesWalkExprs(pSelect->pPartitionByList, walker, pContext);
    case SQL_CLAUSE_PARTITION_BY:
      nodesWalkExpr(pSelect->pWindow, walker, pContext);
    case SQL_CLAUSE_WINDOW:
      nodesWalkExprs(pSelect->pGroupByList, walker, pContext);
    case SQL_CLAUSE_GROUP_BY:
      nodesWalkExpr(pSelect->pHaving, walker, pContext);
    case SQL_CLAUSE_HAVING:
    case SQL_CLAUSE_DISTINCT:
      nodesWalkExprs(pSelect->pOrderByList, walker, pContext);
    case SQL_CLAUSE_ORDER_BY:
      nodesWalkExprs(pSelect->pProjectionList, walker, pContext);      
    default:
      break;
  }

  return;
}

void nodesRewriteSelectStmt(SSelectStmt* pSelect, ESqlClause clause, FNodeRewriter rewriter, void* pContext) {
  if (NULL == pSelect) {
    return;
  }

  switch (clause) {
    case SQL_CLAUSE_FROM:
      nodesRewriteExpr(&(pSelect->pFromTable), rewriter, pContext);
      nodesRewriteExpr(&(pSelect->pWhere), rewriter, pContext);
    case SQL_CLAUSE_WHERE:
      nodesRewriteExprs(pSelect->pPartitionByList, rewriter, pContext);
    case SQL_CLAUSE_PARTITION_BY:
      nodesRewriteExpr(&(pSelect->pWindow), rewriter, pContext);
    case SQL_CLAUSE_WINDOW:
      nodesRewriteExprs(pSelect->pGroupByList, rewriter, pContext);
    case SQL_CLAUSE_GROUP_BY:
      nodesRewriteExpr(&(pSelect->pHaving), rewriter, pContext);
    case SQL_CLAUSE_HAVING:
    case SQL_CLAUSE_DISTINCT:
      nodesRewriteExprs(pSelect->pOrderByList, rewriter, pContext);
    case SQL_CLAUSE_ORDER_BY:
      nodesRewriteExprs(pSelect->pProjectionList, rewriter, pContext);      
    default:
      break;
  }

  return;
}
