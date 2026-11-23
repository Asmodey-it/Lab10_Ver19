#pragma once
#include <memory>
#include <string>

struct BuildDurationMetrics {
    double actualDuration = 0.0;
    double expectedDuration = 0.0;
    double allowedDeviation = 0.0;
};

class IBuildDurationStrategy {
public:
    virtual ~IBuildDurationStrategy() = default;
    virtual std::string name() const = 0;
    virtual double evaluate(const BuildDurationMetrics& metrics) const = 0;
};

class AbsoluteDurationDeviationStrategy final : public IBuildDurationStrategy {
public:
    std::string name() const override;
    double evaluate(const BuildDurationMetrics& metrics) const override;
};

class RelativeDurationDeviationStrategy final : public IBuildDurationStrategy {
public:
    std::string name() const override;
    double evaluate(const BuildDurationMetrics& metrics) const override;
};

class NormalizedDurationDeviationStrategy final : public IBuildDurationStrategy {
public:
    std::string name() const override;
    double evaluate(const BuildDurationMetrics& metrics) const override;
};

class BuildDurationEvaluator {
public:
    explicit BuildDurationEvaluator(std::unique_ptr<IBuildDurationStrategy> strategy);

    void setStrategy(std::unique_ptr<IBuildDurationStrategy> strategy);
    std::string strategyName() const;
    double evaluate(const BuildDurationMetrics& metrics) const;

private:
    std::unique_ptr<IBuildDurationStrategy> strategy_;
};