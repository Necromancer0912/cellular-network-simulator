#include "Utils.h"
#include "IOHelpers.h"
#include <algorithm>
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <cstdio>

namespace {
  int get_key_press() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    
    if (ch == 27) {
      int next1 = getchar();
      if (next1 == 91) {
        int next2 = getchar();
        if (next2 == 'A') {
          tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
          return 1001;
        } else if (next2 == 'B') {
          tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
          return 1002;
        }
      }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
  }
}

// ANSI Escape Codes for Colors
#define ANSI_RESET          "\033[0m"
#define ANSI_BOLD           "\033[1m"
#define ANSI_RED            "\033[31m"
#define ANSI_GREEN          "\033[32m"
#define ANSI_YELLOW         "\033[33m"
#define ANSI_BLUE           "\033[34m"
#define ANSI_MAGENTA        "\033[35m"
#define ANSI_CYAN           "\033[36m"
#define ANSI_WHITE          "\033[37m"

#define ANSI_BOLD_RED       "\033[1;31m"
#define ANSI_BOLD_GREEN     "\033[1;32m"
#define ANSI_BOLD_YELLOW    "\033[1;33m"
#define ANSI_BOLD_BLUE      "\033[1;34m"
#define ANSI_BOLD_MAGENTA   "\033[1;35m"
#define ANSI_BOLD_CYAN      "\033[1;36m"

namespace TerminalColor {
  const std::string RESET = "\033[0m";
  const std::string BOLD = "\033[1m";
  const std::string RED = "\033[31m";
  const std::string GREEN = "\033[32m";
  const std::string YELLOW = "\033[33m";
  const std::string BLUE = "\033[34m";
  const std::string MAGENTA = "\033[35m";
  const std::string CYAN = "\033[36m";
  const std::string WHITE = "\033[37m";
  const std::string BOLD_RED = "\033[1;31m";
  const std::string BOLD_GREEN = "\033[1;32m";
  const std::string BOLD_YELLOW = "\033[1;33m";
  const std::string BOLD_BLUE = "\033[1;34m";
  const std::string BOLD_MAGENTA = "\033[1;35m";
  const std::string BOLD_CYAN = "\033[1;36m";
  const std::string BOLD_WHITE = "\033[1;37m";
}

// Initialize static members
const std::string OutputFormatter::HORIZONTAL = "─";
const std::string OutputFormatter::VERTICAL = "│";
const std::string OutputFormatter::TOP_LEFT = "┌";
const std::string OutputFormatter::TOP_RIGHT = "┐";
const std::string OutputFormatter::BOTTOM_LEFT = "└";
const std::string OutputFormatter::BOTTOM_RIGHT = "┘";
const std::string OutputFormatter::T_DOWN = "┬";
const std::string OutputFormatter::T_UP = "┴";
const std::string OutputFormatter::T_RIGHT = "├";
const std::string OutputFormatter::T_LEFT = "┤";
const std::string OutputFormatter::CROSS = "┼";

void OutputFormatter::print_line(int width) {
  for (int i = 0; i < width; i++) {
    IOHelpers::print(HORIZONTAL.c_str());
  }
  IOHelpers::printNewline();
}

void OutputFormatter::print_double_line(int width) {
  for (int i = 0; i < width; i++) {
    IOHelpers::print("═");
  }
  IOHelpers::printNewline();
}

void OutputFormatter::print_header(const std::string &title, int width) {
  IOHelpers::printNewline();
  IOHelpers::print(ANSI_BOLD_BLUE);
  print_double_line(width);
  print_centered(title, width);
  print_double_line(width);
  IOHelpers::print(ANSI_RESET);
  IOHelpers::printNewline();
}

void OutputFormatter::print_sub_header(const std::string &title, int width) {
  IOHelpers::printNewline();
  IOHelpers::print(ANSI_BOLD_CYAN);
  print_line(width);
  print_centered(title, width);
  print_line(width);
  IOHelpers::print(ANSI_RESET);
}

