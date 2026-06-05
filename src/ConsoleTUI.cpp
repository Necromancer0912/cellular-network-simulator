#include "ConsoleTUI.h"
#include "Utils.h"
#include "IOHelpers.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

// Forward declaration of custom network creation helper from main.cpp
void createCustomNetwork(Simulator &sim);

ConsoleTUI::ConsoleTUI(Simulator &sim) 
  : simulator(sim), analytics(&sim), activeTab(0), selectedTowerIdx(0), devicesScrollOffset(0), quit(false),
    commandMode(false), commandInput(""), commandCursorPos(0), historyIndex(-1), autoSuggestHint(""),
    searchMode(false), searchQuery(""),
    activeModal(ModalType::NONE), activeFieldIdx(0), lastProcessedMessages(0),
    frameCounter(0), startTime(std::chrono::steady_clock::now()), actionsSelectedIdx(0),
    selectedMapType(0), selectedMapId(-1),
    heatmapMode(false), packetFlowMode(true),
    mapCursorMode(false), mapCursorRow(0), mapCursorCol(0) {
      // Pre-fill throughput history
      throughputHistory.assign(50, 0);
    }

ConsoleTUI::~ConsoleTUI() {
  restoreTerminal();
}

int ConsoleTUI::getVisibleLength(const std::string &str) {
  int len = 0;
  bool in_escape = false;
  for (size_t i = 0; i < str.length(); i++) {
    if (str[i] == '\033') {
      in_escape = true;
    } else if (in_escape && str[i] == 'm') {
      in_escape = false;
    } else if (!in_escape) {
      unsigned char c = str[i];
      // UTF-8 continuation bytes are 10xxxxxx (0x80 to 0xBF). Skip them to count UTF-8 characters properly.
      if ((c & 0xC0) != 0x80) {
        len++;
      }
    }
  }
  return len;
}

void ConsoleTUI::getTerminalSize(int &rows, int &cols) {
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 0 && w.ws_col > 0) {
    rows = w.ws_row;
    cols = w.ws_col;
  } else {
    rows = 24;
    cols = 80;
  }
}

void ConsoleTUI::setupTerminal() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  setvbuf(stdin, NULL, _IONBF, 0); // Disable stdio buffering for immediate reading
  std::cout << "\033[?1049h\033[H\033[?25l\033[?1000h\033[?1006h" << std::flush;
}

void ConsoleTUI::restoreTerminal() {
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
  std::cout << "\033[?1049l\033[?25h\033[?1000l\033[?1006l" << std::flush;
}

void ConsoleTUI::printAt(int row, int col, const std::string &text, const std::string &color) {
  std::cout << "\033[" << (row + 1) << ";" << (col + 1) << "H";
  if (!color.empty()) {
    std::cout << color;
  }
  std::cout << text;
  if (!color.empty()) {
    std::cout << TerminalColor::RESET;
  }
}

// Helper: create a string of 'n' repetitions of a single-byte char
static std::string repeatChar(char c, int n) {
  if (n <= 0) return "";
  return std::string(n, c);
}

// Helper: create a string of 'n' repetitions of a multi-byte string (like "─")
static std::string repeatStr(const std::string &s, int n) {
  std::string result;
  result.reserve(s.size() * n);
  for (int i = 0; i < n; i++) result += s;
  return result;
}

// Helper: truncate a potentially ANSI-colored string to maxVis visible characters
std::string ConsoleTUI_truncate(const std::string &str, int maxVis) {
  if (maxVis <= 0) return "";
  std::string result;
  int vis = 0;
  bool in_esc = false;
  for (size_t i = 0; i < str.size(); i++) {
    unsigned char c = str[i];
    if (c == '\033') {
      in_esc = true;
      result += c;
    } else if (in_esc && c == 'm') {
      in_esc = false;
      result += c;
    } else if (in_esc) {
      result += c;
    } else {
      // Visible character
      if ((c & 0xC0) != 0x80) {
        // Start of a new character
        if (vis >= maxVis) break;
        vis++;
      }
      result += c;
    }
  }
  return result;
}

std::string ConsoleTUI::getUptime() const {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
  int hrs = elapsed / 3600;
  int mins = (elapsed % 3600) / 60;
  int secs = elapsed % 60;
  std::stringstream ss;
  if (hrs > 0) ss << hrs << "h ";
  ss << mins << "m " << secs << "s";
  return ss.str();
}

void ConsoleTUI::addToast(const std::string &msg, const std::string &color) {
  toasts.push_back({msg, color, 15}); // show for ~15 frames (~4.5s at 300ms)
  if (toasts.size() > 5) {
    toasts.erase(toasts.begin());
  }
}

void ConsoleTUI::drawToasts(int rows, int cols) {
  (void)rows; // suppress unused warning
  int startRow = 2;
  int maxW = 50;
  for (size_t i = 0; i < toasts.size(); i++) {
    auto &t = toasts[i];
    if (t.ttl <= 0) continue;
    std::string msg = t.message;
    if ((int)msg.length() > maxW - 4) msg = msg.substr(0, maxW - 7) + "...";
    int boxW = (int)msg.length() + 4;
    int startCol = cols - boxW - 1;
    // Draw a small toast box
    printAt(startRow + (int)i * 3, startCol, "╭" + repeatStr("─", boxW - 2) + "╮", t.color);
    printAt(startRow + (int)i * 3 + 1, startCol, "│ " + msg + repeatChar(' ', boxW - (int)msg.length() - 4) + " │", t.color);
    printAt(startRow + (int)i * 3 + 2, startCol, "╰" + repeatStr("─", boxW - 2) + "╯", t.color);
    t.ttl--;
  }
  // Remove expired toasts
  toasts.erase(std::remove_if(toasts.begin(), toasts.end(), [](const ToastNotification &t) { return t.ttl <= 0; }), toasts.end());
}

void ConsoleTUI::drawHorizontalBar(int row, int col, int width, double value, double maxVal, const std::string &color) {
  if (maxVal <= 0) maxVal = 1;
  int filled = (int)((value / maxVal) * width);
  if (filled > width) filled = width;
  if (filled < 0) filled = 0;
  std::string bar = "";
  for (int i = 0; i < filled; i++) bar += "█";
  for (int i = filled; i < width; i++) bar += "░";
  printAt(row, col, bar, color);
}

void ConsoleTUI::drawBox(int start_row, int start_col, int height, int width, const std::string &title) {
  if (width < 2 || height < 2) return;
  int inner = width - 2;

  // Top border
  std::string top = "╭";
  if (!title.empty()) {
    std::string t = " " + title + " ";
    int tlen = (int)t.length(); // title is ASCII, no ANSI
    if (tlen > inner - 2) tlen = inner - 2;
    t = t.substr(0, tlen);
    top += repeatStr("─", 1) + t + repeatStr("─", std::max(0, inner - 1 - tlen));
  } else {
    top += repeatStr("─", inner);
  }
  top += "╮";
  printAt(start_row, start_col, top, TerminalColor::CYAN);

  // Side borders + clear inside
  for (int r = 1; r < height - 1; r++) {
    printAt(start_row + r, start_col, "│", TerminalColor::CYAN);
    printAt(start_row + r, start_col + 1, repeatChar(' ', inner));
    printAt(start_row + r, start_col + width - 1, "│", TerminalColor::CYAN);
  }

  // Bottom border
  std::string bottom = "╰" + repeatStr("─", inner) + "╯";
  printAt(start_row + height - 1, start_col, bottom, TerminalColor::CYAN);
}

void ConsoleTUI::drawCard(int row, int col, int height, int width, const std::string &title, const std::vector<std::pair<std::string, std::string>> &fields, const std::string &title_color) {
  if (width < 4 || height < 3) return;
  int inner = width - 2;

  // Top border with title
  std::string tc = title_color.empty() ? TerminalColor::CYAN : title_color;
  std::string top = "╭";
  if (!title.empty()) {
    int title_vis = (int)title.length(); // titles are plain ASCII
    int max_title = inner - 4;
    std::string t = title;
    if (title_vis > max_title) t = title.substr(0, max_title);
    top += "─ " + tc + t + TerminalColor::CYAN + " ─";
    int used = (int)t.length() + 4; // " " + title + " " + 2x"─"
    int fill = inner - used;
    if (fill > 0) top += repeatStr("─", fill);
  } else {
    top += repeatStr("─", inner);
  }
  top += "╮";
  printAt(row, col, top, TerminalColor::CYAN);

  // Side borders + clear inside, then fill content
  for (int r = 1; r < height - 1; r++) {
    printAt(row + r, col, "│", TerminalColor::CYAN);
    printAt(row + r, col + 1, repeatChar(' ', inner));
    printAt(row + r, col + width - 1, "│", TerminalColor::CYAN);
  }

  // Bottom border
  std::string bottom = "╰" + repeatStr("─", inner) + "╯";
  printAt(row + height - 1, col, bottom, TerminalColor::CYAN);

  // Print fields inside the box
  int max_lines = height - 2;
  for (int i = 0; i < max_lines && i < (int)fields.size(); i++) {
    std::string label = fields[i].first;
    std::string val = fields[i].second;
    std::string line = " " + label;
    if (!val.empty()) {
      line += " " + val;
    }

    // Truncate if needed
    int vis_len = getVisibleLength(line);
    if (vis_len > inner) {
      line = ConsoleTUI_truncate(line, inner - 2) + "..";
      vis_len = getVisibleLength(line);
    }

    // The interior is already cleared with spaces, just print the content
    printAt(row + 1 + i, col + 1, line, TerminalColor::WHITE);
  }
}

void ConsoleTUI::drawHeader(int cols) {
  // Build tab labels
  std::vector<std::string> tabNames = {
    "Dashboard", "Towers", "Devices", "Analytics", "Visual Map", "Actions", "Help"
  };

  std::string title = "⚡ CELLULAR SIMULATOR";
  int titleLen = 21; // visible length of title

  // Calculate how much space tabs need
  int tabsVisLen = 0;
  for (size_t i = 0; i < tabNames.size(); i++) {
    // " [N] Name " = 6 + name.length()
    tabsVisLen += (int)tabNames[i].length() + 6;
  }
  // Add spaces between tabs (1 space per gap)
  tabsVisLen += (int)tabNames.size() - 1;

  // Base background color escape sequence: dark gray bg
  std::string bg = "\033[48;5;236m";
  std::string hdr = bg + " " + TerminalColor::BOLD_YELLOW + title + bg + " ";

  int fillLen = cols - 2 - titleLen - tabsVisLen;
  if (fillLen < 1) fillLen = 1;
  hdr += repeatChar(' ', fillLen);

  // Add tabs
  for (size_t i = 0; i < tabNames.size(); i++) {
    if (i > 0) hdr += " ";
    if ((int)i == activeTab) {
      hdr += "\033[44m\033[1;37m [" + std::to_string(i + 1) + "] " + tabNames[i] + " " + bg;
    } else {
      hdr += "\033[38;5;250m [" + std::to_string(i + 1) + "] " + tabNames[i] + " ";
    }
  }

  // Pad remaining to full cols to make sure background spans the whole width
  int currentVis = titleLen + 2 + fillLen + tabsVisLen;
  if (currentVis < cols) {
    hdr += repeatChar(' ', cols - currentVis);
  }
  hdr += TerminalColor::RESET;
  printAt(0, 0, hdr);
}

void ConsoleTUI::drawFooter(int cols) {
  int rows, c;
  getTerminalSize(rows, c);

  // Animated activity spinner
  std::string spinChars = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏";
  // Each braille char is 3 bytes in UTF-8
  int spinIdx = (frameCounter / 2) % 10;
  std::string spinner = spinChars.substr(spinIdx * 3, 3);

  std::string uptime = getUptime();

  // Bottom status bar (floating solid bar)
  std::string content = " " + spinner + " [:] Console │ [1-7] Tabs │ [Q] Quit │ Towers: " + 
                        std::to_string(simulator.get_tower_count()) + " │ Devices: " + 
                        std::to_string(simulator.get_device_count()) + 
                        " │ Uptime: " + uptime + " ";
  if (searchMode) {
    content += "│ Filter: \"" + searchQuery + "\" ";
  }
  int vis_len = getVisibleLength(content);

  std::string bg = "\033[48;5;238m";
  std::string bottom = bg + TerminalColor::BOLD_WHITE + content;
  int fill = cols - vis_len;
  if (fill > 0) bottom += repeatChar(' ', fill);
  bottom += TerminalColor::RESET;

  printAt(rows - 1, 0, bottom);
}

void clearContentArea(int rows, int cols) {
  (void)cols; // suppress unused warning
  std::cout << "\033[0m";
  for (int r = 0; r < rows; r++) {
    std::cout << "\033[" << (r + 1) << ";1H\033[K";
  }
}

