
#include <shoom.h>

#include "lest.hpp"

using namespace std;

const lest::test specification[] = {
    CASE("shared memory can be created and opened") {
        shoom::Shm server{"shoomtest", 64};
        EXPECT(shoom::kOK == server.Create());

        (*server)[0] = 0x12;
        (*server)[1] = 0x34;

        shoom::Shm client{"shoomtest", 64};
        EXPECT(shoom::kOK == client.Open());

        EXPECT(0x12 == (*client)[0]);
        EXPECT(0x34 == (*client)[1]);
    },

    CASE("non-existing shared memory objects err") {
        shoom::Shm client{"shoomtest", 64};
        EXPECT(shoom::kErrorOpeningFailed == client.Open());
    },
};

int main (int argc, char *argv[]) {
  return lest::run(specification, argc, argv);
}
