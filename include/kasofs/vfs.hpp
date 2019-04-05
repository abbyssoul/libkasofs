/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
/*******************************************************************************
 * KasoFS: Virtual filesystem
 *	@file		vfs.hpp
 ******************************************************************************/
#pragma once
#ifndef KASOFS_VFS_HPP
#define KASOFS_VFS_HPP

#include "vinode.hpp"

#include <solace/path.hpp>
#include <solace/result.hpp>
#include <solace/error.hpp>
#include <solace/posixErrorDomain.hpp>

#include <vector>
#include <functional>   // FIXME: Review - is it the best implementation?


namespace kasofs {

struct VfsIndex {
    Solace::uint32 vfsIndex;
    Solace::uint32 nodexIndex;
};

struct Entry {
    constexpr Entry(Solace::StringView n, VfsIndex i) noexcept
        : name{n}
        , index{i}
    {}

    Solace::StringView  name;
    VfsIndex            index;
};


struct EntriesIterator {
    Entry const* begin() const  { return _begin; }
    Entry const* end() const    { return _end; }

    Entry const* _begin;
    Entry const* _end;
};


struct VfsOps {

    std::function<int(INode&)> read;
    std::function<int(INode&)> write;

    std::function<Solace::Optional<Entry>(INode&, Solace::StringView)>   findEntry;
    std::function<Solace::Optional<Solace::Error>(User, INode&, Solace::StringView, int)>   addEntry;
    std::function<Solace::Optional<Solace::Error>(INode&, Solace::StringView)>   removeEntry;
    std::function<EntriesIterator(INode&)>   listEntries;

    std::function<Solace::Optional<INode*>(VfsIndex)>                  nodeById;
};





struct Vfs {
    static const Solace::StringLiteral kThisDir;

    using index_type = VfsIndex;
    using size_type = std::vector<INode>::size_type;

    /**
     * Descriptor of a mounted vfs
     */
    struct Mount {
        Solace::uint32  vfsIndex;
        index_type      mountingPoint;
    };


public:

    Vfs(Vfs const&) = delete;
    Vfs& operator= (Vfs const&) = delete;

    Vfs(User rootOwner, FilePermissions rootPerms);

    Vfs(Vfs&&) = default;
    Vfs& operator= (Vfs&&) noexcept = default;

    INode& root();
    INode const& root() const;

    /// @return Number of INode in the filesystem
    size_type size() const noexcept { return inodes.size(); }

    /**
     * Find an inode by node Id.
     * @param id inode number.
     * @return Optional INode if given inode was found, none otherwise.
     */
    auto nodeById(index_type id) noexcept -> Solace::Optional<INode*>;
    auto nodeById(index_type id) const noexcept -> Solace::Optional<INode const*>;

    /**
     * Create a named link from inode A to inode B
     * @param user A user owner of the link. Note: this user must have write permission to the A inode where the link is added.
     * @param linkName Name of the link. Directory entry name to identify inode B in A.
     * @param from The index of the directory node to link to B
     * @param to The index of the link target
     * @return Result<void> on success or an error.
     *
     * @note User must have write permission to the A node in order to link it to B.
     * The link created does not change ownership or access permissions of B.
     *
     * @note It is not an error to create multiple links to the same node B with different names.
     */
    Solace::Result<void, Solace::Error>
    link(User user, Solace::StringView linkName, index_type from, index_type to);

    /**
     * Unlink given name from the given node
     * @param user Credentials of the user performing the operation. Note: User must have write permission to the from node.
     * @param linkName Name of the link to remove.
     * @param from Node a link should be removed from.
     * @return Void or error.
     */
    Solace::Result<void, Solace::Error>
    unlink(User user, Solace::StringView linkName, index_type from);

    /// Return iterator for directory entries of the give dirNode
    Solace::Result<EntriesIterator, Solace::Error>
    entries(User user, index_type dirNodeId) const;