void ConsoleTUI::drawDashboard(int rows, int cols) {
  int contentStart = 1;  // row after header
  int contentEnd = rows - 4; // row before command console

  // Layout: top cards, throughput chart, event logs
  int cardHeight = 5;
  int sparkHeight = 7;
  int cardCount = 4;
  int cardWidth = (cols - 3) / cardCount;
  if (cardWidth < 15) cardWidth = 15;

  auto metrics = analytics.calculate_current_metrics();

  // Card 1: HEALTH
  std::string health_status;
  std::string health_color;
  if (metrics.average_utilization < 50.0) {
    health_status = "OPTIMAL";
    health_color = TerminalColor::BOLD_GREEN;
  } else if (metrics.average_utilization < 75.0) {
    health_status = "MODERATE";
    health_color = TerminalColor::BOLD_YELLOW;
  } else {
    health_status = "CRITICAL";
    health_color = TerminalColor::BOLD_RED;
  }
  std::stringstream ss_util;
  ss_util << std::fixed << std::setprecision(1) << metrics.average_utilization << "% Util";
  int barW = cardWidth - 12;
  if (barW < 5) barW = 5;
  std::string health_bar = OutputFormatter::get_progress_bar(metrics.average_utilization / 100.0, barW);

  drawCard(contentStart, 0, cardHeight, cardWidth, "HEALTH", {
    {"Status:", health_color + health_status + TerminalColor::RESET},
    {"Load:  ", ss_util.str()},
    {"Bar:   ", health_bar}
  }, health_color);

  // Card 2: CONNECTIONS
  drawCard(contentStart, cardWidth + 1, cardHeight, cardWidth, "CONNECTIONS", {
    {"Devices:  ", std::to_string(simulator.get_device_count())},
    {"Connected:", std::to_string(metrics.connected_devices) + " " + TerminalColor::BOLD_GREEN + "●" + TerminalColor::RESET},
    {"Failed:   ", std::to_string(metrics.failed_connections) + " " + TerminalColor::BOLD_RED + "○" + TerminalColor::RESET}
  }, TerminalColor::BOLD_BLUE);

  // Card 3: TRAFFIC LOAD
  std::stringstream ss_rate;
  ss_rate << std::fixed << std::setprecision(2) << metrics.success_rate << "%";
  std::string rate_bar = OutputFormatter::get_progress_bar(metrics.success_rate / 100.0, barW);
  drawCard(contentStart, 2 * cardWidth + 2, cardHeight, cardWidth, "TRAFFIC LOAD", {
    {"Total Msg:", std::to_string(simulator.get_total_messages())},
    {"Success:  ", ss_rate.str()},
    {"Rate:     ", rate_bar}
  }, TerminalColor::BOLD_CYAN);

  // Card 4: RESOURCES
  int total_cores = 0;
  for (int i = 0; i < simulator.get_tower_count(); i++) {
    total_cores += simulator.get_tower(i)->get_number_of_cores();
  }
  int card4W = cols - (3 * cardWidth + 3); // fill remaining
  if (card4W < 10) card4W = cardWidth;
  drawCard(contentStart, 3 * cardWidth + 3, cardHeight, card4W, "RESOURCES", {
    {"Towers: ", std::to_string(simulator.get_tower_count())},
    {"Cores:  ", std::to_string(total_cores)},
    {"Threads:", "Active"}
  }, TerminalColor::BOLD_MAGENTA);

  // Real-time Traffic Throughput Box
  int sparkRow = contentStart + cardHeight;
  drawBox(sparkRow, 0, sparkHeight, cols, "REAL-TIME TRAFFIC THROUGHPUT");

  int max_val = 1;
  for (int val : throughputHistory) {
    if (val > max_val) max_val = val;
  }

  printAt(sparkRow + 1, 2, "Max: " + std::to_string(max_val) + " msg | Current: " + std::to_string(throughputHistory.back()) + " msg", TerminalColor::BOLD_YELLOW);

  std::vector<std::string> blocks = {" ", " ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
  int graph_h = 4;
  std::vector<std::string> graph_lines(graph_h, "");
  int available_w = cols - 4;

  std::vector<int> visibleHistory = throughputHistory;
  if ((int)visibleHistory.size() > available_w) {
    visibleHistory.erase(visibleHistory.begin(), visibleHistory.begin() + (visibleHistory.size() - available_w));
  } else if ((int)visibleHistory.size() < available_w) {
    visibleHistory.insert(visibleHistory.begin(), available_w - visibleHistory.size(), 0);
  }

  for (int r = 0; r < graph_h; r++) {
    for (int val : visibleHistory) {
      int total_eighths = (val * graph_h * 8) / max_val;
      int row_eighths = total_eighths - (graph_h - 1 - r) * 8;
      if (row_eighths <= 0) {
        graph_lines[r] += " ";
      } else if (row_eighths >= 8) {
        graph_lines[r] += "█";
      } else {
        graph_lines[r] += blocks[row_eighths];
      }
    }
  }

  // Draw 4-row high graph with vertical gradient colors
  printAt(sparkRow + 2, 2, graph_lines[0], TerminalColor::BOLD_RED);
  printAt(sparkRow + 3, 2, graph_lines[1], TerminalColor::BOLD_YELLOW);
  printAt(sparkRow + 4, 2, graph_lines[2], TerminalColor::BOLD_GREEN);
  printAt(sparkRow + 5, 2, graph_lines[3], TerminalColor::BOLD_CYAN);

  // System Event Logs Box
  int logBoxRow = sparkRow + sparkHeight;
  int logBoxHeight = contentEnd - logBoxRow;
  if (logBoxHeight < 3) logBoxHeight = 3;
  drawBox(logBoxRow, 0, logBoxHeight, cols, "SYSTEM EVENT LOGS");

  int logs_visible = logBoxHeight - 2;
  const auto &logs = Logger::get_logs();
  int total_logs = logs.size();
  int start_log_idx = std::max(0, total_logs - logs_visible);
  int maxLogW = cols - 4;

  for (int i = 0; i < logs_visible; i++) {
    int logIdx = start_log_idx + i;
    std::string logLine = "";
    if (logIdx < total_logs) {
      logLine = logs[logIdx];
      // Strip ANSI for length check, but keep for display
      int logVis = getVisibleLength(logLine);
      if (logVis > maxLogW) {
        logLine = ConsoleTUI_truncate(logLine, maxLogW - 3) + "...";
      }
    }
    // Pad the rest with spaces to clear old content
    int logVis = getVisibleLength(logLine);
    int pad = maxLogW - logVis;
    if (pad < 0) pad = 0;
    printAt(logBoxRow + 1 + i, 2, logLine + repeatChar(' ', pad));
  }
}

void ConsoleTUI::drawTowersTab(int rows, int cols) {
  int contentStart = 1;
  int contentEnd = rows - 4;
  int boxHeight = contentEnd - contentStart;
  if (boxHeight < 5) boxHeight = 5;

  int listWidth = 30;
  if (listWidth > cols / 3) listWidth = cols / 3;
  int detailsWidth = cols - (listWidth + 1);

  drawBox(contentStart, 0, boxHeight, listWidth, "CELL TOWERS");
  drawBox(contentStart, listWidth + 1, boxHeight, detailsWidth, "TOWER DETAILS");

  int towerCount = simulator.get_tower_count();
  if (towerCount == 0) {
    printAt(contentStart + 2, 2, "No towers.", TerminalColor::BOLD_RED);
    printAt(contentStart + 2, listWidth + 3, "Deploy a tower via [Actions] tab or command console.", TerminalColor::WHITE);
    return;
  }

  if (selectedTowerIdx >= towerCount) selectedTowerIdx = towerCount - 1;
  if (selectedTowerIdx < 0) selectedTowerIdx = 0;

  int visible_towers = boxHeight - 2;
  int start_tower = std::max(0, selectedTowerIdx - visible_towers / 2);
  int maxListTextW = listWidth - 2; // interior width of list box

  for (int i = 0; i < visible_towers && (start_tower + i) < towerCount; i++) {
    int idx = start_tower + i;
    auto tower = simulator.get_tower(idx);
    if (!tower) continue;

    std::string text = "Tower #" + std::to_string(idx) + " [" + tower->get_location() + "]";
    if ((int)text.length() > maxListTextW - 2) {
      text = text.substr(0, maxListTextW - 5) + "...";
    }
    int pad = maxListTextW - 2 - (int)text.length();
    if (pad < 0) pad = 0;
    std::string full_line = " " + text + repeatChar(' ', pad) + " ";

    std::string color = (idx == selectedTowerIdx) ? (TerminalColor::BOLD_WHITE + "\033[44m") : (TerminalColor::WHITE);
    printAt(contentStart + 1 + i, 1, full_line, color);
  }

  // Tower details on the right
  auto tower = simulator.get_tower(selectedTowerIdx);
  if (!tower) return;

  int dc = listWidth + 3; // detail column start
  int dr = contentStart + 1;   // detail row start
  int maxDetailW = detailsWidth - 4;

  auto printDetail = [&](const std::string &label, const std::string &value, const std::string &color = TerminalColor::WHITE) {
    std::string line = label + value;
    int vis = getVisibleLength(line);
    int pad = maxDetailW - vis;
    if (pad < 0) pad = 0;
    printAt(dr++, dc, line + repeatChar(' ', pad), color);
  };

  int total_tower_msg = 0;
  for (auto core : tower->get_cores()) {
    if (core) total_tower_msg += core->get_total_messages_generated();
  }

  printDetail("ID:          ", "Tower #" + std::to_string(selectedTowerIdx), TerminalColor::BOLD_CYAN);
  printDetail("Location:    ", tower->get_location() + " (" + std::to_string((int)tower->get_x()) + ", " + std::to_string((int)tower->get_y()) + ")");
  printDetail("Protocol:    ", tower->get_protocol()->get_protocol_name(), TerminalColor::BOLD_YELLOW);
  printDetail("Bandwidth:   ", std::to_string((int)tower->get_total_bandwidth()) + " MHz");

  double util = tower->get_utilization();
  printDetail("Utilization: ", std::to_string((int)util) + "%");

  int progW = std::min(25, maxDetailW - 2);
  if (progW < 5) progW = 5;
  std::string progBar = OutputFormatter::get_progress_bar(util / 100.0, progW);
  int progVis = getVisibleLength(progBar);
  printAt(dr, dc, "             " + progBar + repeatChar(' ', std::max(0, maxDetailW - 13 - progVis)));
  dr++;
  dr++;

  bool isBeam = tower->get_beamforming_factor() > 1.0;
  std::stringstream ss_bf;
  ss_bf << tower->get_beamforming_factor();
  std::string beamStr = isBeam ? (std::string(TerminalColor::BOLD_GREEN) + "ACTIVE (" + ss_bf.str() + "x)" + TerminalColor::RESET) : "INACTIVE";
  printDetail("Beamforming: ", beamStr);
  printDetail("Channels:    ", std::to_string(tower->get_total_channels()));
  printDetail("Devices:     ", std::to_string(tower->get_current_device_count()) + " / " + std::to_string(tower->get_max_supported_devices()));
  printDetail("Total Msg:   ", std::to_string(total_tower_msg) + " processed", TerminalColor::BOLD_CYAN);
  dr++;
  printAt(dr++, dc, "[B] Toggle beamforming on this tower", TerminalColor::BOLD_YELLOW);
  dr++;

  printAt(dr++, dc, "CPU Core Clusters:", TerminalColor::BOLD_MAGENTA);
  const auto &cores = tower->get_cores();
  for (size_t ci = 0; ci < cores.size() && dr < (contentEnd - 1); ci++) {
    auto core = cores[ci];
    if (core) {
      double core_util = core->get_utilization();
      int cBarW = std::min(15, maxDetailW - 35);
      if (cBarW < 5) cBarW = 5;
      std::string core_bar = OutputFormatter::get_progress_bar(core_util / 100.0, cBarW);
      std::stringstream core_ss;
      core_ss << "  Core " << std::setw(2) << core->get_core_id() << " " << core_bar
              << " " << core->get_current_device_count() << "/" << core->get_max_devices() << " Dev"
              << " | " << core->get_total_messages_generated() << " Msg";
      std::string cLine = core_ss.str();
      int cVis = getVisibleLength(cLine);
      int cPad = maxDetailW - cVis;
      if (cPad < 0) cPad = 0;
      printAt(dr++, dc, cLine + repeatChar(' ', cPad), TerminalColor::WHITE);
    }
  }
}

void ConsoleTUI::drawDevicesTab(int rows, int cols) {
  int contentStart = 1;
  int contentEnd = rows - 4;
  int boxHeight = contentEnd - contentStart;
  if (boxHeight < 5) boxHeight = 5;

  drawBox(contentStart, 0, boxHeight, cols, "REGISTERED DEVICES");

  // Filter devices based on searchQuery
  std::vector<int> filteredIndices;
  int totalDevices = simulator.get_device_count();
  for (int i = 0; i < totalDevices; i++) {
    auto device = simulator.get_device(i);
    if (!device) continue;

    bool match = false;
    if (searchQuery.empty()) {
      match = true;
    } else {
      std::string query = searchQuery;
      std::transform(query.begin(), query.end(), query.begin(), ::tolower);

      std::string name = device->get_device_name();
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);

      std::string gen = device->get_device_type();
      std::transform(gen.begin(), gen.end(), gen.begin(), ::tolower);

      std::string ct = device->get_communication_type_string();
      std::transform(ct.begin(), ct.end(), ct.begin(), ::tolower);

      std::string idStr = std::to_string(device->get_device_id());

      if (name.find(query) != std::string::npos ||
          gen.find(query) != std::string::npos ||
          ct.find(query) != std::string::npos ||
          idStr.find(query) != std::string::npos) {
        match = true;
      }
    }
    if (match) {
      filteredIndices.push_back(i);
    }
  }

  int deviceCount = filteredIndices.size();

  // Search box at contentStart + 1
  std::string searchPrompt;
  std::string searchColor;
  if (searchMode) {
    searchPrompt = "Search: [" + searchQuery + "_] ";
    searchColor = TerminalColor::BOLD_YELLOW;
  } else {
    searchPrompt = "Search: [" + (searchQuery.empty() ? "Press '/' to filter devices" : searchQuery) + "] ";
    searchColor = searchQuery.empty() ? "\033[90m" : TerminalColor::BOLD_CYAN;
  }
  printAt(contentStart + 1, 2, searchPrompt, searchColor);

  if (deviceCount == 0) {
    printAt(contentStart + 3, 2, "No devices match query.", TerminalColor::BOLD_RED);
    return;
  }

  // Table headers and widths - fit within cols - 4
  int tableW = cols - 4;
  std::vector<std::string> headers = {"ID", "Device Name", "Gen", "Comm Type", "Msgs", "Status", "Tower"};
  std::vector<int> widths = {5, 18, 4, 14, 8, 14, 14};
  
  // Adjust widths to fit
  int totalW = 0;
  for (int w : widths) totalW += w + 1; // +1 for gap
  if (totalW > tableW) {
    // Scale down proportionally
    double scale = (double)(tableW - (int)widths.size()) / (totalW - (int)widths.size());
    for (auto &w : widths) {
      w = std::max(3, (int)(w * scale));
    }
  }

  int headerRow = contentStart + 3;
  int startCol = 2;

  // Print headers
  int curCol = startCol;
  for (size_t i = 0; i < headers.size(); i++) {
    std::string h = headers[i];
    if ((int)h.length() > widths[i]) h = h.substr(0, widths[i]);
    int hpad = widths[i] - (int)h.length();
    printAt(headerRow, curCol, h + repeatChar(' ', hpad), TerminalColor::BOLD_CYAN);
    curCol += widths[i] + 1;
  }

  // Separator
  curCol = startCol;
  std::string sep = "";
  for (size_t i = 0; i < widths.size(); i++) {
    sep += repeatStr("─", widths[i]);
    if (i < widths.size() - 1) sep += "┬";
  }
  printAt(headerRow + 1, startCol, sep, TerminalColor::CYAN);

  // Device rows
  int visible_rows = boxHeight - 6;
  if (devicesScrollOffset < 0) devicesScrollOffset = 0;
  if (devicesScrollOffset > deviceCount - visible_rows) devicesScrollOffset = deviceCount - visible_rows;
  if (devicesScrollOffset < 0) devicesScrollOffset = 0;

  for (int i = 0; i < visible_rows && (devicesScrollOffset + i) < deviceCount; i++) {
    int idx = filteredIndices[devicesScrollOffset + i];
    auto device = simulator.get_device(idx);
    if (!device) continue;

    std::string towerStr = "None";
    for (int t = 0; t < simulator.get_tower_count(); t++) {
      auto tw = simulator.get_tower(t);
      if (tw && tw->get_device(device->get_device_id()) != nullptr) {
        towerStr = "Tower #" + std::to_string(t);
        break;
      }
    }

    bool is_connected = device->get_connection_status();
    std::string status_str = is_connected ? "● Connected" : "○ Disconn";
    std::string status_color = is_connected ? TerminalColor::BOLD_GREEN : TerminalColor::RED;

    std::vector<std::string> cells = {
      std::to_string(device->get_device_id()),
      device->get_device_name(),
      device->get_device_type(),
      device->get_communication_type_string(),
      std::to_string(device->get_message_count()),
      status_str,
      towerStr
    };

    curCol = startCol;
    for (size_t c = 0; c < cells.size(); c++) {
      std::string cell = cells[c];
      // For status column, we need to account for the ● character
      int cellVis = getVisibleLength(cell);
      if (cellVis > widths[c]) {
        cell = cell.substr(0, std::max(0, widths[c] - 2)) + "..";
        cellVis = getVisibleLength(cell);
      }
      int cpad = widths[c] - cellVis;
      if (cpad < 0) cpad = 0;
      std::string c_color = (c == 5) ? status_color : TerminalColor::WHITE;
      printAt(headerRow + 2 + i, curCol, cell + repeatChar(' ', cpad), c_color);
      curCol += widths[c] + 1;
    }
  }

  // Scroll indicator at bottom
  std::string scroll_msg = "[Up/Down to scroll | " + 
                           std::to_string(devicesScrollOffset + 1) + "-" + 
                           std::to_string(std::min(deviceCount, devicesScrollOffset + visible_rows)) + 
                           " of " + std::to_string(deviceCount) + "]";
  int smCol = (cols - (int)scroll_msg.length()) / 2;
  printAt(contentEnd - 1, smCol, scroll_msg, TerminalColor::BOLD_YELLOW);
}

