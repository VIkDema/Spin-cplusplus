#pragma once
#include "launch_settings.hpp"
#include <string>

class PreProcSettings {
public:
  void Init();
  void SetCommand(const std::string &command);
  std::string GetCommand();
  bool IsDefault();
  void Preprocess(const std::string &a, const std::string &b, int a_tmp,
                  LaunchSettings &launch_settings);

private:
  bool was_changed_ = false;
  std::string command_;
};

static PreProcSettings pre_proc_processor;