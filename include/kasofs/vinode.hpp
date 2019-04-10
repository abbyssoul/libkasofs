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

#include <solace/result.hpp>
#include <solace/error.hpp>

#include <solace/byteReader.hpp>
#include <solace/byteWriter.hpp>


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

    /// Get type of the node
    Type type() const noexcept { return _type; }

    /// Get type of the node
    FileMode mode() const noexcept {
        return (_type == Type::Directory)
                ? FileMode{DirMode::DIR, permissions}
                : FileMode{DirMode::REGULAR, permissions};
    }

    /// Test if a given user can perform operation
    bool userCan(User user, Permissions perms) const noexcept;

    /** @return Length of the file data in bytes */
    Solace::uint64 length() const noexcept;

private:

    Type                _type;              //!< Type of the node

};


//inline
//auto initMeta(INode::TypeTag<INode::Type::Directory>, User owner, Solace::uint64 path, FilePermissions perms) noexcept {
//    return INode::Meta{path, owner, {DirMode::DIR, perms}};
//}

//inline
//auto initMeta(INode::TypeTag<INode::Type::Data>, User owner, Solace::uint64 path, FilePermissions perms) noexcept {
//    return INode::Meta{path, owner, {DirMode::REGULAR, perms}};
//}

//inline
//auto initMeta(INode::TypeTag<INode::Type::Synthetic>, User owner, Solace::uint64 path, FilePermissions perms) noexcept {
//    return INode::Meta{path, owner, {DirMode::TMP, perms}};
//}

//inline
//auto initMeta(INode::TypeTag<INode::Type::SyntheticDir>, User owner, Solace::uint64 path, FilePermissions perms) noexcept {
//    return INode::Meta{path, owner, {DirMode::DIR, perms}};
//}



//template<>
//inline
//INode::INode(INode::TypeTag<INode::Type::Directory> tag, User owner, Solace::uint64 path, FilePermissions perms)
//    : _meta{initMeta(tag, owner, path, perms)}
//    , _nodeData{std::in_place_type<INode::Dir>}
//{}


//template<>
//inline
//INode::INode(INode::TypeTag<INode::Type::Data> tag, User owner, Solace::uint64 path, FilePermissions perms)
//    : _meta{initMeta(tag, owner, path, perms)}
//    , _nodeData{std::in_place_type<INode::Data>}
//{ }


//template<typename R,
//         typename W>
//inline
//INode::INode(User owner, Solace::uint64 path, FilePermissions perms, R&& r, W&& w)
//    : _meta{initMeta(INode::TypeTag<INode::Type::Synthetic>{}, owner, path, perms)}
//    , _nodeData{std::in_place_type<INode::Synth>, std::forward<R>(r), std::forward<W>(w)}
//{ }

//template<typename F>
//inline
//INode::INode(User owner, Solace::uint64 path, FilePermissions perms, F&& f)
//    : _meta{initMeta(INode::TypeTag<INode::Type::SyntheticDir>{}, owner, path, perms)}
//    , _nodeData{std::in_place_type<INode::SynthDir>, std::forward<F>(f)}
//{ }

}  // namespace kasofs
#endif  // KASOFS_VINODE_HPP