void ConsoleTUI::drawAnalyticsTab(int rows, int cols) {
  int contentStart = 1;
  int contentEnd = rows - 4;
  int boxHeight = contentEnd - contentStart;
  if (boxHeight < 5) boxHeight = 5;

  // Three panels: Left (Generation comparison), Right-Top (Tower Health), Right-Bottom (Network Summary)
  int leftW = cols / 2;
  int rightW = cols - leftW - 1;
  int rightTopH = boxHeight * 2 / 3;
  if (rightTopH < 5) rightTopH = 5;
  int rightBottomH = boxHeight - rightTopH;
  if (rightBottomH < 3) rightBottomH = 3;

  drawBox(contentStart, 0, boxHeight, leftW, "GENERATIONS COMPARISON");
  drawBox(contentStart, leftW + 1, rightTopH, rightW, "TOWER HEALTH MONITOR");
  drawBox(contentStart + rightTopH, leftW + 1, rightBottomH, rightW, "NETWORK SUMMARY");

  // LEFT: Generation comparison table + horizontal bar charts
  int lr = contentStart + 2;
  int lc = 2;
  int leftInner = leftW - 4;

  printAt(lr++, lc, "Normalized to 10 MHz carrier bandwidth:", TerminalColor::BOLD_CYAN);
  lr++;

  // Table headers
  std::string hdrLine = "Gen   Max Users   Msgs/User  Efficiency   Capacity";
  printAt(lr++, lc, hdrLine, TerminalColor::BOLD_CYAN);
  printAt(lr++, lc, repeatStr("─", std::min(leftInner, (int)hdrLine.length())), TerminalColor::CYAN);

  std::vector<std::string> generations = {"2G", "3G", "4G", "5G", "6G", "7G"};
  double bandwidth = 10.0;
  double maxCapacity = 0;
  
  // Pre-calculate max capacity for bar scaling
  struct GenData { std::string gen; int max_users; int msgs; double efficiency; };
  std::vector<GenData> genData;
  for (const auto &gen : generations) {
    auto protocol = simulator.get_protocol(gen);
    if (protocol) {
      int max_users = protocol->calculate_max_users(bandwidth);
      int msgs = protocol->calculate_messages_per_user();
      int req_cores = protocol->calculate_required_cores(max_users, msgs);
      double efficiency = req_cores > 0 ? static_cast<double>(max_users) / req_cores : 0.0;
      genData.push_back({gen, max_users, msgs, efficiency});
      if (max_users > maxCapacity) maxCapacity = max_users;
    }
  }

  int barW = std::min(20, leftInner - 45);
  if (barW < 5) barW = 5;

  std::vector<std::string> genColors = {
    "\033[38;5;196m", // 2G - red
    "\033[38;5;208m", // 3G - orange
    "\033[38;5;220m", // 4G - yellow
    TerminalColor::BOLD_GREEN, // 5G - green
    TerminalColor::BOLD_CYAN,  // 6G - cyan
    TerminalColor::BOLD_MAGENTA // 7G - magenta
  };

  for (size_t i = 0; i < genData.size() && lr < contentEnd - 2; i++) {
    auto &gd = genData[i];
    std::stringstream ss;
    ss << std::left << std::setw(4) << gd.gen
       << "  " << std::right << std::setw(9) << gd.max_users
       << "  " << std::setw(9) << gd.msgs
       << "  " << std::setw(7) << (int)gd.efficiency << "/core  ";
    printAt(lr, lc, ss.str(), TerminalColor::WHITE);
    
    std::string barColor = (i < genColors.size()) ? genColors[i] : TerminalColor::WHITE;
    drawHorizontalBar(lr, lc + 42, barW, gd.max_users, maxCapacity, barColor);
    lr++;
  }

  // Visual separator and generation evolution summary
  lr += 2;
  if (lr < contentEnd - 4) {
    printAt(lr++, lc, "GENERATION EVOLUTION", TerminalColor::BOLD_MAGENTA);
    printAt(lr++, lc, repeatStr("─", std::min(leftInner, 40)), TerminalColor::CYAN);
    if (genData.size() >= 2) {
      double ratio = (double)genData.back().max_users / std::max(1, genData.front().max_users);
      std::stringstream ss;
      ss << std::fixed << std::setprecision(0);
      ss << "Capacity growth " << genData.front().gen << " -> " << genData.back().gen << ": ";
      ss << ratio << "x improvement";
      printAt(lr++, lc, ss.str(), TerminalColor::BOLD_YELLOW);
    }
    if (genData.size() >= 4) {
      double ratio5g = (double)genData[3].max_users / std::max(1, genData[2].max_users);
      std::stringstream ss;
      ss << std::fixed << std::setprecision(1);
      ss << "4G -> 5G leap: " << ratio5g << "x capacity increase";
      printAt(lr++, lc, ss.str(), TerminalColor::WHITE);
    }
  }

  // RIGHT-TOP: Tower Health with utilization bars
  int rr = contentStart + 2;
  int rc = leftW + 3;
  int maxRightW = rightW - 4;

  if (simulator.get_tower_count() == 0) {
    printAt(rr, rc, "No towers deployed.", TerminalColor::BOLD_RED);
  } else {
    std::vector<std::pair<int, double>> towerUtils;
    for (int i = 0; i < simulator.get_tower_count(); i++) {
      auto tw = simulator.get_tower(i);
      if (tw) towerUtils.push_back({i, tw->get_utilization()});
    }
    std::sort(towerUtils.begin(), towerUtils.end(), [](const auto &a, const auto &b) { return a.second > b.second; });

    for (size_t i = 0; i < towerUtils.size() && rr < (contentStart + rightTopH - 2); i++) {
      auto tw = simulator.get_tower(towerUtils[i].first);
      double u = towerUtils[i].second;

      std::string badge;
      std::string badge_color;
      if (u > 85.0) {
        badge = "CRIT";
        badge_color = TerminalColor::BOLD_RED;
      } else if (u > 60.0) {
        badge = "HIGH";
        badge_color = TerminalColor::BOLD_YELLOW;
      } else {
        badge = " OK ";
        badge_color = TerminalColor::BOLD_GREEN;
      }

      // Badge
      printAt(rr, rc, " " + badge + " ", badge_color + "\033[7m");
      
      // Tower info
      std::stringstream ss_u;
      ss_u << " T#" << towerUtils[i].first << " " << tw->get_location();
      std::string info = ss_u.str();
      int infoW = 18;
      if ((int)info.length() > infoW) info = info.substr(0, infoW - 2) + "..";
      int infoPad = infoW - (int)info.length();
      if (infoPad < 0) infoPad = 0;
      printAt(rr, rc + 6, info + repeatChar(' ', infoPad), TerminalColor::WHITE);

      // Utilization bar
      int utilBarW = std::min(15, maxRightW - 30);
      if (utilBarW < 5) utilBarW = 5;
      std::string barColor = u > 85 ? TerminalColor::BOLD_RED : (u > 60 ? TerminalColor::BOLD_YELLOW : TerminalColor::BOLD_GREEN);
      drawHorizontalBar(rr, rc + 25, utilBarW, u, 100.0, barColor);

      // Percentage
      std::stringstream ss_pct;
      ss_pct << " " << std::fixed << std::setprecision(1) << u << "%";
      printAt(rr, rc + 26 + utilBarW, ss_pct.str(), barColor);

      rr += 2;
    }
  }

  // RIGHT-BOTTOM: Network Summary KPIs
  int sr = contentStart + rightTopH + 1;
  int sc = leftW + 3;
  // summaryW not needed currently

  auto metrics = analytics.calculate_current_metrics();

  // KPI cards in a row
  std::stringstream ss_sr;
  ss_sr << std::fixed << std::setprecision(1) << metrics.success_rate << "%";
  std::string sr_color = metrics.success_rate > 95 ? TerminalColor::BOLD_GREEN : 
                         (metrics.success_rate > 80 ? TerminalColor::BOLD_YELLOW : TerminalColor::BOLD_RED);

  printAt(sr, sc, "Success Rate: ", TerminalColor::WHITE);
  printAt(sr, sc + 14, ss_sr.str(), sr_color);

  std::stringstream ss_util;
  ss_util << std::fixed << std::setprecision(1) << metrics.average_utilization << "%";
  printAt(sr, sc + 26, "Avg Load: ", TerminalColor::WHITE);
  printAt(sr, sc + 36, ss_util.str(), TerminalColor::BOLD_CYAN);

  printAt(sr, sc + 48, "Msgs: ", TerminalColor::WHITE);
  printAt(sr, sc + 54, std::to_string(metrics.total_messages), TerminalColor::BOLD_YELLOW);

  if (sr + 1 < contentEnd - 1) {
    std::stringstream ss_mpc;
    ss_mpc << std::fixed << std::setprecision(1) << metrics.messages_per_core;
    printAt(sr + 1, sc, "Msgs/Core: ", TerminalColor::WHITE);
    printAt(sr + 1, sc + 11, ss_mpc.str(), TerminalColor::BOLD_CYAN);
    
    printAt(sr + 1, sc + 26, "Connected: ", TerminalColor::WHITE);
    printAt(sr + 1, sc + 37, std::to_string(metrics.connected_devices) + "/" + std::to_string(metrics.total_devices), TerminalColor::BOLD_GREEN);

    printAt(sr + 1, sc + 48, "Failed: ", TerminalColor::WHITE);
    printAt(sr + 1, sc + 56, std::to_string(metrics.failed_connections), metrics.failed_connections > 0 ? TerminalColor::BOLD_RED : TerminalColor::BOLD_GREEN);
  }
}

void ConsoleTUI::computeGridCoordinates(int mapH, int mapW) {
  int dev_count = simulator.get_device_count();
  int tower_count = simulator.get_tower_count();
  
  towerGridCoords.assign(tower_count, {-1, -1});
  deviceGridCoords.assign(dev_count, {-1, -1});
  
  std::vector<std::vector<bool>> occupied(mapH, std::vector<bool>(mapW, false));

  // Helper: map coordinate to grid
  auto getGridCoords = [&](double x, double y, int &r, int &c) {
    r = (int)((y * mapH) / 1000.0);
    c = (int)((x * mapW) / 1000.0);
    if (r < 0) r = 0;
    if (r >= mapH) r = mapH - 1;
    if (c < 0) c = 0;
    if (c >= mapW) c = mapW - 1;
  };

  // 1. Core Router occupied area (center of the map)
  int core_r = mapH / 2;
  int core_c = mapW / 2;
  for (int dr = -1; dr <= 1; dr++) {
    for (int dc = -1; dc <= 1; dc++) {
      int nr = core_r + dr;
      int nc = core_c + dc;
      if (nr >= 0 && nr < mapH && nc >= 0 && nc < mapW) {
        occupied[nr][nc] = true;
      }
    }
  }

  // 2. Towers
  for (int i = 0; i < tower_count; i++) {
    auto tw = simulator.get_tower(i);
    if (!tw) continue;
    int tr, tc;
    getGridCoords(tw->get_x(), tw->get_y(), tr, tc);
    
    int dist = 0;
    bool placed = false;
    while (!placed && dist < 15) {
      for (int dr = -dist; dr <= dist && !placed; dr++) {
        for (int dc = -dist; dc <= dist && !placed; dc++) {
          if (abs(dr) != dist && abs(dc) != dist) continue;
          int nr = tr + dr;
          int nc = tc + dc;
          if (nr >= 0 && nr < mapH && nc >= 0 && nc < mapW && !occupied[nr][nc]) {
            tr = nr;
            tc = nc;
            placed = true;
          }
        }
      }
      if (dist == 0) dist = 1;
      else dist++;
    }
    
    towerGridCoords[i] = {tr, tc};
    occupied[tr][tc] = true;
  }

  // 3. Buildings
  for (int r = 0; r < mapH; r++) {
    for (int c = 0; c < mapW; c++) {
      int gx = (c * 100) / mapW;
      int gy = (r * 100) / mapH;
      if (simulator.has_obstacle_at(gx, gy)) {
        occupied[r][c] = true;
      }
    }
  }

  // 4. Devices
  for (int i = 0; i < dev_count; i++) {
    auto dev = simulator.get_device(i);
    if (!dev) continue;
    int dr, dc;
    getGridCoords(dev->get_x(), dev->get_y(), dr, dc);
    
    int dist = 0;
    bool placed = false;
    while (!placed && dist < 30) {
      for (int dr_offset = -dist; dr_offset <= dist && !placed; dr_offset++) {
        for (int dc_offset = -dist; dc_offset <= dist && !placed; dc_offset++) {
          if (abs(dr_offset) != dist && abs(dc_offset) != dist) continue;
          int nr = dr + dr_offset;
          int nc = dc + dc_offset;
          if (nr >= 0 && nr < mapH && nc >= 0 && nc < mapW && !occupied[nr][nc]) {
            dr = nr;
            dc = nc;
            placed = true;
          }
        }
      }
      if (dist == 0) dist = 1;
      else dist++;
    }
    
    deviceGridCoords[i] = {dr, dc};
    occupied[dr][dc] = true;
  }
}

