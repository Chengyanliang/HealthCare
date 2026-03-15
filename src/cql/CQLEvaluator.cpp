// CQLEvaluator — Tree-walking CQL interpreter

#include "cql/CQLEvaluator.h"
#include "measure/ValueSetManager.h"
#include <algorithm>
#include <cmath>

namespace hedis {

CQLEvaluator::CQLEvaluator(const ValueSetManager& vsMgr)
    : m_vsMgr(vsMgr) {}

CQLEvaluator::~CQLEvaluator() = default;

// ---------------------------------------------------------------------------
// evaluateLibrary — evaluate all defines, return name → value map
// ---------------------------------------------------------------------------
std::map<std::string, CQLValue> CQLEvaluator::evaluateLibrary(
        const LibraryNode* library, CQLContext& ctx) {
    m_currentLibrary = library;
    ctx.clearDefineCache();

    std::map<std::string, CQLValue> results;
    for (const auto& def : library->definitions) {
        CQLValue val = evaluateDefine(library, def->name, ctx);
        results[def->name] = val;
    }

    m_currentLibrary = nullptr;
    return results;
}

// ---------------------------------------------------------------------------
// evaluateDefine — evaluate a named define (with memoization)
// ---------------------------------------------------------------------------
CQLValue CQLEvaluator::evaluateDefine(const LibraryNode* library,
                                       const std::string& defineName,
                                       CQLContext& ctx) {
    // Check cache
    if (ctx.hasDefineResult(defineName))
        return ctx.getDefineResult(defineName);

    m_currentLibrary = library;

    // Find the define
    for (const auto& def : library->definitions) {
        if (def->name == defineName) {
            CQLValue result = evaluate(def->expression.get(), ctx);
            ctx.cacheDefineResult(defineName, result);
            return result;
        }
    }

    return CQLValue::null();  // define not found
}

// ---------------------------------------------------------------------------
// evaluate — dispatch by node type
// ---------------------------------------------------------------------------
CQLValue CQLEvaluator::evaluate(const CQLNode* node, CQLContext& ctx) {
    if (!node) return CQLValue::null();

    if (auto* n = dynamic_cast<const LiteralNode*>(node))
        return evalLiteral(n, ctx);
    if (auto* n = dynamic_cast<const IdentifierNode*>(node))
        return evalIdentifier(n, ctx);
    if (auto* n = dynamic_cast<const BinaryOpNode*>(node))
        return evalBinaryOp(n, ctx);
    if (auto* n = dynamic_cast<const UnaryOpNode*>(node))
        return evalUnaryOp(n, ctx);
    if (auto* n = dynamic_cast<const RetrieveNode*>(node))
        return evalRetrieve(n, ctx);
    if (auto* n = dynamic_cast<const QueryNode*>(node))
        return evalQuery(n, ctx);
    if (auto* n = dynamic_cast<const FunctionCallNode*>(node))
        return evalFunction(n, ctx);
    if (auto* n = dynamic_cast<const PropertyNode*>(node))
        return evalProperty(n, ctx);
    if (auto* n = dynamic_cast<const IntervalNode*>(node))
        return evalInterval(n, ctx);
    if (auto* n = dynamic_cast<const DateTimeOpNode*>(node))
        return evalDateTimeOp(n, ctx);
    if (auto* n = dynamic_cast<const IfNode*>(node))
        return evalIf(n, ctx);
    if (auto* n = dynamic_cast<const ParameterRefNode*>(node))
        return evalParameterRef(n, ctx);

    return CQLValue::null();
}

// ---------------------------------------------------------------------------
// Node evaluators
// ---------------------------------------------------------------------------

CQLValue CQLEvaluator::evalLiteral(const LiteralNode* n, CQLContext& /*ctx*/) {
    return n->value;
}

CQLValue CQLEvaluator::evalIdentifier(const IdentifierNode* n, CQLContext& ctx) {
    // Check scope variables first (query aliases)
    CQLValue v = ctx.getVariable(n->name);
    if (!v.isNull()) return v;

    // Check parameters
    v = ctx.getParameter(n->name);
    if (!v.isNull()) return v;

    // Patient reference
    if (n->name == "Patient")
        return CQLValue::fromString(ctx.patient().patientId);

    // Try to evaluate as a define reference
    if (m_currentLibrary) {
        for (const auto& def : m_currentLibrary->definitions) {
            if (def->name == n->name) {
                return evaluateDefine(m_currentLibrary, n->name, ctx);
            }
        }
    }

    return CQLValue::null();
}

CQLValue CQLEvaluator::evalBinaryOp(const BinaryOpNode* n, CQLContext& ctx) {
    // Short-circuit for And/Or
    if (n->op == BinaryOpNode::And) {
        CQLValue left = evaluate(n->left.get(), ctx);
        if (left.isBool() && !left.asBool()) return CQLValue::fromBool(false);
        CQLValue right = evaluate(n->right.get(), ctx);
        return left.cqlAnd(right);
    }
    if (n->op == BinaryOpNode::Or) {
        CQLValue left = evaluate(n->left.get(), ctx);
        if (left.isBool() && left.asBool()) return CQLValue::fromBool(true);
        CQLValue right = evaluate(n->right.get(), ctx);
        return left.cqlOr(right);
    }

    CQLValue left  = evaluate(n->left.get(), ctx);
    CQLValue right = evaluate(n->right.get(), ctx);

    switch (n->op) {
        case BinaryOpNode::Eq:  return left.cqlEqual(right);
        case BinaryOpNode::Neq: return left.cqlNotEqual(right);
        case BinaryOpNode::Lt:  return left.cqlLess(right);
        case BinaryOpNode::Gt:  return left.cqlGreater(right);
        case BinaryOpNode::Le:  return left.cqlLessOrEqual(right);
        case BinaryOpNode::Ge:  return left.cqlGreaterOrEqual(right);
        case BinaryOpNode::Add: return left.cqlAdd(right);
        case BinaryOpNode::Sub: return left.cqlSubtract(right);
        case BinaryOpNode::Mul: return left.cqlMultiply(right);
        case BinaryOpNode::Div: return left.cqlDivide(right);
        case BinaryOpNode::In:  return left.cqlIn(right);
        case BinaryOpNode::Contains: return left.cqlContains(right);
        case BinaryOpNode::During:   return evalDuring(left, right);
        case BinaryOpNode::Overlaps: return evalOverlaps(left, right);
        case BinaryOpNode::Before:   return evalBefore(left, right);
        case BinaryOpNode::After:    return evalAfter(left, right);
        case BinaryOpNode::Between:  return evalBetween(left, right);
        default: return CQLValue::null();
    }
}

CQLValue CQLEvaluator::evalUnaryOp(const UnaryOpNode* n, CQLContext& ctx) {
    CQLValue val = evaluate(n->operand.get(), ctx);
    switch (n->op) {
        case UnaryOpNode::Not:    return val.cqlNot();
        case UnaryOpNode::Exists: return fnExists(val);
        case UnaryOpNode::IsNull: return CQLValue::fromBool(val.isNull());
        case UnaryOpNode::IsTrue: return CQLValue::fromBool(val.isTruthy());
        case UnaryOpNode::IsFalse:
            return CQLValue::fromBool(val.isBool() && !val.asBool());
        case UnaryOpNode::Negate:
            if (val.type() == CQLType::Integer)
                return CQLValue::fromInt(-val.asInt());
            if (val.type() == CQLType::Decimal)
                return CQLValue::fromDecimal(-val.asDecimal());
            return CQLValue::null();
        default: return CQLValue::null();
    }
}

CQLValue CQLEvaluator::evalRetrieve(const RetrieveNode* n, CQLContext& ctx) {
    return m_retriever.retrieve(n->dataType, n->valueSetRef, ctx, m_vsMgr);
}

CQLValue CQLEvaluator::evalQuery(const QueryNode* n, CQLContext& ctx) {
    // Evaluate the source (should be a list)
    CQLValue source = evaluate(n->source.get(), ctx);
    if (!source.isList()) return CQLValue::null();

    CQLList results;
    ctx.pushScope();

    for (const auto& item : source.asList()) {
        // Bind the alias
        if (!n->alias.empty())
            ctx.setVariable(n->alias, item);

        // Apply where clause
        if (n->where) {
            CQLValue cond = evaluate(n->where.get(), ctx);
            if (!cond.isTruthy()) continue;
        }

        // Apply return expression
        if (n->returnExpr) {
            results.push_back(evaluate(n->returnExpr.get(), ctx));
        } else {
            results.push_back(item);
        }
    }

    ctx.popScope();
    return CQLValue::fromList(results);
}

CQLValue CQLEvaluator::evalFunction(const FunctionCallNode* n, CQLContext& ctx) {
    if (n->name == "AgeInYearsAt" && !n->args.empty()) {
        CQLValue date = evaluate(n->args[0].get(), ctx);
        return fnAgeInYearsAt(ctx, date);
    }
    if (n->name == "AgeInMonthsAt" && !n->args.empty()) {
        CQLValue date = evaluate(n->args[0].get(), ctx);
        return fnAgeInMonthsAt(ctx, date);
    }
    if (n->name == "Count" && !n->args.empty()) {
        return fnCount(evaluate(n->args[0].get(), ctx));
    }
    if (n->name == "Exists" && !n->args.empty()) {
        return fnExists(evaluate(n->args[0].get(), ctx));
    }
    if (n->name == "First" && !n->args.empty()) {
        return fnFirst(evaluate(n->args[0].get(), ctx));
    }
    if (n->name == "Last" && !n->args.empty()) {
        return fnLast(evaluate(n->args[0].get(), ctx));
    }
    if (n->name == "ToDate" && !n->args.empty()) {
        return fnToDate(evaluate(n->args[0].get(), ctx));
    }
    if (n->name == "ToString" && !n->args.empty()) {
        return fnToString(evaluate(n->args[0].get(), ctx));
    }

    // DurationBetween(from, to, precision)
    if (n->name == "DurationBetween" && n->args.size() >= 2) {
        CQLValue from = evaluate(n->args[0].get(), ctx);
        CQLValue to   = evaluate(n->args[1].get(), ctx);
        std::string prec = "days";
        if (n->args.size() >= 3) {
            CQLValue p = evaluate(n->args[2].get(), ctx);
            if (p.type() == CQLType::String) prec = p.asString();
        }
        return fnDurationBetween(from, to, prec);
    }

    return CQLValue::null();
}

CQLValue CQLEvaluator::evalProperty(const PropertyNode* n, CQLContext& ctx) {
    CQLValue source = evaluate(n->source.get(), ctx);

    // Patient.property
    auto* idNode = dynamic_cast<const IdentifierNode*>(n->source.get());
    if (idNode && idNode->name == "Patient") {
        return ctx.patientProperty(n->property);
    }

    // Tuple field access
    if (source.isTuple()) {
        return source.field(n->property);
    }

    return CQLValue::null();
}

CQLValue CQLEvaluator::evalInterval(const IntervalNode* n, CQLContext& ctx) {
    CQLValue low  = evaluate(n->low.get(), ctx);
    CQLValue high = evaluate(n->high.get(), ctx);
    return CQLValue::fromInterval(low, high, n->lowClosed, n->highClosed);
}

CQLValue CQLEvaluator::evalDateTimeOp(const DateTimeOpNode* n, CQLContext& ctx) {
    CQLValue operand = evaluate(n->operand.get(), ctx);

    switch (n->op) {
        case DateTimeOpNode::StartOf:
            if (operand.type() == CQLType::Interval)
                return operand.intervalLow();
            return operand;

        case DateTimeOpNode::EndOf:
            if (operand.type() == CQLType::Interval)
                return operand.intervalHigh();
            return operand;

        case DateTimeOpNode::Year:
            if (operand.type() == CQLType::Date)
                return CQLValue::fromInt(operand.asDate().year);
            return CQLValue::null();

        case DateTimeOpNode::Month:
            if (operand.type() == CQLType::Date)
                return CQLValue::fromInt(operand.asDate().month);
            return CQLValue::null();

        case DateTimeOpNode::Day:
            if (operand.type() == CQLType::Date)
                return CQLValue::fromInt(operand.asDate().day);
            return CQLValue::null();

        case DateTimeOpNode::DurationBetween:
            // Requires two operands — handled via function call
            return CQLValue::null();

        case DateTimeOpNode::DateFrom:
        case DateTimeOpNode::Now:
        case DateTimeOpNode::Today:
        default:
            return operand;
    }
}

CQLValue CQLEvaluator::evalIf(const IfNode* n, CQLContext& ctx) {
    CQLValue cond = evaluate(n->condition.get(), ctx);
    if (cond.isTruthy())
        return evaluate(n->thenExpr.get(), ctx);
    if (n->elseExpr)
        return evaluate(n->elseExpr.get(), ctx);
    return CQLValue::null();
}

CQLValue CQLEvaluator::evalParameterRef(const ParameterRefNode* n, CQLContext& ctx) {
    return ctx.getParameter(n->name);
}

// ---------------------------------------------------------------------------
// Built-in functions
// ---------------------------------------------------------------------------

CQLValue CQLEvaluator::fnExists(const CQLValue& arg) {
    if (arg.isNull()) return CQLValue::fromBool(false);
    if (arg.isList()) return CQLValue::fromBool(!arg.asList().empty());
    return CQLValue::fromBool(true);
}

CQLValue CQLEvaluator::fnCount(const CQLValue& arg) {
    if (!arg.isList()) return CQLValue::fromInt(0);
    return CQLValue::fromInt(static_cast<int64_t>(arg.asList().size()));
}

CQLValue CQLEvaluator::fnFirst(const CQLValue& arg) {
    if (!arg.isList() || arg.asList().empty()) return CQLValue::null();
    return arg.asList().front();
}

CQLValue CQLEvaluator::fnLast(const CQLValue& arg) {
    if (!arg.isList() || arg.asList().empty()) return CQLValue::null();
    return arg.asList().back();
}

CQLValue CQLEvaluator::fnAgeInYearsAt(CQLContext& ctx, const CQLValue& date) {
    if (date.isNull() || date.type() != CQLType::Date)
        return CQLValue::null();

    CQLDate birthDate = ctx.patient().birthDate.toCQLDate();
    if (birthDate.isNull()) return CQLValue::null();

    CQLDate asOf = date.asDate();
    int age = asOf.year - birthDate.year;
    if (asOf.month < birthDate.month ||
        (asOf.month == birthDate.month && asOf.day < birthDate.day)) {
        --age;
    }
    return CQLValue::fromInt(age);
}

CQLValue CQLEvaluator::fnAgeInMonthsAt(CQLContext& ctx, const CQLValue& date) {
    if (date.isNull() || date.type() != CQLType::Date)
        return CQLValue::null();

    CQLDate birthDate = ctx.patient().birthDate.toCQLDate();
    if (birthDate.isNull()) return CQLValue::null();

    CQLDate asOf = date.asDate();
    int months = (asOf.year - birthDate.year) * 12 + (asOf.month - birthDate.month);
    if (asOf.day < birthDate.day) --months;
    return CQLValue::fromInt(months);
}

CQLValue CQLEvaluator::fnToDate(const CQLValue& val) {
    if (val.type() == CQLType::Date) return val;
    if (val.type() == CQLType::String) {
        // Parse "YYYY-MM-DD"
        const std::string& s = val.asString();
        CQLDate d;
        if (s.size() >= 10) {
            d.year  = std::stoi(s.substr(0, 4));
            d.month = std::stoi(s.substr(5, 2));
            d.day   = std::stoi(s.substr(8, 2));
            return CQLValue::fromDate(d);
        }
    }
    return CQLValue::null();
}

CQLValue CQLEvaluator::fnToString(const CQLValue& val) {
    return CQLValue::fromString(val.toString());
}

CQLValue CQLEvaluator::fnDurationBetween(const CQLValue& from, const CQLValue& to,
                                           const std::string& precision) {
    if (from.isNull() || to.isNull()) return CQLValue::null();
    if (from.type() != CQLType::Date || to.type() != CQLType::Date)
        return CQLValue::null();

    CQLDate df = from.asDate();
    CQLDate dt = to.asDate();

    if (precision == "years" || precision == "year") {
        int years = dt.year - df.year;
        if (dt.month < df.month || (dt.month == df.month && dt.day < df.day))
            --years;
        return CQLValue::fromInt(years);
    }
    if (precision == "months" || precision == "month") {
        int months = (dt.year - df.year) * 12 + (dt.month - df.month);
        if (dt.day < df.day) --months;
        return CQLValue::fromInt(months);
    }
    if (precision == "days" || precision == "day") {
        return CQLValue::fromInt(CQLDate::daysBetween(df, dt));
    }
    return CQLValue::null();
}

// ---------------------------------------------------------------------------
// Temporal operators
// ---------------------------------------------------------------------------

CQLValue CQLEvaluator::evalDuring(const CQLValue& point, const CQLValue& interval) {
    if (point.isNull() || interval.isNull()) return CQLValue::null();

    // Point during Interval
    if (interval.type() == CQLType::Interval) {
        if (point.type() == CQLType::Date) {
            return CQLValue::fromBool(interval.inInterval(point));
        }
        // Interval during Interval (fully contained)
        if (point.type() == CQLType::Interval) {
            bool lowIn  = interval.inInterval(point.intervalLow());
            bool highIn = interval.inInterval(point.intervalHigh());
            return CQLValue::fromBool(lowIn && highIn);
        }
    }
    return CQLValue::null();
}

CQLValue CQLEvaluator::evalOverlaps(const CQLValue& iv1, const CQLValue& iv2) {
    if (iv1.isNull() || iv2.isNull()) return CQLValue::null();
    if (iv1.type() != CQLType::Interval || iv2.type() != CQLType::Interval)
        return CQLValue::null();

    // Two intervals overlap if one contains any boundary of the other
    bool a = iv1.inInterval(iv2.intervalLow()) || iv1.inInterval(iv2.intervalHigh());
    bool b = iv2.inInterval(iv1.intervalLow()) || iv2.inInterval(iv1.intervalHigh());
    return CQLValue::fromBool(a || b);
}

CQLValue CQLEvaluator::evalBefore(const CQLValue& left, const CQLValue& right) {
    if (left.isNull() || right.isNull()) return CQLValue::null();

    CQLValue leftDate = left;
    CQLValue rightDate = right;

    // Extract dates from intervals
    if (left.type() == CQLType::Interval) leftDate = left.intervalHigh();
    if (right.type() == CQLType::Interval) rightDate = right.intervalLow();

    return leftDate.cqlLess(rightDate);
}

CQLValue CQLEvaluator::evalAfter(const CQLValue& left, const CQLValue& right) {
    return evalBefore(right, left);
}

CQLValue CQLEvaluator::evalBetween(const CQLValue& val, const CQLValue& interval) {
    if (val.isNull() || interval.isNull()) return CQLValue::null();
    if (interval.type() == CQLType::Interval) {
        return CQLValue::fromBool(interval.inInterval(val));
    }
    return CQLValue::null();
}

}  // namespace hedis
