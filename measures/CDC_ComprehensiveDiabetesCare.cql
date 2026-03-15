library CDC version '2026'
using HEDIS version '2026'

parameter "Measurement Period" Interval<Date>

valueset "Diabetes": 'hedis:2.16.840.1.113883.3.464.1004.1077'
valueset "HbA1c Lab Test": 'hedis:2.16.840.1.113883.3.464.1004.1116'
valueset "Eye Exam": 'hedis:2.16.840.1.113883.3.464.1004.1088'
valueset "Diabetic Retinal Screening": 'hedis:2.16.840.1.113883.3.464.1004.1078'
valueset "Urine Protein Tests": 'hedis:2.16.840.1.113883.3.464.1004.1333'
valueset "ACE Inhibitors": 'hedis:2.16.840.1.113883.3.464.1004.1001'
valueset "ARB Medications": 'hedis:2.16.840.1.113883.3.464.1004.1007'

context Patient

define "Initial Population":
  AgeInYearsAt(end of "Measurement Period") between 18 and 75
    and exists([Condition: "Diabetes"] D
      where D.date during Interval[
        start of "Measurement Period" - 2 years,
        end of "Measurement Period"])

define "Denominator":
  "Initial Population"
    and exists([Coverage] C
               where C.type = 'medical'
                 and C.period overlaps "Measurement Period")

// HbA1c control < 8.0%
define "Most Recent HbA1c":
  Last([Observation: "HbA1c Lab Test"] H
    where H.date during "Measurement Period"
    and H.status = 'final')

define "Numerator":
  "Most Recent HbA1c".numericValue < 8.0

// HbA1c poor control > 9.0%
define "HbA1c Poor Control":
  "Most Recent HbA1c".numericValue > 9.0
  or not exists([Observation: "HbA1c Lab Test"] H
    where H.date during "Measurement Period")

// Eye exam sub-measure
define "Eye Exam Performed":
  exists([Procedure: "Eye Exam"] E
    where E.date during Interval[
      start of "Measurement Period" - 2 years,
      end of "Measurement Period"])
  or exists([Procedure: "Diabetic Retinal Screening"] R
    where R.date during "Measurement Period")

// Nephropathy screening sub-measure
define "Nephropathy Screening":
  exists([Observation: "Urine Protein Tests"] U
    where U.date during "Measurement Period")
  or exists([MedicationOrder: "ACE Inhibitors"] A
    where A.date during "Measurement Period")
  or exists([MedicationOrder: "ARB Medications"] B
    where B.date during "Measurement Period")