void OutputFormatter::print_section(const std::string &title) {
  IOHelpers::printNewline();
  IOHelpers::print(ANSI_BOLD_CYAN);
  IOHelpers::print("> ");
  IOHelpers::print(ANSI_RESET);
  IOHelpers::println(title.c_str());
  IOHelpers::printNewline();
}

void OutputFormatter::print_step_header(int step_number,
                                        const std::string &title) {
  IOHelpers::printNewline();
  IOHelpers::print(ANSI_BOLD_YELLOW);
  IOHelpers::print("STEP ");
  IOHelpers::printInt(step_number);
  IOHelpers::print(": ");
  IOHelpers::print(ANSI_RESET);
  IOHelpers::println(title.c_str());
  IOHelpers::print(ANSI_CYAN);
  for (size_t i = 0; i < title.length() + 8; i++)
    IOHelpers::print(HORIZONTAL.c_str());
  IOHelpers::print(ANSI_RESET);
  IOHelpers::printNewline();
  IOHelpers::printNewline();
}

void OutputFormatter::print_summary_table(
    const std::vector<std::pair<std::string, std::string>> &data, int width) {
  IOHelpers::print(ANSI_CYAN);
  print_line(width);
  IOHelpers::print(ANSI_RESET);

  for (const auto &row : data) {
    int keyWidth = width / 2;
    IOHelpers::print(" ");
    IOHelpers::print(StringUtils::pad_right(row.first, keyWidth).c_str());
    IOHelpers::print(ANSI_CYAN);
    IOHelpers::print(" │ ");
    IOHelpers::print(ANSI_RESET);
    
    // Check if it's network status/health and color accordingly
    if (row.second.find("Optimal") != std::string::npos || row.second.find("Excellent") != std::string::npos) {
      IOHelpers::print(ANSI_BOLD_GREEN);
    } else if (row.second.find("High Load") != std::string::npos || row.second.find("Warning") != std::string::npos) {
      IOHelpers::print(ANSI_BOLD_YELLOW);
    } else if (row.second.find("Critical") != std::string::npos || row.second.find("Full") != std::string::npos) {
      IOHelpers::print(ANSI_BOLD_RED);
    }
    IOHelpers::println(row.second.c_str());
    IOHelpers::print(ANSI_RESET);
  }

  IOHelpers::print(ANSI_CYAN);
  print_line(width);
  IOHelpers::print(ANSI_RESET);
}

void OutputFormatter::print_box(const std::vector<std::string> &lines,
                                int width) {
  IOHelpers::print(ANSI_CYAN);
  IOHelpers::print(TOP_LEFT.c_str());
  for (int i = 0; i < width - 2; i++)
    IOHelpers::print(HORIZONTAL.c_str());
  IOHelpers::println(TOP_RIGHT.c_str());
  IOHelpers::print(ANSI_RESET);

  for (const auto &line : lines) {
    IOHelpers::print(ANSI_CYAN);
    IOHelpers::print(VERTICAL.c_str());
    IOHelpers::print(ANSI_RESET);
    IOHelpers::print(" ");
    IOHelpers::print(StringUtils::pad_right(line, width - 4).c_str());
    IOHelpers::print(" ");
    IOHelpers::print(ANSI_CYAN);
    IOHelpers::println(VERTICAL.c_str());
    IOHelpers::print(ANSI_RESET);
  }

  IOHelpers::print(ANSI_CYAN);
  IOHelpers::print(BOTTOM_LEFT.c_str());
  for (int i = 0; i < width - 2; i++)
    IOHelpers::print(HORIZONTAL.c_str());
  IOHelpers::println(BOTTOM_RIGHT.c_str());
  IOHelpers::print(ANSI_RESET);
}

void OutputFormatter::print_centered(const std::string &text, int width) {
  int padding = (width - text.length()) / 2;
  for (int i = 0; i < padding; i++)
    IOHelpers::print(" ");
  IOHelpers::print(ANSI_BOLD);
  IOHelpers::print(text.c_str());
  IOHelpers::print(ANSI_RESET);
  IOHelpers::printNewline();
}

