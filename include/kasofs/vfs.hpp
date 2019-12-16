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
#include "fs.hpp"

#include <solace/result.hpp>
#include <solace/error.hpp>
#include <solace/posixErrorDomain.hpp>

#include <solace/path.hpp>

#include <vector>
#include <unordered_map>


namespace kasofs {


/**
 * Directory entry
 */
struct Entry {
	constexpr Entry(Solace::StringView n, INode::Id i) noexcept
        : name{n}
        , nodeId{i}
    {}

    Solace::StringView  name;
	INode::Id			nodeId;
};


struct EntriesIterator {
    Entry const* begin() const  { return _begin; }
    Entry const* end() const    { return _end; }

    Entry const* _begin;
    Entry const* _end;
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

	static VfsId const kVfsTypeDirectory;
	static VfsNodeType const kVfsDirectoryNodeType;


    using size_type = std::vector<INode>::size_type;

    /**
     * Descriptor of a mounted vfs
     */
    struct Mount {
		constexpr Mount(VfsId fsId, INode::Id nodeId) noexcept
            : vfsIndex{fsId}
            , mountingPoint{nodeId}
        {}

		VfsId			vfsIndex;
		INode::Id		mountingPoint;
    };


public:

    Vfs(Vfs const&) = delete;
    Vfs& operator= (Vfs const&) = delete;

    Vfs(User rootOwner, FilePermissions rootPerms);

    Vfs(Vfs&&) = default;
    Vfs& operator= (Vfs&&) noexcept = default;

	/////////////////////////////////////////////////////////////
	/// Index management
	/////////////////////////////////////////////////////////////

	INode::Id rootId() const noexcept { return 0; }

	/**
	 * Get number of nodes in this VFS
	 * @return Number of nodes in the index
	 */
	size_type size() const noexcept {
		return _index.size();
	}


	/**
	 * Find an inode by node Id. Equivalent to FS stat call
	 * @param id inode number.
	 * @return Optional INode if given inode was found, none otherwise.
	 */
	auto nodeById(INode::Id id) const noexcept -> Solace::Optional<INode>;

	auto nodeById(Solace::Result<INode::Id, Error> const& maybeId) const noexcept {
		return maybeId
				? nodeById(*maybeId)
				: Solace::none;
	}

	void updateNode(INode::Id id, INode inode);


    /////////////////////////////////////////////////////////////
    /// VFS management
    /////////////////////////////////////////////////////////////

	/**
	 * Register a new type of FS
	 * @return Id of the registered vfs or an error.
	 */
	template<class Type, typename...Args>
	Solace::Result<VfsId, Error>
	registerFilesystem(Args&& ...args) {
		auto fs = std::make_unique<Type>(std::forward<Args>(args)...);

		const auto regId = _nextId;
		_vfs.emplace(regId, std::move(fs));
		_nextId += 1;

		return Solace::Ok(regId);
	}

	/**
	 * Find previously registered FS driver
	 * @param vfs New virtual file system operations.
	 * @return Id of the registered vfs or an error.
	 */
	Solace::Optional<Filesystem*>
	findFs(VfsId id) const;

	/**
	 * Un-Register previously registered vFS
	 * @param VfsId Id of the previously registered vfs
	 * @return Void or an error.
	 */
	Solace::Result<void, Error>
	unregisterFileSystem(VfsId VfsId);

	/**
	 * Mount registered vfs to a given mount point.
	 * @param user User credentials to perform the operation as.
	 * @param mountingPoint INode where the vfs should be mounted.
	 * @param fsId Id of the previously registered vfs.
	 * @return Void or an error.
	 */
// Solace::Result<void, Error>
// mount(User user, INode::Id mountingPoint, VfsId fsId);

	/**
	 * Unmount vfs from the given mounting point.
	 * @param user User credentials to perform the operation as.
	 * @param mountingPoint INode to unmount vfs from.
	 * @return Void or an error.
	 */
// Solace::Result<void, Error>
// umount(User user, INode::Id mountingPoint);

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
	link(User user, Solace::StringView name, INode::Id from, INode::Id to);

