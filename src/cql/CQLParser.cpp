// CQLParser — ANTLR4-based CQL parser implementation
//
// This wraps the ANTLR4 C++ runtime.  The generated lexer/parser files
// (CQLLexer.cpp, CQLParserGenerated.cpp) are produced offline by running
//   antlr4 -Dlanguage=Cpp CQLLexer.g4 CQLParser.g4
// and placed alongside this file.  The build system compiles them.

#include "cql/CQLParser.h"
#include <fstream>
#include <sstream>

// ANTLR4 headers — included when the generated runtime is available
// #include "antlr4-runtime.h"
// #include "CQLLexerGenerated.h"
// #include "CQLParserGenerated.h"

namespace hedis {

CQLMeasureParser::CQLMeasureParser()  = default;
CQLMeasureParser::~CQLMeasureParser() = default;

// ---------------------------------------------------------------------------
// parseFile — read CQL from disk, then parse
// ---------------------------------------------------------------------------
std::unique_ptr<LibraryNode> CQLMeasureParser::parseFile(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        m_errors.push_back({0, 0, "Cannot open file: " + filepath});
        return nullptr;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return parse(oss.str());
}

// ---------------------------------------------------------------------------
// validate — parse and return errors only
// ---------------------------------------------------------------------------
std::vector<CQLParseError> CQLMeasureParser::validate(const std::string& cqlText) {
    parse(cqlText);
    return m_errors;
}

// ---------------------------------------------------------------------------
// parse — main entry: CQL text → LibraryNode AST
//
// The implementation below is a hand-written recursive-descent parser that
// handles the HEDIS-relevant CQL subset.  When the full ANTLR4 generated
// parser is available it replaces this with a visitor walk.
// ---------------------------------------------------------------------------

namespace {

enum class TokenKind {
    Eof, Identifier, StringLit, QuotedIdentifier, IntLit, DecimalLit,
    // Keywords
    Library, Using, Include, Called, Parameter, ValueSet,
    Define, Context, Where, Return, Distinct,
    And, Or, Not, Exists, In, During, Overlaps, Before, After, Between,
    True, False, Null,
    If, Then, Else, Case, When, End,
    // Date keywords
    Start, Of, Interval, Date, DateTime,
    Year, Years, Month, Months, Day, Days,
    // Punctuation
    LParen, RParen, LBracket, RBracket, LBrace, RBrace,
    Colon, Comma, Dot, DotDot,
    Eq, Neq, Lt, Gt, Le, Ge,
    Plus, Minus, Star, Slash,
    Arrow,  // ->
    // Special
    Unknown,
};

struct Token {
    TokenKind kind = TokenKind::Eof;
    std::string text;
    int line = 0;
    int col  = 0;
};

class Lexer {
public:
    explicit Lexer(const std::string& src) : m_src(src) {}