void OutputFormatter::print_table_header(
    const std::vector<std::string> &headers, const std::vector<int> &widths) {
  IOHelpers::print(ANSI_CYAN);
  for (size_t i = 0; i < widths.size(); i++) {
    for (int j = 0; j < widths[i]; j++)
      IOHelpers::print("─");
    if (i < widths.size() - 1)
      IOHelpers::print("──");
  }
  IOHelpers::print(ANSI_RESET);
  IOHelpers::printNewline();

  for (size_t i = 0; i < headers.size(); i++) {
    IOHelpers::print(ANSI_BOLD);
    IOHelpers::print(StringUtils::pad_right(headers[i], widths[i]).c_str());
    IOHelpers::print(ANSI_RESET);
    if (i < headers.size() - 1) {
      IOHelpers::print(ANSI_CYAN);
      IOHelpers::print(" │ ");
      IOHelpers::print(ANSI_RESET);
    }
  }
  IOHelpers::printNewline();

  IOHelpers::print(ANSI_CYAN);
  for (size_t i = 0; i < widths.size(); i++) {
    for (int j = 0; j < widths[i]; j++)
      IOHelpers::print("─");
    if (i < widths.size() - 1)
      IOHelpers::print("─┼─");
  }
  IOHelpers::print(ANSI_RESET);
  IOHelpers::printNewline();
}

void OutputFormatter::print_table_row(const std::vector<std::string> &cells,
                                      const std::vector<int> &widths) {
  for (size_t i = 0; i < cells.size(); i++) {
    // Check if cell is active status or health indicator and color it
    if (cells[i].find("Optimal") != std::string::npos || cells[i].find("Excellent") != std::string::npos || cells[i].find("Normal") != std::string::npos || cells[i].find("Active") != std::string::npos) {
      IOHelpers::print(ANSI_BOLD_GREEN);
    } else if (cells[i].find("High Load") != std::string::npos || cells[i].find("Monitor") != std::string::npos || cells[i].find("Warning") != std::string::npos) {
      IOHelpers::print(ANSI_BOLD_YELLOW);
    } else if (cells[i].find("Critical") != std::string::npos || cells[i].find("Full") != std::string::npos || cells[i].find("Error") != std::string::npos) {
      IOHelpers::print(ANSI_BOLD_RED);
    }
    
    IOHelpers::print(StringUtils::pad_right(cells[i], widths[i]).c_str());
    IOHelpers::print(ANSI_RESET);
    if (i < cells.size() - 1) {
      IOHelpers::print(ANSI_CYAN);
      IOHelpers::print(" │ ");
      IOHelpers::print(ANSI_RESET);
    }
  }
  IOHelpers::printNewline();
}

void OutputFormatter::print_table_separator(const std::vector<int> &widths) {
  IOHelpers::print(ANSI_CYAN);
  for (size_t i = 0; i < widths.size(); i++) {
    for (int j = 0; j < widths[i]; j++)
      IOHelpers::print("─");
    if (i < widths.size() - 1)
      IOHelpers::print("─┴─");
  }
  IOHelpers::print(ANSI_RESET);
  IOHelpers::printNewline();
}

std::string OutputFormatter::get_progress_bar(double percentage, int width) {
  int filled = static_cast<int>(percentage * width);
  std::string bar = "";
  if (percentage >= 0.90) {
    bar += ANSI_BOLD_RED;
  } else if (percentage >= 0.75) {
    bar += ANSI_BOLD_YELLOW;
  } else {
    bar += ANSI_BOLD_GREEN;
  }
  bar += "▕";
  for (int i = 0; i < width; i++) {
    if (i < filled)
      bar += "█";
    else
      bar += "░";
  }
  bar += "▏";
  bar += ANSI_RESET;
  return bar;
}

