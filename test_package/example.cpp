#include "kasofs/kasofs.hpp"
#include <cstdlib>  // EXIT_SUCCESS/EXIT_FAILURE


int main() {
    auto owner = kasofs::User{0, 0};
    auto vfs = kasofs::Vfs{owner, kasofs::FilePermissions{0707}};

    auto maybeRoot = vfs.nodeById(vfs.rootId());
    if (!maybeRoot)
        return EXIT_FAILURE;

    return (*maybeRoot).userCan(owner, kasofs::Permissions::WRITE)
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
}
