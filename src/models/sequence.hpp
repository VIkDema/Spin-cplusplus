#pragma once

#include "models_fwd.hpp"

namespace models {

/**
 * @struct Sequence
 * Structure representing a sequence.
 */
struct Sequence {
    Element *frst; /**< First element in the sequence. */
    Element *last; /**< Last element in the sequence, links onto continuations. */
    Element *extent; /**< Last element in the original sequence. */
    int minel; /**< Minimum Seqno, set and used only in guided.c. */
    int maxel; /**< 1+largest id in the sequence. */
};

/**
 * @struct SeqList
 * Structure representing a sequence list.
 */
struct SeqList {
    Sequence *this_sequence; /**< Pointer to a sequence. */
    SeqList *next; /**< Pointer to the next sequence in the linked list. */
    
    /**
     * @brief Build a sequence list with a given sequence.
     * @param sequence The sequence to build the list with.
     * @return The constructed sequence list.
     */
    static SeqList *Build(Sequence *sequence);
    
    /**
     * @brief Add a sequence to the sequence list.
     * @param sequence The sequence to add.
     * @return The updated sequence list.
     */
    SeqList *Add(Sequence *sequence);
};

} // namespace models