    Token next() {
        skipWhitespaceAndComments();
        if (m_pos >= m_src.size()) return {TokenKind::Eof, "", m_line, m_col};

        int startLine = m_line;
        int startCol  = m_col;
        char c = m_src[m_pos];

        // String literal
        if (c == '\'' || c == '"') {
            return lexString(startLine, startCol);
        }

        // Number
        if (isdigit(c)) {
            return lexNumber(startLine, startCol);
        }

        // Identifier / keyword
        if (isalpha(c) || c == '_') {
            return lexIdentifier(startLine, startCol);
        }

        // Two-character operators
        if (m_pos + 1 < m_src.size()) {
            std::string two = m_src.substr(m_pos, 2);
            if (two == "!=") { advance(2); return {TokenKind::Neq, "!=", startLine, startCol}; }
            if (two == "<=") { advance(2); return {TokenKind::Le, "<=", startLine, startCol}; }
            if (two == ">=") { advance(2); return {TokenKind::Ge, ">=", startLine, startCol}; }
            if (two == "->") { advance(2); return {TokenKind::Arrow, "->", startLine, startCol}; }
            if (two == "..") { advance(2); return {TokenKind::DotDot, "..", startLine, startCol}; }
        }

        // Single-character operators
        advance(1);
        switch (c) {
            case '(': return {TokenKind::LParen,   "(", startLine, startCol};
            case ')': return {TokenKind::RParen,   ")", startLine, startCol};
            case '[': return {TokenKind::LBracket, "[", startLine, startCol};
            case ']': return {TokenKind::RBracket, "]", startLine, startCol};
            case '{': return {TokenKind::LBrace,   "{", startLine, startCol};
            case '}': return {TokenKind::RBrace,   "}", startLine, startCol};
            case ':': return {TokenKind::Colon,    ":", startLine, startCol};
            case ',': return {TokenKind::Comma,    ",", startLine, startCol};
            case '.': return {TokenKind::Dot,      ".", startLine, startCol};
            case '=': return {TokenKind::Eq,       "=", startLine, startCol};
            case '<': return {TokenKind::Lt,       "<", startLine, startCol};
            case '>': return {TokenKind::Gt,       ">", startLine, startCol};
            case '+': return {TokenKind::Plus,     "+", startLine, startCol};
            case '-': return {TokenKind::Minus,    "-", startLine, startCol};
            case '*': return {TokenKind::Star,     "*", startLine, startCol};
            case '/': return {TokenKind::Slash,    "/", startLine, startCol};
            default:  return {TokenKind::Unknown,  std::string(1,c), startLine, startCol};
        }
    }

private:
    const std::string& m_src;
    size_t m_pos  = 0;
    int    m_line = 1;
    int    m_col  = 1;

    void advance(int n) {
        for (int i = 0; i < n && m_pos < m_src.size(); ++i) {
            if (m_src[m_pos] == '\n') { ++m_line; m_col = 1; }
            else { ++m_col; }
            ++m_pos;
        }
    }

    void skipWhitespaceAndComments() {
        while (m_pos < m_src.size()) {
            char c = m_src[m_pos];
            if (isspace(c)) { advance(1); continue; }
            // Line comment
            if (c == '/' && m_pos + 1 < m_src.size() && m_src[m_pos+1] == '/') {
                while (m_pos < m_src.size() && m_src[m_pos] != '\n') advance(1);
                continue;
            }
            // Block comment
            if (c == '/' && m_pos + 1 < m_src.size() && m_src[m_pos+1] == '*') {
                advance(2);
                while (m_pos + 1 < m_src.size()) {
                    if (m_src[m_pos] == '*' && m_src[m_pos+1] == '/') { advance(2); break; }
                    advance(1);
                }
                continue;
            }
            break;
        }
    }

    Token lexString(int sl, int sc) {
        char quote = m_src[m_pos];
        advance(1);
        std::string val;
        while (m_pos < m_src.size() && m_src[m_pos] != quote) {
            if (m_src[m_pos] == '\\' && m_pos + 1 < m_src.size()) {
                advance(1);
                val += m_src[m_pos];
                advance(1);
            } else {
                val += m_src[m_pos];
                advance(1);
            }
        }
        if (m_pos < m_src.size()) advance(1);  // closing quote
        // CQL: single-quoted = string literal, double-quoted = identifier reference
        TokenKind kind = (quote == '"') ? TokenKind::QuotedIdentifier : TokenKind::StringLit;
        return {kind, val, sl, sc};
    }

    Token lexNumber(int sl, int sc) {
        std::string val;
        bool hasDot = false;
        while (m_pos < m_src.size() && (isdigit(m_src[m_pos]) || m_src[m_pos] == '.')) {
            if (m_src[m_pos] == '.') {
                if (hasDot) break;
                if (m_pos + 1 < m_src.size() && m_src[m_pos+1] == '.') break;  // ".." range
                hasDot = true;
            }
            val += m_src[m_pos];
            advance(1);
        }
        return {hasDot ? TokenKind::DecimalLit : TokenKind::IntLit, val, sl, sc};
    }

