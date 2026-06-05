#include "AdvancedMetrics.h"
#include "Utils.h"
#include "IOHelpers.h"
#include <algorithm>
#include <cmath>
#include <random>

// QoS Metrics Calculation
QoSMetrics AdvancedMetrics::calculateQoSMetrics(const std::string &generation,
                                                int num_users,
                                                double bandwidth_mhz) {
  QoSMetrics metrics;

  // Base latency by generation
  metrics.latency_ms = getBaseLatency(generation);

  // Add congestion factor based on user load
  double congestion_factor = 1.0 + (num_users / 100.0) * 0.5;
  metrics.latency_ms *= congestion_factor;

  // Jitter is typically 10-20% of latency
  metrics.jitter_ms = metrics.latency_ms * 0.15;

  // Packet loss increases with congestion
  metrics.packet_loss_rate = std::min(0.1, (num_users / 500.0) * 0.02);

  // Throughput calculation
  metrics.throughput_mbps = getBaseThroughput(generation) * bandwidth_mhz;
  metrics.throughput_mbps /= std::max(1, num_users / 10); // Shared bandwidth

  // Calculate MOS score
  metrics.mos_score = calculateMOS(metrics.latency_ms, metrics.jitter_ms,
                                   metrics.packet_loss_rate);

  return metrics;
}

// Signal Quality Calculation
SignalMetrics AdvancedMetrics::calculateSignalMetrics(
    const std::string &generation, double bandwidth_mhz, int num_antennas) {
  // Suppress unused parameter warning - bandwidth_mhz reserved for future use
  (void)bandwidth_mhz;

  SignalMetrics metrics;

  // Base signal strength (RSSI - Received Signal Strength Indicator)
  metrics.rssi_dbm = -70.0; // Good signal baseline

  // Generation-specific adjustments
  if (generation == "2G") {
    metrics.rssi_dbm = -65.0;
    metrics.rsrp_dbm = -75.0;
    metrics.rsrq_db = -8.0;
    metrics.sinr_db = 12.0;
    metrics.coverage_radius_km = 35.0;
  } else if (generation == "3G") {
    metrics.rssi_dbm = -70.0;
    metrics.rsrp_dbm = -80.0;
    metrics.rsrq_db = -9.0;
    metrics.sinr_db = 10.0;
    metrics.coverage_radius_km = 10.0;
  } else if (generation == "4G") {
    metrics.rssi_dbm = -75.0;
    metrics.rsrp_dbm = -85.0;
    metrics.rsrq_db = -10.0;
    metrics.sinr_db = 15.0;
    metrics.coverage_radius_km = 5.0;
  } else if (generation == "5G") {
    metrics.rssi_dbm = -80.0;
    metrics.rsrp_dbm = -90.0;
    metrics.rsrq_db = -11.0;
    metrics.sinr_db = 20.0;
    metrics.coverage_radius_km = 1.0;
  } else if (generation == "6G") {
    metrics.rssi_dbm = -85.0;
    metrics.rsrp_dbm = -95.0;
    metrics.rsrq_db = -12.0;
    metrics.sinr_db = 25.0;
    metrics.coverage_radius_km = 0.5;
  } else { // 7G
    metrics.rssi_dbm = -90.0;
    metrics.rsrp_dbm = -100.0;
    metrics.rsrq_db = -13.0;
    metrics.sinr_db = 30.0;
    metrics.coverage_radius_km = 0.3;
  }

  // MIMO antenna gain (improves signal quality)
  if (num_antennas > 1) {
    double antenna_gain = 3.0 * std::log10(num_antennas);
    metrics.rssi_dbm += antenna_gain;
    metrics.rsrp_dbm += antenna_gain;
    metrics.sinr_db += antenna_gain / 2.0;
  }

  return metrics;
}

