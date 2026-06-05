#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <string>
#include <vector>
#include <type_traits>



namespace TerminalColor {
  extern const std::string RESET;
  extern const std::string BOLD;
  extern const std::string RED;
  extern const std::string GREEN;
  extern const std::string YELLOW;
  extern const std::string BLUE;
  extern const std::string MAGENTA;
  extern const std::string CYAN;
  extern const std::string WHITE;
  extern const std::string BOLD_RED;
  extern const std::string BOLD_GREEN;
  extern const std::string BOLD_YELLOW;
  extern const std::string BOLD_BLUE;
  extern const std::string BOLD_MAGENTA;
  extern const std::string BOLD_CYAN;
  extern const std::string BOLD_WHITE;
}

/**
 * Utility class for beautiful console output formatting
 */
class OutputFormatter {
public:
  // Box drawing characters
  static const std::string HORIZONTAL;
  static const std::string VERTICAL;
  static const std::string TOP_LEFT;
  static const std::string TOP_RIGHT;
  static const std::string BOTTOM_LEFT;
  static const std::string BOTTOM_RIGHT;
  static const std::string T_DOWN;
  static const std::string T_UP;
  static const std::string T_RIGHT;
  static const std::string T_LEFT;
  static const std::string CROSS;

  // Print decorative lines
  static void print_line(int width = 80);
  static void print_double_line(int width = 80);
  static void print_header(const std::string &title, int width = 80);
  static void print_sub_header(const std::string &title, int width = 80);
  static void print_section(const std::string &title);
  static void print_step_header(int step_number, const std::string &title);

  // Box printing
  static void print_box(const std::vector<std::string> &lines, int width = 80);
  static void print_centered(const std::string &text, int width = 80);
  static void print_summary_table(
      const std::vector<std::pair<std::string, std::string>> &data,
      int width = 60);

  // Table printing
  static void print_table_header(const std::vector<std::string> &headers,
                                 const std::vector<int> &widths);
  static void print_table_row(const std::vector<std::string> &cells,
                              const std::vector<int> &widths);
  static void print_table_separator(const std::vector<int> &widths);

  // Status indicators
  static std::string get_status_icon(bool success);
  static std::string get_progress_bar(double percentage, int width = 20);
  static void draw_bar_graph(const std::string &label, double value,
                             double max_value, int width = 40);
  static void clear_screen();
  static void print_banner();
  static int select_from_menu(const std::string &title, const std::vector<std::string> &options, int width = 80);
};

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

/**
 * Logger class for tracking simulation events
 */
class Logger {
private:
  static bool enabled;
  static std::vector<std::string> logs;

  // Async logging components
  static std::queue<std::string> logQueue;
  static std::mutex queueMutex;
  static std::condition_variable queueCV;
  static std::thread loggingThread;
  static std::atomic<bool> running;

  static void processLogs();

public:
  static void initialize();
  static void shutdown();
  static void enable() { enabled = true; }
  static void disable() { enabled = false; }
  static void log(const std::string &message);
  static void info(const std::string &message);
  static void warning(const std::string &message);
  static void error(const std::string &message);
  static void success(const std::string &message);
  static void display_logs();
  static void clear_logs() { logs.clear(); }
  static const std::vector<std::string>& get_logs() { return logs; }
};

#include <functional>
#include <future>
#include <vector>

/**
 * Thread Pool for parallel task execution
 */
class ThreadPool {
private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex queueMutex;
  std::condition_variable condition;
  std::atomic<bool> stop;

public:
  explicit ThreadPool(size_t numThreads);
  ~ThreadPool();

  template <class F, class... Args>
  auto enqueue(F &&f, Args &&...args)
      -> std::future<std::invoke_result_t<F, Args...>>;

  size_t size() const { return workers.size(); }
};

// Template implementation must be in header
template <class F, class... Args>
auto ThreadPool::enqueue(F &&f, Args &&...args)
    -> std::future<std::invoke_result_t<F, Args...>> {
  using return_type = std::invoke_result_t<F, Args...>;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    if (stop) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }
    tasks.emplace([task]() { (*task)(); });
  }
  condition.notify_one();
  return res;
}

/**
 * String utility functions
 */
class StringUtils {
public:
  static std::string to_upper(const std::string &str);
  static std::string to_lower(const std::string &str);
  static std::string trim(const std::string &str);
  static std::vector<std::string> split(const std::string &str, char delimiter);
  static std::string pad_left(const std::string &str, int width,
                              char fill = ' ');
  static std::string pad_right(const std::string &str, int width,
                               char fill = ' ');
  static std::string center(const std::string &str, int width, char fill = ' ');
};

#endif // UTILS_H
