#pragma once

#include "launch_settings.hpp"

class MainProcessor {
public:
  int main(int argc, char *argv[]);
  static int e_system(int code, const std::string &message);
  static void Exit(int error_status);

private:
  static int ChangeParam(std::string &t, const std::string &what, int range,
                         int bottom);
  void InitSeed();
  void InitStreams();
  void InitScope();
  void InitPreProcSettings();
  bool HandleLaunchSettings(int argc, char *argv[]);
  void InitSymbols();
  static void ChangeRandomSeed(std::string &t);
  static int OmitStr(std::string &in, const std::string &s);
  static void StringTrim(std::string &t);

  static std::string out_;
};
