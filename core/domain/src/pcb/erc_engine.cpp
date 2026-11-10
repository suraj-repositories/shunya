#include "shunya/pcb/erc_engine.hpp"

namespace shunya::pcb {

size_t ErcReport::error_count() const noexcept {
    size_t c = 0;
    for (const auto& v : violations) {
        if (v.severity == ErcSeverity::Error) ++c;
    }
    return c;
}

size_t ErcReport::warning_count() const noexcept {
    size_t c = 0;
    for (const auto& v : violations) {
        if (v.severity == ErcSeverity::Warning) ++c;
    }
    return c;
}

ErcReport ErcEngine::run_checks(const SchematicDocument& document) {
    return run_checks_on_netlist(document.compile_netlist());
}

ErcReport ErcEngine::run_checks_on_netlist(const CompiledNetlist& netlist) {
    ErcReport report;

    // 1. Check floating unconnected pins
    for (const auto& pin : netlist.unconnected_pins) {
        if (pin.electrical_type == PinElectricalType::Input) {
            report.violations.push_back(ErcViolation{
                ErcSeverity::Warning,
                "ERC_FLOATING_INPUT",
                pin.component_ref + "." + pin.pin_number,
                "Input pin '" + pin.pin_name + "' on " + pin.component_ref + " is floating/unconnected"
            });
        } else if (pin.electrical_type == PinElectricalType::PowerInput) {
            report.violations.push_back(ErcViolation{
                ErcSeverity::Error,
                "ERC_UNCONNECTED_POWER_PIN",
                pin.component_ref + "." + pin.pin_number,
                "PowerInput pin '" + pin.pin_name + "' on " + pin.component_ref + " is unconnected"
            });
        }
    }

    // 2. Check pairwise electrical pin conflicts & undriven power nets per net
    for (const auto& net : netlist.nets) {
        bool has_power_in = false;
        bool has_power_out = false;
        bool has_any_driver = false;

        for (size_t i = 0; i < net.pins.size(); ++i) {
            const auto& pi = net.pins[i];
            if (pi.electrical_type == PinElectricalType::PowerInput) has_power_in = true;
            if (pi.electrical_type == PinElectricalType::PowerOutput) has_power_out = true;
            if (pi.electrical_type == PinElectricalType::Output ||
                pi.electrical_type == PinElectricalType::PowerOutput ||
                pi.electrical_type == PinElectricalType::Bidirectional ||
                pi.electrical_type == PinElectricalType::TriState ||
                pi.electrical_type == PinElectricalType::Passive) {
                has_any_driver = true;
            }

            for (size_t j = i + 1; j < net.pins.size(); ++j) {
                const auto& pj = net.pins[j];
                const auto sev = ErcMatrix::check_pin_pair(pi.electrical_type, pj.electrical_type);
                if (sev != ErcSeverity::Ok) {
                    report.violations.push_back(ErcViolation{
                        sev,
                        (sev == ErcSeverity::Error) ? "ERC_DRIVER_CONFLICT" : "ERC_DRIVER_WARNING",
                        net.name,
                        "Pin " + pi.component_ref + "." + pi.pin_number +
                            " conflicts with " + pj.component_ref + "." + pj.pin_number +
                            " on net '" + net.name + "'"
                    });
                }
            }
        }

        if (has_power_in && !has_power_out) {
            report.violations.push_back(ErcViolation{
                ErcSeverity::Warning,
                "ERC_UNDRIVEN_POWER_NET",
                net.name,
                "Power rail '" + net.name + "' has PowerInput pins but no driving PowerOutput/PWR_FLAG"
            });
        }
        if (!has_any_driver && !net.pins.empty()) {
            report.violations.push_back(ErcViolation{
                ErcSeverity::Warning,
                "ERC_UNDRIVEN_INPUT_NET",
                net.name,
                "Net '" + net.name + "' has input/power-in pins without an active source"
            });
        }
        if (net.pins.size() == 1 && net.pins[0].electrical_type == PinElectricalType::Input) {
            report.violations.push_back(ErcViolation{
                ErcSeverity::Warning,
                "ERC_SINGLE_PIN_INPUT_NET",
                net.name,
                "Named net '" + net.name + "' connects only to a single Input pin"
            });
        }
    }

    return report;
}

} // namespace shunya::pcb