void ConsoleTUI::drawMapTab(int rows, int cols) {
  int contentStart = 1;
  int contentEnd = rows - 4;
  int boxHeight = contentEnd - contentStart;
  if (boxHeight < 5) boxHeight = 5;

  int mapW_box = cols * 2 / 3;
  int inspectW_box = cols - mapW_box - 1;

  drawBox(contentStart, 0, boxHeight, mapW_box, "2D SPATIAL TOPOLOGY MAP");
  drawBox(contentStart, mapW_box + 1, boxHeight, inspectW_box, "MAP INSPECTOR");

  int mapH = boxHeight - 3; // -2 for borders, -1 for legend at the bottom
  int mapW = mapW_box - 4;  // -2 for borders, -2 for margin
  if (mapH < 3 || mapW < 5) return;

  // Legend and instructions
  std::string legend;
  if (mapCursorMode) {
    legend = "Move: Arrows/WASD | [Enter/Space]: Toggle Bldg | [C] Exit Cursor Mode";
  } else {
    legend = "▲=Tower  ▣=Core  ●=Connected  x=Disconnected  █=Bldg  [C] Keyboard Mode";
  }
  int legendCol = (mapW_box - (int)legend.length()) / 2;
  if (legendCol < 2) legendCol = 2;
  printAt(contentStart + boxHeight - 2, legendCol, legend, TerminalColor::BOLD_YELLOW);

  int dev_count = simulator.get_device_count();
  int tower_count = simulator.get_tower_count();

  // Compute resolved grid positions with collision resolution
  computeGridCoordinates(mapH, mapW);

  // Create grid overlay
  struct GridCell {
    std::string ch;
    std::string fg;
    bool has_element;
    int type; // 0: empty/heatmap, 1: tower, 2: device, 3: core, 4: building
    int id;   // element index
  };
  std::vector<std::vector<GridCell>> mapGrid(mapH, std::vector<GridCell>(mapW, {" ", "", false, 0, -1}));

  // 1. Draw Heatmap (if enabled)
  if (heatmapMode && tower_count > 0) {
    for (int r = 0; r < mapH; r++) {
      for (int c = 0; c < mapW; c++) {
        // Map to meters
        double mx = (c * 1000.0) / mapW;
        double my = (r * 1000.0) / mapH;
        
        // Find nearest tower
        int nearestIdx = -1;
        double minDist = 99999.0;
        for (int t = 0; t < tower_count; t++) {
          auto tw = simulator.get_tower(t);
          if (tw) {
            double dx = mx - tw->get_x();
            double dy = my - tw->get_y();
            double d = sqrt(dx*dx + dy*dy);
            if (d < minDist) {
              minDist = d;
              nearestIdx = t;
            }
          }
        }
        
        if (nearestIdx != -1) {
          auto tw = simulator.get_tower(nearestIdx);
          bool has_los = simulator.check_line_of_sight(mx, my, tw->get_x(), tw->get_y());
          double rssi = simulator.compute_rssi(minDist, !has_los);
          
          // Shading and colors based on signal strength
          if (rssi > -65.0) {
            mapGrid[r][c] = {"▓", "\033[38;5;40m", false, 0, -1}; // Strong signal green
          } else if (rssi > -80.0) {
            mapGrid[r][c] = {"▒", "\033[38;5;148m", false, 0, -1}; // Fair signal yellow-green
          } else if (rssi > -95.0) {
            mapGrid[r][c] = {"░", "\033[38;5;214m", false, 0, -1}; // Weak signal orange
          } else {
            mapGrid[r][c] = {".", "\033[38;5;240m", false, 0, -1}; // Very weak signal dark gray
          }
        }
      }
    }
  }

  // 2. Draw Buildings/Obstacles
  for (int r = 0; r < mapH; r++) {
    for (int c = 0; c < mapW; c++) {
      int gx = (c * 100) / mapW;
      int gy = (r * 100) / mapH;
      if (simulator.has_obstacle_at(gx, gy)) {
        mapGrid[r][c] = {"█", "\033[38;5;244m", true, 4, -1}; // Gray block
      }
    }
  }

  // Helper: map coordinate to grid
  auto getGridCoords = [&](double x, double y, int &r, int &c) {
    r = (int)((y * mapH) / 1000.0);
    c = (int)((x * mapW) / 1000.0);
    if (r < 0) r = 0;
    if (r >= mapH) r = mapH - 1;
    if (c < 0) c = 0;
    if (c >= mapW) c = mapW - 1;
  };

  // 3. Draw Core Router (▣) at center of the grid map
  int core_r = mapH / 2;
  int core_c = mapW / 2;
  bool is_core_selected = (selectedMapType == 3);
  std::string core_color = is_core_selected ? "\033[1;37m\033[45m" : "\033[1;35m";
  mapGrid[core_r][core_c] = {"▣", core_color, true, 3, 0};

  // 4. Draw Fiber Backhaul Lines (Towers to Core Router)
  for (int i = 0; i < tower_count; i++) {
    if (i >= (int)towerGridCoords.size()) break;
    int tr = towerGridCoords[i].first;
    int tc = towerGridCoords[i].second;
    if (tr == -1 || tc == -1) continue;

    bool highlight_backhaul = (selectedMapType == 3) || (selectedMapType == 1 && selectedMapId == i);
    std::string line_fg = highlight_backhaul ? "\033[1;35m" : "\033[38;5;238m"; // Magenta / Dim Gray

    int r0 = tr, c0 = tc;
    int r1 = core_r, c1 = core_c;
    int dr_diff = abs(r1 - r0), dc_diff = abs(c1 - c0);
    int sr = r0 < r1 ? 1 : -1, sc = c0 < c1 ? 1 : -1;
    int err = dr_diff - dc_diff;

    while (true) {
      if ((r0 != tr || c0 != tc) && (r0 != core_r || c0 != core_c)) {
        if (mapGrid[r0][c0].type == 0) {
          mapGrid[r0][c0] = {"═", line_fg, false, 0, -1};
        }
      }
      if (r0 == r1 && c0 == c1) break;
      int e2 = 2 * err;
      if (e2 > -dc_diff) { err -= dc_diff; r0 += sr; }
      if (e2 < dr_diff) { err += dr_diff; c0 += sc; }
    }
  }

  // 5. Draw Towers (▲)
  for (int i = 0; i < tower_count; i++) {
    if (i >= (int)towerGridCoords.size()) break;
    int tr = towerGridCoords[i].first;
    int tc = towerGridCoords[i].second;
    if (tr == -1 || tc == -1) continue;

    bool is_selected = (selectedMapType == 1 && selectedMapId == i);
    std::string tw_color = is_selected ? "\033[1;37m\033[46m" : "\033[1;36m"; // Highlighted Cyan / Cyan
    mapGrid[tr][tc] = {"▲", tw_color, true, 1, i};

    // Put tower label nearby if room
    std::string label = "T" + std::to_string(i);
    for (size_t l = 0; l < label.length(); l++) {
      int lc = tc + 1 + l;
      if (lc < mapW && mapGrid[tr][lc].type == 0) {
        mapGrid[tr][lc] = {std::string(1, label[l]), is_selected ? "\033[1;36m" : "\033[38;5;246m", true, 0, -1};
      }
    }
  }

  // 6. Draw Device-to-Tower Signal Paths (only for selected device or selected tower's devices)
  for (int i = 0; i < dev_count; i++) {
    auto dev = simulator.get_device(i);
    if (!dev) continue;
    bool is_conn = dev->get_connection_status();
    if (!is_conn) continue;

    int tw_idx = -1;
    for (int t = 0; t < tower_count; t++) {
      auto tw = simulator.get_tower(t);
      if (tw && tw->get_device(dev->get_device_id()) != nullptr) {
        tw_idx = t;
        break;
      }
    }
    if (tw_idx == -1) continue;

    bool draw_path = false;
    if (selectedMapType == 2 && selectedMapId == i) {
      draw_path = true; // Selected device
    } else if (selectedMapType == 1 && selectedMapId == tw_idx) {
      draw_path = true; // Devices connected to selected tower
    }

    if (draw_path && packetFlowMode) {
      if (i >= (int)deviceGridCoords.size() || tw_idx >= (int)towerGridCoords.size()) continue;
      int dr = deviceGridCoords[i].first;
      int dc = deviceGridCoords[i].second;
      int tr = towerGridCoords[tw_idx].first;
      int tc = towerGridCoords[tw_idx].second;
      if (dr == -1 || dc == -1 || tr == -1 || tc == -1) continue;

      int r0 = dr, c0 = dc;
      int r1 = tr, c1 = tc;
      int dr_diff = abs(r1 - r0), dc_diff = abs(c1 - c0);
      int sr = r0 < r1 ? 1 : -1, sc = c0 < c1 ? 1 : -1;
      int err = dr_diff - dc_diff;

      while (true) {
        if ((r0 != dr || c0 != dc) && (r0 != tr || c0 != tc)) {
          if (mapGrid[r0][c0].type == 0) {
            mapGrid[r0][c0] = {"·", "\033[38;5;236m", false, 0, -1}; // Subtle signal dots
          }
        }
        if (r0 == r1 && c0 == c1) break;
        int e2 = 2 * err;
        if (e2 > -dc_diff) { err -= dc_diff; r0 += sr; }
        if (e2 < dr_diff) { err += dr_diff; c0 += sc; }
      }
    }
  }

  // 7. Draw Live Packets from active packet list
  if (packetFlowMode) {
    const auto &active_pkts = simulator.get_active_packets();
    for (const auto &pkt : active_pkts) {
      int g_r0 = -1, g_c0 = -1, g_r1 = -1, g_c1 = -1;
      
      if (pkt.current_hop == 0) {
        // Dev -> SrcTower
        if (pkt.source_id >= 0 && pkt.source_id < (int)deviceGridCoords.size()) {
          g_r0 = deviceGridCoords[pkt.source_id].first;
          g_c0 = deviceGridCoords[pkt.source_id].second;
        }
        if (pkt.src_tower_idx >= 0 && pkt.src_tower_idx < (int)towerGridCoords.size()) {
          g_r1 = towerGridCoords[pkt.src_tower_idx].first;
          g_c1 = towerGridCoords[pkt.src_tower_idx].second;
        }
      } else if (pkt.current_hop == 1) {
        // SrcTower -> Core Router
        if (pkt.src_tower_idx >= 0 && pkt.src_tower_idx < (int)towerGridCoords.size()) {
          g_r0 = towerGridCoords[pkt.src_tower_idx].first;
          g_c0 = towerGridCoords[pkt.src_tower_idx].second;
        }
        g_r1 = core_r;
        g_c1 = core_c;
      } else if (pkt.current_hop == 2) {
        // Core Router -> DstTower
        g_r0 = core_r;
        g_c0 = core_c;
        if (pkt.dst_tower_idx >= 0 && pkt.dst_tower_idx < (int)towerGridCoords.size()) {
          g_r1 = towerGridCoords[pkt.dst_tower_idx].first;
          g_c1 = towerGridCoords[pkt.dst_tower_idx].second;
        }
      } else if (pkt.current_hop == 3) {
        // DstTower -> DstDev
        if (pkt.dst_tower_idx >= 0 && pkt.dst_tower_idx < (int)towerGridCoords.size()) {
          g_r0 = towerGridCoords[pkt.dst_tower_idx].first;
          g_c0 = towerGridCoords[pkt.dst_tower_idx].second;
        }
        if (pkt.dest_id >= 0 && pkt.dest_id < (int)deviceGridCoords.size()) {
          g_r1 = deviceGridCoords[pkt.dest_id].first;
          g_c1 = deviceGridCoords[pkt.dest_id].second;
        }
      }

      if (g_r0 != -1 && g_c0 != -1 && g_r1 != -1 && g_c1 != -1) {
        int pr = (int)(g_r0 + (g_r1 - g_r0) * pkt.progress);
        int pc = (int)(g_c0 + (g_c1 - g_c0) * pkt.progress);

        if (pr >= 0 && pr < mapH && pc >= 0 && pc < mapW) {
          std::string pkt_icon = (pkt.type == "VOICE") ? "✦" : "◆";
          std::string pkt_color = (pkt.type == "VOICE") ? "\033[1;33m" : "\033[1;36m"; // Bold Yellow / Bold Cyan
          
          if (mapGrid[pr][pc].type == 0) {
            mapGrid[pr][pc] = {pkt_icon, pkt_color, false, 0, -1};
          }
        }
      }
    }
  }

  // 8. Draw Devices (●/x)
  for (int i = 0; i < dev_count; i++) {
    if (i >= (int)deviceGridCoords.size()) break;
    int dr = deviceGridCoords[i].first;
    int dc = deviceGridCoords[i].second;
    if (dr == -1 || dc == -1) continue;

    auto dev = simulator.get_device(i);
    if (!dev) continue;

    bool is_selected = (selectedMapType == 2 && selectedMapId == i);
    bool is_conn = dev->get_connection_status();
    std::string dev_color = is_selected ? "\033[1;30m\033[42m" : (is_conn ? "\033[1;32m" : "\033[1;31m"); // Green / Red

    mapGrid[dr][dc] = {is_conn ? "●" : "x", dev_color, true, 2, i};
  }

  // 9. Draw Map Cursor (┼) if active
  if (mapCursorMode) {
    if (mapCursorRow >= 0 && mapCursorRow < mapH && mapCursorCol >= 0 && mapCursorCol < mapW) {
      auto &cell = mapGrid[mapCursorRow][mapCursorCol];
      cell.ch = "┼";
      cell.fg = "\033[1;33;5m"; // Bright blinking yellow cursor
    }
  }

  // Render composite map row by row
  for (int r = 0; r < mapH; ++r) {
    std::string line = "";
    for (int c = 0; c < mapW; ++c) {
      auto cell = mapGrid[r][c];
      if (cell.fg.empty()) {
        line += cell.ch;
      } else {
        line += cell.fg + cell.ch + "\033[0m";
      }
    }
    printAt(contentStart + 1 + r, 2, line);
  }

  // Draw Map Inspector (Right Panel)
  int ic = mapW_box + 3; // Inspector content column
  int ir = contentStart + 1; // Inspector content row start
  int innerInspectW = inspectW_box - 4;

  auto printInspect = [&](const std::string &label, const std::string &value, const std::string &color = TerminalColor::WHITE) {
    std::string line = label + value;
    int vis = getVisibleLength(line);
    int pad = innerInspectW - vis;
    if (pad < 0) pad = 0;
    printAt(ir++, ic, line + repeatChar(' ', pad), color);
  };

  if (selectedMapType == 0) {
    // General Stats & Routing Dashboard
    printInspect("STATUS: ", "General Summary", TerminalColor::BOLD_YELLOW);
    ir++;
    printInspect("Towers:       ", std::to_string(tower_count), TerminalColor::BOLD_CYAN);
    
    int connected = 0;
    for (int i = 0; i < dev_count; ++i) {
      auto d = simulator.get_device(i);
      if (d && d->get_connection_status()) connected++;
    }
    printInspect("Devices:      ", std::to_string(connected) + " / " + std::to_string(dev_count), TerminalColor::BOLD_GREEN);
    
    double avg_util = 0.0;
    int active_t = 0;
    for (int i = 0; i < tower_count; ++i) {
      auto tw = simulator.get_tower(i);
      if (tw) {
        avg_util += tw->get_utilization();
        active_t++;
      }
    }
    if (active_t > 0) avg_util /= active_t;
    printInspect("Avg Util:     ", std::to_string((int)avg_util) + "%");
    ir++;

    printInspect("Packet Routing Engine:", "", TerminalColor::BOLD_MAGENTA);
    printInspect("Routed Pkts:  ", std::to_string(simulator.get_total_packets_routed()), TerminalColor::BOLD_CYAN);
    printInspect("Processed:    ", std::to_string(simulator.get_processed_packets()), TerminalColor::BOLD_GREEN);
    printInspect("Dropped Pkts: ", std::to_string(simulator.get_dropped_packets()), TerminalColor::BOLD_RED);
    
    // Router Queue Congestion progress bar
    int active_pkts_cnt = simulator.get_active_packets().size();
    double congestion = (active_pkts_cnt / 30.0) * 100.0;
    if (congestion > 100.0) congestion = 100.0;
    printInspect("Congestion:   ", std::to_string((int)congestion) + "%");
    
    int cBarW = std::min(15, innerInspectW - 13);
    if (cBarW > 4) {
      std::string cBar = OutputFormatter::get_progress_bar(congestion / 100.0, cBarW);
      printAt(ir++, ic, "             " + cBar);
    }
    ir++;

    printInspect("Console Controls:", "", TerminalColor::BOLD_YELLOW);
    printInspect("  [H] Heatmap overlay: ", heatmapMode ? "ON" : "OFF", heatmapMode ? TerminalColor::BOLD_GREEN : TerminalColor::WHITE);
    printInspect("  [P] Packet flows:    ", packetFlowMode ? "ON" : "OFF", packetFlowMode ? TerminalColor::BOLD_GREEN : TerminalColor::WHITE);
    ir++;
    
    // Interactive mouse tip
    printAt(ir++, ic, "Interactive Tip:", TerminalColor::BOLD_YELLOW);
    printAt(ir++, ic, "Click map coordinates with", TerminalColor::WHITE);
    printAt(ir++, ic, "your mouse to draw/erase", TerminalColor::WHITE);
    printAt(ir++, ic, "concrete buildings!", TerminalColor::WHITE);
  }
  else if (selectedMapType == 1) {
    // Tower Inspector
    auto tw = simulator.get_tower(selectedMapId);
    if (!tw) {
      selectedMapType = 0;
      selectedMapId = -1;
      return;
    }
    printInspect("INSPECTING:  ", "Tower #" + std::to_string(selectedMapId), TerminalColor::BOLD_CYAN);
    ir++;
    printInspect("Gen/Proto:   ", tw->get_protocol()->get_protocol_name(), TerminalColor::BOLD_YELLOW);
    printInspect("Location:    ", tw->get_location() + " (" + std::to_string((int)tw->get_x()) + "," + std::to_string((int)tw->get_y()) + ")");
    printInspect("Bandwidth:   ", std::to_string((int)tw->get_total_bandwidth()) + " MHz");
    printInspect("Devices:     ", std::to_string(tw->get_current_device_count()) + " / " + std::to_string(tw->get_max_supported_devices()));
    printInspect("Utilization: ", std::to_string((int)tw->get_utilization()) + "%");
    
    int progW = std::min(20, innerInspectW - 13);
    if (progW > 4) {
      std::string bar = OutputFormatter::get_progress_bar(tw->get_utilization() / 100.0, progW);
      printAt(ir++, ic, "             " + bar);
    }
    ir++;

    printInspect("Connected Devices:", "", TerminalColor::BOLD_MAGENTA);
    auto all_devs = tw->get_all_devices();
    if (all_devs.empty()) {
      printInspect("  No active devices", "", TerminalColor::WHITE);
    } else {
      for (size_t di = 0; di < all_devs.size() && ir < (contentEnd - 1); di++) {
        auto dev = all_devs[di];
        if (dev) {
          double dx = dev->get_x() - tw->get_x();
          double dy = dev->get_y() - tw->get_y();
          double dist = sqrt(dx*dx + dy*dy);
          std::string name = dev->get_device_name();
          if (name.length() > 8) name = name.substr(0, 6) + "..";
          std::string dev_info = "  - " + name + " (" + dev->get_device_type() + ") " + std::to_string((int)dist) + "m";
          printAt(ir++, ic, dev_info, TerminalColor::WHITE);
        }
      }
    }
  }
  else if (selectedMapType == 2) {
    // Device Inspector
    auto dev = simulator.get_device(selectedMapId);
    if (!dev) {
      selectedMapType = 0;
      selectedMapId = -1;
      return;
    }
    printInspect("INSPECTING:  ", dev->get_device_name(), TerminalColor::BOLD_GREEN);
    ir++;
    printInspect("Generation:  ", dev->get_device_type(), TerminalColor::BOLD_YELLOW);
    printInspect("Position:    ", "(" + std::to_string((int)dev->get_x()) + "," + std::to_string((int)dev->get_y()) + ")");
    printInspect("Comm Type:   ", dev->get_communication_type_string());
    
    bool is_conn = dev->get_connection_status();
    std::string statusStr = is_conn ? (std::string(TerminalColor::BOLD_GREEN) + "Connected" + TerminalColor::RESET) 
                                    : (std::string(TerminalColor::BOLD_RED) + "Disconnected" + TerminalColor::RESET);
    printInspect("Status:      ", statusStr);
    printInspect("Total Msg:   ", std::to_string(dev->get_message_count()));

    if (is_conn) {
      int tw_idx = -1;
      for (int t = 0; t < tower_count; ++t) {
        auto tw = simulator.get_tower(t);
        if (tw && tw->get_device(dev->get_device_id()) != nullptr) {
          tw_idx = t;
          break;
        }
      }
      if (tw_idx != -1) {
        auto tw = simulator.get_tower(tw_idx);
        double dx = dev->get_x() - tw->get_x();
        double dy = dev->get_y() - tw->get_y();
        double dist = sqrt(dx*dx + dy*dy);
        
        bool has_los = simulator.check_line_of_sight(dev->get_x(), dev->get_y(), tw->get_x(), tw->get_y());
        double rssi = simulator.compute_rssi(dist, !has_los);

        printInspect("Connected To:", "Tower #" + std::to_string(tw_idx), TerminalColor::BOLD_CYAN);
        printInspect("Distance:    ", std::to_string((int)dist) + " meters");
        printInspect("RSSI Signal: ", std::to_string((int)rssi) + " dBm", (rssi > -75.0) ? TerminalColor::BOLD_GREEN : ((rssi > -95.0) ? TerminalColor::BOLD_YELLOW : TerminalColor::BOLD_RED));
        
        int sBarW = std::min(15, innerInspectW - 13);
        if (sBarW > 4) {
          // Normalize signal: -50 dBm = 100%, -115 dBm = 0%
          double signal_pct = (rssi + 115.0) / 65.0;
          if (signal_pct < 0.0) signal_pct = 0.0;
          if (signal_pct > 1.0) signal_pct = 1.0;
          std::string sigBar = OutputFormatter::get_progress_bar(signal_pct, sBarW);
          printAt(ir++, ic, "             " + sigBar);
        }
      }
    }
  } else if (selectedMapType == 3) {
    // Core Router Inspector
    printInspect("INSPECTING:  ", "Core Router", TerminalColor::BOLD_MAGENTA);
    ir++;
    printInspect("Location:    ", "Central Hub (500,500)");
    printInspect("Capacity:    ", "30 Packets Max");
    printInspect("Active Pkts: ", std::to_string(simulator.get_active_packets().size()), TerminalColor::BOLD_CYAN);
    
    int active_pkts_cnt = simulator.get_active_packets().size();
    double congestion = (active_pkts_cnt / 30.0) * 100.0;
    if (congestion > 100.0) congestion = 100.0;
    printInspect("Congestion:  ", std::to_string((int)congestion) + "%", (congestion > 80.0) ? TerminalColor::BOLD_RED : ((congestion > 40.0) ? TerminalColor::BOLD_YELLOW : TerminalColor::BOLD_GREEN));
    
    int cBarW = std::min(15, innerInspectW - 13);
    if (cBarW > 4) {
      std::string cBar = OutputFormatter::get_progress_bar(congestion / 100.0, cBarW);
      printAt(ir++, ic, "             " + cBar);
    }
    ir++;
    
    printInspect("Throughput stats:", "", TerminalColor::BOLD_YELLOW);
    printInspect("  Total Routed:", std::to_string(simulator.get_total_packets_routed()));
    printInspect("  Processed:   ", std::to_string(simulator.get_processed_packets()), TerminalColor::BOLD_GREEN);
    printInspect("  Dropped:     ", std::to_string(simulator.get_dropped_packets()), TerminalColor::BOLD_RED);
  }
}

