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


#include <solace/result.hpp>
#include <solace/error.hpp>
#include <solace/posixErrorDomain.hpp>

#include <solace/byteReader.hpp>
#include <solace/byteWriter.hpp>

#include <solace/path.hpp>

#include <vector>
#include <functional>   // FIXME: Review - is it the best implementation?


namespace kasofs {

using VfsID = Solace::uint32;
using VNodeId = Solace::uint32;

using Error = Solace::Error;

//struct VNodeId {
////    VfsID           fsId;
//    Solace::uint32  nodeId;
//};

//inline bool operator== (VNodeId const& lhs, VNodeId const& rhs) noexcept {
//    return ((lhs.fsId == rhs.fsId) && (lhs.nodeId == rhs.nodeId));
//}

//inline bool operator!= (VNodeId const& lhs, VNodeId const& rhs) noexcept {
//    return ((lhs.fsId != rhs.fsId) || (lhs.nodeId != rhs.nodeId));
//}

/**
 * Directory entry
 */
struct Entry {
    constexpr Entry(Solace::StringView n, VNodeId i) noexcept
        : name{n}
        , nodeId{i}
    {}

    Solace::StringView  name;
    VNodeId             nodeId;
};


struct EntriesIterator {
    Entry const* begin() const  { return _begin; }
    Entry const* end() const    { return _end; }

    Entry const* _begin;
    Entry const* _end;
};


struct VfsOps {
    // Read  for `regular` files
    std::function<Solace::ByteReader(INode const&)> read{};
    // Write for `regular` files
    std::function<Solace::ByteWriter(INode const&)> write{};

    // Read for directories
//    std::function<Solace::Optional<Error>(User, NodexIndex, Solace::StringView, VNodeId)>  link{};
//    std::function<Solace::Optional<Error>(NodexIndex, Solace::StringView)>                  unlink{};

//    std::function<Solace::Optional<INode*>(VNodeId)>                  nodeById{};
};



/**
 * Virtual file system.
 * File system - is a graph with named edges. Each node in the graph is either a file or a directory.
 * Files are leafs of the graph. Directories - are regular vertices.
 *
 * Virtual file system allows creation of user defined graphs just like real file system. By default nodes
 * do not persist as the graph is created entirely in memory.
 *
 * Much like moder file systems - VFS allows mouning of other VFSs at a given location in the graph. That is
 * sub graph can be 'served' by a different file system.
 *
 * VFS also features simple ownerhip and permission model based on Unix uid & gid along with file access permissions.
 *
 * ```
 * /// VFS level opeartions
 * registerFileSystem
 * unregisterFileSystem
 * mount
 * unmount
 *
 * /// Universal operations on nodes of the graph
 * link
 * unlink
 * stat
 * walk / namei
 *
 * // File operations
 * createFile
 * openFile
 * read
 * write
 * close
 *
 * createDirectory
 * openDirectory
 * enumerateDirectory  // Equivalent of file's read
 * close
 * # Note: link is directory's write
 * ```
 *
 */
struct Vfs {
	static Solace::StringLiteral const kThisDir;
	static Solace::StringLiteral const kParentDir;


    using size_type = std::vector<INode>::size_type;

    /**
     * Descriptor of a mounted vfs
     */
    struct Mount {
        constexpr Mount(VfsID fsId, VNodeId nodeId) noexcept
            : vfsIndex{fsId}
            , mountingPoint{nodeId}
        {}

        VfsID       vfsIndex;
        VNodeId     mountingPoint;
    };

    struct OpenFileHandle{};

public:

    Vfs(Vfs const&) = delete;
    Vfs& operator= (Vfs const&) = delete;

    Vfs(User rootOwner, FilePermissions rootPerms);

    Vfs(Vfs&&) = default;
    Vfs& operator= (Vfs&&) noexcept = default;

    VNodeId rootId() const noexcept { return 0; }

    /////////////////////////////////////////////////////////////
    /// VFS management
    /////////////////////////////////////////////////////////////

	/**
	 * Register a new type of FS
	 * @param vfs New virtual file system operations.
	 * @return Id of the registered vfs or an error.
	 */
	Solace::Result<VfsID, Error>
    registerFileSystem(VfsOps&& vfs);

	/**
	 * Un-Register previously registered vFS
	 * @param vfsId Id of the previously registered vfs
	 * @return Void or an error.
	 */
	Solace::Result<void, Error>
	unregisterFileSystem(VfsID vfsId);

	/**
	 * Mount registered vfs to a given mount point.
	 * @param user User credentials to perform the operation as.
	 * @param mountingPoint INode where the vfs should be mounted.
	 * @param fsId Id of the previously registered vfs.
	 * @return Void or an error.
	 */
	Solace::Result<void, Error>
    mount(User user, VNodeId mountingPoint, VfsID fsId);

	/**
	 * Unmount vfs from the given mounting point.
	 * @param user User credentials to perform the operation as.
	 * @param mountingPoint INode to unmount vfs from.
	 * @return Void or an error.
	 */
	Solace::Result<void, Error>
    umount(User user, VNodeId mountingPoint);

    /////////////////////////////////////////////////////////////
    /// Graph node linking
    /////////////////////////////////////////////////////////////