    Token lexIdentifier(int sl, int sc) {
        std::string val;
        while (m_pos < m_src.size() && (isalnum(m_src[m_pos]) || m_src[m_pos] == '_')) {
            val += m_src[m_pos];
            advance(1);
        }
        // Keyword lookup
        static const std::map<std::string, TokenKind> kw = {
            {"library",   TokenKind::Library},   {"using",     TokenKind::Using},
            {"include",   TokenKind::Include},   {"called",    TokenKind::Called},
            {"parameter", TokenKind::Parameter}, {"valueset",  TokenKind::ValueSet},
            {"define",    TokenKind::Define},     {"context",   TokenKind::Context},
            {"where",     TokenKind::Where},      {"return",    TokenKind::Return},
            {"distinct",  TokenKind::Distinct},
            {"and",       TokenKind::And},        {"or",        TokenKind::Or},
            {"not",       TokenKind::Not},        {"exists",    TokenKind::Exists},
            {"in",        TokenKind::In},         {"during",    TokenKind::During},
            {"overlaps",  TokenKind::Overlaps},   {"before",    TokenKind::Before},
            {"after",     TokenKind::After},      {"between",   TokenKind::Between},
            {"true",      TokenKind::True},       {"false",     TokenKind::False},
            {"null",      TokenKind::Null},
            {"if",        TokenKind::If},         {"then",      TokenKind::Then},
            {"else",      TokenKind::Else},       {"case",      TokenKind::Case},
            {"when",      TokenKind::When},       {"end",       TokenKind::End},
            {"start",     TokenKind::Start},      {"of",        TokenKind::Of},
            {"Interval",  TokenKind::Interval},   {"Date",      TokenKind::Date},
            {"DateTime",  TokenKind::DateTime},
            {"year",      TokenKind::Year},       {"years",     TokenKind::Years},
            {"month",     TokenKind::Month},      {"months",    TokenKind::Months},
            {"day",       TokenKind::Day},        {"days",      TokenKind::Days},
        };
        auto it = kw.find(val);
        if (it != kw.end()) return {it->second, val, sl, sc};
        return {TokenKind::Identifier, val, sl, sc};
    }
};

// ---------------------------------------------------------------------------
// Recursive descent parser
// ---------------------------------------------------------------------------
class RDParser {
public:
    explicit RDParser(const std::string& src, std::vector<CQLParseError>& errors)
        : m_lexer(src), m_errors(errors) {
        advance();
    }

    std::unique_ptr<LibraryNode> parseLibrary() {
        auto lib = std::make_unique<LibraryNode>();

        // library <name> version '<ver>'
        if (match(TokenKind::Library)) {
            lib->name = expect(TokenKind::Identifier).text;
            if (match(TokenKind::Identifier)) {  // "version"
                lib->version = expect(TokenKind::StringLit).text;
            }
        }

        // using / include / parameter / valueset / define
        while (m_cur.kind != TokenKind::Eof) {
            if (m_cur.kind == TokenKind::Using) {
                lib->usings.push_back(parseUsing());
            } else if (m_cur.kind == TokenKind::Include) {
                lib->includes.push_back(parseInclude());
            } else if (m_cur.kind == TokenKind::Parameter) {
                lib->parameters.push_back(parseParameter());
            } else if (m_cur.kind == TokenKind::ValueSet) {
                lib->valueSets.push_back(parseValueSetDef());
            } else if (m_cur.kind == TokenKind::Define) {
                lib->definitions.push_back(parseDefine());
            } else if (m_cur.kind == TokenKind::Context) {
                advance();  // skip "context"
                advance();  // skip context name (e.g., "Patient")
            } else {
                error("Unexpected token: " + m_cur.text);
                advance();
            }
        }
        return lib;
    }

private:
    Lexer m_lexer;
    Token m_cur;
    std::vector<CQLParseError>& m_errors;

