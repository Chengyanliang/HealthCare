#pragma once
// CQLAst.h — AST node types for parsed CQL expressions

#include "cql/CQLValue.h"
#include <memory>
#include <string>
#include <vector>

namespace hedis {

// ---------------------------------------------------------------------------
// Base AST node
// ---------------------------------------------------------------------------
struct CQLNode {
    virtual ~CQLNode() = default;
    int line = 0;
    int col  = 0;
};

using CQLNodePtr = std::unique_ptr<CQLNode>;

// ---------------------------------------------------------------------------
// Expression nodes
// ---------------------------------------------------------------------------

struct LiteralNode : CQLNode {
    CQLValue value;
};

struct IdentifierNode : CQLNode {
    std::string name;
};

struct BinaryOpNode : CQLNode {
    enum Op {
        And, Or,
        Eq, Neq, Lt, Gt, Le, Ge,
        Add, Sub, Mul, Div,
        In, Contains,
        During, Overlaps, Before, After,
        Between,
    };
    Op op;
    std::unique_ptr<CQLNode> left;
    std::unique_ptr<CQLNode> right;
};

struct UnaryOpNode : CQLNode {
    enum Op { Not, Exists, IsNull, IsTrue, IsFalse, Negate };
    Op op;
    std::unique_ptr<CQLNode> operand;
};

struct RetrieveNode : CQLNode {
    std::string dataType;      // "Encounter", "Condition", "Procedure", etc.
    std::string valueSetRef;   // Value set name or OID
    std::string codeProperty;  // Optional: which property to match
};

struct WhereNode : CQLNode {
    std::unique_ptr<CQLNode> source;
    std::unique_ptr<CQLNode> condition;
};

struct QueryNode : CQLNode {
    std::string alias;
    std::unique_ptr<CQLNode> source;
    std::unique_ptr<CQLNode> where;
    std::unique_ptr<CQLNode> returnExpr;
    bool returnDistinct = false;
};

struct FunctionCallNode : CQLNode {
    std::string name;
    std::vector<std::unique_ptr<CQLNode>> args;
};

struct IntervalNode : CQLNode {
    std::unique_ptr<CQLNode> low;
    std::unique_ptr<CQLNode> high;
    bool lowClosed  = true;
    bool highClosed = true;
};

struct PropertyNode : CQLNode {
    std::unique_ptr<CQLNode> source;
    std::string property;
};

struct DateTimeOpNode : CQLNode {
    enum Op {
        DurationBetween,
        DateFrom,
        Year, Month, Day,
        StartOf, EndOf,
        Now, Today,
    };
    Op op;
    std::unique_ptr<CQLNode> operand;
    std::string precision;  // "years", "months", "days"
};

struct IfNode : CQLNode {
    std::unique_ptr<CQLNode> condition;
    std::unique_ptr<CQLNode> thenExpr;
    std::unique_ptr<CQLNode> elseExpr;
};

struct CaseNode : CQLNode {
    struct WhenClause {
        std::unique_ptr<CQLNode> condition;
        std::unique_ptr<CQLNode> result;
    };
    std::unique_ptr<CQLNode> comparand;  // optional
    std::vector<WhenClause> whens;
    std::unique_ptr<CQLNode> elseExpr;
};

struct CoalesceNode : CQLNode {
    std::vector<std::unique_ptr<CQLNode>> operands;
};

struct ParameterRefNode : CQLNode {
    std::string name;
};

struct ValueSetRefNode : CQLNode {
    std::string name;
};

// ---------------------------------------------------------------------------
// Top-level definitions
// ---------------------------------------------------------------------------

struct UsingNode : CQLNode {
    std::string modelName;
    std::string version;
};

struct IncludeNode : CQLNode {
    std::string libraryName;
    std::string version;
    std::string alias;
};

struct ParameterDefNode : CQLNode {
    std::string name;
    std::string typeSpecifier;
    std::unique_ptr<CQLNode> defaultExpr;
};

struct ValueSetDefNode : CQLNode {
    std::string name;
    std::string oid;
};

struct DefineNode : CQLNode {
    std::string name;
    std::string accessLevel;  // "public" or "private"
    std::unique_ptr<CQLNode> expression;
};

struct LibraryNode : CQLNode {
    std::string name;
    std::string version;

    std::vector<std::unique_ptr<UsingNode>>        usings;
    std::vector<std::unique_ptr<IncludeNode>>      includes;
    std::vector<std::unique_ptr<ParameterDefNode>> parameters;
    std::vector<std::unique_ptr<ValueSetDefNode>>  valueSets;
    std::vector<std::unique_ptr<DefineNode>>       definitions;
};

}  // namespace hedis
