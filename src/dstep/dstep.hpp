#pragma once
#include "../models/models_fwd.hpp"
#include <cstdio>

namespace dstep {
    int putcode(FILE *fd, models::Sequence *s, models::Element *next,
                int justguards, int ln, int seqno);
}