#include "BuildDurationStrategy.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

    void validateMetrics(const BuildDurationMetrics& metrics) {
        if (metrics.actualDuration < 0.0 ||
            metrics.expectedDuration < 0.0 ||
            metrics.allowedDeviation < 0.0) {
            throw std::invalid_argument("Build duration parameters must be non-negative.");
        }
    }

} 

std::string AbsoluteDurationDeviationStrategy::name() const {
    return "Absolute Deviation";
}

double AbsoluteDurationDeviationStrategy::evaluate(const BuildDurationMetrics& metrics) const {
    validateMetrics(metrics);
    return std::abs(metrics.actualDuration - metrics.expectedDuration);
}

std::string RelativeDurationDeviationStrategy::name() const {
    return "Relative Deviation";
}

double RelativeDurationDeviationStrategy::evaluate(const BuildDurationMetrics& metrics) const {
    validateMetrics(metrics);
    if (metrics.expectedDuration <= 0.0) {
        throw std::invalid_argument("Expected duration must be positive for relative deviation.");
    }
    return std::abs(metrics.actualDuration - metrics.expectedDuration) / metrics.expectedDuration;
}

std::string NormalizedDurationDeviationStrategy::name() const {
    return "Normalized Deviation";
}

double NormalizedDurationDeviationStrategy::evaluate(const BuildDurationMetrics& metrics) const {
    validateMetrics(metrics);
    if (metrics.allowedDeviation <= 0.0) {
        throw std::invalid_argument("Allowed deviation must be positive for normalized deviation.");
    }
    return std::abs(metrics.actualDuration - metrics.expectedDuration) / metrics.allowedDeviation;
}

BuildDurationEvaluator::BuildDurationEvaluator(std::unique_ptr<IBuildDurationStrategy> strategy)
    : strategy_(std::move(strategy)) {
    if (!strategy_) {
        throw std::invalid_argument("Duration strategy cannot be null.");
    }
}

void BuildDurationEvaluator::setStrategy(std::unique_ptr<IBuildDurationStrategy> strategy) {
    if (!strategy) {
        throw std::invalid_argument("Duration strategy cannot be null.");
    }
    strategy_ = std::move(strategy);
}

std::string BuildDurationEvaluator::strategyName() const {
    return strategy_->name();
}

double BuildDurationEvaluator::evaluate(const BuildDurationMetrics& metrics) const {
    return strategy_->evaluate(metrics);
}