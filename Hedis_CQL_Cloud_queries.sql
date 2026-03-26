 -- mocked meta data and patients
select * from SMA_MEMBER_MASTER;
select * from SMA_CQL_MEASURES;
select * from SMA_CQL_VALUESETS;
select * from SMA_DIAGNOSIS;
select * from SMA_LAB_RESULTS;
select * from SMA_PHARMACY;
select * from SMA_PROCEDURE;


SELECT * FROM SMA_CQL_RESULTS WHERE JOB_ID = 'E2E_TEST' ORDER BY PATIENT_ID, MEASURE_ID;     

SELECT PATIENT_ID, MEASURE_ID, INITIAL_POP, DENOMINATOR, NUMERATOR, DENOM_EXCLUSION, JOB_ID                                                                                    
  FROM SMA_CQL_RESULTS                                                                                                                                                           
  WHERE JOB_ID = 'E2E_TEST'
  ORDER BY PATIENT_ID, MEASURE_ID;    
  
  