#pragma once
// MeasureDefinition — Container for a single HEDIS CQL measure

#include "cql/CQLAst.h"
#include "cql/CQLContext.h"
#include "cql/CQLEvaluator.h"
#include "measure/MeasureResult.h"
#include <memory>
#include <string>
#include <vector>

namespace hedis {

struct MeasurePopulation {
    std::string name;        // "Initial Population", "Denominator", "Numerator", etc.
    std::string defineRef;   // CQL define name to evaluate
};

class MeasureDefinition {
public:
    std::string measureId;   // e.g., "CCS", "BCS", "COL"
    std::string version;     // e.g., "HEDIS-MY2026"
    std::string cqlText;     // Full CQL source

    std::vector<MeasurePopulation> populations;
    std::vector<std::string> valueSetRefs;  // Required value sets

    // Pre-parsed AST (parsed once, evaluated many times)
    std::shared_ptr<LibraryNode> ast;

    // Populate standard populations from CQL defines
    void resolvePopulations();

    // Evaluate this measure for one patient
    MeasureResult evaluate(CQLEvaluator& evaluator, CQLContext& ctx) const;
};

}  // namespace hedis
