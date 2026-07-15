#ifndef NETWORK_ANALYTICS_H
#define NETWORK_ANALYTICS_H

#include "Simulator.h"
#include <string>
#include <vector>
#include <map>

/**
 * Quality of Service levels for devices
 */
enum class QoSLevel
{
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

/**
 * Network performance metrics
 */
struct PerformanceMetrics
{
    double average_utilization;
    double peak_utilization;
    int total_devices;
    int connected_devices;
    int failed_connections;
    double success_rate;
    int total_messages;
    double messages_per_core;
};

/**
 * Comparison data for different generations
 */
struct GenerationComparison
{
    std::string generation;
    int max_users;
    int messages_per_user;
    int required_cores;
    double efficiency;
};

/**
 * Network Analytics class for advanced analysis
 */
class NetworkAnalytics
{
private:
    const Simulator *simulator;
    std::map<std::string, PerformanceMetrics> metrics_history;

public:
    NetworkAnalytics(const Simulator *sim);
    ~NetworkAnalytics();

    // Performance analysis
    PerformanceMetrics calculate_current_metrics() const;
    void display_performance_report() const;
    void display_utilization_graph() const;

    // Comparison tools
    void compare_all_generations() const;
    std::vector<GenerationComparison> get_generation_comparisons() const;
    void display_comparison_table() const;

    // Optimization
    void suggest_optimal_configuration(const std::string &generation) const;
    void analyze_bottlenecks() const;

    // Stress testing
    void run_stress_test(const std::string &generation, int targetDevices);

    // Network visualization
    void display_network_topology() const;
    void display_frequency_allocation() const;
};

/**
 * Load Balancer for distributing devices across towers
 */
class LoadBalancer
{
private:
    Simulator *simulator;
    std::map<int, QoSLevel> device_qos;

    // Falls back to a QoS level inferred from the device's communication
    // type when nothing was explicitly set via set_device_qos(): voice
    // traffic is latency-sensitive (HIGH), mixed traffic is MEDIUM, and
    // data-only traffic can tolerate more delay (LOW).
    QoSLevel effective_qos(const std::shared_ptr<UserDevice> &device) const;

public:
    LoadBalancer(Simulator *sim);
    ~LoadBalancer();

    // Load balancing operations
    void balance_load();
    void redistribute_devices();
    int find_best_tower(QoSLevel qos) const;

    // QoS management
    void set_device_qos(int device_id, QoSLevel level);
    QoSLevel get_device_qos(int device_id) const;
    void display_qos_statistics() const;

    // Tower selection
    int find_least_loaded_tower() const;
    int find_most_loaded_tower() const;
};

/**
 * Handover Manager for simulating device mobility
 */
class HandoverManager
{
private:
    Simulator *simulator;
    struct HandoverEvent
    {
        int device_id;
        int from_tower;
        int to_tower;
        std::string timestamp;
        bool success;
    };
    std::vector<HandoverEvent> handover_history;

public:
    HandoverManager(Simulator *sim);
    ~HandoverManager();

    // Handover operations
    bool perform_handover(int deviceIndex, int from_tower, int to_tower);
    void simulate_random_handovers(int numHandovers);
    void display_handover_history() const;

    // Statistics
    double get_handover_success_rate() const;
    int get_total_handovers() const { return handover_history.size(); }
};

/**
 * Scenario Manager for pre-defined network configurations
 */
class ScenarioManager
{
public:
    enum class ScenarioType
    {
        URBAN_DENSE,
        SUBURBAN,
        RURAL,
        HIGHWAY,
        STADIUM,
        DISASTER_RECOVERY,
        MIXED_GENERATION
    };

    static void load_scenario(Simulator &sim, ScenarioType type);
    static void display_available_scenarios();
    static std::string get_scenario_description(ScenarioType type);
};

#endif // NETWORK_ANALYTICS_H
