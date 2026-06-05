#ifndef ADVANCED_METRICS_H
#define ADVANCED_METRICS_H

#include <map>
#include <string>
#include <vector>

/**
 * Quality of Service Metrics
 */
struct QoSMetrics {
  double latency_ms;       // Average latency in milliseconds
  double jitter_ms;        // Jitter in milliseconds
  double packet_loss_rate; // Packet loss rate (0.0 - 1.0)
  double throughput_mbps;  // Throughput in Mbps
  double mos_score;        // Mean Opinion Score (1.0 - 5.0)

  QoSMetrics()
      : latency_ms(0.0), jitter_ms(0.0), packet_loss_rate(0.0),
        throughput_mbps(0.0), mos_score(0.0) {}
};

/**
 * Signal Quality Metrics
 */
struct SignalMetrics {
  double rssi_dbm;           // Received Signal Strength Indicator
  double rsrp_dbm;           // Reference Signal Received Power
  double rsrq_db;            // Reference Signal Received Quality
  double sinr_db;            // Signal to Interference plus Noise Ratio
  double coverage_radius_km; // Estimated coverage radius

  SignalMetrics()
      : rssi_dbm(-70.0), rsrp_dbm(-80.0), rsrq_db(-10.0), sinr_db(15.0),
        coverage_radius_km(1.0) {}
};

/**
 * Network KPI Metrics
 */
struct NetworkKPIs {
  double call_drop_rate;        // Call drop rate (0.0 - 1.0)
  double handover_success_rate; // Handover success rate (0.0 - 1.0)
  double accessibility;         // Network accessibility (0.0 - 1.0)
  double retainability;         // Connection retainability (0.0 - 1.0)
  double availability;          // Network availability (0.0 - 1.0)

  NetworkKPIs()
      : call_drop_rate(0.02), handover_success_rate(0.98), accessibility(0.99),
        retainability(0.98), availability(0.999) {}
};

/**
 * Traffic Pattern Data
 */
struct TrafficPattern {
  std::string time_period;     // e.g., "Peak Hours", "Off-Peak", "Night"
  double load_factor;          // Load multiplier (0.0 - 2.0)
  int active_users;            // Number of active users
  double avg_session_duration; // Average session duration in minutes

  TrafficPattern(const std::string &period = "Normal", double load = 1.0)
      : time_period(period), load_factor(load), active_users(0),
        avg_session_duration(5.0) {}
};

/**
 * Interference Analysis
 */
struct InterferenceData {
  double co_channel_interference;       // Co-channel interference level
  double adjacent_channel_interference; // Adjacent channel interference
  double external_interference;         // External interference sources
  double total_interference_dbm;        // Total interference in dBm

  InterferenceData()
      : co_channel_interference(0.1), adjacent_channel_interference(0.05),
        external_interference(0.02), total_interference_dbm(-100.0) {}
};

/**
 * Capacity Forecast Data
 */
struct CapacityForecast {
  int current_capacity;       // Current number of users
  int max_capacity;           // Maximum supported users
  double utilization_percent; // Current utilization percentage
  int hours_to_capacity;      // Estimated hours until full capacity
  std::string recommendation; // Capacity planning recommendation

  CapacityForecast()
      : current_capacity(0), max_capacity(100), utilization_percent(0.0),
        hours_to_capacity(-1), recommendation("Capacity available") {}
};

/**
 * Advanced Metrics Calculator
 * Provides comprehensive network quality and performance metrics
 */
class AdvancedMetrics {
public:
  // QoS Metrics Calculation
  static QoSMetrics calculateQoSMetrics(const std::string &generation,
                                        int num_users, double bandwidth_mhz);

  // Signal Quality Calculation
  static SignalMetrics calculateSignalMetrics(const std::string &generation,
                                              double bandwidth_mhz,
                                              int num_antennas = 1);

  // Network KPIs Calculation
  static NetworkKPIs calculateNetworkKPIs(const std::string &generation,
                                          double utilization_percent);

  // Traffic Pattern Generation
  static std::vector<TrafficPattern> generateTrafficPatterns(int num_users);

  // Interference Analysis
  static InterferenceData analyzeInterference(const std::string &generation,
                                              int num_channels, int num_towers);

  // Capacity Forecasting
  static CapacityForecast forecastCapacity(int current_users, int max_users,
                                           double growth_rate);

  // Display Functions
  static void displayQoSMetrics(const QoSMetrics &metrics);
  static void displaySignalMetrics(const SignalMetrics &metrics);
  static void displayNetworkKPIs(const NetworkKPIs &kpis);
  static void
  displayTrafficPatterns(const std::vector<TrafficPattern> &patterns);
  static void displayInterferenceAnalysis(const InterferenceData &interference);
  static void displayCapacityForecast(const CapacityForecast &forecast);

  // Utility Functions
  static double calculateMOS(double latency_ms, double jitter_ms,
                             double packet_loss);
  static double calculateSpectralEfficiency(const std::string &generation);
  static double calculateEnergyEfficiency(const std::string &generation,
                                          int num_users);

private:
  // Helper functions for realistic metric generation
  static double getBaseLatency(const std::string &generation);
  static double getBaseThroughput(const std::string &generation);
  static double getCoverageRadius(const std::string &generation);
};

#endif // ADVANCED_METRICS_H
