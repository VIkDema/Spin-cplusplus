#include "sequence.hpp"
#include "../utils/memory.hpp"

namespace models {

SeqList *SeqList::Build(Sequence *sequence) {
  SeqList *t = (models::SeqList *)emalloc(sizeof(models::SeqList));
  t->this_sequence = sequence;
  t->next = nullptr;
  return t;
}

SeqList *SeqList::Add(Sequence *sequence) {
  SeqList *t = (models::SeqList *)emalloc(sizeof(models::SeqList));
  t->this_sequence = sequence;
  t->next = this;
  return t;
}

} // namespace models
