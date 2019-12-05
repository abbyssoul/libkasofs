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
 *	@file		vinode.hpp
 ******************************************************************************/
#pragma once
#ifndef KASOFS_VINODE_HPP
#define KASOFS_VINODE_HPP

#include "credentials.hpp"


namespace kasofs {

using VfsId = Solace::uint32;
using VfsNodeType = Solace::uint32;

/**
 * Node of a virtual file system
 */
struct INode {

	using Id = Solace::uint32;
	using VfsData = Solace::uint64;
	using size_type = Solace::uint64;

	Id					nodeId;			//!< Id of the node used vfs owning it.

	VfsId				fsTypeId;			//!< Id of the vfs type of the node
	VfsNodeType			nodeTypeId;			//!< FS specific type of the node

    User                owner;              //!< Owner of the node
    FilePermissions     permissions;        //!< Permissions and flags


    Solace::uint32      atime{0};           //!< last read time
    Solace::uint32      mtime{0};           //!< last write time

	Solace::uint32      version{0};         //!< Version of the node
	VfsData				vfsData;         //!< Data storage used by vfs.
	size_type			dataSize{0};         //!< Size of the data stored by vfs

public:

	constexpr INode(Id id, VfsId major, VfsNodeType minor, VfsData data, User user, FilePermissions perms) noexcept
		: nodeId{id}
		, fsTypeId{major}
		, nodeTypeId{minor}
		, owner{user}
        , permissions{perms}
		, vfsData{data}
    {
    }

	INode& swap(INode& rhs) noexcept {
		using std::swap;

		swap(nodeId, rhs.nodeId);
		swap(fsTypeId, rhs.fsTypeId);
		swap(nodeTypeId, rhs.nodeTypeId);

		swap(owner, rhs.owner);
		swap(permissions, rhs.permissions);

		swap(version, rhs.version);

		swap(atime, rhs.atime);
		swap(mtime, rhs.mtime);

		swap(vfsData, rhs.vfsData);
		swap(dataSize, rhs.dataSize);

		return *this;
	}

	/// Test if a given user can perform operation
	constexpr bool userCan(User user, Permissions action) const noexcept {
		return canUserPerformAction(owner, permissions, user, action);
	}

	/// Get type of the node
//    FileMode mode() const noexcept {
//        return (_type == Type::Directory)
//				? FileMode{FileTypeMask::Dir, permissions}
//				: FileMode{FileTypeMask::File, permissions};
//    }

};


inline void swap(INode& lhs, INode& rhs) noexcept {
	lhs.swap(rhs);
}

}  // namespace kasofs
#endif  // KASOFS_VINODE_HPP
