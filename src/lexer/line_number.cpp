#include "line_number.hpp"

namespace file {

void LineNumber::Inc() { line_number_++; }

void LineNumber::Set(int new_line_number) { line_number_ = new_line_number; }

int LineNumber::Get() { return line_number_; }

int LineNumber::line_number_ = 0;
} // namespace file