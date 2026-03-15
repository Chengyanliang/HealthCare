library BCS version '2026'
using HEDIS version '2026'

parameter "Measurement Period" Interval<Date>

valueset "Mammography": 'hedis:2.16.840.1.113883.3.464.1004.1168'
valueset "Bilateral Mastectomy": 'hedis:2.16.840.1.113883.3.464.1004.1042'
valueset "Unilateral Mastectomy Left": 'hedis:2.16.840.1.113883.3.464.1004.1334'
valueset "Unilateral Mastectomy Right": 'hedis:2.16.840.1.113883.3.464.1004.1335'
valueset "History of Bilateral Mastectomy": 'hedis:2.16.840.1.113883.3.464.1004.1140'

context Patient

define "Initial Population":
  AgeInYearsAt(end of "Measurement Period") between 52 and 74
    and Patient.gender = 'female'

define "Denominator":
  "Initial Population"
    and exists([Coverage] C
               where C.type = 'medical'
                 and C.period overlaps "Measurement Period")

define "Numerator":
  exists([Procedure: "Mammography"] M
    where M.date during Interval[
      start of "Measurement Period" - 2 years,
      end of "Measurement Period"])

define "Denominator Exclusion":
  exists([Procedure: "Bilateral Mastectomy"] B
    where B.date before end of "Measurement Period")
  or exists([Condition: "History of Bilateral Mastectomy"])
  or (
    exists([Procedure: "Unilateral Mastectomy Left"] L
      where L.date before end of "Measurement Period")
    and exists([Procedure: "Unilateral Mastectomy Right"] R
      where R.date before end of "Measurement Period")
  )
