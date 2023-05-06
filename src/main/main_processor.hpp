#pragma once

#include "launch_settings.hpp"

class MainProcessor {
public:
  int main(int argc, char *argv[]);

private:
  int ChangeParam(std::string &t, const std::string &what, int range,
                  int bottom);
  void InitSeed();
  void InitStreams();
  void InitScope();
  void InitPreProcSettings();
  bool HandleLaunchSettings(LaunchSettings &launch_settings, int argc,
                            char *argv[]);
  void InitSymbols();
  void Exit(int error_status, LaunchSettings &launch_settings);
  int e_system(int code, const std::string &message);
  void ChangeRandomSeed(std::string& t);
  int OmitStr(std::string& in, const std::string& s);
  void StringTrim(std::string& t);

  std::string out_;
};