// Network KPIs Calculation
NetworkKPIs AdvancedMetrics::calculateNetworkKPIs(const std::string &generation,
                                                  double utilization_percent) {
  NetworkKPIs kpis;

  // Better generations have better KPIs
  double gen_factor = 1.0;
  if (generation == "2G")
    gen_factor = 0.90;
  else if (generation == "3G")
    gen_factor = 0.93;
  else if (generation == "4G")
    gen_factor = 0.96;
  else if (generation == "5G")
    gen_factor = 0.98;
  else if (generation == "6G")
    gen_factor = 0.99;
  else
    gen_factor = 0.995; // 7G

  // KPIs degrade with high utilization
  double util_factor = 1.0 - (utilization_percent / 200.0);

  kpis.call_drop_rate =
      (1.0 - gen_factor) * (1.0 + utilization_percent / 100.0);
  kpis.call_drop_rate = std::min(0.15, kpis.call_drop_rate);

  kpis.handover_success_rate = gen_factor * util_factor;
  kpis.handover_success_rate = std::max(0.85, kpis.handover_success_rate);

  kpis.accessibility = gen_factor * util_factor;
  kpis.accessibility = std::max(0.90, kpis.accessibility);

  kpis.retainability = gen_factor * util_factor;
  kpis.retainability = std::max(0.92, kpis.retainability);

  kpis.availability = 0.999 * gen_factor;

  return kpis;
}

// Traffic Pattern Generation
std::vector<TrafficPattern>
AdvancedMetrics::generateTrafficPatterns(int num_users) {
  std::vector<TrafficPattern> patterns;

  // Morning Peak (8-10 AM)
  TrafficPattern morning("Morning Peak (8-10 AM)", 1.5);
  morning.active_users = static_cast<int>(num_users * 0.7);
  morning.avg_session_duration = 8.0;
  patterns.push_back(morning);

  // Midday (10 AM - 5 PM)
  TrafficPattern midday("Midday (10 AM - 5 PM)", 1.0);
  midday.active_users = static_cast<int>(num_users * 0.5);
  midday.avg_session_duration = 6.0;
  patterns.push_back(midday);

  // Evening Peak (5-9 PM)
  TrafficPattern evening("Evening Peak (5-9 PM)", 2.0);
  evening.active_users = static_cast<int>(num_users * 0.9);
  evening.avg_session_duration = 12.0;
  patterns.push_back(evening);

  // Night (9 PM - 8 AM)
  TrafficPattern night("Night (9 PM - 8 AM)", 0.3);
  night.active_users = static_cast<int>(num_users * 0.2);
  night.avg_session_duration = 3.0;
  patterns.push_back(night);

  return patterns;
}

// Interference Analysis
InterferenceData
AdvancedMetrics::analyzeInterference(const std::string &generation,
                                     int num_channels, int num_towers) {
  InterferenceData interference;

  // More channels and towers = more interference
  interference.co_channel_interference = 0.05 * (num_towers / 5.0);
  interference.adjacent_channel_interference = 0.03 * (num_channels / 10.0);
  interference.external_interference = 0.01 + (rand() % 10) / 1000.0;

  // Total interference in dBm
  double total_linear = interference.co_channel_interference +
                        interference.adjacent_channel_interference +
                        interference.external_interference;
  interference.total_interference_dbm = -100.0 + (total_linear * 20.0);

  // Better generations have better interference mitigation
  if (generation == "5G" || generation == "6G" || generation == "7G") {
    interference.co_channel_interference *= 0.5;
    interference.adjacent_channel_interference *= 0.6;
  }

  return interference;
}

// Capacity Forecasting
CapacityForecast AdvancedMetrics::forecastCapacity(int current_users,
                                                   int max_users,
                                                   double growth_rate) {
  CapacityForecast forecast;

  forecast.current_capacity = current_users;
  forecast.max_capacity = max_users;
  forecast.utilization_percent =
      (100.0 * current_users) / std::max(1, max_users);

  // Calculate time to capacity
  if (current_users < max_users && growth_rate > 0) {
    int users_remaining = max_users - current_users;
    forecast.hours_to_capacity =
        static_cast<int>(users_remaining / growth_rate);
  } else if (current_users >= max_users) {
    forecast.hours_to_capacity = 0;
  } else {
    forecast.hours_to_capacity = -1; // No growth
  }

  // Generate recommendation
  if (forecast.utilization_percent > 90.0) {
    forecast.recommendation = "CRITICAL: Add capacity immediately!";
  } else if (forecast.utilization_percent > 75.0) {
    forecast.recommendation = "WARNING: Plan capacity expansion soon";
  } else if (forecast.utilization_percent > 50.0) {
    forecast.recommendation = "MODERATE: Monitor growth trends";
  } else {
    forecast.recommendation = "OPTIMAL: Sufficient capacity available";
  }

  return forecast;
}

