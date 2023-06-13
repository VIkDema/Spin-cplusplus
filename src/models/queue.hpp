#pragma once

#include "models_fwd.hpp"

namespace models {

/**
 * @struct Queue
 * Structure representing a queue.
 */
struct Queue {
    short qid; /**< Runtime queue index. */
    int qlen; /**< Number of messages stored. */
    int nslots; /**< Capacity of the queue. */
    int nflds; /**< Number of fields per slot. */
    int setat; /**< Last depth value changed. */
    int *fld_width; /**< Type of each field. */
    int *contents; /**< Values stored in the queue. */
    int *stepnr; /**< Depth when each message was sent. */
    char **mtp; /**< If mtype, name of the list; otherwise, NULL. */
    struct Queue *next; /**< Pointer to the next queue in the linked list. */
};


} // namespace models