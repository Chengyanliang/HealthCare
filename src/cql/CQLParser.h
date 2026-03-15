#pragma once
// CQLParser — Parses CQL text into AST using ANTLR4 C++ runtime

#include "cql/CQLAst.h"
#include <memory>
#include <string>
#include <vector>

namespace hedis {

// Syntax/parse error collected during parsing
struct CQLParseError {
    int line;
    int col;
    std::string message;
};

class CQLMeasureParser {
public:
    CQLMeasureParser();
    ~CQLMeasureParser();

    // Parse CQL text → AST
    std::unique_ptr<LibraryNode> parse(const std::string& cqlText);

    // Parse from file
    std::unique_ptr<LibraryNode> parseFile(const std::string& filepath);

    // Validate syntax only (returns errors; empty = valid)
    std::vector<CQLParseError> validate(const std::string& cqlText);

    // Get errors from last parse
    const std::vector<CQLParseError>& errors() const { return m_errors; }

private:
    std::vector<CQLParseError> m_errors;

    // Internal: build AST from ANTLR4 parse tree
    std::unique_ptr<LibraryNode> buildAst(void* parseTreeCtx);

    // Expression builders (map ANTLR4 contexts → AST nodes)
    std::unique_ptr<CQLNode> buildExpression(void* ctx);
    std::unique_ptr<CQLNode> buildRetrieve(void* ctx);
    std::unique_ptr<CQLNode> buildQuery(void* ctx);
    std::unique_ptr<CQLNode> buildLiteral(void* ctx);
    std::unique_ptr<CQLNode> buildBinaryOp(void* ctx);
    std::unique_ptr<CQLNode> buildUnaryOp(void* ctx);
    std::unique_ptr<CQLNode> buildFunctionCall(void* ctx);
    std::unique_ptr<CQLNode> buildInterval(void* ctx);
    std::unique_ptr<CQLNode> buildPropertyAccess(void* ctx);
};

}  // namespace hedis