// Display Functions
void AdvancedMetrics::displayQoSMetrics(const QoSMetrics &metrics) {
  IOHelpers::println("  Quality of Service Metrics");
  
  IOHelpers::print("  Latency: ");
  IOHelpers::printDouble(metrics.latency_ms, 2);
  IOHelpers::println(" ms");
  
  IOHelpers::print("  Jitter: ");
  IOHelpers::printDouble(metrics.jitter_ms, 2);
  IOHelpers::println(" ms");
  
  IOHelpers::print("  Packet Loss: ");
  IOHelpers::printDouble(metrics.packet_loss_rate * 100.0, 2);
  IOHelpers::println("%");
  
  IOHelpers::print("  Throughput: ");
  IOHelpers::printDouble(metrics.throughput_mbps, 2);
  IOHelpers::println(" Mbps");

  IOHelpers::print("  MOS Score: ");
  IOHelpers::printDouble(metrics.mos_score, 2);
  IOHelpers::print("/5.0 (");
  if (metrics.mos_score >= 4.0)
    IOHelpers::print("Excellent");
  else if (metrics.mos_score >= 3.5)
    IOHelpers::print("Good");
  else if (metrics.mos_score >= 3.0)
    IOHelpers::print("Fair");
  else
    IOHelpers::print("Poor");
  IOHelpers::println(")");
}

void AdvancedMetrics::displaySignalMetrics(const SignalMetrics &metrics) {
  IOHelpers::println("  Signal Quality Metrics");
  IOHelpers::print("  RSSI: ");
  IOHelpers::printDouble(metrics.rssi_dbm, 1);
  IOHelpers::println(" dBm");
  IOHelpers::print("  RSRP: ");
  IOHelpers::printDouble(metrics.rsrp_dbm, 1);
  IOHelpers::println(" dBm");
  IOHelpers::print("  RSRQ: ");
  IOHelpers::printDouble(metrics.rsrq_db, 1);
  IOHelpers::println(" dB");
  IOHelpers::print("  SINR: ");
  IOHelpers::printDouble(metrics.sinr_db, 1);
  IOHelpers::println(" dB");
  IOHelpers::print("  Coverage Radius: ");
  IOHelpers::printDouble(metrics.coverage_radius_km, 1);
  IOHelpers::println(" km");
}

void AdvancedMetrics::displayNetworkKPIs(const NetworkKPIs &kpis) {
  IOHelpers::println("  Network KPIs");
  IOHelpers::print("  Call Drop Rate: ");
  IOHelpers::printDouble(kpis.call_drop_rate * 100.0, 2);
  IOHelpers::println("%");
  IOHelpers::print("  Handover Success Rate: ");
  IOHelpers::printDouble(kpis.handover_success_rate * 100.0, 2);
  IOHelpers::println("%");
  IOHelpers::print("  Accessibility: ");
  IOHelpers::printDouble(kpis.accessibility * 100.0, 2);
  IOHelpers::println("%");
  IOHelpers::print("  Retainability: ");
  IOHelpers::printDouble(kpis.retainability * 100.0, 2);
  IOHelpers::println("%");
  IOHelpers::print("  Availability: ");
  IOHelpers::printDouble(kpis.availability * 100.0, 2);
  IOHelpers::println("%");
}

void AdvancedMetrics::displayTrafficPatterns(
    const std::vector<TrafficPattern> &patterns) {
  IOHelpers::println("  Traffic Patterns (24-hour cycle)");
  IOHelpers::printNewline();

  for (const auto &pattern : patterns) {
    IOHelpers::print("  > ");
    IOHelpers::println(pattern.time_period.c_str());
    IOHelpers::print("    Load Factor: ");
    IOHelpers::printDouble(pattern.load_factor, 1);
    IOHelpers::println("x");
    IOHelpers::print("    Active Users: ");
    IOHelpers::printInt(pattern.active_users);
    IOHelpers::printNewline();
    IOHelpers::print("    Avg Session: ");
    IOHelpers::printDouble(pattern.avg_session_duration, 1);
    IOHelpers::println(" min");
    IOHelpers::printNewline();
  }
}

void AdvancedMetrics::displayInterferenceAnalysis(
    const InterferenceData &interference) {
  IOHelpers::println("  Interference Analysis");
  IOHelpers::print("  Co-Channel Interference: ");
  IOHelpers::printDouble(interference.co_channel_interference * 100.0, 3);
  IOHelpers::println("%");
  IOHelpers::print("  Adjacent Channel Interference: ");
  IOHelpers::printDouble(interference.adjacent_channel_interference * 100.0, 3);
  IOHelpers::println("%");
  IOHelpers::print("  External Interference: ");
  IOHelpers::printDouble(interference.external_interference * 100.0, 3);
  IOHelpers::println("%");
  IOHelpers::print("  Total Interference: ");
  IOHelpers::printDouble(interference.total_interference_dbm, 1);
  IOHelpers::println(" dBm");
}

