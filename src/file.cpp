/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
#include "kasofs/fs.hpp"
#include "kasofs/vfs.hpp"


#include <solace/posixErrorDomain.hpp>
#include <solace/unit.hpp>


#include <functional>


using namespace kasofs;
using namespace Solace;


File::~File() {
	if (_vfs) {
		_vfs->findFs(_inode.fsTypeId)
				.flatMap([this](Filesystem* fs) -> Optional<Unit>{
					fs->close(_inode, _fid);
					return none;
				});
	}
}


Result<File::size_type, Error>
File::read(MutableMemoryView dest) {
	auto maybeFs = _vfs->findFs(_inode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "File::read");
	}

	auto readResult = (*maybeFs)->read(_inode, _readOffset, dest);
	if (!readResult) {
		return readResult.moveError();
	}

	_readOffset += *readResult;

	return readResult;
}


Result<File::size_type, Error>
File::write(MemoryView src) {
	auto maybeFs = _vfs->findFs(_inode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "File::write");
	}

	auto writeResult = (*maybeFs)->write(_inode, _writeOffset, src);
	if (!writeResult) {
		return writeResult.moveError();
	}

	_writeOffset += writeResult.unwrap();

	_vfs->updateNode(_nodeId, _inode);

	return writeResult;
}


/*
Result<ByteReader, Error>
Vfs::reader(User user, INode::Id nodeId) {
	auto maybeNode = nodeById(nodeId);
	if (!maybeNode) {
		return makeError(GenericError::BADF, "reader");
	}

	auto& node = *maybeNode;
	if (node.type() != INode::Type::Data) {
		return makeError(GenericError::ISDIR, "reader");
	}

	if (!node.userCan(user, Permissions::READ)) {
		return makeError(GenericError::PERM, "reader");
	}

	auto& fs = vfs[node.deviceId];
	if (!fs.read)
		return makeError(GenericError::IO, "reader");

	return Ok(fs.read(node));
}


Result<ByteWriter, Error>
Vfs::writer(User user, INode::Id nodeId) {
	auto maybeNode = nodeById(nodeId);
	if (!maybeNode) {
		return makeError(GenericError::BADF, "writer");
	}

	auto& node = *maybeNode;
	if (node.type() != INode::Type::Data) {
		return makeError(GenericError::ISDIR, "writer");
	}

	if (!node.userCan(user, Permissions::WRITE)) {
		return makeError(GenericError::PERM, "writer");
	}

	auto& fs = vfs[node.deviceId];
	if (!fs.write)
		return makeError(GenericError::IO, "writer");

	return Ok(fs.write(node));
}
*/
