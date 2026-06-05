#ifndef CONSOLE_TUI_H
#define CONSOLE_TUI_H

#include "Simulator.h"
#include "NetworkAnalytics.h"
#include <string>
#include <vector>
#include <termios.h>
#include <chrono>

enum class ModalType {
  NONE,
  CREATE_TOWER,
  CREATE_DEVICE,
  LOAD_SCENARIO
};

struct ModalField {
  std::string label;
  std::string value;
  std::string placeholder;
  bool is_numeric;
};

struct ToastNotification {
  std::string message;
  std::string color;
  int ttl; // frames remaining
};

class ConsoleTUI {
private:
  Simulator &simulator;
  NetworkAnalytics analytics;
  struct termios orig_termios;
  int activeTab;             // 0: Dashboard, 1: Towers, 2: Devices, 3: Analytics, 4: Visual Map, 5: Actions, 6: Help
  int selectedTowerIdx;      // For Towers tab
  int devicesScrollOffset;   // For scrollable devices list
  bool quit;

  // Command prompt and Search variables
  bool commandMode;
  std::string commandInput;
  int commandCursorPos;
  std::vector<std::string> commandHistory;
  int historyIndex;
  std::string autoSuggestHint;
  
  bool searchMode;           // Device tab search mode toggle
  std::string searchQuery;   // Device tab search query

  // Popup Modal variables
  ModalType activeModal;
  int activeFieldIdx;
  std::vector<ModalField> modalFields;

  // Throughput and stats tracking
  std::vector<int> throughputHistory;
  long long lastProcessedMessages;

  // Animation & UI state
  int frameCounter;
  std::chrono::steady_clock::time_point startTime;
  int actionsSelectedIdx;  // For Actions tab keyboard navigation
  std::vector<ToastNotification> toasts;
  int selectedMapType;     // 0: None, 1: Tower, 2: Device, 3: Core Router
  int selectedMapId;       // Selected element index/ID
  bool heatmapMode;        // Toggle RF signal heatmap mode
  bool packetFlowMode;     // Toggle live packet hop animations mode
  bool mapCursorMode;      // Toggle keyboard cursor mode for Visual Map
  int mapCursorRow;        // Keyboard cursor row
  int mapCursorCol;        // Keyboard cursor column

  // Per-tower throughput history for detailed analytics
  std::vector<std::vector<int>> towerThroughputHistory;
  std::vector<long long> lastTowerMessages;
  std::vector<std::pair<int, int>> towerGridCoords;
  std::vector<std::pair<int, int>> deviceGridCoords;
  void computeGridCoordinates(int mapH, int mapW);

  // Terminal settings
  void setupTerminal();
  void restoreTerminal();
  void getTerminalSize(int &rows, int &cols);

  // Drawing helpers
  void drawHeader(int cols);
  void drawFooter(int cols);
  void drawDashboard(int rows, int cols);
  void drawTowersTab(int rows, int cols);
  void drawDevicesTab(int rows, int cols);
  void drawAnalyticsTab(int rows, int cols);
  void drawMapTab(int rows, int cols);
  void drawActionsTab(int rows, int cols);
  void drawHelpTab(int rows, int cols);
  void drawCommandBar(int rows, int cols);
  void drawModal(int rows, int cols);
  void drawToasts(int rows, int cols);

  // Utility drawing methods
  void drawBox(int start_row, int start_col, int height, int width, const std::string &title = "");
  void drawCard(int row, int col, int height, int width, const std::string &title, const std::vector<std::pair<std::string, std::string>> &fields, const std::string &title_color = "");
  void drawHorizontalBar(int row, int col, int width, double value, double maxVal, const std::string &color);
  void printAt(int row, int col, const std::string &text, const std::string &color = "");
  int getVisibleLength(const std::string &str);
  std::string getUptime() const;
  void addToast(const std::string &msg, const std::string &color);
  
  // Interactive handlers
  void handleInput(int key);
  void handleCommandInput(int key);
  void handleModalInput(int key);
  void handleMouseEvent(int button, int col, int row, bool is_press);
  void executeCommand(const std::string &cmd);
  void updateMobility();
  void updateAutoSuggest();

public:
  explicit ConsoleTUI(Simulator &sim);
  ~ConsoleTUI();

  void run();
};

#endif // CONSOLE_TUI_H