void AdvancedMetrics::displayCapacityForecast(
    const CapacityForecast &forecast) {
  IOHelpers::println("  Capacity Forecast");
  IOHelpers::print("  Current Capacity: ");
  IOHelpers::printInt(forecast.current_capacity);
  IOHelpers::print("/");
  IOHelpers::printInt(forecast.max_capacity);
  IOHelpers::printNewline();

  IOHelpers::print("  Utilization: ");
  IOHelpers::printDouble(forecast.utilization_percent, 1);
  IOHelpers::println("%");

  if (forecast.hours_to_capacity >= 0) {
    IOHelpers::print("  Time to Capacity: ");
    IOHelpers::printInt(forecast.hours_to_capacity);
    IOHelpers::println(" hours");
  }

  IOHelpers::print("  Recommendation: ");
  IOHelpers::println(forecast.recommendation.c_str());
}

// Utility Functions
double AdvancedMetrics::calculateMOS(double latency_ms, double jitter_ms,
                                     double packet_loss) {
  // Simplified E-Model for MOS calculation
  // MOS ranges from 1.0 (bad) to 5.0 (excellent)

  double R = 93.2; // Base R-factor

  // Latency impact
  double delay_impairment = 0.024 * latency_ms + 0.11 * (latency_ms - 177.3);
  if (delay_impairment < 0)
    delay_impairment = 0;

  // Packet loss impact
  double loss_impairment = packet_loss * 100.0 * 2.5;

  // Jitter impact (simplified)
  double jitter_impairment = jitter_ms * 0.1;

  R -= (delay_impairment + loss_impairment + jitter_impairment);
  R = std::max(0.0, std::min(100.0, R));

  // Convert R-factor to MOS
  double mos;
  if (R < 0)
    mos = 1.0;
  else if (R > 100)
    mos = 4.5;
  else
    mos = 1.0 + 0.035 * R + 7.0e-6 * R * (R - 60.0) * (100.0 - R);

  return std::max(1.0, std::min(5.0, mos));
}

double
AdvancedMetrics::calculateSpectralEfficiency(const std::string &generation) {
  // Spectral efficiency in bits/Hz/cell
  if (generation == "2G")
    return 0.5;
  else if (generation == "3G")
    return 1.0;
  else if (generation == "4G")
    return 3.0;
  else if (generation == "5G")
    return 7.5;
  else if (generation == "6G")
    return 15.0;
  else
    return 30.0; // 7G
}

double AdvancedMetrics::calculateEnergyEfficiency(const std::string &generation,
                                                  int num_users) {
  // Energy efficiency in bits/Joule (higher is better)
  double base_efficiency = calculateSpectralEfficiency(generation) * 1000.0;

  // More users = better energy efficiency (amortized)
  double user_factor = 1.0 + std::log10(std::max(1, num_users));

  return base_efficiency * user_factor;
}

// Helper Functions
double AdvancedMetrics::getBaseLatency(const std::string &generation) {
  if (generation == "2G")
    return 500.0; // 500ms
  else if (generation == "3G")
    return 100.0; // 100ms
  else if (generation == "4G")
    return 50.0; // 50ms
  else if (generation == "5G")
    return 10.0; // 10ms
  else if (generation == "6G")
    return 5.0; // 5ms
  else
    return 1.0; // 1ms for 7G
}

double AdvancedMetrics::getBaseThroughput(const std::string &generation) {
  // Throughput per MHz in Mbps
  if (generation == "2G")
    return 0.1;
  else if (generation == "3G")
    return 2.0;
  else if (generation == "4G")
    return 10.0;
  else if (generation == "5G")
    return 50.0;
  else if (generation == "6G")
    return 100.0;
  else
    return 500.0; // 7G
}

double AdvancedMetrics::getCoverageRadius(const std::string &generation) {
  // Coverage radius in km (typical urban environment)
  if (generation == "2G")
    return 35.0;
  else if (generation == "3G")
    return 10.0;
  else if (generation == "4G")
    return 5.0;
  else if (generation == "5G")
    return 1.0;
  else if (generation == "6G")
    return 0.5;
  else
    return 0.3; // 7G (higher frequency = shorter range)
}
