#pragma once
// CQLValue — Variant type for CQL three-valued logic
// Wraps all CQL data: bool, int, decimal, date, string, list, tuple, interval

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace hedis {

enum class CQLType {
    Null,
    Boolean,
    Integer,
    Decimal,
    String,
    DateTime,
    Date,
    Time,
    Quantity,
    Code,
    Concept,
    List,
    Tuple,
    Interval,
};

// Forward declaration
class CQLValue;

using CQLList   = std::vector<CQLValue>;
using CQLTuple  = std::map<std::string, CQLValue>;

struct CQLCode {
    std::string code;
    std::string system;   // "ICD10", "CPT", "LOINC", "NDC", "SNOMED"
    std::string display;
};

// Simple date representation (no timezone for HEDIS)
struct CQLDate {
    int year  = 0;
    int month = 0;
    int day   = 0;

    bool operator==(const CQLDate& o) const;
    bool operator!=(const CQLDate& o) const;
    bool operator<(const CQLDate& o)  const;
    bool operator<=(const CQLDate& o) const;
    bool operator>(const CQLDate& o)  const;
    bool operator>=(const CQLDate& o) const;

    int toJulianDay() const;
    static CQLDate fromJulianDay(int jd);
    CQLDate addYears(int n) const;
    CQLDate addMonths(int n) const;
    CQLDate addDays(int n) const;
    static int daysBetween(const CQLDate& a, const CQLDate& b);
    bool isNull() const { return year == 0; }
};

struct CQLInterval {
    CQLValue* low  = nullptr;   // heap-allocated; owned by the enclosing CQLValue
    CQLValue* high = nullptr;
    bool lowClosed  = true;
    bool highClosed = true;
};

struct CQLQuantity {
    double value = 0.0;
    std::string unit;
};

// ---------------------------------------------------------------------------
// CQLValue
// ---------------------------------------------------------------------------
class CQLValue {
public:
    // Construct null
    CQLValue();
    ~CQLValue();

    CQLValue(const CQLValue& o);
    CQLValue& operator=(const CQLValue& o);
    CQLValue(CQLValue&& o) noexcept;
    CQLValue& operator=(CQLValue&& o) noexcept;

    // Typed constructors
    static CQLValue null();
    static CQLValue fromBool(bool v);
    static CQLValue fromInt(int64_t v);
    static CQLValue fromDecimal(double v);
    static CQLValue fromString(const std::string& v);
    static CQLValue fromDate(const CQLDate& v);
    static CQLValue fromCode(const CQLCode& v);
    static CQLValue fromList(const CQLList& v);
    static CQLValue fromTuple(const CQLTuple& v);
    static CQLValue fromInterval(const CQLValue& low, const CQLValue& high,
                                  bool lowClosed = true, bool highClosed = true);
    static CQLValue fromQuantity(double value, const std::string& unit);

    // Type query
    CQLType type() const { return m_type; }
    bool isNull() const  { return m_type == CQLType::Null; }
    bool isBool() const  { return m_type == CQLType::Boolean; }
    bool isList() const  { return m_type == CQLType::List; }
    bool isTuple() const { return m_type == CQLType::Tuple; }

    // Accessors (undefined behaviour if wrong type)
    bool           asBool()    const { return m_bool; }
    int64_t        asInt()     const { return m_int; }
    double         asDecimal() const { return m_decimal; }
    const std::string& asString() const { return m_string; }
    const CQLDate& asDate()    const { return m_date; }
    const CQLCode& asCode()    const { return m_code; }
    const CQLList& asList()    const { return m_list; }
    CQLList&       asList()          { return m_list; }
    const CQLTuple& asTuple()  const { return m_tuple; }
    CQLTuple&       asTuple()        { return m_tuple; }
    const CQLQuantity& asQuantity() const { return m_quantity; }

    // Interval accessors
    const CQLValue& intervalLow()  const;
    const CQLValue& intervalHigh() const;
    bool intervalLowClosed()  const;
    bool intervalHighClosed() const;

    // CQL three-valued logic operators
    CQLValue cqlAnd(const CQLValue& rhs) const;
    CQLValue cqlOr(const CQLValue& rhs)  const;
    CQLValue cqlNot() const;

    // Comparison (null-propagating)
    CQLValue cqlEqual(const CQLValue& rhs) const;
    CQLValue cqlNotEqual(const CQLValue& rhs) const;
    CQLValue cqlLess(const CQLValue& rhs) const;
    CQLValue cqlGreater(const CQLValue& rhs) const;
    CQLValue cqlLessOrEqual(const CQLValue& rhs) const;
    CQLValue cqlGreaterOrEqual(const CQLValue& rhs) const;

    // Arithmetic (null-propagating)
    CQLValue cqlAdd(const CQLValue& rhs) const;
    CQLValue cqlSubtract(const CQLValue& rhs) const;
    CQLValue cqlMultiply(const CQLValue& rhs) const;
    CQLValue cqlDivide(const CQLValue& rhs) const;

    // List operations
    CQLValue cqlContains(const CQLValue& element) const;
    CQLValue cqlIn(const CQLValue& list) const;

    // Interval membership
    bool inInterval(const CQLValue& point) const;

    // Tuple field access
    CQLValue field(const std::string& name) const;

    // String representation for debugging
    std::string toString() const;

    // Truthiness for conditionals (null → false)
    bool isTruthy() const;

private:
    CQLType     m_type = CQLType::Null;
    bool        m_bool = false;
    int64_t     m_int  = 0;
    double      m_decimal = 0.0;
    std::string m_string;
    CQLDate     m_date;
    CQLCode     m_code;
    CQLList     m_list;
    CQLTuple    m_tuple;
    CQLQuantity m_quantity;

    // Interval members (heap-allocated to break recursion)
    CQLValue*   m_intervalLow    = nullptr;
    CQLValue*   m_intervalHigh   = nullptr;
    bool        m_intervalLowClosed  = true;
    bool        m_intervalHighClosed = true;

    void freeInterval();
    void copyFrom(const CQLValue& o);
};

}  // namespace hedis
