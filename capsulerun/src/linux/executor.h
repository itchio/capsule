
#pragma once

#include "../runner.h"

namespace capsule {
namespace linux {

class Executor : public ExecutorInterface {
  public:
    Executor();
    ~Executor() override;
}

} // namespace linux
} // namespace capsule
