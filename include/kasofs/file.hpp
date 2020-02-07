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
	{}

	Result<size_type>
	seekRead(size_type offset, Filesystem::SeekDirection direction);

	Result<size_type>
	seekWrite(size_type offset, Filesystem::SeekDirection direction);

	Result<size_type>
	read(Solace::MutableMemoryView dest);

	Result<size_type>
	write(Solace::MemoryView src);

	Result<INode> stat() const noexcept;

	Result<INode::size_type> size() const noexcept {
		return stat().then([](INode const& node) { return node.dataSize; });
	}

private:
	struct Vfs* const			_vfs;
	Filesystem::OpenFID const	_fid;
	INode::Id const				_nodeId;

	size_type					_readOffset{0};
	size_type					_writeOffset{0};
};


}  // namespace kasofs
#endif  // KASOFS_FILE_HPP
