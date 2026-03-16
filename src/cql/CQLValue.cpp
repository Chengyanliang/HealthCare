// CQLValue — implementation of CQL three-valued variant type

#include "cql/CQLValue.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace hedis {

// ===========================================================================
// CQLDate
// ===========================================================================

bool CQLDate::operator==(const CQLDate& o) const {
    return year == o.year && month == o.month && day == o.day;
}
bool CQLDate::operator!=(const CQLDate& o) const { return !(*this == o); }
bool CQLDate::operator<(const CQLDate& o) const {
    if (year != o.year)  return year  < o.year;
    if (month != o.month) return month < o.month;
    return day < o.day;
}
bool CQLDate::operator<=(const CQLDate& o) const { return !(o < *this); }
bool CQLDate::operator>(const CQLDate& o) const  { return o < *this; }
bool CQLDate::operator>=(const CQLDate& o) const { return !(*this < o); }

int CQLDate::toJulianDay() const {
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    return day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
}

CQLDate CQLDate::fromJulianDay(int jd) {
    int a = jd + 32044;
    int b = (4 * a + 3) / 146097;
    int c = a - (146097 * b) / 4;
    int d = (4 * c + 3) / 1461;
    int e = c - (1461 * d) / 4;
    int m = (5 * e + 2) / 153;
    CQLDate dt;
    dt.day   = e - (153 * m + 2) / 5 + 1;
    dt.month = m + 3 - 12 * (m / 10);
    dt.year  = 100 * b + d - 4800 + m / 10;
    return dt;
}

CQLDate CQLDate::addYears(int n) const {
    CQLDate r = *this;
    r.year += n;
    // Clamp Feb 29 → Feb 28 on non-leap years
    if (r.month == 2 && r.day == 29) {
        bool leap = (r.year % 4 == 0 && (r.year % 100 != 0 || r.year % 400 == 0));
        if (!leap) r.day = 28;
    }
    return r;
}

