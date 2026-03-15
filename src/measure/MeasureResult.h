#pragma once
// MeasureResult — Per-patient, per-measure evaluation result

#include "cql/CQLContext.h"  // for HADate
#include <string>

namespace hedis {

struct MeasureResult {
    std::string measureId;
    std::string patientId;

    bool initialPopulation    = false;
    bool denominator          = false;
    bool numerator            = false;
    bool denominatorExclusion = false;
    bool denominatorException = false;

    HADate failureDate;           // When compliance expires
    std::string failureReason;
    double numericResult = 0.0;   // For continuous-variable measures (e.g., HbA1c)

    // Computed compliance flag
    bool isCompliant() const {
        return denominator && !denominatorExclusion && numerator;
    }

    // Is the patient in the eligible population?
    bool isEligible() const {
        return denominator && !denominatorExclusion;
    }
};

}  // namespace hedis