    /**
     * Create a named link from inode A to inode B
     * @param user A user owner of the link. Note: this user must have write permission to the A inode where the link is added.
     * @param name Name of the link. Directory entry name to identify inode B in A.
     * @param from The index of the directory node to link to B
     * @param to The index of the link target
     * @return Result<void> on success or an error.
     *
     * @note User must have write permission to the A node in order to link it to B.
     * The link created does not change ownership or access permissions of B.
     *
     * @note It is not an error to create multiple links to the same node B with different names.
     */
	Solace::Result<void, Error>
    link(User user, Solace::StringView name, VNodeId from, VNodeId to);

    /**
     * Unlink given name from the given node
     * @param user Credentials of the user performing the operation. Note: User must have write permission to the from node.
     * @param name Name of the link to remove.
     * @param from Node a link should be removed from.
     * @return Void or error.
     */
	Solace::Result<void, Error>
    unlink(User user, Solace::StringView name, VNodeId from);

    /**
     * Find an inode by node Id. Equivalent to FS stat call
     * @param id inode number.
     * @return Optional INode if given inode was found, none otherwise.
     */
	auto nodeById(VNodeId id) const noexcept -> Solace::Optional<INode>;

	auto nodeById(Solace::Result<VNodeId, Error> const& maybeId) const noexcept {
		return maybeId
				? nodeById(*maybeId)
				: Solace::none;
	}


	Solace::Result<Entry, Error>
    walk(User user, Solace::Path const& path) const {
        return walk(user, rootId(), path);
    }

	Solace::Result<Entry, Error>
    walk(User user, VNodeId rootId, Solace::Path const& path) const {
        return walk(user, rootId, path, [](auto const& ){});
    }

    template<typename F>
	Solace::Result<Entry, Error>
    walk(User user, VNodeId rootId, Solace::Path const& path, F&& f) const {
        auto maybeNode = nodeById(rootId);
        if (!maybeNode) {   // Valid file id required to start the walk
            return Err(makeError(Solace::GenericError::BADF , "walk"));
        }

        auto resultingEntry = Entry{kThisDir, rootId};
        for (auto const& segment : path) {
			INode node = *maybeNode;
			if (!node.userCan(user, Permissions::READ)) {
                return Err(makeError(Solace::GenericError::PERM, "walk"));
            }

            auto maybeEntry = lookup(resultingEntry.nodeId, segment.view());
            if (!maybeEntry) {
                return Err(makeError(Solace::GenericError::NOENT, "walk"));
            }

			resultingEntry = maybeEntry.move();
            maybeNode = nodeById(resultingEntry.nodeId);
            if (!maybeNode) {  // FIXME: It is fs consistency error if entry.index does not exist. Must be hadled here.
                return Err(makeError(Solace::GenericError::NXIO, "walk"));
            }

			f(*maybeNode);
        }

        return Solace::Ok(resultingEntry);
    }

    /////////////////////////////////////////////////////////////
    /// Directory management
    /////////////////////////////////////////////////////////////
	Solace::Result<VNodeId, Error>
    createDirectory(VNodeId where, Solace::StringView name, User user, FilePermissions perms);

    /// Return iterator for directory's entries of the give dirNode
	Solace::Result<EntriesIterator, Error>
    enumerateDirectory(VNodeId dirId, User user) const;

    /////////////////////////////////////////////////////////////
    /// File management/access
    /////////////////////////////////////////////////////////////
	Solace::Result<VNodeId, Error>
    createFile(VNodeId where, Solace::StringView name, User user, FilePermissions perms);

    /// Get a byte reader for the file content
	Solace::Result<Solace::ByteReader, Error>
    reader(User user, VNodeId fid);

    /// Get a writer for the file content
	Solace::Result<Solace::ByteWriter, Error>
    writer(User user, VNodeId fid);

    /**
     * Create a node of the given type and link it to the specified root.
	 * @param user Owner of the node to be created. Note this user must have write permission to the location.
     * @param type - Type of the node to be created.
     * @param perms Permission to be set for a newly created node.
     * @param where - An index of the node that likes to the newly created one.
     * @param name Name of the link from the 'where' node to the newly created node.
     *
     * @return Index of the new node on success or an error.
     */
	Solace::Result<VNodeId, Error>
	mknode(INode::Type type, User user, FilePermissions perms, VNodeId where, Solace::StringView name);

protected:

	Solace::Result<FilePermissions, Error>
    effectivePermissionsFor(User user, VNodeId rootIndex, FilePermissions perms, INode::Type type) const noexcept;

    /**
     * @brief Find directory entry in the given inode.
     * @param dirNodeId Id of the Node to search link from.
     * @param name Name of the link to find.
     * @return Either an entry record or none.
     */
    Solace::Optional<Entry>
    lookup(VNodeId dirNodeId, Solace::StringView name) const;

private:

    /// Index nodes are vertices of a graph: e.g all addressable nodes
    std::vector<INode> index;

    /// Directory entries - Named Graph edges
    std::vector<std::vector<Entry>> adjacencyList;

    /// Mounted filesystems
    std::vector<Mount> mounts;

    /// Registered virtual filesystems
    std::vector<VfsOps> vfs;
};


}  // namespace kasofs
#endif  // KASOFS_VFS_HPP