    void advance() { m_cur = m_lexer.next(); }

    bool match(TokenKind k) {
        if (m_cur.kind == k) { advance(); return true; }
        return false;
    }

    Token expect(TokenKind k) {
        Token t = m_cur;
        if (m_cur.kind != k) {
            error("Expected token kind, got: " + m_cur.text);
        } else {
            advance();
        }
        return t;
    }

    void error(const std::string& msg) {
        m_errors.push_back({m_cur.line, m_cur.col, msg});
    }

    // using <model> version '<v>'
    std::unique_ptr<UsingNode> parseUsing() {
        auto n = std::make_unique<UsingNode>();
        advance();  // "using"
        n->modelName = expect(TokenKind::Identifier).text;
        if (match(TokenKind::Identifier))  // "version"
            n->version = expect(TokenKind::StringLit).text;
        return n;
    }

    // include <lib> version '<v>' called <alias>
    std::unique_ptr<IncludeNode> parseInclude() {
        auto n = std::make_unique<IncludeNode>();
        advance();  // "include"
        n->libraryName = expect(TokenKind::Identifier).text;
        if (match(TokenKind::Identifier))  // "version"
            n->version = expect(TokenKind::StringLit).text;
        if (match(TokenKind::Called))
            n->alias = expect(TokenKind::Identifier).text;
        return n;
    }

    // parameter "<name>" Interval<Date>
    std::unique_ptr<ParameterDefNode> parseParameter() {
        auto n = std::make_unique<ParameterDefNode>();
        advance();  // "parameter"
        // name may be a double-quoted string or identifier
        if (m_cur.kind == TokenKind::QuotedIdentifier) {
            n->name = m_cur.text; advance();
        } else {
            n->name = expect(TokenKind::Identifier).text;
        }
        // type specifier — just collect tokens until next top-level keyword
        while (m_cur.kind != TokenKind::Eof &&
               m_cur.kind != TokenKind::Define &&
               m_cur.kind != TokenKind::Parameter &&
               m_cur.kind != TokenKind::ValueSet &&
               m_cur.kind != TokenKind::Context &&
               m_cur.kind != TokenKind::Using) {
            n->typeSpecifier += m_cur.text + " ";
            advance();
        }
        return n;
    }

    // valueset "<name>": '<oid>'
    std::unique_ptr<ValueSetDefNode> parseValueSetDef() {
        auto n = std::make_unique<ValueSetDefNode>();
        advance();  // "valueset"
        n->name = expect(TokenKind::QuotedIdentifier).text;
        expect(TokenKind::Colon);
        n->oid = expect(TokenKind::StringLit).text;
        return n;
    }

    // define "<name>": <expr>
    std::unique_ptr<DefineNode> parseDefine() {
        auto n = std::make_unique<DefineNode>();
        n->line = m_cur.line;
        n->col  = m_cur.col;
        advance();  // "define"
        n->accessLevel = "public";
        // name is double-quoted string
        if (m_cur.kind == TokenKind::QuotedIdentifier) {
            n->name = m_cur.text; advance();
        } else {
            n->name = expect(TokenKind::Identifier).text;
        }
        expect(TokenKind::Colon);
        n->expression = parseExpression();
        return n;
    }

    // ----- Expression parsing (precedence climbing) -----

    std::unique_ptr<CQLNode> parseExpression() {
        return parseOr();
    }

    std::unique_ptr<CQLNode> parseOr() {
        auto left = parseAnd();
        while (m_cur.kind == TokenKind::Or) {
            advance();
            auto node = std::make_unique<BinaryOpNode>();
            node->op = BinaryOpNode::Or;
            node->left = std::move(left);
            node->right = parseAnd();
            left = std::move(node);
        }
        return left;
    }