    /**
     * Create a node of the given type and link it to the specified root.
     * @param user User - owner of a node to be created. Note this user must have write permission to the root.
     * @param rootIndex - An index of the root node to link to new node.
     * @param entName Name of the link from root node to the newly created node.
     * @param perms Permission to be set for a newly created node.
     * @param type - Type of the node to be created.
     * @return Index of the new node on success or an error.
     */
    Solace::Result<index_type, Solace::Error>
    mknode(User user, index_type rootIndex, Solace::StringView entName, FilePermissions perms, INode::Type type);

    Solace::Result<index_type, Solace::Error>
    mount(User user, index_type mountingPoint, VfsOps&& vfs);

    Solace::Result<index_type, Solace::Error>
    umount(User user, index_type mountingPoint);

    Solace::Result<Entry, Solace::Error>
    walk(User user, Solace::Path const& path) const {
        return walk(user, {0,0}, path);
    }

    Solace::Result<Entry, Solace::Error>
    walk(User user, index_type rootId, Solace::Path const& path) const {
        return walk(user, rootId, path, [](auto const& ){});
    }

    template<typename F>
    Solace::Result<Entry, Solace::Error>
    walk(User user, index_type rootId, Solace::Path const& path, F&& f) const {
        auto maybeNode = nodeById(rootId);
        if (!maybeNode) {   // Valid file id required to start the walk
            return Err(makeError(Solace::GenericError::BADF , "walk"));
        }

        auto resultingEntry = Entry{kThisDir, rootId};
        for (auto const& segment : path) {
            INode const* const node = *maybeNode;
            if (!node->userCan(user, Permissions::READ)) {
                return Err(Solace::makeError(Solace::GenericError::PERM, "walk"));
            }

            auto maybeEntry = findEntry(*node, segment.view());
            if (!maybeEntry) {
                return Err(makeError(Solace::GenericError::NOENT, "walk"));
            }

            resultingEntry = *maybeEntry;
            maybeNode = nodeById(resultingEntry.index);
            if (!maybeNode) {  // FIXME: It is fs consistency error if entry.index does not exist. Must be hadled here.
                return Err(makeError(Solace::GenericError::NXIO, "walk"));
            }

            f(**maybeNode);
        }

        return Solace::Ok(resultingEntry);
    }

protected:
    Solace::Result<void, Solace::Error>
    link(User user, Solace::StringView linkName, INode& dir, index_type to);

    Solace::Result<FilePermissions, Solace::Error>
    effectivePermissionsFor(User user, index_type rootIndex, FilePermissions perms, INode::Type type) {
        auto maybeRoot = nodeById(rootIndex);
        if (!maybeRoot) {
            return Err(makeError(Solace::GenericError::NOENT , "mkNode"));
        }

        auto& dir = **maybeRoot;
        if (!dir.userCan(user, Permissions::WRITE)) {
            return Err(makeError(Solace::GenericError::PERM, "mkNode"));
        }

        auto const dirPerms = dir.permissions;
        Solace::uint32 const permBase = (type == INode::Type::Data)
                ? 0666
                : 0777;

        return Solace::Ok(FilePermissions{perms.value & (~permBase | (dirPerms.value & permBase))});
    }

    /**
     * @brief Find directory entry in the given inode.
     * @param node Node that should represent direcotry inode.
     * @param name Name of the entry to find.
     * @return Either an enry record or none.
     */
    Solace::Optional<Entry>
    findEntry(INode const& node, Solace::StringView name) const;

private:

    /// Index nodes - vertices of a graph
    std::vector<INode> inodes;

    /// Directory entries - Named Graph edges
    std::vector<std::vector<Entry>> namedEntries;    // FIXME: Is there a better representation?

    /// Mounted filesystems
    std::vector<Mount> mounts;

    /// Registered virtual filesystems
    std::vector<VfsOps> vfs;
};


}  // namespace kasofs
#endif  // KASOFS_VFS_HPP
