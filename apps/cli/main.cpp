#include <cstdio>
#include <string>

#include "e1cipher/diagnostics/platform_report.hpp"
#include "e1cipher/diagnostics/security_gate.hpp"

#include "drone_inspection/inspect.hpp"
#include "fleet_gateway/fleet.hpp"

using namespace e1cipher;

namespace {

void print_usage() {
    std::puts("e1cipher <subcommand> [options]\n");
    std::puts("Subcommands:");
    std::puts(
        "  inspect    Run the drone-inspection reference application (--drones N --scenario S --seed N --tick N)");
    std::puts("  fleet      Run the fleet-gateway plaintext-vs-CKKS aggregation comparison");
    std::puts("  platform   Print a platform report (host, compiler, E1 toolchain status)");
    std::puts("  security   Run the security gate (PASS/WARN/FAIL)");
    std::puts("  demo       One-command end-to-end demo (alias for `inspect` with defaults)");
}

telemetry::Scenario parse_scenario(const std::string& s) {
    if (s == "normal") return telemetry::Scenario::Normal;
    if (s == "battery_stress") return telemetry::Scenario::BatteryStress;
    if (s == "network_degradation") return telemetry::Scenario::NetworkDegradation;
    if (s == "sensor_anomaly") return telemetry::Scenario::SensorAnomaly;
    if (s == "infra_anomaly") return telemetry::Scenario::InfraAnomaly;
    return telemetry::Scenario::InfraAnomaly;
}

int run_inspect_cli(int argc, char** argv) {
    apps::InspectOptions opts;
    for (int i = 2; i + 1 < argc; i += 2) {
        const std::string flag = argv[i];
        const std::string value = argv[i + 1];
        if (flag == "--drones")
            opts.drones = static_cast<std::uint32_t>(std::stoul(value));
        else if (flag == "--scenario")
            opts.scenario = parse_scenario(value);
        else if (flag == "--seed")
            opts.seed = static_cast<std::uint32_t>(std::stoul(value));
        else if (flag == "--tick")
            opts.tick = static_cast<std::uint32_t>(std::stoul(value));
    }
    return apps::run_inspect(opts);
}

int run_fleet_cli(int argc, char** argv) {
    apps::FleetOptions opts;
    for (int i = 2; i + 1 < argc; i += 2) {
        const std::string flag = argv[i];
        const std::string value = argv[i + 1];
        if (flag == "--seed")
            opts.seed = static_cast<std::uint32_t>(std::stoul(value));
        else if (flag == "--scenario")
            opts.scenario = parse_scenario(value);
        else if (flag == "--tick")
            opts.tick = static_cast<std::uint32_t>(std::stoul(value));
    }
    return apps::run_fleet(opts);
}

int run_platform_cli() {
    const auto report = diagnostics::build_platform_report();
    std::fputs(diagnostics::format_platform_report(report).c_str(), stdout);
    return 0;
}

int run_security_cli() {
    const auto result = diagnostics::run_security_gate();
    std::fputs(diagnostics::format_security_gate(result).c_str(), stdout);
    return result.overall == diagnostics::GateStatus::Fail ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    const std::string subcommand = argv[1];

    if (subcommand == "inspect" || subcommand == "demo") return run_inspect_cli(argc, argv);
    if (subcommand == "fleet") return run_fleet_cli(argc, argv);
    if (subcommand == "platform") return run_platform_cli();
    if (subcommand == "security") return run_security_cli();

    std::printf("Unknown subcommand: %s\n\n", subcommand.c_str());
    print_usage();
    return 1;
}