    std::unique_ptr<CQLNode> parseAnd() {
        auto left = parseComparison();
        while (m_cur.kind == TokenKind::And) {
            advance();
            auto node = std::make_unique<BinaryOpNode>();
            node->op = BinaryOpNode::And;
            node->left = std::move(left);
            node->right = parseComparison();
            left = std::move(node);
        }
        return left;
    }

    std::unique_ptr<CQLNode> parseComparison() {
        auto left = parseAddSub();

        // between X and Y
        if (m_cur.kind == TokenKind::Between) {
            advance();
            auto bnode = std::make_unique<BinaryOpNode>();
            bnode->op = BinaryOpNode::Between;
            bnode->left = std::move(left);
            auto low = parseAddSub();
            expect(TokenKind::And);
            auto high = parseAddSub();
            // Build interval [low, high]
            auto interval = std::make_unique<IntervalNode>();
            interval->low  = std::move(low);
            interval->high = std::move(high);
            bnode->right = std::move(interval);
            return bnode;
        }

        // Comparison operators
        while (true) {
            BinaryOpNode::Op op;
            if      (m_cur.kind == TokenKind::Eq)  { op = BinaryOpNode::Eq;  }
            else if (m_cur.kind == TokenKind::Neq)  { op = BinaryOpNode::Neq; }
            else if (m_cur.kind == TokenKind::Lt)   { op = BinaryOpNode::Lt;  }
            else if (m_cur.kind == TokenKind::Gt)   { op = BinaryOpNode::Gt;  }
            else if (m_cur.kind == TokenKind::Le)   { op = BinaryOpNode::Le;  }
            else if (m_cur.kind == TokenKind::Ge)   { op = BinaryOpNode::Ge;  }
            else if (m_cur.kind == TokenKind::In)   { op = BinaryOpNode::In;  }
            else if (m_cur.kind == TokenKind::During)   { op = BinaryOpNode::During;   }
            else if (m_cur.kind == TokenKind::Overlaps)  { op = BinaryOpNode::Overlaps; }
            else if (m_cur.kind == TokenKind::Before)    { op = BinaryOpNode::Before;   }
            else if (m_cur.kind == TokenKind::After)     { op = BinaryOpNode::After;    }
            else break;
            advance();
            auto node = std::make_unique<BinaryOpNode>();
            node->op = op;
            node->left = std::move(left);
            node->right = parseAddSub();
            left = std::move(node);
        }
        return left;
    }

    std::unique_ptr<CQLNode> parseAddSub() {
        auto left = parseMulDiv();
        while (m_cur.kind == TokenKind::Plus || m_cur.kind == TokenKind::Minus) {
            auto op = (m_cur.kind == TokenKind::Plus) ? BinaryOpNode::Add : BinaryOpNode::Sub;
            advance();
            auto node = std::make_unique<BinaryOpNode>();
            node->op = op;
            node->left = std::move(left);
            node->right = parseMulDiv();
            left = std::move(node);
        }
        return left;
    }

    std::unique_ptr<CQLNode> parseMulDiv() {
        auto left = parseUnary();
        while (m_cur.kind == TokenKind::Star || m_cur.kind == TokenKind::Slash) {
            auto op = (m_cur.kind == TokenKind::Star) ? BinaryOpNode::Mul : BinaryOpNode::Div;
            advance();
            auto node = std::make_unique<BinaryOpNode>();
            node->op = op;
            node->left = std::move(left);
            node->right = parseUnary();
            left = std::move(node);
        }
        return left;
    }

    std::unique_ptr<CQLNode> parseUnary() {
        if (m_cur.kind == TokenKind::Not) {
            advance();
            auto node = std::make_unique<UnaryOpNode>();
            node->op = UnaryOpNode::Not;
            node->operand = parseUnary();
            return node;
        }
        if (m_cur.kind == TokenKind::Exists) {
            advance();
            auto node = std::make_unique<UnaryOpNode>();
            node->op = UnaryOpNode::Exists;
            node->operand = parsePrimary();
            return node;
        }
        if (m_cur.kind == TokenKind::Minus) {
            advance();
            auto node = std::make_unique<UnaryOpNode>();
            node->op = UnaryOpNode::Negate;
            node->operand = parsePrimary();
            return node;
        }
        return parsePrimary();
    }