    /**
     * Unlink given name from the given node
     * @param user Credentials of the user performing the operation. Note: User must have write permission to the from node.
     * @param name Name of the link to remove.
     * @param from Node a link should be removed from.
     * @return Void or error.
     */
	Solace::Result<void, Error>
	unlink(User user, Solace::StringView name, INode::Id from);


    template<typename F>
	Solace::Result<Entry, Error>
	walk(User user, INode::Id rootId, Solace::Path const& path, F&& f) const {
        auto maybeNode = nodeById(rootId);
        if (!maybeNode) {   // Valid file id required to start the walk
			return makeError(Solace::GenericError::BADF , "walk");
        }

        auto resultingEntry = Entry{kThisDir, rootId};
		for (auto const& pathSegment : path) {
			INode node = *maybeNode;
			if (!node.userCan(user, Permissions::READ)) {
				return makeError(Solace::GenericError::PERM, "walk");
            }

			auto maybeEntry = lookup(resultingEntry.nodeId, pathSegment.view());
            if (!maybeEntry) {
				return makeError(Solace::GenericError::NOENT, "walk");
            }

			resultingEntry = maybeEntry.move();
            maybeNode = nodeById(resultingEntry.nodeId);
            if (!maybeNode) {  // FIXME: It is fs consistency error if entry.index does not exist. Must be hadled here.
				return makeError(Solace::GenericError::NXIO, "walk");
            }

			// Invoke the callback handler
			f(*maybeNode);
        }

        return Solace::Ok(resultingEntry);
    }

	auto walk(User user, INode::Id rootId, Solace::Path const& path) const {
		return walk(user, rootId, path, [](auto const& ){});
	}

	auto walk(User user, Solace::Path const& path) const {
		return walk(user, rootId(), path);
	}

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
	Solace::Result<INode::Id, Error>
	mknode(INode::Id where,
		   Solace::StringView name,
		   VfsId fsType,
		   VfsNodeType nodeType,
		   User user,
		   FilePermissions perms = {0777});

	/**
	 * Create a directory node
	 * @param where Id of the root to link a new node to.
	 * @param name Name of the link for a new node.
	 * @param user User owner of a new node.
	 * @param perms Access permissions for a new node.
	 * @return Either an entry record or none.
	 */
	Solace::Result<INode::Id, Error>
	createDirectory(INode::Id where, Solace::StringView name, User user, FilePermissions perms = {0666});

    /// Return iterator for directory's entries of the give dirNode
	Solace::Result<EntriesIterator, Error>
	enumerateDirectory(INode::Id dirId, User user) const;

	/**
	 * Open a file for IO operations.
	 * @param user User principal requesting operation.
	 * @param fid Id of the file to be opened.
	 * @param op Opearions to be performed on the file.
	 * @return Result - either an IO object or an error.
	 */
	Solace::Result<File, Error>
	open(User user, INode::Id fid, Permissions op);

protected:

	/**
     * @brief Find directory entry in the given inode.
     * @param dirNodeId Id of the Node to search link from.
     * @param name Name of the link to find.
     * @return Either an entry record or none.
     */
    Solace::Optional<Entry>
	lookup(INode::Id dirNodeId, Solace::StringView name) const;

	Solace::Optional<Filesystem*>
	findFsOf(INode const& vnode) const {
		return findFs(vnode.fsTypeId);
	}

	/// Create unlinked node
	Solace::Result<INode::Id, Error>
	createUnlinkedNode(VfsId type, VfsNodeType nodeType, User owner, FilePermissions perms, FilePermissions dirPerms);

private:

    /// Index nodes are vertices of a graph: e.g all addressable nodes
	std::vector<INode> _index;

    /// Mounted filesystems
//    std::vector<Mount> mounts;

    /// Registered virtual filesystems
	VfsId _nextId{0};
	std::unordered_map<VfsId, std::unique_ptr<Filesystem>> _vfs;
};


}  // namespace kasofs
#endif  // KASOFS_VFS_HPP
