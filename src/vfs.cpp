/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
#include "vfs.hpp"

#include <solace/posixErrorDomain.hpp>


using namespace kasofs;
using namespace Solace;


const StringLiteral Vfs::kThisDir{"."};

//static const StringLiteral kParentDir("..");


Vfs::Vfs(User owner, FilePermissions rootPerms)
    : inodes{{INode{INode::TypeTag<INode::Type::Directory>{}, owner, 0, rootPerms}}}
{}


INode const& Vfs::root() const { return inodes.at(0); }
INode& Vfs::root() { return inodes.at(0); }


Optional<INode*> Vfs::nodeById(index_type id) noexcept {
    if (id >= inodes.size()) {
        return none;
    }

    return &(inodes.at(id));
}

Optional<INode const*> Vfs::nodeById(index_type id) const noexcept {
    if (id >= inodes.size()) {
        return none;
    }

    return &(inodes.at(id));
}

Result<void, Error>
Vfs::link(User user, StringView linkName, index_type from, index_type to) {
    if (from == to) {
        return Err(makeError(GenericError::BADF, "link:from::to"));
    }

    if (from >= inodes.size()) {
        return Err(makeError(GenericError::NOENT, "link:from"));
    }

    return link(user, linkName, inodes.at(from), to);
}


Result<void, Error>
Vfs::link(User user, StringView linkName, INode& dir, index_type to) {
    if (!dir.userCan(user, Permissions::WRITE)) {
        return Err(makeError(GenericError::PERM, "link"));
    }

    if (inodes.size() < to) {
        return Err(makeError(GenericError::NOENT, "link:to"));
    }

    dir.addDirEntry(linkName, to);

    return Ok();
}


Result<Vfs::index_type, Error>
Vfs::mknode(User owner, index_type rootIndex, StringView entName, FilePermissions perms, INode::Type type) {
    auto maybePermissions = effectivePermissionsFor(owner, rootIndex, perms, type);
    if (!maybePermissions)
        return Err(maybePermissions.getError());

    auto const nextIndex = inodes.size();
    switch (type) {
    case INode::Type::Directory: {
         inodes.emplace_back(INode::TypeTag<INode::Type::Directory>{}, owner, nextIndex, *maybePermissions);
        } break;
    case INode::Type::Data: {
        inodes.emplace_back(INode::TypeTag<INode::Type::Data>{}, owner, nextIndex, *maybePermissions);
        } break;
    case INode::Type::SyntheticDir:
    case INode::Type::Synthetic:
        return Err(makeError(GenericError::PERM, "mkNode"));
    }

    // Link
    nodeById(rootIndex)
            .map([entName, nextIndex](auto& parent) {
                parent->addDirEntry(entName, nextIndex);
                return 0;
            });

    return Ok(nextIndex);
}