void ConsoleTUI::drawActionsTab(int rows, int cols) {
  int contentStart = 1;
  int contentEnd = rows - 4;
  int boxHeight = contentEnd - contentStart;
  if (boxHeight < 5) boxHeight = 5;

  // Two panels: Actions list and Quick Stats
  int leftW = cols * 2 / 3;
  int rightW = cols - leftW - 1;

  drawBox(contentStart, 0, boxHeight, leftW, "ACTIONS PANEL");
  drawBox(contentStart, leftW + 1, boxHeight, rightW, "QUICK STATS");

  int r = contentStart + 2;
  int c = 3;
  int innerW = leftW - 6;

  printAt(r++, c, "Use [Up/Down] to navigate, [Enter] to execute:", TerminalColor::BOLD_CYAN);
  r++;

  struct ActionItem {
    std::string key;
    std::string label;
    std::string desc;
    std::string icon;
  };

  std::vector<ActionItem> actions = {
    { "A", "Traffic Spike",       "Simulate real-time device movements and traffic spikes", "⚡" },
    { "S", "Load Scenario",       "Open dialog to select a predefined network scenario",   "📂" },
    { "F", "Create Tower",        "Open modal to create a tower with custom settings",      "🗼" },
    { "D", "Create Device",       "Open modal to register and connect a new device",        "📱" },
    { "G", "Toggle Beamforming",  "Boost the selected tower's capacity by 2.5x",            "📡" },
    { "H", "Reset Simulator",     "Purge all towers, devices, and metrics",                 "🔄" },
    { "Q", "Exit Simulator",      "Safely close the TUI and restore terminal",              "🚪" }
  };

  // Clamp actionsSelectedIdx
  if (actionsSelectedIdx < 0) actionsSelectedIdx = 0;
  if (actionsSelectedIdx >= (int)actions.size()) actionsSelectedIdx = (int)actions.size() - 1;

  for (int i = 0; i < (int)actions.size() && r < contentEnd - 1; i++) {
    auto &act = actions[i];
    bool selected = (i == actionsSelectedIdx);

    if (selected) {
      // Highlighted row with accent background
      std::string selBg = "\033[48;5;24m";
      std::string line = " ▶ [" + act.key + "]  " + act.label;
      int pad = innerW - getVisibleLength(line);
      if (pad < 0) pad = 0;
      printAt(r, c, line + repeatChar(' ', pad), selBg + TerminalColor::BOLD_WHITE);
      r++;
      // Show description below highlighted item
      std::string descLine = "   " + act.desc;
      if ((int)descLine.length() > innerW) descLine = descLine.substr(0, innerW - 2) + "..";
      int dPad = innerW - (int)descLine.length();
      if (dPad < 0) dPad = 0;
      printAt(r, c, descLine + repeatChar(' ', dPad), selBg + TerminalColor::CYAN);
      r++;
    } else {
      std::string line = "   [" + act.key + "]  " + act.label;
      printAt(r, c, line, TerminalColor::WHITE);
      r++;
    }
  }

  // RIGHT: Quick Stats panel
  int sr = contentStart + 2;
  int sc = leftW + 3;
  int srW = rightW - 4;

  printAt(sr++, sc, "SIMULATOR STATUS", TerminalColor::BOLD_MAGENTA);
  printAt(sr++, sc, repeatStr("─", std::min(srW, 30)), TerminalColor::CYAN);
  sr++;

  // Tower stats
  int towerCount = simulator.get_tower_count();
  int deviceCount = simulator.get_device_count();
  
  printAt(sr, sc, "Towers:", TerminalColor::WHITE);
  printAt(sr++, sc + 12, std::to_string(towerCount), TerminalColor::BOLD_GREEN);
  
  printAt(sr, sc, "Devices:", TerminalColor::WHITE);
  printAt(sr++, sc + 12, std::to_string(deviceCount), TerminalColor::BOLD_GREEN);

  // Connected vs disconnected
  int connected = 0, disconnected = 0;
  for (int i = 0; i < deviceCount; i++) {
    auto dev = simulator.get_device(i);
    if (dev && dev->get_connection_status()) connected++;
    else disconnected++;
  }
  printAt(sr, sc, "Connected:", TerminalColor::WHITE);
  printAt(sr++, sc + 12, std::to_string(connected), TerminalColor::BOLD_GREEN);
  printAt(sr, sc, "Dropped:", TerminalColor::WHITE);
  printAt(sr++, sc + 12, std::to_string(disconnected), disconnected > 0 ? TerminalColor::BOLD_RED : TerminalColor::BOLD_GREEN);

  sr++;
  // Average utilization
  double avgUtil = 0;
  for (int i = 0; i < towerCount; i++) {
    auto tw = simulator.get_tower(i);
    if (tw) avgUtil += tw->get_utilization();
  }
  if (towerCount > 0) avgUtil /= towerCount;
  printAt(sr, sc, "Avg Load:", TerminalColor::WHITE);
  std::stringstream ss_au;
  ss_au << std::fixed << std::setprecision(1) << avgUtil << "%";
  printAt(sr++, sc + 12, ss_au.str(), TerminalColor::BOLD_CYAN);

  // Uptime
  sr++;
  printAt(sr, sc, "Uptime:", TerminalColor::WHITE);
  printAt(sr++, sc + 12, getUptime(), TerminalColor::BOLD_YELLOW);
}

void ConsoleTUI::drawHelpTab(int rows, int cols) {
  int contentStart = 1;
  int contentEnd = rows - 4;
  int boxHeight = contentEnd - contentStart;
  if (boxHeight < 5) boxHeight = 5;

  drawBox(contentStart, 0, boxHeight, cols, "SYSTEM HELP & QUICK REFERENCE");

  int r = contentStart + 2;
  int c1 = 3;
  int c2 = cols / 2 + 1;

  // Left Column: Navigation & Hotkeys
  printAt(r++, c1, "NAVIGATION & HOTKEYS", TerminalColor::BOLD_MAGENTA);
  r++;
  
  auto printShortcut = [&](int row, int col, const std::string &key, const std::string &desc) {
    printAt(row, col, " " + key + " ", TerminalColor::BOLD_YELLOW + "\033[7m");
    printAt(row, col + 18, "▸ " + desc, TerminalColor::WHITE);
  };

  printShortcut(r++, c1, " [1-7]           ", "Switch between TUI Tabs directly");
  printShortcut(r++, c1, " [:]             ", "Open interactive Command Console");
  printShortcut(r++, c1, " [Q / q]         ", "Exit cellular simulator safely");
  printShortcut(r++, c1, " [Up/Down Arrow] ", "Navigate lists and select items");
  printShortcut(r++, c1, " [Enter]         ", "Execute selected action (Actions tab)");
  printShortcut(r++, c1, " [B / b]         ", "Toggle Beamforming (Towers tab)");
  printShortcut(r++, c1, " [/]             ", "Filter devices (Devices tab)");
  printShortcut(r++, c1, " [Esc]           ", "Cancel Console/Search/Modal");
  printShortcut(r++, c1, " [Mouse Click]   ", "Click tabs, towers, or actions");
  printShortcut(r++, c1, " [Mouse Scroll]  ", "Scroll devices list dynamically");

  // Right Column: Console Commands
  int rr = contentStart + 2;
  printAt(rr++, c2, "CONSOLE COMMANDS", TerminalColor::BOLD_MAGENTA);
  rr++;

  auto printCommand = [&](int row, int col, const std::string &cmd, const std::string &desc) {
    printAt(row, col, " " + cmd, TerminalColor::BOLD_CYAN);
    printAt(row, col + 22, "▸ " + desc, TerminalColor::WHITE);
  };

  printCommand(rr++, c2, "help", "List all available commands");
  printCommand(rr++, c2, "reset", "Reset all simulator configurations");
  printCommand(rr++, c2, "spike", "Trigger high traffic fluctuations");
  printCommand(rr++, c2, "tab <1-7>", "Switch to the specified tab");
  printCommand(rr++, c2, "scenario <name>", "Load (urban|suburban|rural|stadium...)");
  printCommand(rr++, c2, "addtower <args>", "Deploy custom core tower");
  printCommand(rr++, c2, "adddevice <args>", "Register new cellular device");
  printCommand(rr++, c2, "beamforming <id> <x>", "Boost custom tower capacity");
  printCommand(rr++, c2, "handovers <count>", "Simulate random roam handovers");

  // About section
  rr += 2;
  if (rr < contentEnd - 3) {
    printAt(rr++, c2, "ABOUT", TerminalColor::BOLD_MAGENTA);
    printAt(rr++, c2, repeatStr("─", 30), TerminalColor::CYAN);
    printAt(rr++, c2, "Cellular Network Simulator v2.0", TerminalColor::BOLD_CYAN);
    printAt(rr++, c2, "Multi-generation (2G-7G) simulation", TerminalColor::WHITE);
    printAt(rr++, c2, "Built with C++17 | Terminal UI", TerminalColor::WHITE);
  }
}

