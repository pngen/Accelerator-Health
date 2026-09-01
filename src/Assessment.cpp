// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/Assessment.hpp"
#include "accelerator-health/Freshness.hpp"
#include "accelerator-health/Util.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace ah {

namespace {
// Returns s as a JSON string literal (quoted, with control chars escaped).
std::string to_json_str(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('"');
  for (char ch : s) {
    switch (ch) {
      case '"': out += "\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out.push_back(ch); break;
    }
  }
  out.push_back('"');
  return out;
}

std::string number(double v) {
  if (!std::isfinite(v)) return "0";
  std::ostringstream os;
  os << std::setprecision(4) << v;
  return os.str();
}
}  // namespace

HealthAssessment AssessmentEngine::assess(const AssessmentInput& in) const {
  HealthAssessment a;
  a.accelerator = in.accelerator;
  a.uuid = in.uuid;
  a.validationDepth = in.highestValidationDepth;
  a.timestamp = in.now;

  FreshnessEvaluator feval(in.policy.freshness);
  const bool haveEvidence = in.lastObservation.nanos != 0;
  const FreshnessStatus freshness = haveEvidence ? feval.evaluate(in.lastObservation, in.now)
                                                 : FreshnessStatus::EXPIRED;

  bool anyFailed = false;
  bool anyDegraded = false;
  bool anyFatalDim = false;
  bool haveNominal = false;
  std::string worstReason;
  Severity worstSeverity = Severity::INFO;
  for (const auto& [kind, dim] : in.dimensions) {
    (void)kind;
    if (dim.state == DimensionState::FAILED) {
      anyFailed = true;
      if (dim.severity == Severity::FATAL) anyFatalDim = true;
      if (static_cast<int>(dim.severity) > static_cast<int>(worstSeverity)) {
        worstSeverity = dim.severity;
        worstReason = dim.reason;
      }
    } else if (dim.state == DimensionState::DEGRADED) {
      anyDegraded = true;
      if (static_cast<int>(dim.severity) > static_cast<int>(worstSeverity)) {
        worstSeverity = dim.severity;
        worstReason = dim.reason;
      }
    } else if (dim.state == DimensionState::NOMINAL) {
      haveNominal = true;
    }
  }
  (void)haveNominal;

  const int consecutive = static_cast<int>(in.faultStats.consecutiveLatest);
  const bool hasOpenFatal = in.faultStats.hasOpenFatal;
  const bool hasOpenCritical = in.faultStats.hasOpenCritical;

  HealthState target = in.currentState;
  if (in.inRecovery) {
    target = (in.currentState == HealthState::RECOVERING || in.currentState == HealthState::REVALIDATING ||
              in.currentState == HealthState::DRAINING || in.currentState == HealthState::DRAINED)
                 ? in.currentState
                 : HealthState::RECOVERING;
  } else if (in.quarantined) {
    target = HealthState::QUARANTINED;
  } else if (hasOpenFatal || anyFatalDim) {
    target = HealthState::FAILED;
  } else if (anyFailed) {
    target = HealthState::UNHEALTHY;
  } else if (hasOpenCritical && consecutive >= static_cast<int>(in.policy.quarantineAfterFailures)) {
    target = HealthState::QUARANTINED;
  } else if (consecutive >= static_cast<int>(in.policy.unhealthyAfterFailures)) {
    target = HealthState::UNHEALTHY;
  } else if (consecutive >= static_cast<int>(in.policy.degradeAfterFailures)) {
    target = HealthState::DEGRADED;
  } else if (freshness == FreshnessStatus::EXPIRED || freshness == FreshnessStatus::STALE) {
    if (in.currentState == HealthState::HEALTHY) target = HealthState::DEGRADED;
  } else {
    target = HealthState::HEALTHY;
  }

  a.state = target;

  if (in.inRecovery) a.reasons.push_back("device in recovery/revalidation");
  if (in.quarantined) a.reasons.push_back("quarantine active");
  if (hasOpenFatal) a.reasons.push_back("open fatal fault present");
  if (anyFailed) {
    a.reasons.push_back("dimension failure: " + (worstReason.empty() ? "unknown" : worstReason));
  } else if (anyDegraded) {
    a.reasons.push_back("dimension degraded: " + (worstReason.empty() ? "unknown" : worstReason));
  }
  if (consecutive >= static_cast<int>(in.policy.degradeAfterFailures))
    a.reasons.push_back("repeated failures: " + std::to_string(consecutive));
  if (freshness == FreshnessStatus::STALE) a.reasons.push_back("evidence stale");
  if (freshness == FreshnessStatus::EXPIRED) a.reasons.push_back("evidence expired");
  if (static_cast<int>(in.highestValidationDepth) < static_cast<int>(in.policy.requiredValidationDepth))
    a.reasons.push_back("validation depth below required");
  if (in.deviceGenerationChanged) a.reasons.push_back("device generation changed");
  if (in.policyGenerationChanged) a.reasons.push_back("policy generation changed");
  if (a.reasons.empty() && target == HealthState::HEALTHY) a.reasons.push_back("all evidence nominal and fresh");
  if (a.reasons.empty()) a.reasons.push_back("no adverse evidence");

  float conf = 1.0f;
  if (in.inRecovery) conf *= 0.4f;
  if (in.quarantined) conf *= 0.1f;
  if (freshness == FreshnessStatus::AGING) conf *= 0.9f;
  if (freshness == FreshnessStatus::STALE) conf *= 0.6f;
  if (freshness == FreshnessStatus::EXPIRED) conf *= 0.3f;
  switch (target) {
    case HealthState::DEGRADED: conf *= 0.8f; break;
    case HealthState::UNHEALTHY: conf *= 0.5f; break;
    case HealthState::FAILED: conf *= 0.2f; break;
    case HealthState::QUARANTINED: conf *= 0.1f; break;
    case HealthState::RECOVERING:
    case HealthState::REVALIDATING: conf *= 0.5f; break;
    default: break;
  }
  if (static_cast<int>(in.highestValidationDepth) < static_cast<int>(in.policy.requiredValidationDepth)) conf *= 0.8f;
  if (hasOpenCritical) conf *= 0.5f;
  conf = std::clamp(conf, 0.0f, 1.0f);
  a.confidence = conf;

  if (in.quarantined) a.action = ActionRequired::RECOVER;
  else if (target == HealthState::FAILED) a.action = ActionRequired::DRAIN;
  else if (target == HealthState::UNHEALTHY) a.action = ActionRequired::DRAIN;
  else if (target == HealthState::QUARANTINED) a.action = ActionRequired::QUARANTINE;
  else if (in.inRecovery || static_cast<int>(in.highestValidationDepth) < static_cast<int>(in.policy.requiredValidationDepth))
    a.action = ActionRequired::REVALIDATE;
  else if (target == HealthState::DEGRADED) a.action = ActionRequired::MONITOR;
  else a.action = ActionRequired::NONE;

  ReadinessInput ri;
  ri.health = target;
  ri.freshness = freshness;
  ri.validationDepth = in.highestValidationDepth;
  ri.requiredValidationDepth = in.policy.requiredValidationDepth;
  ri.quarantined = in.quarantined;
  ri.inRecovery = in.inRecovery;
  ri.deviceGenerationChanged = in.deviceGenerationChanged;
  ri.policyGenerationChanged = in.policyGenerationChanged;
  ri.allowDegradedReadiness = in.policy.allowDegradedReadiness;
  ReadinessResult rr = ReadinessEvaluator{}.evaluate(ri);
  a.readiness = rr.state;
  a.executionReady = rr.executionReady;

  wire::Writer w;
  in.accelerator.write(w);
  in.uuid.write(w);
  in.deviceGeneration.write(w);
  write(w, target);
  write(w, a.readiness);
  write(w, freshness);
  write(w, in.highestValidationDepth);
  write(w, in.policy.requiredValidationDepth);
  w.u32(in.faultStats.total);
  w.u32(in.faultStats.criticalFatalOpen);
  w.u32(in.faultStats.consecutiveLatest);
  w.u8(in.faultStats.hasOpenFatal ? 1 : 0);
  w.u8(in.faultStats.hasOpenCritical ? 1 : 0);
  // Deliberately exclude absolute clock timestamps so the digest is a stable,
  // reproducible summary of the semantic state rather than a point-in-time value.
  w.u32(in.policy.degradeAfterFailures);
  w.u32(in.policy.unhealthyAfterFailures);
  w.u32(in.policy.quarantineAfterFailures);
  a.digest = "ah-" + util::to_hex(w.data().data(), w.data().size());

  a.quarantined = in.quarantined;
  a.inRecovery = in.inRecovery;
  return a;
}

