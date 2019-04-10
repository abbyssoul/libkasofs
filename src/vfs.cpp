/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
#include "kasofs/vfs.hpp"

#include <solace/posixErrorDomain.hpp>


using namespace kasofs;
using namespace Solace;


const StringLiteral Vfs::kThisDir{"."};
//static const StringLiteral kParentDir("..");


static VfsOps ramFS {
    .read  = [](INode const&) -> ByteReader { return {}; },
    .write = [](INode const&) -> ByteWriter { return {}; }
};


Vfs::Vfs(User owner, FilePermissions rootPerms)
    : index{{INode{INode::Type::Directory, owner, rootPerms, 0}}}
    , adjacencyList{1}
    , vfs{ {ramFS} }
{
    auto& root = index[0];
    root.dataIndex = 0;
    root.dataCount = 1;
}


//INode const& Vfs::root() const { return inodes.at(0); }
//INode& Vfs::root() { return inodes.at(0); }

Result<VfsID, Error>
Vfs::registerFileSystem(VfsOps&& fs) {
    auto const fsId = vfs.size();
    vfs.emplace_back(std::move(fs));

    return Ok(static_cast<VfsID>(fsId));
}

Result<void, Error>
Vfs::unregisterFileSystem(VfsID fsId) {
    if (fsId >= vfs.size()) {
        return Err(makeError(GenericError::BADF, "unregisterFileSystem"));
    }

    // FIXME: Check if fs is busy / in use
    vfs.erase(vfs.begin() + fsId);

    return Ok();
}


Result<void, Error>
Vfs::mount(User user, VNodeId mountingPoint, VfsID fs) {
    auto maybeMountingPoint = nodeById(mountingPoint);
    if (!maybeMountingPoint) {
        return Err(makeError(GenericError::BADF, "mount"));
    }

    auto& dir = **maybeMountingPoint;
    if (!dir.userCan(user, Permissions::WRITE)) {
        return Err(makeError(GenericError::PERM, "mount"));
    }

    mounts.emplace_back(fs, mountingPoint);

    return Ok();
}


Result<void, Error>
Vfs::umount(User user, VNodeId mountingPoint) {
    auto maybeMountingPoint = nodeById(mountingPoint);
    if (!maybeMountingPoint) {
        return Err(makeError(GenericError::BADF, "mount"));
    }

    auto& dir = **maybeMountingPoint;
    if (!dir.userCan(user, Permissions::WRITE)) {
        return Err(makeError(GenericError::PERM, "mount"));
    }

    auto mntEntriesId = std::remove_if(mounts.begin(), mounts.end(), [mountingPoint](auto const& n) { return (mountingPoint == n.mountingPoint); });
    for (auto i = mntEntriesId; i != mounts.end(); ++i) {
        vfs.erase(vfs.end());
    }
    mounts.erase(mntEntriesId, mounts.end());

    return Ok();
}


Result<void, Error>
Vfs::link(User user, StringView linkName, VNodeId from, VNodeId to) {
    if (from == to) {
        return Err(makeError(GenericError::BADF, "link:from::to"));
    }

    auto maybeNode = nodeById(from);
    if (!maybeNode) {
        return Err(makeError(GenericError::NOENT, "link:from"));
    }

    auto& node = **maybeNode;
    if (node.type() != INode::Type::Directory) {
        return Err(makeError(GenericError::NOTDIR, "link"));
    }

    if (!node.userCan(user, Permissions::WRITE)) {
        return Err(makeError(GenericError::PERM, "link"));
    }

    if (nodeById(to).isNone()) {
        return Err(makeError(GenericError::NOENT, "link:to"));
    }

    // Add new entry:
    // TODO: Check dataIndex exist?
    adjacencyList[node.dataIndex].emplace_back(linkName, to);

    return Ok();
}


Result<void, Error>
Vfs::unlink(User user, StringView name, VNodeId from) {
    auto maybeNode = nodeById(from);
    if (!maybeNode) {
        return Err(makeError(GenericError::BADF, "unlink"));
    }

    auto& node = **maybeNode;
    if (node.type() != INode::Type::Directory) {
        return Err(makeError(GenericError::NOTDIR, "unlink"));
    }

    if (!node.userCan(user, Permissions::WRITE)) {
        return Err(makeError(GenericError::PERM, "unlink"));
    }

    // Remove named entry:
    auto& entries = adjacencyList[node.dataIndex];
    entries.erase(std::remove_if(entries.begin(), entries.end(), [name](auto const& entry) { return (name == entry.name); }),
                              entries.end());
//        return Err(makeError(GenericError::NOENT, "link:to"));

    return Ok();
}

Optional<Entry>
Vfs::lookup(VNodeId dirNodeId, StringView name) const {
    auto const maybeNode = nodeById(dirNodeId);
    if (!maybeNode) {
        return none;
    }

    auto const& node = **maybeNode;
    if (node.type() != INode::Type::Directory) {
        return none;
    }

    auto const& enties = adjacencyList[node.dataIndex];
    auto it = std::find_if(enties.begin(), enties.end(), [name](Entry const& e) { return e.name == name; });

    return (it == enties.end())
            ? none
            : Optional{*it};
}


