
#pragma once

#include "../runner.h"

namespace capsule {
namespace macos {

class Executor : public ExecutorInterface {
  public:
    Executor();
    ~Executor() override;
}

} // namespace macos
} // namespace capsule