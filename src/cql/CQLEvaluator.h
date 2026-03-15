#pragma once
// CQLEvaluator — Tree-walking interpreter for CQL AST

#include "cql/CQLAst.h"
#include "cql/CQLContext.h"
#include "cql/CQLValue.h"
#include "cql/RetrieveProvider.h"
#include <map>
#include <string>
#include <vector>

namespace hedis {

class ValueSetManager;

class CQLEvaluator {
public:
    explicit CQLEvaluator(const ValueSetManager& vsMgr);
    ~CQLEvaluator();

    // Evaluate a single expression node against a context
    CQLValue evaluate(const CQLNode* node, CQLContext& ctx);

    // Evaluate an entire library — returns map of define name → result
    std::map<std::string, CQLValue> evaluateLibrary(
        const LibraryNode* library, CQLContext& ctx);

    // Evaluate a specific define by name within a library
    CQLValue evaluateDefine(const LibraryNode* library,
                            const std::string& defineName,
                            CQLContext& ctx);

private:
    const ValueSetManager& m_vsMgr;
    RetrieveProvider m_retriever;

    // Current library being evaluated (for define cross-references)
    const LibraryNode* m_currentLibrary = nullptr;

    // Dispatch by node type
    CQLValue evalLiteral(const LiteralNode* n, CQLContext& ctx);
    CQLValue evalIdentifier(const IdentifierNode* n, CQLContext& ctx);
    CQLValue evalBinaryOp(const BinaryOpNode* n, CQLContext& ctx);
    CQLValue evalUnaryOp(const UnaryOpNode* n, CQLContext& ctx);
    CQLValue evalRetrieve(const RetrieveNode* n, CQLContext& ctx);
    CQLValue evalQuery(const QueryNode* n, CQLContext& ctx);
    CQLValue evalFunction(const FunctionCallNode* n, CQLContext& ctx);
    CQLValue evalProperty(const PropertyNode* n, CQLContext& ctx);
    CQLValue evalInterval(const IntervalNode* n, CQLContext& ctx);
    CQLValue evalDateTimeOp(const DateTimeOpNode* n, CQLContext& ctx);
    CQLValue evalIf(const IfNode* n, CQLContext& ctx);
    CQLValue evalParameterRef(const ParameterRefNode* n, CQLContext& ctx);

    // Built-in CQL functions
    CQLValue fnExists(const CQLValue& arg);
    CQLValue fnCount(const CQLValue& arg);
    CQLValue fnFirst(const CQLValue& arg);
    CQLValue fnLast(const CQLValue& arg);
    CQLValue fnAgeInYearsAt(CQLContext& ctx, const CQLValue& date);
    CQLValue fnAgeInMonthsAt(CQLContext& ctx, const CQLValue& date);
    CQLValue fnToDate(const CQLValue& val);
    CQLValue fnToString(const CQLValue& val);
    CQLValue fnDurationBetween(const CQLValue& from, const CQLValue& to,
                                const std::string& precision);

    // Temporal operators
    CQLValue evalDuring(const CQLValue& point, const CQLValue& interval);
    CQLValue evalOverlaps(const CQLValue& interval1, const CQLValue& interval2);
    CQLValue evalBefore(const CQLValue& left, const CQLValue& right);
    CQLValue evalAfter(const CQLValue& left, const CQLValue& right);
    CQLValue evalBetween(const CQLValue& val, const CQLValue& interval);
};

}  // namespace hedis
