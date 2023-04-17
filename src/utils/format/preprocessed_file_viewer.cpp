#include "preprocessed_file_viewer.hpp"

#include "../../spin.hpp"
#include "y.tab.h"
#include <cstdio>
#include <fmt/core.h>
#include <iostream>
#include <optional>
#include <sstream>

extern ProcList *ready;

namespace format {

void PreprocessedFileViewer::decrease_indentation() { indent--; }
void PreprocessedFileViewer::increase_indentation() { indent++; }
void PreprocessedFileViewer::doindent() {
  for (int i = 0; i < indent; i++) {
    std::cout << "   ";
  }
}

void PreprocessedFileViewer::recursive_view_sequence(Sequence *sequence) {
  Symbol *v;
  SeqList *h;

  for (Element *element = sequence->frst; element; element = element->nxt) {
    v = has_lab(element, 0);
    if (v) {
      std::cout << fmt::format("{}:", v->name) << std::endl;
    }

    if (element->n->ntyp == UNLESS) {
      std::cout << "/* normal */{" << std::endl;
      recursive_view_sequence(element->n->sl->this_sequence);
      doindent();
      std::cout << "} unless {" << std::endl;
      recursive_view_sequence(element->n->sl->nxt->this_sequence);
      doindent();
      std::cout << "}; /* end unless */" << std::endl;
    } else if (element->sub) {

      switch (element->n->ntyp) {
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

      for (h = element->sub; h; h = h->nxt) {
        decrease_indentation();
        doindent();
        increase_indentation();
        std::cout << "::" << std::endl;
        recursive_view_sequence(h->this_sequence);
        std::cout << std::endl;
      }

      switch (element->n->ntyp) {
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
      if (element->n->ntyp == ATOMIC || element->n->ntyp == D_STEP ||
          element->n->ntyp == NON_ATOMIC) {
        recursive_view_element(element);
      } else if (element->n->ntyp != '.' && element->n->ntyp != '@' &&
                 element->n->ntyp != BREAK) {
        doindent();
        if (element->n->ntyp == C_CODE) {
          std::cout << "c_code ";
          plunk_inline(stdout, element->n->sym->name, 1, 1);
        } else if (element->n->ntyp == 'c' && element->n->lft->ntyp == C_EXPR) {
          std::cout << "c_expr { ";
          plunk_expr(stdout, element->n->lft->sym->name);
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

void PreprocessedFileViewer::recursive_view(ProcList *node) {
  if (!node)
    return;
  if (node->nxt) {
    recursive_view(node->nxt);
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

void PreprocessedFileViewer::recursive_view_element(Element *element) {
  doindent();
  switch (element->n->ntyp) {
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
  recursive_view_sequence(element->n->sl->this_sequence);
  decrease_indentation();

  doindent();
  std::cout << " };" << std::endl;
}

void PreprocessedFileViewer::view() { recursive_view(ready); }
} // namespace format