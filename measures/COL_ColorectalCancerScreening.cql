library COL version '2026'
using HEDIS version '2026'

parameter "Measurement Period" Interval<Date>

valueset "Colonoscopy": 'hedis:2.16.840.1.113883.3.464.1004.1064'
valueset "Flexible Sigmoidoscopy": 'hedis:2.16.840.1.113883.3.464.1004.1102'
valueset "FOBT": 'hedis:2.16.840.1.113883.3.464.1004.1093'
valueset "FIT-DNA": 'hedis:2.16.840.1.113883.3.464.1004.1420'
valueset "CT Colonography": 'hedis:2.16.840.1.113883.3.464.1004.1421'
valueset "Colorectal Cancer": 'hedis:2.16.840.1.113883.3.464.1004.1065'
valueset "Total Colectomy": 'hedis:2.16.840.1.113883.3.464.1004.1316'

context Patient

define "Initial Population":
  AgeInYearsAt(end of "Measurement Period") between 46 and 75

define "Denominator":
  "Initial Population"
    and exists([Coverage] C
               where C.type = 'medical'
                 and C.period overlaps "Measurement Period")

define "Numerator":
  exists([Procedure: "Colonoscopy"] P
    where P.date during Interval[
      start of "Measurement Period" - 10 years,
      end of "Measurement Period"])
  or exists([Procedure: "Flexible Sigmoidoscopy"] S
    where S.date during Interval[
      start of "Measurement Period" - 5 years,
      end of "Measurement Period"])
  or exists([Observation: "FOBT"] F
    where F.date during "Measurement Period")
  or exists([Observation: "FIT-DNA"] D
    where D.date during Interval[
      start of "Measurement Period" - 3 years,
      end of "Measurement Period"])
  or exists([Procedure: "CT Colonography"] C
    where C.date during Interval[
      start of "Measurement Period" - 5 years,
      end of "Measurement Period"])

define "Denominator Exclusion":
  exists([Condition: "Colorectal Cancer"])
  or exists([Procedure: "Total Colectomy"] T
    where T.date before end of "Measurement Period")
