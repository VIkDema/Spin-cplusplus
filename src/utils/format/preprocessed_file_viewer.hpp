#pragma once
#include "../../spin.hpp"
#include <string>

namespace format {
// считывает stdin и выводит в stdout
class PreprocessedFileViewer {
public:
  void view();

private:
  void recursive_view(models::ProcList *node);
  void recursive_view_sequence(models::Sequence *s);
  void recursive_view_element(models::Element *e);

  void doindent();
  void decrease_indentation();
  void increase_indentation();

  int indent = 0;
};
} // namespace format