void ConsoleTUI::drawCommandBar(int rows, int cols) {
  int r = rows - 4;

  if (commandMode) {
    drawBox(r, 0, 3, cols, "COMMAND CONSOLE");
    std::string inputLine = " > " + commandInput;
    int inputVis = getVisibleLength(inputLine);

    // Print input first (so it doesn't overwrite suggestion if cursor is at end)
    printAt(r + 1, 1, inputLine, TerminalColor::BOLD_GREEN);

    // Suggestion hint
    if (!autoSuggestHint.empty()) {
      printAt(r + 1, 1 + inputVis, "\033[90m" + autoSuggestHint + TerminalColor::RESET);
    }

    // Position cursor
    int cursor_col = 3 + commandCursorPos;
    std::cout << "\033[" << (r + 2) << ";" << (cursor_col + 1) << "H\033[?25h" << std::flush;
  } else {
    drawBox(r, 0, 3, cols, " [:] Open Console ");
    std::string msg = " Press [:] to enter command mode, [Q] to quit, or [1-7] to switch tabs.";
    int pad = cols - 2 - (int)msg.length();
    if (pad < 0) pad = 0;
    printAt(r + 1, 1, msg + repeatChar(' ', pad), "\033[90m");
    std::cout << "\033[?25l" << std::flush;
  }
}

void ConsoleTUI::drawModal(int rows, int cols) {
  int box_h = (int)modalFields.size() * 2 + 6;
  if (activeModal == ModalType::LOAD_SCENARIO) {
    box_h += 1; // Extra line for helper text
  }
  int box_w = (activeModal == ModalType::LOAD_SCENARIO) ? 68 : 58;
  if (box_w > cols - 4) box_w = cols - 4;

  int start_r = (rows - box_h) / 2;
  int start_c = (cols - box_w) / 2;
  int inner = box_w - 2;

  std::string title = "";
  if (activeModal == ModalType::CREATE_TOWER) title = "CREATE CELL TOWER";
  else if (activeModal == ModalType::CREATE_DEVICE) title = "CREATE DEVICE";
  else if (activeModal == ModalType::LOAD_SCENARIO) title = "LOAD SCENARIO";

  // Top border
  std::string top = "┌ " + title + " ";
  int tfill = inner - (int)title.length() - 2;
  if (tfill > 0) top += repeatStr("─", tfill);
  top += "┐";
  printAt(start_r, start_c, top, TerminalColor::BOLD_YELLOW);

  // Clear inside and draw side borders
  for (int rr = 1; rr < box_h - 1; rr++) {
    printAt(start_r + rr, start_c, "│", TerminalColor::BOLD_YELLOW);
    printAt(start_r + rr, start_c + 1, repeatChar(' ', inner));
    printAt(start_r + rr, start_c + box_w - 1, "│", TerminalColor::BOLD_YELLOW);
  }

  // Bottom border
  std::string bottom = "└" + repeatStr("─", inner) + "┘";
  printAt(start_r + box_h - 1, start_c, bottom, TerminalColor::BOLD_YELLOW);

  // Fields
  for (size_t i = 0; i < modalFields.size(); i++) {
    int fr = start_r + 2 + (int)i * 2;
    std::string prompt_str = " " + modalFields[i].label + ": ";
    printAt(fr, start_c + 2, prompt_str, TerminalColor::BOLD_WHITE);

    int input_start = start_c + 2 + (int)prompt_str.length();
    int input_w = start_c + box_w - 3 - input_start;
    if (input_w < 5) input_w = 5;

    std::string val = modalFields[i].value;
    if ((int)val.length() > input_w) {
      val = val.substr(val.length() - input_w);
    }
    int vpad = input_w - (int)val.length();
    if (vpad < 0) vpad = 0;

    std::string color = (i == (size_t)activeFieldIdx) ? (TerminalColor::BOLD_WHITE + "\033[44m") : (TerminalColor::WHITE + "\033[48;5;236m");
    printAt(fr, input_start, "[" + val + repeatChar(' ', vpad) + "]", color);
  }

  // Helper options text for loading scenarios
  if (activeModal == ModalType::LOAD_SCENARIO) {
    std::string h1 = "Options: urban | suburban | rural | highway";
    std::string h2 = "         stadium | disaster | mixed";
    int h1_c = start_c + (box_w - (int)h1.length()) / 2;
    if (h1_c < start_c + 2) h1_c = start_c + 2;
    printAt(start_r + 4, h1_c, h1, TerminalColor::BOLD_CYAN);
    printAt(start_r + 5, h1_c, h2, TerminalColor::BOLD_CYAN);
  }

  // Instructions
  int inst_row = start_r + box_h - 2;
  std::string inst = "[Tab] Next | [Esc] Cancel | [Enter] Confirm";
  int instCol = start_c + (box_w - (int)inst.length()) / 2;
  printAt(inst_row, instCol, inst, TerminalColor::BOLD_YELLOW);

  // Cursor positioning
  int cr = start_r + 2 + activeFieldIdx * 2;
  std::string cprompt = " " + modalFields[activeFieldIdx].label + ": ";
  int cursor_c = start_c + 2 + (int)cprompt.length() + 1 + (int)modalFields[activeFieldIdx].value.length();
  std::cout << "\033[" << (cr + 1) << ";" << (cursor_c + 1) << "H\033[?25h" << std::flush;
}

void ConsoleTUI::handleInput(int key) {
  if (activeModal != ModalType::NONE) {
    handleModalInput(key);
    return;
  }
  if (commandMode) {
    handleCommandInput(key);
    return;
  }
  if (searchMode) {
    if (key == 27) { // Escape
      searchMode = false;
      return;
    }
    if (key == 10 || key == 13) { // Enter
      searchMode = false;
      return;
    }
    if (key == 127 || key == 8) { // Backspace
      if (!searchQuery.empty()) {
        searchQuery.pop_back();
      }
      devicesScrollOffset = 0;
      return;
    }
    if (key >= 32 && key <= 126) {
      searchQuery += static_cast<char>(key);
      devicesScrollOffset = 0;
      return;
    }
    return;
  }

  // Visual Map keyboard cursor handling
  if (activeTab == 4 && mapCursorMode) {
    int rows_sz, cols_sz;
    getTerminalSize(rows_sz, cols_sz);
    int contentStart = 1;
    int contentEnd = rows_sz - 4;
    int boxHeight = contentEnd - contentStart;
    int mapH = boxHeight - 3;
    int mapW = (cols_sz * 2 / 3) - 4;

    if (key == 1001 || key == 'k' || key == 'K' || key == 'w' || key == 'W') { // Up
      if (mapCursorRow > 0) mapCursorRow--;
      return;
    }
    if (key == 1002 || key == 'j' || key == 'J' || key == 's' || key == 'S') { // Down
      if (mapCursorRow < mapH - 1) mapCursorRow++;
      return;
    }
    if (key == 1004 || key == 'h' || key == 'H' || key == 'a' || key == 'A') { // Left
      if (mapCursorCol > 0) mapCursorCol--;
      return;
    }
    if (key == 1003 || key == 'l' || key == 'L' || key == 'd' || key == 'D') { // Right
      if (mapCursorCol < mapW - 1) mapCursorCol++;
      return;
    }
    if (key == 'c' || key == 'C') {
      mapCursorMode = false;
      addToast("Keyboard Cursor Disabled", TerminalColor::BOLD_GREEN);
      return;
    }
    if (key == 27) { // Escape
      mapCursorMode = false;
      selectedMapType = 0;
      selectedMapId = -1;
      addToast("Cleared map selection", TerminalColor::BOLD_YELLOW);
      return;
    }
    if (key == 10 || key == 13 || key == ' ') { // Enter / Space
      int clicked_r = mapCursorRow;
      int clicked_c = mapCursorCol;
      int dev_count = simulator.get_device_count();
      int tower_count = simulator.get_tower_count();
      
      // Ensure coordinate cache is populated
      if (towerGridCoords.empty() || deviceGridCoords.empty()) {
        computeGridCoordinates(mapH, mapW);
      }

      bool found = false;

      // 1. Core Router (▣)
      int core_r = mapH / 2;
      int core_c = mapW / 2;
      if (abs(clicked_r - core_r) <= 1 && abs(clicked_c - core_c) <= 1) {
        selectedMapType = 3;
        selectedMapId = 0;
        addToast("Inspecting Central Core Backhaul Router", TerminalColor::BOLD_MAGENTA);
        found = true;
      }

      // 2. Towers (▲)
      if (!found) {
        for (int i = 0; i < tower_count; i++) {
          if (i >= (int)towerGridCoords.size()) break;
          int tr = towerGridCoords[i].first;
          int tc = towerGridCoords[i].second;
          if (tr == -1 || tc == -1) continue;
          if (abs(clicked_r - tr) <= 1 && abs(clicked_c - tc) <= 2) {
            selectedMapType = 1;
            selectedMapId = i;
            addToast("Inspecting Tower #" + std::to_string(i), TerminalColor::BOLD_CYAN);
            found = true;
            break;
          }
        }
      }

      // 3. Devices (●)
      if (!found) {
        for (int i = 0; i < dev_count; i++) {
          if (i >= (int)deviceGridCoords.size()) break;
          int dr = deviceGridCoords[i].first;
          int dc = deviceGridCoords[i].second;
          if (dr == -1 || dc == -1) continue;
          if (abs(clicked_r - dr) <= 1 && abs(clicked_c - dc) <= 1) {
            selectedMapType = 2;
            selectedMapId = i;
            auto dev = simulator.get_device(i);
            addToast("Inspecting Device: " + (dev ? dev->get_device_name() : "UE"), TerminalColor::BOLD_GREEN);
            found = true;
            break;
          }
        }
      }

      // 4. Click empty/obstacle space -> toggle obstacle!
      if (!found) {
        int gx = (clicked_c * 100) / mapW;
        int gy = (clicked_r * 100) / mapH;
        simulator.toggle_obstacle_at(gx, gy);
        
        bool is_now_bldg = simulator.has_obstacle_at(gx, gy);
        if (is_now_bldg) {
          addToast("Drew building block", TerminalColor::BOLD_YELLOW);
        } else {
          addToast("Cleared building block", TerminalColor::BOLD_GREEN);
        }
      }
      return;
    }
  }

  if (key == 'q' || key == 'Q') {
    quit = true;
  } else if (key == 27) { // Escape
    if (activeTab == 4) {
      mapCursorMode = false;
      selectedMapType = 0;
      selectedMapId = -1;
      addToast("Cleared map inspection", TerminalColor::BOLD_YELLOW);
    }
  } else if (key == ':') {
    commandMode = true;
    commandInput = "";
    commandCursorPos = 0;
    autoSuggestHint = "";
    historyIndex = -1;
  } else if (key == '/' && activeTab == 2) {
    searchMode = true;
    searchQuery = "";
    devicesScrollOffset = 0;
  } else if (key >= '1' && key <= '7') {
    activeTab = key - '1';
    mapCursorMode = false; // exit cursor mode when switching tabs
  } else if ((key == 'c' || key == 'C') && activeTab == 4) {
    mapCursorMode = !mapCursorMode;
    if (mapCursorMode) {
      int rows_sz, cols_sz;
      getTerminalSize(rows_sz, cols_sz);
      int contentStart = 1;
      int contentEnd = rows_sz - 4;
      int boxHeight = contentEnd - contentStart;
      int mapH = boxHeight - 3;
      int mapW = (cols_sz * 2 / 3) - 4;
      mapCursorRow = mapH / 2;
      mapCursorCol = mapW / 2;
      addToast("Keyboard Cursor Enabled", TerminalColor::BOLD_YELLOW);
    } else {
      addToast("Keyboard Cursor Disabled", TerminalColor::BOLD_GREEN);
    }
  } else if (key == 1001 || key == 'k' || key == 'K' || ((key == 'w' || key == 'W') && activeTab != 5)) { // Up / k / w
    if (activeTab == 1) {
      if (selectedTowerIdx > 0) selectedTowerIdx--;
    } else if (activeTab == 2) {
      if (devicesScrollOffset > 0) devicesScrollOffset--;
    } else if (activeTab == 5) {
      if (actionsSelectedIdx > 0) actionsSelectedIdx--;
    }
  } else if (key == 1002 || key == 'j' || key == 'J' || ((key == 's' || key == 'S') && activeTab != 5)) { // Down / j / s
    if (activeTab == 1) {
      if (selectedTowerIdx < simulator.get_tower_count() - 1) selectedTowerIdx++;
    } else if (activeTab == 2) {
      if (devicesScrollOffset < simulator.get_device_count() - 10) devicesScrollOffset++;
    } else if (activeTab == 5) {
      if (actionsSelectedIdx < 6) actionsSelectedIdx++;
    }
  } else if (key == 'b' || key == 'B') {
    if (activeTab == 1 && simulator.get_tower_count() > 0) {
      auto tower = simulator.get_tower(selectedTowerIdx);
      if (tower) {
        if (tower->get_beamforming_factor() > 1.0) {
          simulator.disable_beamforming_on_tower(selectedTowerIdx);
          Logger::success("Disabled Beamforming on Tower #" + std::to_string(selectedTowerIdx));
        } else {
          simulator.apply_beamforming_to_tower(selectedTowerIdx, 2.5);
          Logger::success("Applied 2.5x Beamforming on Tower #" + std::to_string(selectedTowerIdx));
        }
      }
    }
  } else if ((key == 'h' || key == 'H') && activeTab == 4) {
    heatmapMode = !heatmapMode;
    addToast(heatmapMode ? "RF heatmap enabled" : "RF heatmap disabled", TerminalColor::BOLD_YELLOW);
  } else if ((key == 'p' || key == 'P') && activeTab == 4) {
    packetFlowMode = !packetFlowMode;
    addToast(packetFlowMode ? "Packet flow animations enabled" : "Packet flow animations disabled", TerminalColor::BOLD_GREEN);
  } else if (activeTab == 5) {
    // Map Enter key to the selected action's key
    int actionKey = key;
    if (key == 10 || key == 13) { // Enter
      std::string actionKeys = "ASFDGHQ";
      if (actionsSelectedIdx >= 0 && actionsSelectedIdx < (int)actionKeys.length()) {
        actionKey = actionKeys[actionsSelectedIdx];
      }
    }
    if (actionKey == 'a' || actionKey == 'A') {
      simulator.simulate_traffic_fluctuation();
      Logger::info("Triggered traffic fluctuation manually.");
      addToast("Traffic spike triggered!", TerminalColor::BOLD_YELLOW);
    } else if (actionKey == 's' || actionKey == 'S') {
      activeModal = ModalType::LOAD_SCENARIO;
      activeFieldIdx = 0;
      modalFields = {
        {"Scenario Type", "urban", "", false}
      };
    } else if (actionKey == 'f' || actionKey == 'F') {
      activeModal = ModalType::CREATE_TOWER;
      activeFieldIdx = 0;
      modalFields = {
        {"Generation (2G-7G)", "5G", "", false},
        {"Location Name", "Downtown", "", false},
        {"Bandwidth (MHz)", "20.0", "", true},
        {"Number of Cores", "4", "", true}
      };
    } else if (actionKey == 'd' || actionKey == 'D') {
      activeModal = ModalType::CREATE_DEVICE;
      activeFieldIdx = 0;
      modalFields = {
        {"Generation (2G-7G)", "5G", "", false},
        {"Device Name", "User-Phone", "", false},
        {"Type (data|voice|both)", "both", "", false},
        {"Tower Index", "0", "", true}
      };
    } else if (actionKey == 'g' || actionKey == 'G') {
      if (simulator.get_tower_count() > 0) {
        auto tower = simulator.get_tower(selectedTowerIdx);
        if (tower) {
          if (tower->get_beamforming_factor() > 1.0) {
            simulator.disable_beamforming_on_tower(selectedTowerIdx);
            Logger::success("Disabled Beamforming on Tower #" + std::to_string(selectedTowerIdx));
            addToast("Beamforming disabled on T#" + std::to_string(selectedTowerIdx), TerminalColor::BOLD_CYAN);
          } else {
            simulator.apply_beamforming_to_tower(selectedTowerIdx, 2.5);
            Logger::success("Applied 2.5x Beamforming on Tower #" + std::to_string(selectedTowerIdx));
            addToast("Beamforming 2.5x applied to T#" + std::to_string(selectedTowerIdx), TerminalColor::BOLD_GREEN);
          }
        }
      }
    } else if (actionKey == 'h' || actionKey == 'H') {
      simulator.reset();
      selectedTowerIdx = 0;
      devicesScrollOffset = 0;
      Logger::info("Purged simulator memory.");
      addToast("Simulator reset complete", TerminalColor::BOLD_RED);
    }
  }
}

