library CBP version '2026'
using HEDIS version '2026'

parameter "Measurement Period" Interval<Date>

valueset "Essential Hypertension": 'hedis:2.16.840.1.113883.3.464.1004.1122'
valueset "Systolic Blood Pressure": 'hedis:2.16.840.1.113883.3.464.1004.1290'
valueset "Diastolic Blood Pressure": 'hedis:2.16.840.1.113883.3.464.1004.1070'
valueset "ESRD": 'hedis:2.16.840.1.113883.3.464.1004.1083'
valueset "Kidney Transplant": 'hedis:2.16.840.1.113883.3.464.1004.1163'
valueset "Pregnancy": 'hedis:2.16.840.1.113883.3.464.1004.1219'
valueset "Nonacute Inpatient": 'hedis:2.16.840.1.113883.3.464.1004.1189'

context Patient

define "Initial Population":
  AgeInYearsAt(end of "Measurement Period") between 18 and 85
    and exists([Condition: "Essential Hypertension"] H
      where H.date during Interval[
        start of "Measurement Period" - 1 years,
        start of "Measurement Period" + 6 months])

define "Denominator":
  "Initial Population"
    and exists([Coverage] C
               where C.type = 'medical'
                 and C.period overlaps "Measurement Period")

define "Most Recent Systolic":
  Last([Observation: "Systolic Blood Pressure"] S
    where S.date during "Measurement Period"
    and S.status = 'final')

define "Most Recent Diastolic":
  Last([Observation: "Diastolic Blood Pressure"] D
    where D.date during "Measurement Period"
    and D.status = 'final')

define "Numerator":
  "Most Recent Systolic".numericValue < 140.0
    and "Most Recent Diastolic".numericValue < 90.0

define "Denominator Exclusion":
  exists([Condition: "ESRD"])
  or exists([Condition: "Kidney Transplant"])
  or exists([Condition: "Pregnancy"] P
    where P.date during "Measurement Period")
  or exists([Encounter: "Nonacute Inpatient"] N
    where N.date during "Measurement Period")
