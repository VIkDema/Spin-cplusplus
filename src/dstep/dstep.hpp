#pragma once
#include "../models/models_fwd.hpp"
#include <cstdio>

namespace dstep {
/**
 * @brief Processes a sequence and generates code for it.
 *
 * The `putcode` function processes a sequence and generates code for it based
 * on its structure and node types. It handles different cases such as `UNLESS`,
 * `NON_ATOMIC`, `IF`, `'.'`, `'R'`, `'r'`, `'s'`, `'c'`, `ELSE`, and `ASGN`. It
 * also sets various flags and updates the state before generating the code.
 *
 * @param fd The file pointer for the output file.
 * @param s The sequence to process.
 * @param next The next element in the sequence.
 * @param justguards Indicates if only the guards should be processed.
 * @param ln The line number of the sequence.
 * @param seqno The sequence number.
 * @return The value of the LastGoto flag.
 */
int putcode(FILE *fd, models::Sequence *s, models::Element *next,
            int justguards, int ln, int seqno);
} // namespace dstep