void ConsoleTUI::handleCommandInput(int key) {
  if (key == 27) { // Escape
    commandMode = false;
    commandInput = "";
    commandCursorPos = 0;
    autoSuggestHint = "";
    historyIndex = -1;
    return;
  }
  
  if (key == 9) { // Tab
    if (!autoSuggestHint.empty()) {
      commandInput += autoSuggestHint;
      commandCursorPos = commandInput.length();
      autoSuggestHint = "";
    }
    return;
  }

  if (key == 127 || key == 8) { // Backspace
    if (commandCursorPos > 0) {
      commandInput.erase(commandCursorPos - 1, 1);
      commandCursorPos--;
      updateAutoSuggest();
    }
    return;
  }

  if (key == 1004) { // Left arrow
    if (commandCursorPos > 0) commandCursorPos--;
    return;
  }
  if (key == 1003) { // Right arrow
    if (commandCursorPos < (int)commandInput.length()) {
      commandCursorPos++;
    } else if (!autoSuggestHint.empty()) {
      commandInput += autoSuggestHint;
      commandCursorPos = commandInput.length();
      autoSuggestHint = "";
    }
    return;
  }

  if (key == 1001) { // Up arrow (History)
    if (!commandHistory.empty()) {
      if (historyIndex == -1) {
        historyIndex = commandHistory.size() - 1;
      } else if (historyIndex > 0) {
        historyIndex--;
      }
      commandInput = commandHistory[historyIndex];
      commandCursorPos = commandInput.length();
      autoSuggestHint = "";
    }
    return;
  }
  if (key == 1002) { // Down arrow
    if (historyIndex != -1) {
      if (historyIndex < (int)commandHistory.size() - 1) {
        historyIndex++;
        commandInput = commandHistory[historyIndex];
      } else {
        historyIndex = -1;
        commandInput = "";
      }
      commandCursorPos = commandInput.length();
      autoSuggestHint = "";
    }
    return;
  }

  if (key == 10 || key == 13) { // Enter (submit!)
    if (!commandInput.empty()) {
      commandHistory.push_back(commandInput);
      executeCommand(commandInput);
    }
    commandMode = false;
    commandInput = "";
    commandCursorPos = 0;
    autoSuggestHint = "";
    historyIndex = -1;
    return;
  }

  if (key >= 32 && key <= 126) {
    commandInput.insert(commandCursorPos, 1, static_cast<char>(key));
    commandCursorPos++;
    updateAutoSuggest();
  }
}

void ConsoleTUI::handleMouseEvent(int button, int col, int row, bool is_press) {
  if (!is_press) return;

  int rows, cols;
  getTerminalSize(rows, cols);

  // 1. Check if clicked on the header (row 0)
  if (row == 0) {
    std::vector<std::string> tabNames = {
      "Dashboard", "Towers", "Devices", "Analytics", "Visual Map", "Actions", "Help"
    };
    int titleLen = 21;
    int tabsVisLen = 0;
    for (size_t i = 0; i < tabNames.size(); i++) {
      tabsVisLen += (int)tabNames[i].length() + 6;
    }
    tabsVisLen += (int)tabNames.size() - 1;

    int fillLen = cols - 2 - titleLen - tabsVisLen;
    if (fillLen < 1) fillLen = 1;

    int cur_c = titleLen + 2 + fillLen;
    for (size_t i = 0; i < tabNames.size(); i++) {
      int tab_w = (int)tabNames[i].length() + 6;
      int tab_start = cur_c;
      int tab_end = cur_c + tab_w - 1;
      if (col >= tab_start && col <= tab_end) {
        activeTab = static_cast<int>(i);
        commandMode = false;
        searchMode = false;
        activeModal = ModalType::NONE;
        return;
      }
      cur_c += tab_w + 1; // 1 space gap
    }
  }

  // 2. Click in the Towers tab (activeTab == 1)
  if (activeTab == 1 && activeModal == ModalType::NONE && !commandMode) {
    int listWidth = 30;
    if (listWidth > cols / 3) listWidth = cols / 3;
    int contentStart = 1;
    int contentEnd = rows - 4;
    int boxHeight = contentEnd - contentStart;
    int visible_towers = boxHeight - 2;

    // Inside the Towers list (col 1 to listWidth - 2)
    if (col >= 1 && col < listWidth - 1) {
      int clicked_row = row - (contentStart + 1);
      if (clicked_row >= 0 && clicked_row < visible_towers) {
        int towerCount = simulator.get_tower_count();
        int start_tower = std::max(0, selectedTowerIdx - visible_towers / 2);
        int targetIdx = start_tower + clicked_row;
        if (targetIdx >= 0 && targetIdx < towerCount) {
          selectedTowerIdx = targetIdx;
        }
      }
    }
  }

  // 3. Devices tab (activeTab == 2) scroll or click search box
  if (activeTab == 2 && activeModal == ModalType::NONE && !commandMode) {
    // Wheel up/down (64 = up, 65 = down)
    if (button == 64) {
      if (devicesScrollOffset > 0) devicesScrollOffset--;
    } else if (button == 65) {
      // We need to count matching devices
      int matchCount = 0;
      std::string query = searchQuery;
      std::transform(query.begin(), query.end(), query.begin(), ::tolower);
      for (int i = 0; i < simulator.get_device_count(); i++) {
        auto d = simulator.get_device(i);
        if (!d) continue;
        if (query.empty()) {
          matchCount++;
        } else {
          std::string name = d->get_device_name();
          std::transform(name.begin(), name.end(), name.begin(), ::tolower);
          std::string gen = d->get_device_type();
          std::transform(gen.begin(), gen.end(), gen.begin(), ::tolower);
          std::string ct = d->get_communication_type_string();
          std::transform(ct.begin(), ct.end(), ct.begin(), ::tolower);
          std::string idStr = std::to_string(d->get_device_id());
          if (name.find(query) != std::string::npos ||
              gen.find(query) != std::string::npos ||
              ct.find(query) != std::string::npos ||
              idStr.find(query) != std::string::npos) {
            matchCount++;
          }
        }
      }
      int contentStart = 1;
      int contentEnd = rows - 4;
      int boxHeight = contentEnd - contentStart;
      int visible_rows = boxHeight - 6;
      if (devicesScrollOffset < matchCount - visible_rows) devicesScrollOffset++;
    } else {
      // Toggle search Mode by clicking on the search bar area
      // Search prompt is drawn at contentStart + 1, starting at col 2
      int contentStart = 1;
      if (row == contentStart + 1 && col >= 2 && col < 40) {
        searchMode = !searchMode;
        if (searchMode) {
          searchQuery = "";
          devicesScrollOffset = 0;
        }
      }
    }
  }

  // 4. Actions tab (activeTab == 5) mouse clicks
  if (activeTab == 5 && activeModal == ModalType::NONE && !commandMode) {
    int contentStart = 1;
    int contentEnd = rows - 4;
    int leftW = cols * 2 / 3;
    
    // Check if click is in the action list area
    if (col >= 3 && col < leftW - 3) {
      // Actions start at row contentStart + 4 (2 header + 1 instruction + 1 spacing)
      int actionStartRow = contentStart + 4;
      int clickedRow = row - actionStartRow;
      if (clickedRow >= 0) {
        // Calculate which action was clicked (selected item takes 2 rows, others take 1)
        int targetIdx = -1;
        int rowAccum = 0;
        for (int i = 0; i < 7; i++) {
          int itemH = (i == actionsSelectedIdx) ? 2 : 1;
          if (clickedRow >= rowAccum && clickedRow < rowAccum + itemH) {
            targetIdx = i;
            break;
          }
          rowAccum += itemH;
        }
        if (targetIdx >= 0 && targetIdx < 7) {
          actionsSelectedIdx = targetIdx;
        }
      }
    }
  }

  // 5. Visual Map tab (activeTab == 4) mouse clicks
  if (activeTab == 4 && activeModal == ModalType::NONE && !commandMode) {
    int contentStart = 1;
    int contentEnd = rows - 4;
    int boxHeight = contentEnd - contentStart;
    if (boxHeight < 5) boxHeight = 5;

    int mapW_box = cols * 2 / 3;
    int mapH = boxHeight - 3;
    int mapW = mapW_box - 4;

    int mapStartRow = contentStart + 1; // Fixed off-by-one row alignment
    int mapStartCol = 2;

    int dev_count = simulator.get_device_count();
    int tower_count = simulator.get_tower_count();

    if (row >= mapStartRow && row < mapStartRow + mapH &&
        col >= mapStartCol && col < mapStartCol + mapW) {
      int clicked_r = row - mapStartRow;
      int clicked_c = col - mapStartCol;

      // Ensure coordinate cache is populated
      if (towerGridCoords.empty() || deviceGridCoords.empty()) {
        computeGridCoordinates(mapH, mapW);
      }

      bool found = false;

      // 1. Core Router Click check (▣)
      int core_r = mapH / 2;
      int core_c = mapW / 2;
      if (abs(clicked_r - core_r) <= 1 && abs(clicked_c - core_c) <= 1) {
        selectedMapType = 3; // Core Router
        selectedMapId = 0;
        found = true;
        addToast("Inspecting Central Core Backhaul Router", TerminalColor::BOLD_MAGENTA);
      }

      // 2. Towers Click check (▲)
      if (!found) {
        for (int i = 0; i < tower_count; i++) {
          if (i >= (int)towerGridCoords.size()) break;
          int tr = towerGridCoords[i].first;
          int tc = towerGridCoords[i].second;
          if (tr == -1 || tc == -1) continue;
          if (abs(clicked_r - tr) <= 1 && abs(clicked_c - tc) <= 2) {
            selectedMapType = 1; // Tower
            selectedMapId = i;
            addToast("Inspecting Tower #" + std::to_string(i), TerminalColor::BOLD_CYAN);
            found = true;
            break;
          }
        }
      }

      // 3. Devices Click check (●/x)
      if (!found) {
        for (int i = 0; i < dev_count; i++) {
          if (i >= (int)deviceGridCoords.size()) break;
          int dr = deviceGridCoords[i].first;
          int dc = deviceGridCoords[i].second;
          if (dr == -1 || dc == -1) continue;
          if (abs(clicked_r - dr) <= 1 && abs(clicked_c - dc) <= 1) {
            selectedMapType = 2; // Device
            selectedMapId = i;
            auto dev = simulator.get_device(i);
            addToast("Inspecting Device: " + (dev ? dev->get_device_name() : "UE"), TerminalColor::BOLD_GREEN);
            found = true;
            break;
          }
        }
      }

      // 4. Click on empty space clears selection (no more accidental building placements!)
      if (!found) {
        selectedMapType = 0;
        selectedMapId = -1;
        addToast("Cleared map selection", TerminalColor::BOLD_YELLOW);
      }
    }
  }
}

void ConsoleTUI::updateAutoSuggest() {
  autoSuggestHint = "";
  if (commandInput.empty()) return;

  std::vector<std::string> commands = {
    "help", "quit", "reset", "spike", "tab ", "scenario ", "addtower ", "adddevice ", "beamforming ", "handovers "
  };

  for (const auto &cmd : commands) {
    if (cmd.rfind(commandInput, 0) == 0) {
      autoSuggestHint = cmd.substr(commandInput.length());
      break;
    }
  }
}

