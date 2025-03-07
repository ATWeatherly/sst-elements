// Copyright 2009-2023 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2023, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// of the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#include <common/output.h>

namespace SST {
namespace Hg {

std::ostream* output::out0_ = nullptr;
std::ostream* output::outn_ = nullptr;
std::ostream* output::err0_ = nullptr;
std::ostream* output::errn_ = nullptr;

}
}
