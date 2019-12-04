/******************************************************************************
 * Copyright (c) 2019 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include <string>

#include <phasar/PhasarLLVM/Pointer/PointsToInfo.h>

namespace psr {

std::string to_string(AliasResult AR) {
  switch (AR) {
  case AliasResult::NoAlias:
    return "NoAlias";
    break;
  case AliasResult::MayAlias:
    return "MayAlias";
    break;
  case AliasResult::MustAlias:
    return "MustAlias";
    break;
  }
}

AliasResult to_AliasResult(const std::string &S) {
  if (S == "NoAlias") {
    return AliasResult::NoAlias;
  } else if (S == "MayAlias") {
    return AliasResult::MayAlias;
  } else {
    return AliasResult::MustAlias;
  }
}

} // namespace psr