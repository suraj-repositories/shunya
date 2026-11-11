#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

#include "shunya/core/domain/oscilloscope.hpp"

namespace shunya::gui {

class ShunyaOscilloscopeController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString triggerEdge READ triggerEdge WRITE setTriggerEdge NOTIFY triggerChanged)
    Q_PROPERTY(double triggerLevel READ triggerLevel WRITE setTriggerLevel NOTIFY triggerChanged)
    Q_PROPERTY(bool triggered READ triggered NOTIFY waveformChanged)
    Q_PROPERTY(double triggerTime READ triggerTime NOTIFY waveformChanged)
    Q_PROPERTY(double ch1Vpp READ ch1Vpp NOTIFY waveformChanged)
    Q_PROPERTY(double ch1Vrms READ ch1Vrms NOTIFY waveformChanged)
    Q_PROPERTY(double ch1FreqHz READ ch1FreqHz NOTIFY waveformChanged)
    Q_PROPERTY(double ch2Vpp READ ch2Vpp NOTIFY waveformChanged)
    Q_PROPERTY(double ch2FreqHz READ ch2FreqHz NOTIFY waveformChanged)
    Q_PROPERTY(double timeSpanMs READ timeSpanMs WRITE setTimeSpanMs NOTIFY waveformChanged)

public:
    explicit ShunyaOscilloscopeController(QObject* parent = nullptr);
    ~ShunyaOscilloscopeController() override = default;

    QString triggerEdge() const;
    Q_INVOKABLE void setTriggerEdge(const QString& edge);

    double triggerLevel() const { return trigger_cfg_.level; }
    Q_INVOKABLE void setTriggerLevel(double lvl);

    bool triggered() const { return last_trig_.triggered; }
    double triggerTime() const { return last_trig_.trigger_time; }

    double ch1Vpp() const { return ch1_metrics_.v_pp; }
    double ch1Vrms() const { return ch1_metrics_.v_rms; }
    double ch1FreqHz() const { return ch1_metrics_.frequency_hz; }
    double ch2Vpp() const { return ch2_metrics_.v_pp; }
    double ch2FreqHz() const { return ch2_metrics_.frequency_hz; }

    double timeSpanMs() const { return time_span_s_ * 1000.0; }
    Q_INVOKABLE void setTimeSpanMs(double ms);

    Q_INVOKABLE void loadDemoCapture(const QString& mode = "mixed_signal");
    void loadLiveChannels(const std::vector<double>& timeS,
                          const std::vector<double>& ch1Values,
                          const std::vector<double>& ch2Values);
    Q_INVOKABLE QVariantList getAnalogTraceNormalized(int channelIdx) const;
    Q_INVOKABLE QVariantList getLogicChannels() const;
    Q_INVOKABLE QVariantList getDecodedBusWords() const;

signals:
    void triggerChanged();
    void waveformChanged();

private:
    void recompute_trigger_and_metrics();

    std::vector<core::domain::AnalogChannel> analog_channels_;
    std::vector<core::domain::LogicSample> logic_samples_;
    std::vector<core::domain::DecodedBusWord> decoded_words_;

    core::domain::TriggerConfig trigger_cfg_;
    core::domain::TriggerResult last_trig_;
    core::domain::SignalMetrics ch1_metrics_;
    core::domain::SignalMetrics ch2_metrics_;

    double time_span_s_{0.010}; // 10ms window
    double v_range_half_{5.0};  // +/- 5V display range
};

} // namespace shunya::gui