void ConsoleTUI::executeCommand(const std::string &cmd) {
  if (cmd.empty()) return;
  
  std::stringstream ss(cmd);
  std::string action;
  ss >> action;

  if (action == "help") {
    Logger::info("Commands: help, quit, reset, spike");
    Logger::info("          tab <1-7>, scenario <name>");
    Logger::info("          addtower <gen> <loc> <bandwidth> <cores>");
    Logger::info("          adddevice <gen> <name> <type> <tower_idx>");
    Logger::info("          beamforming <tower_idx> <factor>");
    Logger::info("          handovers <count>");
  } else if (action == "quit") {
    quit = true;
  } else if (action == "reset") {
    simulator.reset();
    selectedTowerIdx = 0;
    devicesScrollOffset = 0;
    throughputHistory.assign(throughputHistory.size(), 0);
    lastProcessedMessages = 0;
    Logger::info("Simulator purged.");
  } else if (action == "spike") {
    for (int i = 0; i < 50; i++) {
      simulator.simulate_traffic_fluctuation();
    }
    Logger::success("Simulated traffic spike.");
  } else if (action == "tab") {
    int tabNum;
    if (ss >> tabNum && tabNum >= 1 && tabNum <= 7) {
      activeTab = tabNum - 1;
      Logger::info("Switched to Tab " + std::to_string(tabNum));
    } else {
      Logger::error("Usage: tab <1-7>");
    }
  } else if (action == "scenario") {
    std::string name;
    if (ss >> name) {
      ScenarioManager::ScenarioType type;
      bool valid = true;
      if (name == "urban") type = ScenarioManager::ScenarioType::URBAN_DENSE;
      else if (name == "suburban") type = ScenarioManager::ScenarioType::SUBURBAN;
      else if (name == "rural") type = ScenarioManager::ScenarioType::RURAL;
      else if (name == "highway") type = ScenarioManager::ScenarioType::HIGHWAY;
      else if (name == "stadium") type = ScenarioManager::ScenarioType::STADIUM;
      else if (name == "disaster") type = ScenarioManager::ScenarioType::DISASTER_RECOVERY;
      else if (name == "mixed") type = ScenarioManager::ScenarioType::MIXED_GENERATION;
      else {
        valid = false;
        Logger::error("Unknown scenario. Options: urban, suburban, rural, highway, stadium, disaster, mixed");
      }
      if (valid) {
        ScenarioManager::load_scenario(simulator, type);
        selectedTowerIdx = 0;
        throughputHistory.assign(throughputHistory.size(), 0);
        lastProcessedMessages = 0;
        Logger::success("Loaded scenario: " + name);
      }
    } else {
      Logger::error("Usage: scenario <name>");
    }
  } else if (action == "addtower") {
    std::string gen, loc;
    double bw;
    int cores;
    if (ss >> gen >> loc >> bw >> cores) {
      try {
        simulator.create_tower(gen, loc, bw, cores);
        simulator.get_tower(simulator.get_tower_count() - 1)->set_position(rand() % 800 + 100, rand() % 800 + 100);
      } catch (const std::exception &e) {
        Logger::error(e.what());
      }
    } else {
      Logger::error("Usage: addtower <gen> <loc> <bandwidth> <cores>");
    }
  } else if (action == "adddevice") {
    std::string gen, name, typeStr;
    int tower_idx;
    if (ss >> gen >> name >> typeStr >> tower_idx) {
      CommunicationType ct = CommunicationType::BOTH;
      if (typeStr == "data") ct = CommunicationType::DATA;
      else if (typeStr == "voice") ct = CommunicationType::VOICE;
      
      try {
        simulator.create_and_connect_device(gen, name, ct, tower_idx);
      } catch (const std::exception &e) {
        Logger::error(e.what());
      }
    } else {
      Logger::error("Usage: adddevice <gen> <name> <data|voice|both> <tower_idx>");
    }
  } else if (action == "beamforming") {
    int tower_idx;
    double factor;
    if (ss >> tower_idx >> factor) {
      if (tower_idx >= 0 && tower_idx < simulator.get_tower_count()) {
        simulator.apply_beamforming_to_tower(tower_idx, factor);
        Logger::success("Applied " + std::to_string(factor) + "x beamforming to Tower #" + std::to_string(tower_idx));
      } else {
        Logger::error("Invalid tower index.");
      }
    } else {
      Logger::error("Usage: beamforming <tower_idx> <factor>");
    }
  } else if (action == "handovers") {
    int count;
    if (ss >> count) {
      HandoverManager hom(&simulator);
      hom.simulate_random_handovers(count);
    } else {
      Logger::error("Usage: handovers <count>");
    }
  } else {
    Logger::error("Unknown command. Type 'help' for options.");
  }
}

void ConsoleTUI::handleModalInput(int key) {
  if (key == 27) { // Escape
    activeModal = ModalType::NONE;
    return;
  }
  
  if (key == 9 || key == 1002) { // Tab or Down
    activeFieldIdx = (activeFieldIdx + 1) % modalFields.size();
    return;
  }
  if (key == 1001) { // Up
    activeFieldIdx = (activeFieldIdx - 1 + modalFields.size()) % modalFields.size();
    return;
  }
  
  if (key == 127 || key == 8) { // Backspace
    if (!modalFields[activeFieldIdx].value.empty()) {
      modalFields[activeFieldIdx].value.pop_back();
    }
    return;
  }
  
  if (key == 10 || key == 13) { // Enter (submit!)
    if (activeModal == ModalType::CREATE_TOWER) {
      std::string gen = modalFields[0].value;
      std::string loc = modalFields[1].value;
      double bw = 20.0;
      int cores = 4;
      try {
        bw = std::stod(modalFields[2].value);
        cores = std::stoi(modalFields[3].value);
      } catch (...) {}
      
      try {
        simulator.create_tower(gen, loc, bw, cores);
        simulator.get_tower(simulator.get_tower_count() - 1)->set_position(rand() % 800 + 100, rand() % 800 + 100);
      } catch (const std::exception &e) {
        Logger::error(std::string("Error: ") + e.what());
      }
    } else if (activeModal == ModalType::CREATE_DEVICE) {
      std::string gen = modalFields[0].value;
      std::string name = modalFields[1].value;
      std::string typeStr = modalFields[2].value;
      int tower_idx = 0;
      try {
        tower_idx = std::stoi(modalFields[3].value);
      } catch (...) {}
      
      CommunicationType ct = CommunicationType::BOTH;
      if (typeStr == "data") ct = CommunicationType::DATA;
      else if (typeStr == "voice") ct = CommunicationType::VOICE;
      
      try {
        simulator.create_and_connect_device(gen, name, ct, tower_idx);
      } catch (const std::exception &e) {
        Logger::error(std::string("Error: ") + e.what());
      }
    } else if (activeModal == ModalType::LOAD_SCENARIO) {
      std::string name = modalFields[0].value;
      ScenarioManager::ScenarioType type;
      bool valid = true;
      if (name == "urban") type = ScenarioManager::ScenarioType::URBAN_DENSE;
      else if (name == "suburban") type = ScenarioManager::ScenarioType::SUBURBAN;
      else if (name == "rural") type = ScenarioManager::ScenarioType::RURAL;
      else if (name == "highway") type = ScenarioManager::ScenarioType::HIGHWAY;
      else if (name == "stadium") type = ScenarioManager::ScenarioType::STADIUM;
      else if (name == "disaster") type = ScenarioManager::ScenarioType::DISASTER_RECOVERY;
      else if (name == "mixed") type = ScenarioManager::ScenarioType::MIXED_GENERATION;
      else {
        valid = false;
        Logger::error("Unknown scenario: " + name);
      }
      if (valid) {
        ScenarioManager::load_scenario(simulator, type);
        selectedTowerIdx = 0;
        throughputHistory.assign(throughputHistory.size(), 0);
        lastProcessedMessages = 0;
        Logger::success("Loaded scenario: " + name);
      }
    }
    activeModal = ModalType::NONE;
    return;
  }
  
  if (key >= 32 && key <= 126) {
    modalFields[activeFieldIdx].value += static_cast<char>(key);
  }
}

void ConsoleTUI::updateMobility() {
  int dev_count = simulator.get_device_count();
  int tower_count = simulator.get_tower_count();
  if (dev_count == 0 || tower_count == 0) return;
  
  HandoverManager hom(&simulator);
  
  for (int i = 0; i < dev_count; ++i) {
    auto dev = simulator.get_device(i);
    if (!dev) continue;
    
    // 1. Move connected devices randomly (random walk)
    if (dev->get_connection_status()) {
      double dx = (rand() % 21) - 10; // -10m to 10m
      double dy = (rand() % 21) - 10;
      double new_x = dev->get_x() + dx;
      double new_y = dev->get_y() + dy;
      if (new_x < 0) new_x = 0;
      if (new_x > 1000) new_x = 1000;
      if (new_y < 0) new_y = 0;
      if (new_y > 1000) new_y = 1000;
      dev->set_position(new_x, new_y);
    } else {
      // Disconnected devices also drift slowly
      double dx = (rand() % 11) - 5;
      double dy = (rand() % 11) - 5;
      double new_x = dev->get_x() + dx;
      double new_y = dev->get_y() + dy;
      if (new_x < 0) new_x = 0;
      if (new_x > 1000) new_x = 1000;
      if (new_y < 0) new_y = 0;
      if (new_y > 1000) new_y = 1000;
      dev->set_position(new_x, new_y);
    }
    
    // 2. Find current tower index
    int current_tower_idx = -1;
    for (int t = 0; t < tower_count; ++t) {
      auto tower = simulator.get_tower(t);
      if (tower && tower->get_device(dev->get_device_id())) {
        current_tower_idx = t;
        break;
      }
    }
    
    // 3. Compute RSSI to all towers to find the best connection
    int best_tower_idx = -1;
    double max_rssi = -120.0;
    
    for (int t = 0; t < tower_count; ++t) {
      auto tower = simulator.get_tower(t);
      if (!tower) continue;
      
      double dx = dev->get_x() - tower->get_x();
      double dy = dev->get_y() - tower->get_y();
      double dist = sqrt(dx*dx + dy*dy);
      
      bool has_los = simulator.check_line_of_sight(dev->get_x(), dev->get_y(), tower->get_x(), tower->get_y());
      double rssi = simulator.compute_rssi(dist, !has_los);
      
      if (rssi > max_rssi) {
        max_rssi = rssi;
        best_tower_idx = t;
      }
    }
    
    // 4. Update device connectivity status
    if (max_rssi > -115.0) {
      if (current_tower_idx == -1) {
        // Try connecting if previously disconnected
        auto best_tower = simulator.get_tower(best_tower_idx);
        if (best_tower && best_tower->connect_device(dev)) {
          Logger::success("Device " + dev->get_device_name() + " connected to Tower #" + std::to_string(best_tower_idx) + " (RSSI: " + std::to_string((int)max_rssi) + " dBm)");
        }
      } else if (best_tower_idx != current_tower_idx) {
        // Trigger RSSI-based handover with 3 dB hysteresis margin
        auto current_tower = simulator.get_tower(current_tower_idx);
        double curr_dist = sqrt(pow(dev->get_x() - current_tower->get_x(), 2) + pow(dev->get_y() - current_tower->get_y(), 2));
        bool curr_los = simulator.check_line_of_sight(dev->get_x(), dev->get_y(), current_tower->get_x(), current_tower->get_y());
        double curr_rssi = simulator.compute_rssi(curr_dist, !curr_los);
        
        if (max_rssi > curr_rssi + 3.0) {
          if (hom.perform_handover(dev->get_device_id(), current_tower_idx, best_tower_idx)) {
            std::string logMsg = "Handover: " + dev->get_device_name() + " shifted Tower " + std::to_string(current_tower_idx) + " ➜ " + std::to_string(best_tower_idx) + " (" + std::to_string((int)max_rssi) + " dBm)";
            Logger::info(logMsg);
          }
        }
      }
    } else {
      // Disconnect if RSSI is too weak
      if (current_tower_idx != -1) {
        auto current_tower = simulator.get_tower(current_tower_idx);
        if (current_tower) {
          current_tower->disconnect_device(dev->get_device_id());
          Logger::warning("Device " + dev->get_device_name() + " lost signal (" + std::to_string((int)max_rssi) + " dBm)");
        }
      }
    }
  }
}

void ConsoleTUI::run() {
  setupTerminal();
  
  while (!quit) {
    int rows, cols;
    getTerminalSize(rows, cols);
    
    // Calculate throughput tick diff
    long long total_msg = simulator.get_total_messages();
    int diff = (lastProcessedMessages == 0) ? 0 : static_cast<int>(total_msg - lastProcessedMessages);
    lastProcessedMessages = total_msg;
    throughputHistory.push_back(diff);
    int target_size = cols - 4;
    if (target_size < 10) target_size = 10;
    if ((int)throughputHistory.size() < target_size) {
      throughputHistory.insert(throughputHistory.begin(), target_size - throughputHistory.size(), 0);
    } else if ((int)throughputHistory.size() > target_size) {
      throughputHistory.erase(throughputHistory.begin(), throughputHistory.begin() + (throughputHistory.size() - target_size));
    }

    // Run spatial mobility simulation updates
    updateMobility();

    // CRITICAL: Clear entire screen content area to prevent ghost artifacts
    std::cout << "\033[H\033[?25l";
    clearContentArea(rows, cols);

    drawHeader(cols);
    
    if (activeTab == 0) drawDashboard(rows, cols);
    else if (activeTab == 1) drawTowersTab(rows, cols);
    else if (activeTab == 2) drawDevicesTab(rows, cols);
    else if (activeTab == 3) drawAnalyticsTab(rows, cols);
    else if (activeTab == 4) drawMapTab(rows, cols);
    else if (activeTab == 5) drawActionsTab(rows, cols);
    else if (activeTab == 6) drawHelpTab(rows, cols);
    
    drawCommandBar(rows, cols);
    drawFooter(cols);

    // Draw toast notifications overlay
    drawToasts(rows, cols);

    // If modal is active, draw it on top
    if (activeModal != ModalType::NONE) {
      drawModal(rows, cols);
    }

    // Increment frame counter for animations
    frameCounter++;
    
    std::cout << std::flush;
    
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 300000; // 300ms timeout for live dashboard rendering
    
    int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    if (ret > 0) {
      char c;
      if (::read(STDIN_FILENO, &c, 1) > 0) {
        int ch = static_cast<unsigned char>(c);
        if (ch == 27) {
          // arrow or escape check
          fd_set fds_esc;
          FD_ZERO(&fds_esc);
          FD_SET(STDIN_FILENO, &fds_esc);
          struct timeval tv_esc;
          tv_esc.tv_sec = 0;
          tv_esc.tv_usec = 50000; // 50ms wait for escape sequence characters
          int esc_ret = select(STDIN_FILENO + 1, &fds_esc, nullptr, nullptr, &tv_esc);
          if (esc_ret > 0) {
            char c1;
            if (::read(STDIN_FILENO, &c1, 1) > 0) {
              int next1 = static_cast<unsigned char>(c1);
              if (next1 == 91 || next1 == 79) { // '[' or 'O'
                char c2;
                if (::read(STDIN_FILENO, &c2, 1) > 0) {
                  int next2 = static_cast<unsigned char>(c2);
                  if (next2 == 'A') {
                    handleInput(1001); // Up
                  } else if (next2 == 'B') {
                    handleInput(1002); // Down
                  } else if (next2 == 'C') {
                    handleInput(1003); // Right
                  } else if (next2 == 'D') {
                    handleInput(1004); // Left
                  } else if (next2 == '<') {
                    // SGR Mouse Mode: \033[<button;col;row[M/m]
                    std::string mouse_seq = "";
                    char mc;
                    while (true) {
                      if (::read(STDIN_FILENO, &mc, 1) <= 0) break;
                      mouse_seq += mc;
                      if (mc == 'M' || mc == 'm') break;
                    }
                    int button = 0, col = 0, row = 0;
                    char release_char = mouse_seq.back();
                    // Replace semicolons with spaces for easy parsing
                    for (char &cms : mouse_seq) {
                      if (cms == ';') cms = ' ';
                    }
                    std::stringstream ss(mouse_seq);
                    if (ss >> button >> col >> row) {
                      handleMouseEvent(button, col - 1, row - 1, (release_char == 'M'));
                    }
                  } else if (next2 == 'M') {
                    // Normal Mouse Mode: \033[M followed by 3 bytes: button, col, row
                    char b_char, c_char, r_char;
                    if (::read(STDIN_FILENO, &b_char, 1) > 0 &&
                        ::read(STDIN_FILENO, &c_char, 1) > 0 &&
                        ::read(STDIN_FILENO, &r_char, 1) > 0) {
                      int button = static_cast<unsigned char>(b_char) - 32;
                      int col = static_cast<unsigned char>(c_char) - 32;
                      int row = static_cast<unsigned char>(r_char) - 32;
                      bool is_press = (button != 3);
                      int norm_button = button;
                      if (button >= 64) {
                        norm_button = button;
                      } else {
                        norm_button = button & 3;
                      }
                      handleMouseEvent(norm_button, col - 1, row - 1, is_press);
                    }
                  }
                }
              }
            }
          } else {
            handleInput(27); // Escape key
          }
        } else {
          handleInput(ch);
        }
      }
    } else {
      // Idle cycle: simulate traffic to keep graph going
      if (simulator.get_tower_count() > 0) {
        simulator.simulate_traffic_fluctuation();
      }
    }
  }
  
  restoreTerminal();
}
