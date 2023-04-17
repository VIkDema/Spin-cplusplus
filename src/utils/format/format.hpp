#pragma once
#include <string>

namespace format {
// считывает stdin и выводит в stdout
class PrettyPrint {
public:
  void format();

private:
  void start_new_line(std::string &buf);
  void map_token_to_string(int n, std::string &buf);

  void doindent();
  void decrease_indentation();
  void increase_indentation();

  bool should_start_new_line(int current_token, int last_token);
  void append_space_if_needed(int current_token, int last_token,
                              std::string &buffer);
  int in_decl = 0;
  int in_c_decl = 0;
  int in_c_code = 0;
  int indent = 0;
};
} // namespace format