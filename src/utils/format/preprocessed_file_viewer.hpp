#pragma once
#include "../../spin.hpp"
#include <string>

namespace format {
/**
 * @class PreprocessedFileViewer
 * @brief Provides functionality to view preprocessed files.
 */
class PreprocessedFileViewer {
public:
  /**
   * @brief View the preprocessed file.
   */
  void view();

private:
  /**
   * @brief Recursively view a proc list.
   * @param[in] node The proc list to view.
   */
  void recursive_view(models::ProcList *node);
  /**
   * @brief Recursively view a sequence.
   * @param[in] sequence The sequence to view.
   */
  void recursive_view_sequence(models::Sequence *s);
  /**
   * @brief Recursively view an element.
   * @param[in] element The element to view.
   */
  void recursive_view_element(models::Element *e);
  /**
   * @brief Print the current indentation.
   */
  void doindent();
  /**
   * @brief Decrease the indentation level.
   */
  void decrease_indentation();
  /**
   * @brief Increase the indentation level.
   */
  void increase_indentation();

  int indent = 0;
};
} // namespace format