Diagnosis AssessmentEngine::explain(const HealthAssessment& a) const {
  Diagnosis d;
  std::ostringstream text;
  text << "accelerator=" << a.accelerator.string() << "\n"
       << "health=" << to_string(a.state) << "\n"
       << "readiness=" << to_string(a.readiness) << "\n"
       << "validationDepth=" << to_string(a.validationDepth) << "\n"
       << "confidence=" << number(a.confidence) << "\n"
       << "action=" << to_string(a.action) << "\n"
       << "reasons:\n";
  for (const auto& r : a.reasons) text << "  - " << r << "\n";

  std::ostringstream json;
  auto addKey = [&json](const char* k) {
    json << "\"" << k << "\":";
  };
  json << "{";
  addKey("accelerator"); json << to_json_str(a.accelerator.string()) << ",";
  addKey("uuid"); json << to_json_str(a.uuid.string()) << ",";
  addKey("health"); json << to_json_str(to_string(a.state)) << ",";
  addKey("readiness"); json << to_json_str(to_string(a.readiness)) << ",";
  addKey("validationDepth"); json << to_json_str(to_string(a.validationDepth)) << ",";
  addKey("confidence"); json << number(a.confidence) << ",";
  addKey("action"); json << to_json_str(to_string(a.action)) << ",";
  addKey("executionReady"); json << (a.executionReady ? "true" : "false") << ",";
  addKey("quarantined"); json << (a.quarantined ? "true" : "false") << ",";
  addKey("reasons"); json << "[";
  for (std::size_t i = 0; i < a.reasons.size(); ++i) {
    if (i) json << ",";
    json << to_json_str(a.reasons[i]);
  }
  json << "]";
  addKey("digest"); json << to_json_str(a.digest);
  json << "}";

  d.text = text.str();
  d.json = json.str();
  d.digest = a.digest;
  return d;
}

}  // namespace ah
