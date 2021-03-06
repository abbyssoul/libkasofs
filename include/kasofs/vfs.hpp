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
#include "directoryDriver.hpp"
#include "file.hpp"


#include <solace/result.hpp>
#include <solace/error.hpp>
#include <solace/posixErrorDomain.hpp>
#include <solace/path.hpp>

#include <vector>
#include <unordered_map>


namespace kasofs {



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

	INode::Id rootId() const noexcept { return {0, 0}; }

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

	auto nodeById(Result<INode::Id> const& maybeId) const noexcept {
		return maybeId
				? nodeById(*maybeId)
				: Solace::none;
	}

	Result<void>
	updateNode(INode::Id id, INode inode);


    /////////////////////////////////////////////////////////////
    /// VFS management
    /////////////////////////////////////////////////////////////

	/**
	 * Register a new type of FS
	 * @return Id of the registered vfs or an error.
	 */
	template<class Type, typename...Args>
	Result<VfsId> registerFilesystem(Args&& ...args) {
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
	findFs(VfsId id) const noexcept;

	/**
	 * Un-Register previously registered vFS
	 * @param VfsId Id of the previously registered vfs
	 * @return Void or an error.
	 */
	Result<void>
	unregisterFileSystem(VfsId VfsId);

	/**
	 * Mount registered vfs to a given mount point.
	 * @param user User credentials to perform the operation as.
	 * @param mountingPoint INode where the vfs should be mounted.
	 * @param fsId Id of the previously registered vfs.
	 * @return Void or an error.
	 */
// Result<void>
// mount(User user, INode::Id mountingPoint, VfsId fsId);

	/**
	 * Unmount vfs from the given mounting point.
	 * @param user User credentials to perform the operation as.
	 * @param mountingPoint INode to unmount vfs from.
	 * @return Void or an error.
	 */
// Result<void>
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
	 * @note The link created does not change ownership or access permissions of linked-to node.
     *
	 * @note It is NOT an error to create multiple links to the same node B with different names.
     */
	Result<void>
	link(User user, Solace::StringView name, INode::Id from, INode::Id to);

    /**
     * Unlink given name from the given node
     * @param user Credentials of the user performing the operation. Note: User must have write permission to the from node.
	 * @param from Directory NodeID to remove a link from.
     * @param name Name of the link to remove.
     * @return Void or error.
	 *
	 * @note User must have write permission to the node to unlink name from it.
	 */
	Result<void>
	unlink(User user, INode::Id from, Solace::StringView name);


	template<typename P, typename F>
	Result<Entry>
	walk(User user, INode::Id rootId, P const& path, F&& f) const {
        auto maybeNode = nodeById(rootId);
        if (!maybeNode) {   // Valid file id required to start the walk
			return makeError(Solace::GenericError::BADF , "walk");
        }

        auto resultingEntry = Entry{kThisDir, rootId};
		for (auto pathSegment : path) {
			INode node = *maybeNode;
			if (!node.userCan(user, Permissions::READ)) {
				return makeError(Solace::GenericError::PERM, "walk");
            }

			auto maybeEntry = lookup(resultingEntry.nodeId, pathSegment);
            if (!maybeEntry) {
				return makeError(Solace::GenericError::NOENT, "walk");
            }

			resultingEntry = maybeEntry.move();
            maybeNode = nodeById(resultingEntry.nodeId);
            if (!maybeNode) {  // FIXME: It is fs consistency error if entry.index does not exist. Must be hadled here.
				return makeError(Solace::GenericError::NXIO, "walk");
            }

			// Invoke the callback handler
			f(resultingEntry, *maybeNode);
        }

		return Result<Entry>{Solace::types::okTag, Solace::in_place, resultingEntry};
    }

	auto walk(User user, INode::Id rootId, Solace::Path const& path) const {
		return walk(user, rootId, path, [](Entry const&, INode const&){});
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
	Result<INode::Id>
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
	Result<INode::Id>
	createDirectory(INode::Id where, Solace::StringView name, User user, FilePermissions perms = {0666});

    /// Return iterator for directory's entries of the give dirNode
	Result<EntriesEnumerator>
	enumerateDirectory(User user, INode::Id dirId);

	/**
	 * Open a file for IO operations.
	 * @param user User principal requesting operation.
	 * @param fid Id of the file to be opened.
	 * @param op Opearions to be performed on the file.
	 * @return Result - either an IO object or an error.
	 */
	Result<File>
	open(User user, INode::Id fid, Permissions op);

protected:

	/**
     * @brief Find directory entry in the given inode.
     * @param dirNodeId Id of the Node to search link from.
     * @param name Name of the link to find.
     * @return Either an entry record or none.
     */
    Solace::Optional<Entry>
	lookup(INode::Id dirNodeId, Solace::StringView name) const noexcept;

	Solace::Optional<Filesystem*>
	findFsOf(INode const& vnode) const noexcept {
		return findFs(vnode.fsTypeId);
	}

	/// Create unlinked node
	Result<INode::Id>
	createUnlinkedNode(VfsId type, VfsNodeType nodeType, User owner, FilePermissions perms, FilePermissions dirPerms);

	void
	releaseNode(INode::Id id) noexcept;

	void
	addNodeLink(INode::Id id) noexcept;

	friend struct EntriesEnumerator;

private:

	struct INodeEntry {
		Solace::uint32		gen;				//!< VFS generation of node.
		INode				inode;

		constexpr INodeEntry(Solace::uint32 generation, INode node) noexcept
			: gen{generation}
			, inode{Solace::mv(node)}
		{}
	};

    /// Index nodes are vertices of a graph: e.g all addressable nodes
	std::vector<INodeEntry>		_index;
	DirFs						_directories;

	Solace::uint32				_genCount{0};				//!< VFS generation of node.

    /// Mounted filesystems
//    std::vector<Mount> mounts;

    /// Registered virtual filesystems
	VfsId _nextId{0};
	std::unordered_map<VfsId, std::unique_ptr<Filesystem>> _vfs;
};


}  // namespace kasofs
#endif  // KASOFS_VFS_HPP