Optional<INode*>
Vfs::nodeById(VNodeId id) noexcept {
    if (id >= index.size()) {
        return none;
    }

    return &(index.at(id));
}

Optional<INode const*>
Vfs::nodeById(VNodeId id) const noexcept {
    if (id >= index.size()) {
        return none;
    }

    return &(index.at(id));
}


Result<VNodeId, Error>
Vfs::createDirectory(VNodeId where, StringView name, User user, FilePermissions perms) {
    return mknode(INode::Type::Directory, user, perms, where, name);
}


Result<EntriesIterator, Error>
Vfs::enumerateDirectory(VNodeId dirNodeId, User user) const {
    auto maybeNode = nodeById(dirNodeId);
    if (!maybeNode) {
        return Err(makeError(GenericError::BADF, "enumerateDirectory"));
    }

    auto& dir = **maybeNode;
    if (dir.type() != INode::Type::Directory) {
        return Err(makeError(GenericError::NOTDIR, "enumerateDirectory"));
    }

    if (!dir.userCan(user, Permissions::READ)) {
        return Err(makeError(GenericError::PERM, "enumerateDirectory"));
    }

    auto& entries = adjacencyList[dir.dataIndex];
    return Ok(EntriesIterator{entries.data(), entries.data() + entries.size()});
}


Result<VNodeId, Error>
Vfs::createFile(VNodeId where, StringView name, User user, FilePermissions perms) {
    return mknode(INode::Type::Data, user, perms, where, name);
}


Result<ByteReader, Error>
Vfs::reader(User user, VNodeId nodeId) {
    auto maybeNode = nodeById(nodeId);
    if (!maybeNode) {
        return Err(makeError(GenericError::BADF, "reader"));
    }

    auto& node = **maybeNode;
    if (node.type() != INode::Type::Data) {
        return Err(makeError(GenericError::ISDIR, "reader"));
    }

    if (!node.userCan(user, Permissions::READ)) {
        return Err(makeError(GenericError::PERM, "reader"));
    }

    auto& fs = vfs[node.deviceId];
    if (!fs.read)
        return Err(makeError(GenericError::IO, "reader"));

    return Ok(fs.read(node));
}


Result<ByteWriter, Error>
Vfs::writer(User user, VNodeId nodeId) {
    auto maybeNode = nodeById(nodeId);
    if (!maybeNode) {
        return Err(makeError(GenericError::BADF, "writer"));
    }

    auto& node = **maybeNode;
    if (node.type() != INode::Type::Data) {
        return Err(makeError(GenericError::ISDIR, "writer"));
    }

    if (!node.userCan(user, Permissions::READ)) {
        return Err(makeError(GenericError::PERM, "writer"));
    }

    auto& fs = vfs[node.deviceId];
    if (!fs.write)
        return Err(makeError(GenericError::IO, "writer"));

    return Ok(fs.write(node));
}


Result<VNodeId, Error>
Vfs::mknode(INode::Type type, User owner, FilePermissions perms, VNodeId where, StringView name) {
    auto maybePermissions = effectivePermissionsFor(owner, where, perms, type);
    if (!maybePermissions) {
        return Err(maybePermissions.getError());
    }

    auto const nextIndex = index.size();
    auto const newNodeIndex = VNodeId{static_cast<uint32>(nextIndex)};
    switch (type) {
    case INode::Type::Directory: {
         auto& node = index.emplace_back(INode::Type::Directory, owner, *maybePermissions, nextIndex);
         node.dataIndex = adjacencyList.size();
         adjacencyList.emplace_back();
        } break;
    case INode::Type::Data: {
        index.emplace_back(INode::Type::Data, owner, *maybePermissions, nextIndex);
        } break;
    }

    // Link   
    nodeById(where)
            .map([this, name, newNodeIndex](auto& parent) {
                adjacencyList[parent->dataIndex].emplace_back(name, newNodeIndex);
                return 0;
            });


    return Ok(newNodeIndex);
}


Result<FilePermissions, Error>
Vfs::effectivePermissionsFor(User user, VNodeId rootIndex, FilePermissions perms, INode::Type type) const noexcept {
    auto const maybeRoot = nodeById(rootIndex);
    if (!maybeRoot) {
        return Err(makeError(GenericError::NOENT , "mkNode"));
    }

    auto const& dir = **maybeRoot;
    if (!dir.userCan(user, Permissions::WRITE)) {
        return Err(makeError(GenericError::PERM, "mkNode"));
    }

    auto const dirPerms = dir.permissions;
    uint32 const permBase = (type == INode::Type::Directory)
            ? 0666
            : 0777;

    return Ok(FilePermissions{perms.value & (~permBase | (dirPerms.value & permBase))});
}
