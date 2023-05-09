#pragma once

namespace file {
class LineNumber {
public:
  static void Inc();

  static void Set(int new_line_number);

  static int Get();

private:
  static int line_number_;
};
} // namespace file