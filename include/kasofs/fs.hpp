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

// Alias for the error type used throughout the library
using Error = Solace::Error;



/**
 * A interface of a file system driver that implements IO operations..
 */
struct Filesystem {
	using size_type = Solace::MemoryView::size_type;
	using NodeType = VfsNodeType;
	using OpenFID = INode::Id;

	virtual ~Filesystem();

	virtual FilePermissions defaultFilePermissions(NodeType type) const noexcept = 0;

	virtual auto createNode(NodeType type, User owner, FilePermissions perms) -> Solace::Result<INode, Error> = 0;
	virtual auto destroyNode(INode& node) -> Solace::Result<void, Error> = 0;

	virtual auto open(INode& node, Permissions op) -> Solace::Result<OpenFID, Error> = 0;

	virtual auto read(INode& node, size_type offset, Solace::MutableMemoryView dest) ->
	Solace::Result<size_type, Error> = 0;

	virtual auto write(INode& node, size_type offset, Solace::MemoryView src) -> Solace::Result<size_type, Error> = 0;

	virtual auto close(INode& node, OpenFID fid) -> Solace::Result<void, Error> = 0;
};



struct File {
	using size_type = Filesystem::size_type;

	~File();

	constexpr File(struct Vfs* fs, INode::Id nodeId, INode node, Filesystem::OpenFID openId) noexcept
		: _vfs{fs}
		, _nodeId{nodeId}
		, _fid{openId}
		, _inode{node}
	{}

	enum class SeekDirection {
		FromStart,
		Relative
	};

	Solace::Result<size_type, Error>
	seekRead(size_type offset, SeekDirection direction);

	Solace::Result<size_type, Error>
	seekWrite(size_type offset, SeekDirection direction);


	Solace::Result<size_type, Error>
	read(Solace::MutableMemoryView dest);

	Solace::Result<size_type, Error>
	write(Solace::MemoryView src);

	auto size() const noexcept { return _inode.dataSize; }

private:
	struct Vfs* const			_vfs;
	INode::Id const				_nodeId;
	Filesystem::OpenFID const	_fid;
	INode						_inode;

	size_type					_readOffset{0};
	size_type					_writeOffset{0};
};


}  // namespace kasofs
#endif  // KASOFS_FS_HPP