CQLDate CQLDate::addMonths(int n) const {
    CQLDate r = *this;
    int totalMonths = (r.year * 12 + r.month - 1) + n;
    r.year  = totalMonths / 12;
    r.month = totalMonths % 12 + 1;
    // Clamp day to valid range
    static const int daysInMonth[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int maxDay = daysInMonth[r.month];
    if (r.month == 2) {
        bool leap = (r.year % 4 == 0 && (r.year % 100 != 0 || r.year % 400 == 0));
        if (leap) maxDay = 29;
    }
    if (r.day > maxDay) r.day = maxDay;
    return r;
}

CQLDate CQLDate::addDays(int n) const {
    return fromJulianDay(toJulianDay() + n);
}

int CQLDate::daysBetween(const CQLDate& a, const CQLDate& b) {
    return b.toJulianDay() - a.toJulianDay();
}

// ===========================================================================
// CQLValue lifecycle
// ===========================================================================

CQLValue::CQLValue() : m_type(CQLType::Null) {}

CQLValue::~CQLValue() { freeInterval(); }

void CQLValue::freeInterval() {
    delete m_intervalLow;
    delete m_intervalHigh;
    m_intervalLow = nullptr;
    m_intervalHigh = nullptr;
}

void CQLValue::copyFrom(const CQLValue& o) {
    m_type    = o.m_type;
    m_bool    = o.m_bool;
    m_int     = o.m_int;
    m_decimal = o.m_decimal;
    m_string  = o.m_string;
    m_date    = o.m_date;
    m_code    = o.m_code;
    m_list    = o.m_list;
    m_tuple   = o.m_tuple;
    m_quantity = o.m_quantity;
    m_intervalLowClosed  = o.m_intervalLowClosed;
    m_intervalHighClosed = o.m_intervalHighClosed;
    if (o.m_intervalLow)
        m_intervalLow = new CQLValue(*o.m_intervalLow);
    if (o.m_intervalHigh)
        m_intervalHigh = new CQLValue(*o.m_intervalHigh);
}

CQLValue::CQLValue(const CQLValue& o) { copyFrom(o); }

CQLValue& CQLValue::operator=(const CQLValue& o) {
    if (this != &o) {
        freeInterval();
        m_list.clear();
        m_tuple.clear();
        copyFrom(o);
    }
    return *this;
}

CQLValue::CQLValue(CQLValue&& o) noexcept
    : m_type(o.m_type), m_bool(o.m_bool), m_int(o.m_int),
      m_decimal(o.m_decimal), m_string(std::move(o.m_string)),
      m_date(o.m_date), m_code(std::move(o.m_code)),
      m_list(std::move(o.m_list)), m_tuple(std::move(o.m_tuple)),
      m_quantity(std::move(o.m_quantity)),
      m_intervalLow(o.m_intervalLow), m_intervalHigh(o.m_intervalHigh),
      m_intervalLowClosed(o.m_intervalLowClosed),
      m_intervalHighClosed(o.m_intervalHighClosed)
{
    o.m_intervalLow = nullptr;
    o.m_intervalHigh = nullptr;
    o.m_type = CQLType::Null;
}

CQLValue& CQLValue::operator=(CQLValue&& o) noexcept {
    if (this != &o) {
        freeInterval();
        m_type    = o.m_type;
        m_bool    = o.m_bool;
        m_int     = o.m_int;
        m_decimal = o.m_decimal;
        m_string  = std::move(o.m_string);
        m_date    = o.m_date;
        m_code    = std::move(o.m_code);
        m_list    = std::move(o.m_list);
        m_tuple   = std::move(o.m_tuple);
        m_quantity = std::move(o.m_quantity);
        m_intervalLow  = o.m_intervalLow;
        m_intervalHigh = o.m_intervalHigh;
        m_intervalLowClosed  = o.m_intervalLowClosed;
        m_intervalHighClosed = o.m_intervalHighClosed;
        o.m_intervalLow = nullptr;
        o.m_intervalHigh = nullptr;
        o.m_type = CQLType::Null;
    }
    return *this;
}

// ===========================================================================
// Factories
// ===========================================================================

CQLValue CQLValue::null() { return CQLValue(); }

CQLValue CQLValue::fromBool(bool v) {
    CQLValue r; r.m_type = CQLType::Boolean; r.m_bool = v; return r;
}
CQLValue CQLValue::fromInt(int64_t v) {
    CQLValue r; r.m_type = CQLType::Integer; r.m_int = v; return r;
}
CQLValue CQLValue::fromDecimal(double v) {
    CQLValue r; r.m_type = CQLType::Decimal; r.m_decimal = v; return r;
}
CQLValue CQLValue::fromString(const std::string& v) {
    CQLValue r; r.m_type = CQLType::String; r.m_string = v; return r;
}
CQLValue CQLValue::fromDate(const CQLDate& v) {
    CQLValue r; r.m_type = CQLType::Date; r.m_date = v; return r;
}
CQLValue CQLValue::fromCode(const CQLCode& v) {
    CQLValue r; r.m_type = CQLType::Code; r.m_code = v; return r;
}
CQLValue CQLValue::fromList(const CQLList& v) {
    CQLValue r; r.m_type = CQLType::List; r.m_list = v; return r;
}
CQLValue CQLValue::fromTuple(const CQLTuple& v) {
    CQLValue r; r.m_type = CQLType::Tuple; r.m_tuple = v; return r;
}
CQLValue CQLValue::fromInterval(const CQLValue& low, const CQLValue& high,
                                 bool lowClosed, bool highClosed) {
    CQLValue r;
    r.m_type = CQLType::Interval;
    r.m_intervalLow  = new CQLValue(low);
    r.m_intervalHigh = new CQLValue(high);
    r.m_intervalLowClosed  = lowClosed;
    r.m_intervalHighClosed = highClosed;
    return r;
}
CQLValue CQLValue::fromQuantity(double value, const std::string& unit) {
    CQLValue r;
    r.m_type = CQLType::Quantity;
    r.m_quantity.value = value;
    r.m_quantity.unit  = unit;
    return r;
}

// ===========================================================================
// Interval accessors
// ===========================================================================
static CQLValue g_nullVal;

const CQLValue& CQLValue::intervalLow() const {
    return m_intervalLow ? *m_intervalLow : g_nullVal;
}
const CQLValue& CQLValue::intervalHigh() const {
    return m_intervalHigh ? *m_intervalHigh : g_nullVal;
}
bool CQLValue::intervalLowClosed()  const { return m_intervalLowClosed; }
bool CQLValue::intervalHighClosed() const { return m_intervalHighClosed; }

// ===========================================================================
// Three-valued logic
// ===========================================================================

CQLValue CQLValue::cqlAnd(const CQLValue& rhs) const {
    // CQL three-valued AND truth table
    if (isBool() && !asBool()) return fromBool(false);
    if (rhs.isBool() && !rhs.asBool()) return fromBool(false);
    if (isNull() || rhs.isNull()) return null();
    return fromBool(asBool() && rhs.asBool());
}

CQLValue CQLValue::cqlOr(const CQLValue& rhs) const {
    if (isBool() && asBool()) return fromBool(true);
    if (rhs.isBool() && rhs.asBool()) return fromBool(true);
    if (isNull() || rhs.isNull()) return null();
    return fromBool(asBool() || rhs.asBool());
}

CQLValue CQLValue::cqlNot() const {
    if (isNull()) return null();
    return fromBool(!asBool());
}

// ===========================================================================
// Comparison operators (null-propagating)
// ===========================================================================

CQLValue CQLValue::cqlEqual(const CQLValue& rhs) const {
    if (isNull() || rhs.isNull()) return null();
    if (m_type != rhs.m_type) return fromBool(false);
    switch (m_type) {
        case CQLType::Boolean: return fromBool(m_bool == rhs.m_bool);
        case CQLType::Integer: return fromBool(m_int == rhs.m_int);
        case CQLType::Decimal: return fromBool(std::abs(m_decimal - rhs.m_decimal) < 1e-10);
        case CQLType::String:  return fromBool(m_string == rhs.m_string);
        case CQLType::Date:    return fromBool(m_date == rhs.m_date);
        case CQLType::Code:    return fromBool(m_code.code == rhs.m_code.code &&
                                               m_code.system == rhs.m_code.system);
        default: return fromBool(false);
    }
}

CQLValue CQLValue::cqlNotEqual(const CQLValue& rhs) const {
    CQLValue eq = cqlEqual(rhs);
    return eq.cqlNot();
}

CQLValue CQLValue::cqlLess(const CQLValue& rhs) const {
    if (isNull() || rhs.isNull()) return null();
    if (m_type != rhs.m_type) return null();
    switch (m_type) {
        case CQLType::Integer: return fromBool(m_int < rhs.m_int);
        case CQLType::Decimal: return fromBool(m_decimal < rhs.m_decimal);
        case CQLType::String:  return fromBool(m_string < rhs.m_string);
        case CQLType::Date:    return fromBool(m_date < rhs.m_date);
        default: return null();
    }
}

CQLValue CQLValue::cqlGreater(const CQLValue& rhs) const {
    return rhs.cqlLess(*this);
}

CQLValue CQLValue::cqlLessOrEqual(const CQLValue& rhs) const {
    CQLValue gt = cqlGreater(rhs);
    return gt.cqlNot();
}

CQLValue CQLValue::cqlGreaterOrEqual(const CQLValue& rhs) const {
    CQLValue lt = cqlLess(rhs);
    return lt.cqlNot();
}

// ===========================================================================
// Arithmetic (null-propagating)
// ===========================================================================

CQLValue CQLValue::cqlAdd(const CQLValue& rhs) const {
    if (isNull() || rhs.isNull()) return null();
    if (m_type == CQLType::Integer && rhs.m_type == CQLType::Integer)
        return fromInt(m_int + rhs.m_int);
    if (m_type == CQLType::Decimal || rhs.m_type == CQLType::Decimal) {
        double l = (m_type == CQLType::Integer) ? static_cast<double>(m_int) : m_decimal;
        double r = (rhs.m_type == CQLType::Integer) ? static_cast<double>(rhs.m_int) : rhs.m_decimal;
        return fromDecimal(l + r);
    }
    // Date arithmetic: Date + Quantity(years/months/days)
    if (m_type == CQLType::Date && rhs.m_type == CQLType::Quantity) {
        if (rhs.m_quantity.unit == "years" || rhs.m_quantity.unit == "year")
            return fromDate(m_date.addYears(static_cast<int>(rhs.m_quantity.value)));
        if (rhs.m_quantity.unit == "months" || rhs.m_quantity.unit == "month")
            return fromDate(m_date.addMonths(static_cast<int>(rhs.m_quantity.value)));
        if (rhs.m_quantity.unit == "days" || rhs.m_quantity.unit == "day")
            return fromDate(m_date.addDays(static_cast<int>(rhs.m_quantity.value)));
    }
    return null();
}

CQLValue CQLValue::cqlSubtract(const CQLValue& rhs) const {
    if (isNull() || rhs.isNull()) return null();
    if (m_type == CQLType::Integer && rhs.m_type == CQLType::Integer)
        return fromInt(m_int - rhs.m_int);
    if (m_type == CQLType::Decimal || rhs.m_type == CQLType::Decimal) {
        double l = (m_type == CQLType::Integer) ? static_cast<double>(m_int) : m_decimal;
        double r = (rhs.m_type == CQLType::Integer) ? static_cast<double>(rhs.m_int) : rhs.m_decimal;
        return fromDecimal(l - r);
    }
    // Date arithmetic: Date - Quantity(years/months/days)
    if (m_type == CQLType::Date && rhs.m_type == CQLType::Quantity) {
        if (rhs.m_quantity.unit == "years" || rhs.m_quantity.unit == "year")
            return fromDate(m_date.addYears(-static_cast<int>(rhs.m_quantity.value)));
        if (rhs.m_quantity.unit == "months" || rhs.m_quantity.unit == "month")
            return fromDate(m_date.addMonths(-static_cast<int>(rhs.m_quantity.value)));
        if (rhs.m_quantity.unit == "days" || rhs.m_quantity.unit == "day")
            return fromDate(m_date.addDays(-static_cast<int>(rhs.m_quantity.value)));
    }
    return null();
}

CQLValue CQLValue::cqlMultiply(const CQLValue& rhs) const {
    if (isNull() || rhs.isNull()) return null();
    if (m_type == CQLType::Integer && rhs.m_type == CQLType::Integer)
        return fromInt(m_int * rhs.m_int);
    if (m_type == CQLType::Decimal || rhs.m_type == CQLType::Decimal) {
        double l = (m_type == CQLType::Integer) ? static_cast<double>(m_int) : m_decimal;
        double r = (rhs.m_type == CQLType::Integer) ? static_cast<double>(rhs.m_int) : rhs.m_decimal;
        return fromDecimal(l * r);
    }
    return null();
}

CQLValue CQLValue::cqlDivide(const CQLValue& rhs) const {
    if (isNull() || rhs.isNull()) return null();
    double l = (m_type == CQLType::Integer) ? static_cast<double>(m_int) : m_decimal;
    double r = (rhs.m_type == CQLType::Integer) ? static_cast<double>(rhs.m_int) : rhs.m_decimal;
    if (r == 0.0) return null();
    return fromDecimal(l / r);
}

// ===========================================================================
// List / Interval operations
// ===========================================================================

CQLValue CQLValue::cqlContains(const CQLValue& element) const {
    if (!isList()) return null();
    for (const auto& item : m_list) {
        CQLValue eq = item.cqlEqual(element);
        if (eq.isBool() && eq.asBool()) return fromBool(true);
    }
    return fromBool(false);
}

CQLValue CQLValue::cqlIn(const CQLValue& list) const {
    return list.cqlContains(*this);
}

bool CQLValue::inInterval(const CQLValue& point) const {
    if (m_type != CQLType::Interval || point.isNull()) return false;
    if (!m_intervalLow || !m_intervalHigh) return false;

    CQLValue lowCmp  = m_intervalLowClosed
                       ? point.cqlGreaterOrEqual(*m_intervalLow)
                       : point.cqlGreater(*m_intervalLow);
    CQLValue highCmp = m_intervalHighClosed
                       ? point.cqlLessOrEqual(*m_intervalHigh)
                       : point.cqlLess(*m_intervalHigh);

    return lowCmp.isTruthy() && highCmp.isTruthy();
}

CQLValue CQLValue::field(const std::string& name) const {
    if (!isTuple()) return null();
    auto it = m_tuple.find(name);
    if (it == m_tuple.end()) return null();
    return it->second;
}

bool CQLValue::isTruthy() const {
    if (m_type == CQLType::Boolean) return m_bool;
    return false;  // null and all non-boolean are falsy
}

// ===========================================================================
// Debug string
// ===========================================================================

std::string CQLValue::toString() const {
    switch (m_type) {
        case CQLType::Null:    return "null";
        case CQLType::Boolean: return m_bool ? "true" : "false";
        case CQLType::Integer: return std::to_string(m_int);
        case CQLType::Decimal: {
            std::ostringstream oss;
            oss << m_decimal;
            return oss.str();
        }
        case CQLType::String:  return "'" + m_string + "'";
        case CQLType::Date: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d", m_date.year, m_date.month, m_date.day);
            return buf;
        }
        case CQLType::Code:
            return m_code.system + ":" + m_code.code;
        case CQLType::List: {
            std::string s = "[";
            for (size_t i = 0; i < m_list.size(); ++i) {
                if (i > 0) s += ", ";
                s += m_list[i].toString();
            }
            return s + "]";
        }
        case CQLType::Interval:
            return (m_intervalLowClosed ? "[" : "(") +
                   (m_intervalLow ? m_intervalLow->toString() : "null") + ", " +
                   (m_intervalHigh ? m_intervalHigh->toString() : "null") +
                   (m_intervalHighClosed ? "]" : ")");
        case CQLType::Quantity: {
            std::ostringstream oss;
            oss << m_quantity.value << " " << m_quantity.unit;
            return oss.str();
        }
        default: return "<unknown>";
    }
}

}  // namespace hedis
