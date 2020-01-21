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
#ifndef KASOFS_FILE_HPP
#define KASOFS_FILE_HPP

#include "fs.hpp"


namespace kasofs {

/**
 * A file-like object.
 */
struct File {
	using size_type = Filesystem::size_type;

	~File();

	constexpr File(struct Vfs* fs, INode::Id nodeId, INode node, Filesystem::OpenFID openId) noexcept
		: _vfs{fs}
		, _fid{openId}
		, _nodeId{nodeId}
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
	auto stat() const noexcept { return _inode; }

private:
	struct Vfs* const			_vfs;
	Filesystem::OpenFID const	_fid;
	INode::Id const				_nodeId;
	INode						_inode;

	size_type					_readOffset{0};
	size_type					_writeOffset{0};
};


}  // namespace kasofs
#endif  // KASOFS_FILE_HPP
