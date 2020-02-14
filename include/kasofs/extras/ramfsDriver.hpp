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
 *	@file		vfs.hpp
 ******************************************************************************/
#pragma once
#ifndef KASOFS_RAMFS_DRIVER_HPP
#define KASOFS_RAMFS_DRIVER_HPP

#include "kasofs/fs.hpp"

#include <solace/memoryView.hpp>

#include <unordered_map>
#include <vector>


namespace kasofs {

/**
 * An example Virtual FS driver that exposes RAM as a filesystem.
 */
struct RamFS final : public kasofs::Filesystem {

	static VfsNodeType const kNodeType;

	RamFS(Solace::MemoryView::size_type bufferSize) noexcept
	{}

	// Filesystem interface
	kasofs::FilePermissions defaultFilePermissions(NodeType) const noexcept override { return {0644}; }

	kasofs::Result<kasofs::INode>
	createNode(NodeType type, kasofs::User owner, kasofs::FilePermissions perms) override;

	kasofs::Result<void> destroyNode(kasofs::INode& node) override;

	kasofs::Result<OpenFID>
	open(kasofs::INode&, kasofs::Permissions) override;

	kasofs::Result<size_type>
	read(OpenFID streamId, kasofs::INode& node, size_type offset, Solace::MutableMemoryView dest) override;


	kasofs::Result<size_type>
	write(OpenFID streamId, kasofs::INode& node, size_type offset, Solace::MemoryView src) override;

	kasofs::Result<size_type>
	seek(OpenFID streamId, kasofs::INode& node, size_type offset, SeekDirection direction) override;


	kasofs::Result<void>
	close(OpenFID streamId, kasofs::INode& node) override;

	static bool isRamNode(INode const& node) noexcept {
		return (kNodeType == node.nodeTypeId);
	}

protected:
	using DataId = kasofs::INode::VfsData;
	using Buffer = std::vector<Solace::byte>;

	DataId nextId() noexcept { return _idBase++; }

private:
	DataId								_idBase{0};
	std::unordered_map<DataId, Buffer>	_dataStore;
};


}  // namespace kasofs
#endif  // KASOFS_RAMFS_DRIVER_HPP
