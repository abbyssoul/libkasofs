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

/**
 * Node of a virtual file system
 */
struct INode {

    enum class Type : Solace::byte {
        Directory = 0,
        Data
    };

    Solace::uint32      deviceId{0};        //!< Id of the `superblock` or
    User                owner;              //!< Owner of the node
    FilePermissions     permissions;        //!< Permissions and flags

    Solace::uint64      path{0};            //!< Id of the node used vfs owning it.
    Solace::uint32      version{0};         //!< Version of the node

    Solace::uint32      atime{0};           //!< last read time
    Solace::uint32      mtime{0};           //!< last write time

    Solace::uint32      dataIndex{0};       //!< Index into buffers where data for this node leaves.
    Solace::uint32      dataCount{0};       //!< Number of buffers owner by this node?

public:

    constexpr INode(Type type, User user, FilePermissions perms, Solace::uint64 id = 0) noexcept
        : owner{user}
        , permissions{perms}
        , path{id}
        , _type{type}
    {
    }

	INode& swap(INode& rhs) noexcept {
		using std::swap;

		swap(deviceId, rhs.deviceId);
		swap(owner, rhs.owner);
		swap(permissions, rhs.permissions);

		swap(path, rhs.path);
		swap(version, rhs.version);

		swap(atime, rhs.atime);
		swap(mtime, rhs.mtime);

		swap(dataIndex, rhs.dataIndex);
		swap(dataCount, rhs.dataCount);
		swap(_type, rhs._type);

		return *this;
	}

    /// Get type of the node
    Type type() const noexcept { return _type; }

    /// Get type of the node
    FileMode mode() const noexcept {
        return (_type == Type::Directory)
				? FileMode{FileTypeMask::Dir, permissions}
				: FileMode{FileTypeMask::File, permissions};
    }

    /// Test if a given user can perform operation
    bool userCan(User user, Permissions perms) const noexcept;

    /** @return Length of the file data in bytes */
    Solace::uint64 length() const noexcept;

private:

    Type                _type;              //!< Type of the node

};


inline void swap(INode& lhs, INode& rhs) noexcept {
	lhs.swap(rhs);
}

}  // namespace kasofs
#endif  // KASOFS_VINODE_HPP