void OutputFormatter::draw_bar_graph(const std::string &label, double value,
                                     double max_value, int width) {
  int label_width = 20;
  std::string padded_label = StringUtils::pad_right(label, label_width);

  double percentage = value / max_value;
  if (percentage > 1.0)
    percentage = 1.0;
  if (percentage < 0.0)
    percentage = 0.0;

  int bar_width = static_cast<int>(percentage * width);

  IOHelpers::print(padded_label.c_str());
  IOHelpers::print(" ");

  if (percentage >= 0.90) {
    IOHelpers::print(ANSI_BOLD_RED);
  } else if (percentage >= 0.75) {
    IOHelpers::print(ANSI_BOLD_YELLOW);
  } else {
    IOHelpers::print(ANSI_BOLD_GREEN);
  }

  for (int i = 0; i < bar_width; i++)
    IOHelpers::print("█");
  for (int i = bar_width; i < width; i++)
    IOHelpers::print("░");

  IOHelpers::print(ANSI_RESET);
  IOHelpers::print(" ");
  IOHelpers::printDouble(value, 1);
  IOHelpers::printNewline();
}

void OutputFormatter::clear_screen() {
  IOHelpers::print("\033[H\033[2J");
}

void OutputFormatter::print_banner() {
  IOHelpers::println(ANSI_BOLD_CYAN);
  IOHelpers::println("   ______     __  __      __              _   __     __                      __");
  IOHelpers::println("  / ____/__  / / / /_  __/ /___ ______   / | / /__  / /__      ______  _____/ /__");
  IOHelpers::println(" / /   / _ \\/ / / / / / / / __ `/ ___/  /  |/ / _ \\/ __/ | /| / / __ \\/ ___/ //_/");
  IOHelpers::println("/ /___/  __/ / / / /_/ / / /_/ / /     / /|  /  __/ /_ | |/ |/ / /_/ / /  / ,<   ");
  IOHelpers::println("\\____/\\___/_/_/_/\\__,_/_/\\__,_/_/     /_/ |_/\\___/\\__/ |__/|__/\\____/_/  /_/|_|");
  IOHelpers::println(ANSI_RESET);
  IOHelpers::println("           \033[1;32mComprehensive Cellular Network Simulator v2.0\033[0m");
  IOHelpers::println("");
}

// Logger implementation
bool Logger::enabled = true;
std::vector<std::string> Logger::logs;
std::queue<std::string> Logger::logQueue;
std::mutex Logger::queueMutex;
std::condition_variable Logger::queueCV;
std::thread Logger::loggingThread;
std::atomic<bool> Logger::running(false);
std::atomic<bool> Logger::consoleEcho(true);
std::atomic<bool> Logger::quiet(false);
std::mutex Logger::logsMutex;

void Logger::initialize() {
  if (!running) {
    running = true;
    loggingThread = std::thread(Logger::processLogs);
  }
}

void Logger::shutdown() {
  if (running) {
    running = false;
    queueCV.notify_all();
    if (loggingThread.joinable()) {
      loggingThread.join();
    }
  }
}

void Logger::processLogs() {
  while (running) {
    std::unique_lock<std::mutex> lock(queueMutex);
    queueCV.wait(lock, [] { return !logQueue.empty() || !running; });

    while (!logQueue.empty()) {
      std::string message = logQueue.front();
      logQueue.pop();
      lock.unlock();
      // Skip the raw stdout write while ConsoleTUI owns the terminal - see
      // consoleEcho comment in Utils.h. The message is already in logs[]
      // for the TUI's own event-log panel to pick up.
      if (consoleEcho) {
        IOHelpers::println(message.c_str());
      }
      lock.lock();
    }
  }
}

void Logger::emit(const std::string &formatted) {
  {
    std::lock_guard<std::mutex> lock(logsMutex);
    logs.push_back(formatted);
  }
  if (running) {
    std::lock_guard<std::mutex> lock(queueMutex);
    logQueue.push(formatted);
    queueCV.notify_one();
  } else {
    IOHelpers::println(formatted.c_str());
  }
}

void Logger::log(const std::string &message) {
  if (!enabled) return;
  emit(message);
}

void Logger::info(const std::string &message) {
  if (!enabled || quiet) return;
  emit("\033[1;36m[INFO]\033[0m " + message);
}

