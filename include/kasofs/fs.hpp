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

// Alias for error type used throughout the library
using Error = Solace::Error;



/**
 * A interface of a file system driver that implements IO operations..
 */
struct Filesystem {
	using size_type = Solace::MemoryView::size_type;
	using NodeType = VfsNodeType;
	using OpenFID = INode::Id;

	virtual ~Filesystem();

	virtual Solace::Result<INode::VfsData, Error> createNode(NodeType type) = 0;

	virtual FilePermissions defaultFilePermissions(NodeType type) const noexcept = 0;

	virtual auto open(INode node, Permissions op) -> Solace::Result<OpenFID, Error> = 0;

	virtual auto read(INode node, size_type offset, Solace::MutableMemoryView dest) -> Solace::Result<size_type, Error> = 0;

	virtual auto write(INode node, size_type offset, Solace::MemoryView src) -> Solace::Result<size_type, Error> = 0;

	virtual auto close(OpenFID fid, INode node) -> Solace::Result<void, Error> = 0;
};


struct File {
	using size_type = Filesystem::size_type;

	~File();

	constexpr File(struct Vfs const* fs, INode node, Filesystem::OpenFID openId) noexcept
		: vfs{fs}
		, inode{node}
		, fid{openId}
	{}

	Solace::Result<size_type, Error>
	read(Solace::MutableMemoryView dest);

	Solace::Result<size_type, Error>
	write(Solace::MemoryView src);

private:
	struct Vfs const*			vfs;
	INode const					inode;
	Filesystem::OpenFID const	fid;

	size_type					readOffset{0};
	size_type					writeOffset{0};
};


}  // namespace kasofs
#endif  // KASOFS_FS_HPP
