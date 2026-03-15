// MeasureDefinition — Evaluate a CQL-defined HEDIS measure for one patient

#include "measure/MeasureDefinition.h"

namespace hedis {

// ---------------------------------------------------------------------------
// resolvePopulations — auto-discover population defines from the AST
// ---------------------------------------------------------------------------
void MeasureDefinition::resolvePopulations() {
    if (!ast) return;

    populations.clear();

    // Standard HEDIS population names mapped to expected CQL define names
    static const std::vector<std::pair<std::string, std::string>> standardPops = {
        {"Initial Population",     "Initial Population"},
        {"Denominator",            "Denominator"},
        {"Numerator",              "Numerator"},
        {"Denominator Exclusion",  "Denominator Exclusion"},
        {"Denominator Exception",  "Denominator Exception"},
        {"Numerator Exclusion",    "Numerator Exclusion"},
    };

    for (const auto& [popName, defineName] : standardPops) {
        for (const auto& def : ast->definitions) {
            if (def->name == defineName) {
                populations.push_back({popName, defineName});
                break;
            }
        }
    }

    // Collect value set references
    valueSetRefs.clear();
    for (const auto& vs : ast->valueSets) {
        valueSetRefs.push_back(vs->name);
    }
}

// ---------------------------------------------------------------------------
// evaluate — run all population defines for one patient
// ---------------------------------------------------------------------------
MeasureResult MeasureDefinition::evaluate(CQLEvaluator& evaluator,
                                           CQLContext& ctx) const {
    MeasureResult result;
    result.measureId = measureId;
    result.patientId = ctx.patient().patientId;

    if (!ast) return result;

    // Evaluate each population define
    for (const auto& pop : populations) {
        CQLValue val = evaluator.evaluateDefine(ast.get(), pop.defineRef, ctx);

        if (pop.name == "Initial Population")
            result.initialPopulation = val.isTruthy();
        else if (pop.name == "Denominator")
            result.denominator = val.isTruthy();
        else if (pop.name == "Numerator")
            result.numerator = val.isTruthy();
        else if (pop.name == "Denominator Exclusion")
            result.denominatorExclusion = val.isTruthy();
        else if (pop.name == "Denominator Exception")
            result.denominatorException = val.isTruthy();
    }

    // If not in initial population, clear everything
    if (!result.initialPopulation) {
        result.denominator = false;
        result.numerator = false;
        result.denominatorExclusion = false;
        result.denominatorException = false;
    }

    // If excluded from denominator, not in numerator
    if (result.denominatorExclusion) {
        result.numerator = false;
    }

    return result;
}

}  // namespace hedis
