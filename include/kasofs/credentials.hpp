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
#ifndef KASOFS_CREDENTIALS_HPP
#define KASOFS_CREDENTIALS_HPP

#include <solace/types.hpp>

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

    constexpr bool isReadable() const noexcept { return (value & READ) == READ; }
    constexpr bool isWritable() const noexcept { return (value & WRITE) == WRITE; }
    constexpr bool isExecutable() const noexcept { return (value & EXEC) == EXEC; }

    constexpr bool can(Permissions op) const noexcept {
        return (value & op.value);
    }

    Solace::byte const value;
};


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

    Permissions user() const noexcept   { return {static_cast<Solace::byte>((value & USER ) >> 6)}; }
    Permissions group() const noexcept  { return {static_cast<Solace::byte>((value & GROUP) >> 3)}; }
    Permissions others() const noexcept  { return {static_cast<Solace::byte>((value & OTHER) >> 0)}; }

    Solace::uint32 const    value;
};



/**
 * Bits to help with file mode encoding
 */
enum class DirMode : Solace::uint32 {
    DIR         = 0x80000000,	/* mode bit for directories */
    APPEND      = 0x40000000,	/* mode bit for append only files */
    EXCL        = 0x20000000,	/* mode bit for exclusive use files */
    MOUNT       = 0x10000000,	/* mode bit for mounted channel */
    AUTH        = 0x08000000,	/* mode bit for authentication file */
    TMP         = 0x04000000,	/* mode bit for non-backed-up file */

    REGULAR     = 0x00'00'00'00,	/* mode bit for regular file (Unix, 9P2000.u) */
    SYMLINK     = 0x02'00'00'00,	/* mode bit for symbolic link (Unix, 9P2000.u) */
    DEVICE      = 0x00'80'00'00,	/* mode bit for device file (Unix, 9P2000.u) */
    NAMEDPIPE   = 0x00'20'00'00,	/* mode bit for named pipe (Unix, 9P2000.u) */
    SOCKET      = 0x00'10'00'00,	/* mode bit for socket (Unix, 9P2000.u) */
    SETUID      = 0x00'08'00'00,	/* mode bit for setuid (Unix, 9P2000.u) */
    SETGID      = 0x00'04'00'00,	/* mode bit for setgid (Unix, 9P2000.u) */
};


constexpr
Solace::uint32
makeMode(DirMode type, FilePermissions perms)  noexcept {
    return static_cast<Solace::uint32>(type) | perms.value;
}

constexpr
Solace::uint32
makeMode(DirMode type, Permissions user, Permissions group, Permissions others)  noexcept {
    return makeMode(type, FilePermissions{user, group, others});
}


/**
 * @brief Unix style file mode encoded into an uint32
 */
struct FileMode {
    static Solace::uint32 const IFMT = 0xF000;     /// File type bits

    constexpr FileMode(Solace::uint32 value) noexcept
        : mode{value}
    {}

    constexpr FileMode(DirMode type, FilePermissions perms) noexcept
        : mode{makeMode(type, perms)}
    {}

    FileMode(DirMode type, Permissions user, Permissions group, Permissions others) noexcept
        : mode{makeMode(type, user, group, others)}
    {}

    constexpr FilePermissions permissions() const noexcept { return { mode & ~IFMT }; }
    constexpr FileMode withPermissions(FilePermissions perms) const noexcept {
        return (mode & IFMT) | perms.value;
    }

    constexpr bool isDirectory() const noexcept   { return (mode & IFMT) == 0040000; }
    constexpr bool isFile() const noexcept        { return (mode & IFMT) == 0; }


    Solace::uint32      mode{0};       //!< permissions and flags
};


inline bool operator== (FileMode const& lhs, FileMode const& rhs) noexcept { return (lhs.mode == rhs.mode); }
inline bool operator!= (FileMode const& lhs, FileMode const& rhs) noexcept { return (lhs.mode != rhs.mode); }


/**
 * User credentials
 */
struct User {
    Solace::uint32 uid;
    Solace::uint32 gid;
};

inline bool operator== (User const& lhs, User const& rhs) noexcept { return ((lhs.uid == rhs.uid) && (lhs.gid == rhs.gid)); }
inline bool operator!= (User const& lhs, User const& rhs) noexcept { return ((lhs.uid != rhs.uid) || (lhs.gid != rhs.gid)); }

}  // namespace kasofs
#endif  // KASOFS_CREDENTIALS_HPP
