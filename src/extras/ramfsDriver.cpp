/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
#include "kasofs/extras/ramfsDriver.hpp"

#include <solace/posixErrorDomain.hpp>


using namespace kasofs;
using namespace Solace;


VfsNodeType const RamFS::kNodeType{3213};


uint32 nodeEpochTime() noexcept {
	return time(nullptr);
}

kasofs::Result<INode>
RamFS::createNode(NodeType type, User owner, FilePermissions perms) {
	if (kNodeType != type) {
		return makeError(GenericError::NXIO, "RamFs::createNode");
	}

	INode node{type, owner, perms};
	node.dataSize = 0;
	node.vfsData = nextId();
	node.atime = nodeEpochTime();
	node.mtime = nodeEpochTime();
	_dataStore.emplace(node.vfsData, Buffer{});

	return mv(node);
}


kasofs::Result<void>
RamFS::destroyNode(INode& node) {
	if (!isRamNode(node)) {
		return makeError(GenericError::NXIO, "RamFs::destroyNode");
	}

	_dataStore.erase(node.vfsData);
	return Ok();
}


kasofs::Result<Filesystem::OpenFID>
RamFS::open(INode& node, Permissions) {
	if (!isRamNode(node)) {
		return makeError(GenericError::NXIO, "RamFs::open");
	}

	node.atime = nodeEpochTime();
	return Ok<OpenFID>(0);
}


kasofs::Result<RamFS::size_type>
RamFS::read(OpenFID, INode& node, size_type offset, MutableMemoryView dest) {
	if (!isRamNode(node)) {
		return makeError(GenericError::NXIO, "RamFs::read");
	}

	auto const id = node.vfsData;
	auto it = _dataStore.find(id);
	if (it == _dataStore.end())
		return makeError(GenericError::BADF, "RamFS::read");

	auto& buffer = it->second;
	if (offset > buffer.size())
		return makeError(BasicError::Overflow, "RamFS::read");

	auto data = wrapMemory(buffer.data() + offset, buffer.size() - offset).slice(0, dest.size());
	auto isOk = dest.write(data);
	if (!isOk) {
		return isOk.moveError();
	}

	return Ok(data.size());
}


kasofs::Result<Filesystem::size_type>
RamFS::write(OpenFID, INode& node, size_type offset, MemoryView src) {
	if (!isRamNode(node)) {
		return makeError(GenericError::NXIO, "RamFs::write");
	}

	auto const id = node.vfsData;
	auto it = _dataStore.find(id);
	if (it == _dataStore.end())
		return makeError(GenericError::BADF, "RamFs::write");

	auto& buffer = it->second;
	if (offset > buffer.size())
		return makeError(BasicError::Overflow, "RamFs::write");

	auto const newSize = offset + src.size();
	/// FIXME: Use fixed size pool
	buffer.reserve(newSize);

	if (buffer.size() < newSize) {
		buffer.resize(newSize);
	}
	auto writeResult = wrapMemory(buffer.data(), buffer.size())
			.write(src, offset);

	if (!writeResult)
		return writeResult.moveError();

	node.dataSize = buffer.size();
	node.mtime = nodeEpochTime();

	return Ok(src.size());
}


kasofs::Result<RamFS::size_type>
RamFS::seek(OpenFID, INode& node, size_type offset, SeekDirection direction) {
	if (!isRamNode(node)) {
		return makeError(GenericError::NXIO, "RamFs::seek");
	}

	switch (direction) {
	case SeekDirection::FromStart: return offset;
	case SeekDirection::Relative: return offset;
	}

	return offset;
}


kasofs::Result<void>
RamFS::close(OpenFID, INode& node) {
	if (!isRamNode(node)) {
		return makeError(GenericError::NXIO, "RamFs::close");
	}

	return Ok();
}
