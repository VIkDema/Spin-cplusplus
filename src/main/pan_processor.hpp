#pragma once
#include <fmt/core.h>
#include <string>

class PanProcessor {
public:
  void AddRuntime(const std::string &add);

  void SetItsr_n(const std::string &s);

  void AddComptime(const std::string &add);

  std::string& GetPanRuntime();
  std::string& GetPanComptime();

  int GetItsr() { return itsr_; }
  void IncItsr() { itsr_++; }
  void DesItsr() { itsr_--; }
  void ReverseItsr() { itsr_ = -itsr_; }

  int GetSwOrBt() { return sw_or_bt_; }
  int GetItsrN() { return itsr_n_; }

private:
  std::string pan_runtime_;
  std::string pan_comptime_;
  int itsr_;
  int sw_or_bt_;
  int itsr_n_;
};
