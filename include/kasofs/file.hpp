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
		, _cachedNode{node}
	{}

	constexpr File(File&& rhs) noexcept
		: _vfs{Solace::exchange(rhs._vfs, nullptr)}
		, _fid{Solace::exchange(rhs._fid, -1)}
		, _nodeId{rhs._nodeId}
		, _cachedNode{rhs._cachedNode}
	{}

	File& operator= (File&& rhs) noexcept {
		return swap(rhs);
	}

	File& swap(File& rhs) noexcept {
		using std::swap;
		swap(_vfs, rhs._vfs);
		swap(_fid, rhs._fid);
		swap(_nodeId, rhs._nodeId);
		swap(_cachedNode, rhs._cachedNode);

		swap(_readOffset, rhs._readOffset);
		swap(_writeOffset, rhs._writeOffset);

		return *this;
	}


	Result<size_type>
	seekRead(size_type offset, Filesystem::SeekDirection direction);

	Result<size_type>
	seekWrite(size_type offset, Filesystem::SeekDirection direction);

	Result<size_type>
	read(Solace::MutableMemoryView dest);

	Result<size_type>
	write(Solace::MemoryView src);

	Result<INode> stat() const noexcept;

	void flush();

	Result<INode::size_type> size() const noexcept {
		return stat().then([](INode const& node) { return node.dataSize; });
	}

private:
	struct Vfs*				_vfs;
	Filesystem::OpenFID		_fid;
	INode::Id				_nodeId;
	INode					_cachedNode;

	size_type				_readOffset{0};
	size_type				_writeOffset{0};
};


inline void
swap(File& lhs, File& rhs) noexcept {
	lhs.swap(rhs);
}

}  // namespace kasofs
#endif  // KASOFS_FILE_HPP
