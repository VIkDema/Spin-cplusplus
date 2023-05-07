#pragma once
#include "launch_settings.hpp"

class ArgumentsParser {
public:
  LaunchSettings Parse(int& argc, char **&argv);
};