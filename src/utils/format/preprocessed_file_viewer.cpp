#include "preprocessed_file_viewer.hpp"

#include "../../codegen/codegen.hpp"
#include "../../spin.hpp"
#include "../../run/flow.hpp"
#include "y.tab.h"
#include <cstdio>
#include <fmt/core.h>
#include <iostream>
#include <optional>
#include <sstream>

extern models::ProcList *ready;

namespace format {

void PreprocessedFileViewer::decrease_indentation() { indent--; }
void PreprocessedFileViewer::increase_indentation() { indent++; }
void PreprocessedFileViewer::doindent() {
  for (int i = 0; i < indent; i++) {
    std::cout << "   ";
  }
}

// TODO: fix name variable
void PreprocessedFileViewer::recursive_view_sequence(
    models::Sequence *sequence) {
  models::Symbol *v;
  models::SeqList *h;

  for (models::Element *element = sequence->frst; element;
       element = element->next) {
    v = flow::HasLabel(element, 0);
    if (v) {
      std::cout << fmt::format("{}:", v->name) << std::endl;
    }

    if (element->n->node_type == UNLESS) {
      std::cout << "/* normal */{" << std::endl;
      recursive_view_sequence(element->n->seq_list->this_sequence);
      doindent();
      std::cout << "} unless {" << std::endl;
      recursive_view_sequence(element->n->seq_list->next->this_sequence);
      doindent();
      std::cout << "}; /* end unless */" << std::endl;
    } else if (element->sub) {

      switch (element->n->node_type) {
      case DO:
        doindent();
        std::cout << "do" << std::endl;
        increase_indentation();
        break;
      case IF:
        doindent();
        std::cout << "if" << std::endl;
        increase_indentation();
        break;
      }

      for (h = element->sub; h; h = h->next) {
        decrease_indentation();
        doindent();
        increase_indentation();
        std::cout << "::" << std::endl;
        recursive_view_sequence(h->this_sequence);
        std::cout << std::endl;
      }

      switch (element->n->node_type) {
      case DO:
        decrease_indentation();
        doindent();
        std::cout << "od;" << std::endl;
        break;
      case IF:
        decrease_indentation();
        doindent();
        std::cout << "fi;" << std::endl;
        break;
      }
    } else {
      if (element->n->node_type == ATOMIC || element->n->node_type == D_STEP ||
          element->n->node_type == NON_ATOMIC) {
        recursive_view_element(element);
      } else if (element->n->node_type != '.' && element->n->node_type != '@' &&
                 element->n->node_type != BREAK) {
        doindent();
        if (element->n->node_type == C_CODE) {
          std::cout << "c_code ";
          codegen::PlunkInline(stdout, element->n->symbol->name, 1, 1);
        } else if (element->n->node_type == 'c' &&
                   element->n->left->node_type == C_EXPR) {
          std::cout << "c_expr { ";
          codegen::PlunkExpr(stdout, element->n->left->symbol->name);
          std::cout << "} ->" << std::endl;
        } else {
          comment(stdout, element->n, 0);
          std::cout << ";" << std::endl;
        }
      }
    }
    if (element == sequence->last) {
      break;
    }
  }
}

void PreprocessedFileViewer::recursive_view(models::ProcList *node) {
  if (!node)
    return;
  if (node->next) {
    recursive_view(node->next);
  }

  if (node->det) {
    std::cout << "D"; /* deterministic */
  }

  std::cout << fmt::format("proctype {}()", node->n->name);

  if (node->prov) {
    std::cout << " provided ";
    comment(stdout, node->prov, 0);
  }

  std::cout << std::endl << "{" << std::endl;
  recursive_view_sequence(node->s);
  std::cout << "}" << std::endl;
}

void PreprocessedFileViewer::recursive_view_element(models::Element *element) {
  doindent();
  switch (element->n->node_type) {
  case D_STEP:
    std::cout << "d_step {" << std::endl;
    break;
  case ATOMIC:
    std::cout << "atomic {" << std::endl;
    break;
  case NON_ATOMIC:
    std::cout << " {" << std::endl;
    break;
  }
  increase_indentation();
  recursive_view_sequence(element->n->seq_list->this_sequence);
  decrease_indentation();

  doindent();
  std::cout << " };" << std::endl;
}

void PreprocessedFileViewer::view() { recursive_view(ready); }
} // namespace format