void Logger::warning(const std::string &message) {
  if (!enabled) return;
  emit("\033[1;33m[WARNING]\033[0m " + message);
}

void Logger::error(const std::string &message) {
  if (!enabled) return;
  emit("\033[1;31m[ERROR]\033[0m " + message);
}

void Logger::success(const std::string &message) {
  if (!enabled || quiet) return;
  emit("\033[1;32m[OK]\033[0m " + message);
}


// StringUtils implementation
std::string StringUtils::to_upper(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return result;
}

std::string StringUtils::to_lower(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}

std::string StringUtils::trim(const std::string &str) {
  size_t first = str.find_first_not_of(" \t\n\r");
  if (first == std::string::npos)
    return "";
  size_t last = str.find_last_not_of(" \t\n\r");
  return str.substr(first, last - first + 1);
}

std::vector<std::string> StringUtils::split(const std::string &str,
                                            char delimiter) {
  std::vector<std::string> tokens;
  std::stringstream ss(str);
  std::string token;
  while (std::getline(ss, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

std::string StringUtils::pad_left(const std::string &str, int width,
                                  char fill) {
  if (str.length() >= static_cast<size_t>(width))
    return str;
  return std::string(width - str.length(), fill) + str;
}

std::string StringUtils::pad_right(const std::string &str, int width,
                                   char fill) {
  if (str.length() >= static_cast<size_t>(width))
    return str;
  return str + std::string(width - str.length(), fill);
}

std::string StringUtils::center(const std::string &str, int width, char fill) {
  if (str.length() >= static_cast<size_t>(width))
    return str;
  int totalPad = width - str.length();
  int leftPad = totalPad / 2;
  int rightPad = totalPad - leftPad;
  return std::string(leftPad, fill) + str + std::string(rightPad, fill);
}

// ThreadPool implementation
ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
  for (size_t i = 0; i < numThreads; ++i) {
    workers.emplace_back([this] {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(this->queueMutex);
          this->condition.wait(
              lock, [this] { return this->stop || !this->tasks.empty(); });
          if (this->stop && this->tasks.empty())
            return;
          task = std::move(this->tasks.front());
          this->tasks.pop();
        }
        task();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    stop = true;
  }
  condition.notify_all();
  for (std::thread &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

int OutputFormatter::select_from_menu(const std::string &title, const std::vector<std::string> &options, int width) {
  int selected = 0;
  int num_options = options.size();
  
  // Hide cursor
  IOHelpers::print("\033[?25l");
  
  while (true) {
    clear_screen();
    print_banner();
    print_header(title, width);
    
    for (int i = 0; i < num_options; i++) {
      if (i == selected) {
        IOHelpers::print("  \033[1;32m> \033[1;37;44m ");
        IOHelpers::print(options[i].c_str());
        int padding = width - 6 - options[i].length();
        if (padding > 0) {
          for (int p = 0; p < padding; p++) IOHelpers::print(" ");
        }
        IOHelpers::print("\033[0m\n");
      } else {
        IOHelpers::print("    \033[37m");
        IOHelpers::print(options[i].c_str());
        IOHelpers::print("\033[0m\n");
      }
    }
    
    IOHelpers::printNewline();
    IOHelpers::print(ANSI_CYAN);
    print_line(width);
    IOHelpers::print(ANSI_RESET);
    IOHelpers::println("  \033[1;33mUse Arrow Keys (↑/↓) to navigate, Enter to select.\033[0m");
    IOHelpers::print(ANSI_CYAN);
    print_line(width);
    IOHelpers::print(ANSI_RESET);
    
    int key = get_key_press();
    if (key == 1001) { // Up
      selected = (selected - 1 + num_options) % num_options;
    } else if (key == 1002) { // Down
      selected = (selected + 1) % num_options;
    } else if (key == '\n' || key == '\r' || key == ' ') {
      // Restore cursor
      IOHelpers::print("\033[?25h");
      return selected;
    }
  }
}
