#include "pan_processor.hpp"

PanProcessor pan_processor_;

struct {
  std::string ifsee;
  std::string thendo;
  int keeparg;
} pats[] = {{"-bfspar", "-DBFS_PAR", 0},
            {"-bfs", "-DBFS", 0},
            {"-bcs", "-DBCS", 0},
            {"-bitstate", "-DBITSTATE", 0},
            {"-bit", "-DBITSTATE", 0},
            {"-hc", "-DHC4", 0},
            {"-collapse", "-DCOLLAPSE", 0},
            {"-noclaim", "-DNOCLAIM", 0},
            {"-noreduce", "-DNOREDUCE", 0},
            {"-np", "-DNP", 0},
            {"-permuted", "-DPERMUTED", 0},
            {"-p_permute", "-DPERMUTED", 1},
            {"-p_rotate", "-DPERMUTED", 1},
            {"-p_reverse", "-DPERMUTED", 1},
            {"-rhash", "-DPERMUTED", 1},
            {"-safety", "-DSAFETY", 0},
            {"-i", "-DREACH", 1},
            {"-l", "-DNP", 1},
            {"", ""}};

void PanProcessor::AddRuntime(const std::string &add) {
  if (add.compare(0, 9, "-biterate") == 0) {
    itsr_ = 10; /* default nr of sequential iterations */
    sw_or_bt_ = 1;
    if (isdigit(add[9])) {
      itsr_ = std::stoi(add.substr(9));
      if (itsr_ < 1) {
        itsr_ = 1;
      }
      SetItsr_n(add);
    }
    return;
  }
  if (add.compare(0, 6, "-swarm") == 0) {
    itsr_ = -10; /* parallel iterations */
    sw_or_bt_ = 1;
    if (isdigit(add[6])) {
      itsr_ = std::stoi(add.substr(6));
      if (itsr_ < 1) {
        itsr_ = 1;
      }
      itsr_ = -itsr_;
      SetItsr_n(add);
    }
    return;
  }

  for (const auto &pat : pats) {
    if (add.find(pat.ifsee) == 0) {
      AddComptime(pat.thendo);
      if (pat.keeparg) {
        break;
      }
      return;
    }
  }

  if (add.find("-dfspar") == 0) {
    AddComptime("-DNCORE=4");
    return;
  }

  pan_runtime_ = fmt::format(" {}", add);
  return;
}

void PanProcessor::SetItsr_n(const std::string &s) /* e.g., -swarm12,3 */
{
  auto tmp = std::find(s.begin(), s.end(), ',');
  if (tmp != s.end() && std::isdigit(*(tmp + 1))) {
    itsr_n_ = std::stoi(std::string(tmp + 1, s.end()));
    if (itsr_n_ < 2) {
      itsr_n_ = 0;
    }
  }
}

void PanProcessor::AddComptime(const std::string &add) {
  if (pan_comptime_.find(add) != std::string::npos) {
    return;
  }
  pan_comptime_ += fmt::format(" {}", add);
}

std::string& PanProcessor::GetPanRuntime() { return pan_runtime_; }

std::string& PanProcessor::GetPanComptime() { return pan_comptime_; }