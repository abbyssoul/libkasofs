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

#include "permissions.hpp"


namespace kasofs {

using VfsId = Solace::uint32;
using VfsNodeType = Solace::uint32;

/**
 * Node of a virtual file system
 */
struct INode {

	using size_type = Solace::uint64;
	using VfsData = Solace::uint64;

	struct Id {
		Solace::uint32 index;
		Solace::uint32 gen;

		constexpr Id(Solace::uint32 i, Solace::uint32 g) noexcept
			: index{i}
			, gen{g}
		{}

		constexpr Id(INode::Id const& rhs) noexcept
			: index{rhs.index}
			, gen{rhs.gen}
		{}


		Id& operator= (INode::Id const& rhs) noexcept {
			index = rhs.index;
			gen = rhs.gen;

			return *this;
		}
	};


	VfsId				fsTypeId;			//!< Id of the vfs type of the node.
	VfsNodeType			nodeTypeId;			//!< FS specific type of the node.

    User                owner;              //!< Owner of the node
    FilePermissions     permissions;        //!< Permissions and flags

    Solace::uint32      atime{0};           //!< last read time
    Solace::uint32      mtime{0};           //!< last write time

	Solace::uint32      nLinks{0};			//!< Number of links to this node from other nodes / direcotries

	Solace::uint32      version{0};         //!< Version of the node
	VfsData				vfsData{0};			//!< Data storage used by vfs.
	size_type			dataSize{0};		//!< Size of the data stored by vfs

public:

	constexpr INode(VfsNodeType minor, User user, FilePermissions perms) noexcept
		: fsTypeId{0}
		, nodeTypeId{minor}
		, owner{std::move(user)}
		, permissions{std::move(perms)}
	{
	}


	INode& swap(INode& rhs) noexcept {
		using std::swap;

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

};


inline bool
operator== (INode::Id const& lhs, INode::Id const& rhs) noexcept {
	return (lhs.index == rhs.index) && (lhs.gen == rhs.gen);
}

inline
void swap(INode& lhs, INode& rhs) noexcept {
	lhs.swap(rhs);
}

}  // namespace kasofs
#endif  // KASOFS_VINODE_HPP
