/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
/*******************************************************************************
 * KasoFS: Virtual filesystem driver
 *	@file		filesystem.hpp
 ******************************************************************************/
#pragma once
#ifndef KASOFS_FS_HPP
#define KASOFS_FS_HPP

#include "vinode.hpp"

#include <solace/result.hpp>
#include <solace/error.hpp>
#include <solace/mutableMemoryView.hpp>


namespace kasofs {

/// Alias for the error type used throughout the library
using Error = Solace::Error;

template<typename T>
using Result = Solace::Result<T, Error>;

/**
 * A interface of a file system driver that implements IO operations..
 */
struct Filesystem {
	using size_type = Solace::MemoryView::size_type;
	using NodeType = VfsNodeType;
	using OpenFID = INode::Id;

	enum class SeekDirection {
		FromStart,
		Relative
	};

	virtual ~Filesystem();

	virtual FilePermissions defaultFilePermissions(NodeType type) const noexcept = 0;

	virtual auto createNode(NodeType type, User owner, FilePermissions perms) -> Result<INode> = 0;
	virtual auto destroyNode(INode& node) -> Result<void> = 0;

	virtual auto open(INode& node, Permissions op) -> Result<OpenFID> = 0;

	virtual auto read(OpenFID fid, INode& node, size_type off, Solace::MutableMemoryView dest) ->Result<size_type> = 0;

	virtual auto write(OpenFID fid, INode& node, size_type offset, Solace::MemoryView src) -> Result<size_type> = 0;

	virtual auto seek(OpenFID fid, INode& node, size_type offset, SeekDirection direction) -> Result<size_type> = 0;

	virtual auto close(OpenFID fid, INode& node) -> Result<void> = 0;
};


}  // namespace kasofs
#endif  // KASOFS_FS_HPP