    std::unique_ptr<CQLNode> parsePrimary() {
        auto node = parseAtom();

        // Property access chain: expr.property
        while (m_cur.kind == TokenKind::Dot) {
            advance();
            auto prop = std::make_unique<PropertyNode>();
            prop->source = std::move(node);
            prop->property = expect(TokenKind::Identifier).text;
            node = std::move(prop);
        }
        return node;
    }

    std::unique_ptr<CQLNode> parseAtom() {
        int sl = m_cur.line, sc = m_cur.col;

        // Literals
        if (m_cur.kind == TokenKind::IntLit) {
            auto n = std::make_unique<LiteralNode>();
            n->value = CQLValue::fromInt(std::stoll(m_cur.text));
            n->line = sl; n->col = sc;
            advance();
            // Check for quantity: 3 years
            if (m_cur.kind == TokenKind::Years || m_cur.kind == TokenKind::Year ||
                m_cur.kind == TokenKind::Months || m_cur.kind == TokenKind::Month ||
                m_cur.kind == TokenKind::Days || m_cur.kind == TokenKind::Day) {
                auto q = std::make_unique<LiteralNode>();
                q->value = CQLValue::fromQuantity(
                    static_cast<double>(n->value.asInt()), m_cur.text);
                q->line = sl; q->col = sc;
                advance();
                return q;
            }
            return n;
        }
        if (m_cur.kind == TokenKind::DecimalLit) {
            auto n = std::make_unique<LiteralNode>();
            n->value = CQLValue::fromDecimal(std::stod(m_cur.text));
            n->line = sl; n->col = sc;
            advance();
            return n;
        }
        if (m_cur.kind == TokenKind::StringLit) {
            // Single-quoted string → CQL string literal
            auto n = std::make_unique<LiteralNode>();
            n->value = CQLValue::fromString(m_cur.text);
            n->line = sl; n->col = sc;
            advance();
            return n;
        }
        if (m_cur.kind == TokenKind::QuotedIdentifier) {
            // Double-quoted string → CQL identifier reference (define/parameter)
            auto n = std::make_unique<IdentifierNode>();
            n->name = m_cur.text;
            n->line = sl; n->col = sc;
            advance();
            return n;
        }
        if (m_cur.kind == TokenKind::True) {
            advance();
            auto n = std::make_unique<LiteralNode>();
            n->value = CQLValue::fromBool(true);
            return n;
        }
        if (m_cur.kind == TokenKind::False) {
            advance();
            auto n = std::make_unique<LiteralNode>();
            n->value = CQLValue::fromBool(false);
            return n;
        }
        if (m_cur.kind == TokenKind::Null) {
            advance();
            auto n = std::make_unique<LiteralNode>();
            n->value = CQLValue::null();
            return n;
        }

        // Interval[low, high] or Interval(low, high)
        if (m_cur.kind == TokenKind::Interval) {
            return parseIntervalExpr();
        }

        // Retrieve: [DataType: "ValueSet"]
        if (m_cur.kind == TokenKind::LBracket) {
            return parseRetrieveExpr();
        }

        // Parenthesized expression
        if (m_cur.kind == TokenKind::LParen) {
            advance();
            auto expr = parseExpression();
            expect(TokenKind::RParen);
            return expr;
        }

        // start of / end of — these are unary prefix operators with high
        // precedence so that "start of X - 3 years" parses as
        // "(start of X) - 3 years", not "start of (X - 3 years)"
        if (m_cur.kind == TokenKind::Start) {
            advance();
            expect(TokenKind::Of);
            auto n = std::make_unique<DateTimeOpNode>();
            n->op = DateTimeOpNode::StartOf;
            n->operand = parsePrimary();
            return n;
        }
        if (m_cur.kind == TokenKind::End) {
            // "end of" vs "end" (case-end)
            Token peek = m_cur;
            advance();
            if (m_cur.kind == TokenKind::Of) {
                advance();
                auto n = std::make_unique<DateTimeOpNode>();
                n->op = DateTimeOpNode::EndOf;
                n->operand = parsePrimary();
                return n;
            }
            // It was just the "end" keyword — push back somehow
            // For simplicity, treat as identifier
            auto n = std::make_unique<IdentifierNode>();
            n->name = "end";
            return n;
        }

        // Function call or identifier
        if (m_cur.kind == TokenKind::Identifier) {
            std::string name = m_cur.text;
            advance();

            // AgeInYearsAt(expr) style function call
            if (m_cur.kind == TokenKind::LParen) {
                advance();
                auto fn = std::make_unique<FunctionCallNode>();
                fn->name = name;
                fn->line = sl; fn->col = sc;
                if (m_cur.kind != TokenKind::RParen) {
                    fn->args.push_back(parseExpression());
                    while (match(TokenKind::Comma)) {
                        fn->args.push_back(parseExpression());
                    }
                }
                expect(TokenKind::RParen);
                return fn;
            }

            // Patient reference or define reference
            auto n = std::make_unique<IdentifierNode>();
            n->name = name;
            n->line = sl; n->col = sc;
            return n;
        }

        // Date literal
        if (m_cur.kind == TokenKind::Date) {
            advance();
            // Expect @YYYY-MM-DD style or just pass through
            auto n = std::make_unique<IdentifierNode>();
            n->name = "Date";
            return n;
        }

        error("Unexpected token in expression: " + m_cur.text);
        advance();
        return std::make_unique<LiteralNode>();  // null literal as fallback
    }

