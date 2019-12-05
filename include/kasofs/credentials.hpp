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
 *	@file		permissions.hpp
 ******************************************************************************/
#pragma once
#ifndef KASOFS_CREDENTIALS_HPP
#define KASOFS_CREDENTIALS_HPP

#include <solace/types.hpp>

#include <utility>


namespace kasofs {

/**
 * @brief Single operation permission
 */
struct Permissions {
    static Solace::byte const READ  = 0x4;		/* mode bit for read permission */
    static Solace::byte const WRITE = 0x2;		/* mode bit for write permission */
    static Solace::byte const EXEC  = 0x1;		/* mode bit for execute permission */

    constexpr Permissions(Solace::byte v) noexcept
        : value{v}
    {}

	constexpr bool can(Permissions op) const noexcept {
		return (value & op.value) == op.value;
	}

	constexpr bool isReadable() const noexcept { return can(READ); }
	constexpr bool isWritable() const noexcept { return can(WRITE); }
	constexpr bool isExecutable() const noexcept { return can(EXEC); }

	Solace::byte value;
};


inline constexpr
bool operator== (Permissions const& lhs, Permissions const& rhs) noexcept { return (lhs.value == rhs.value); }

inline constexpr
bool operator!= (Permissions const& lhs, Permissions const& rhs) noexcept { return (lhs.value != rhs.value); }

inline
void swap(Permissions& lhs, Permissions& rhs) noexcept {
	std::swap(lhs.value, rhs.value);
}


/**
 * A model of an actor.
 * Actor may want to perform actions on a file that requires permission.
 */
struct User {
    Solace::uint32 uid;
    Solace::uint32 gid;
};


inline constexpr
bool operator== (User const& lhs, User const& rhs) noexcept { return ((lhs.uid == rhs.uid) && (lhs.gid == rhs.gid)); }

inline constexpr
bool operator!= (User const& lhs, User const& rhs) noexcept { return ((lhs.uid != rhs.uid) || (lhs.gid != rhs.gid)); }

inline
void swap(User& lhs, User& rhs) noexcept {
	std::swap(lhs.uid, rhs.uid);
	std::swap(lhs.gid, rhs.gid);
}



/**
 * Unix style file permission for {Owner, Owner's group, Others}
 */
struct FilePermissions {
	static Solace::uint32 const USER  = 0700;		/// mode bits for user permissions
	static Solace::uint32 const GROUP = 0070;		/// mode bits for group permissions
	static Solace::uint32 const OTHER = 0007;		/// mode bits for other permissions

	constexpr FilePermissions(Solace::uint32 perm) noexcept
		: value{perm}
	{}

	constexpr FilePermissions(Permissions user, Permissions group, Permissions others) noexcept
		: value((user.value << 6) | (group.value << 3) | (others.value << 0))
	{}

	constexpr Permissions user() const noexcept		{ return {static_cast<Solace::byte>((value & USER ) >> 6)}; }
	constexpr Permissions group() const noexcept	{ return {static_cast<Solace::byte>((value & GROUP) >> 3)}; }
	constexpr Permissions others() const noexcept	{ return {static_cast<Solace::byte>((value & OTHER) >> 0)}; }

	Solace::uint32 value;
};

inline constexpr
bool operator== (FilePermissions const& lhs, FilePermissions const& rhs) noexcept { return (lhs.value == rhs.value); }

inline constexpr
bool operator!= (FilePermissions const& lhs, FilePermissions const& rhs) noexcept { return (lhs.value != rhs.value); }

inline
void swap(FilePermissions& lhs, FilePermissions& rhs) noexcept {
	std::swap(lhs.value, rhs.value);
}


enum class FileTypeMask : Solace::uint16 {
	File = 0,
	Dir = 0040000,
};

constexpr
Solace::uint32
makeMode(FileTypeMask type, FilePermissions perms)  noexcept {
	return static_cast<Solace::uint32>(type) | perms.value;
}

constexpr
Solace::uint32
makeMode(FileTypeMask type, Permissions user, Permissions group, Permissions others)  noexcept {
	return makeMode(type, FilePermissions{user, group, others});
}


/**
 * @brief Unix style file mode encoded into an uint32
 */
struct FileMode {
	constexpr static Solace::uint32 const IFMT = 0xF000;     /// File type bits

	constexpr FileMode(Solace::uint32 value) noexcept
		: mode{value}
	{}

	constexpr FileMode(FileTypeMask type, FilePermissions perms) noexcept
		: mode{makeMode(type, perms)}
	{}

	constexpr FileMode(FileTypeMask type, Permissions user, Permissions group, Permissions others) noexcept
		: mode{makeMode(type, user, group, others)}
	{}

	constexpr FilePermissions permissions() const noexcept { return { mode & ~IFMT }; }
	constexpr FileMode withPermissions(FilePermissions perms) const noexcept {
		return (mode & IFMT) | perms.value;
	}

	constexpr bool isDirectory() const noexcept   { return (mode & IFMT) == static_cast<Solace::uint32>(FileTypeMask::Dir); }
	constexpr bool isFile() const noexcept        { return (mode & IFMT) == static_cast<Solace::uint32>(FileTypeMask::File); }

	constexpr bool operator== (Solace::uint32 rhs) const noexcept {
		return (mode == rhs);
	}

	constexpr bool operator!= (Solace::uint32 rhs) const noexcept {
		return (mode != rhs);
	}

	Solace::uint32      mode;       //!< permissions and flags
};


inline bool operator== (FileMode const& lhs, FileMode const& rhs) noexcept { return (lhs.mode == rhs.mode); }
inline bool operator!= (FileMode const& lhs, FileMode const& rhs) noexcept { return (lhs.mode != rhs.mode); }


constexpr
bool canUserPerformAction(User owner, FilePermissions acl, User actor, Permissions action) noexcept {
	auto const perms = (owner.uid == actor.uid)
			? acl.user()
			: (owner.gid == actor.gid)
			  ? acl.group()
			  : acl.others();

	return perms.can(action);
}


}  // namespace kasofs
#endif  // KASOFS_CREDENTIALS_HPP
