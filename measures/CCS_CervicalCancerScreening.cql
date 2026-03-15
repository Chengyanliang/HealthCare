library CCS version '2026'
using HEDIS version '2026'

parameter "Measurement Period" Interval<Date>

valueset "Cervical Cytology": 'hedis:2.16.840.1.113883.3.464.1004.1208'
valueset "HPV Tests": 'hedis:2.16.840.1.113883.3.464.1004.1209'
valueset "Hysterectomy": 'hedis:2.16.840.1.113883.3.464.1004.1210'
valueset "Cervical Cancer": 'hedis:2.16.840.1.113883.3.464.1004.1211'

context Patient

define "Initial Population":
  AgeInYearsAt(end of "Measurement Period") between 24 and 64
    and Patient.gender = 'female'

define "Denominator":
  "Initial Population"
    and exists([Coverage] C
               where C.type = 'medical'
                 and C.period overlaps "Measurement Period")

define "Numerator":
  exists([Procedure: "Cervical Cytology"] P
    where P.date during Interval[
      start of "Measurement Period" - 3 years,
      end of "Measurement Period"])
  or (
    AgeInYearsAt(end of "Measurement Period") >= 30
    and exists([Procedure: "Cervical Cytology"] C
      where C.date during Interval[
        start of "Measurement Period" - 5 years,
        end of "Measurement Period"])
    and exists([Observation: "HPV Tests"] H
      where H.date during Interval[C.date - 4 days, C.date + 4 days])
  )

define "Denominator Exclusion":
  exists([Condition: "Cervical Cancer"])
  or exists([Procedure: "Hysterectomy"] H
    where H.date before end of "Measurement Period")