    std::unique_ptr<CQLNode> parseIntervalExpr() {
        advance();  // "Interval"
        auto n = std::make_unique<IntervalNode>();
        if (m_cur.kind == TokenKind::LBracket) {
            n->lowClosed = true; advance();
        } else if (m_cur.kind == TokenKind::LParen) {
            n->lowClosed = false; advance();
        }
        n->low = parseExpression();
        expect(TokenKind::Comma);
        n->high = parseExpression();
        if (m_cur.kind == TokenKind::RBracket) {
            n->highClosed = true; advance();
        } else if (m_cur.kind == TokenKind::RParen) {
            n->highClosed = false; advance();
        }
        return n;
    }

    std::unique_ptr<CQLNode> parseRetrieveExpr() {
        advance();  // "["
        auto n = std::make_unique<RetrieveNode>();
        n->dataType = expect(TokenKind::Identifier).text;
        if (match(TokenKind::Colon)) {
            // value set reference (double-quoted in CQL)
            if (m_cur.kind == TokenKind::QuotedIdentifier) {
                n->valueSetRef = m_cur.text;
                advance();
            }
        }
        expect(TokenKind::RBracket);

        // Optional alias and where clause
        if (m_cur.kind == TokenKind::Identifier && m_cur.text.size() == 1) {
            // Single-letter alias like "P" in "[Procedure: ...] P where ..."
            std::string alias = m_cur.text;
            advance();

            if (m_cur.kind == TokenKind::Where) {
                advance();
                auto query = std::make_unique<QueryNode>();
                query->alias = alias;
                query->source = std::move(n);
                query->where = parseExpression();
                return query;
            }
            // Just aliased retrieve, wrap in query
            auto query = std::make_unique<QueryNode>();
            query->alias = alias;
            query->source = std::move(n);
            return query;
        }

        return n;
    }
};

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::unique_ptr<LibraryNode> CQLMeasureParser::parse(const std::string& cqlText) {
    m_errors.clear();
    RDParser parser(cqlText, m_errors);
    return parser.parseLibrary();
}

}  // namespace hedis
