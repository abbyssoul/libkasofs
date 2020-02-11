/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
#include "kasofs/file.hpp"
#include "kasofs/vfs.hpp"


#include <solace/posixErrorDomain.hpp>
#include <solace/unit.hpp>


#include <functional>


using namespace kasofs;
using namespace Solace;


File::~File() {
	if (_vfs) {
		_vfs->findFs(_cachedNode.fsTypeId)
				.flatMap([this](Filesystem* fs) -> Optional<Unit> {
					fs->close(_fid, _cachedNode);
					_vfs->updateNode(_nodeId, _cachedNode);
					return none;
				 });
	}
}

void File::flush() {
	if (_vfs) {
		_vfs->updateNode(_nodeId, _cachedNode);
	}
}


kasofs::Result<INode>
File::stat() const noexcept {
	return kasofs::Result<INode>{types::okTag, _cachedNode};
}


kasofs::Result<File::size_type>
File::read(MutableMemoryView dest) {
	if (!_vfs)
		return makeError(GenericError::NODEV, "File::read");

	auto maybeFs = _vfs->findFs(_cachedNode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "File::read");
	}

	return (*maybeFs)->read(_fid, _cachedNode, _readOffset, dest)
			.then([this](File::size_type byteTransferred) {
				_vfs->updateNode(_nodeId, _cachedNode);
				_readOffset += byteTransferred;

				return byteTransferred;
			});
}


kasofs::Result<File::size_type>
File::write(MemoryView src) {
	if (!_vfs)
		return makeError(GenericError::NODEV, "File::write");

	auto maybeFs = _vfs->findFs(_cachedNode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "File::write");
	}

	return (*maybeFs)->write(_fid, _cachedNode, _writeOffset, src)
			.then([this](File::size_type byteTransferred) {
				_vfs->updateNode(_nodeId, _cachedNode);
				_writeOffset += byteTransferred;

				return byteTransferred;
			});
}


kasofs::Result<File::size_type>
File::seekRead(size_type offset, Filesystem::SeekDirection direction) {
	auto maybeFs = _vfs->findFs(_cachedNode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "File::seekWrite");
	}

	return (*maybeFs)->seek(_fid, _cachedNode, offset, direction)
			.then([this](File::size_type pos) {
				_readOffset = pos;
				return _readOffset;
			});
}


kasofs::Result<File::size_type>
File::seekWrite(size_type offset, Filesystem::SeekDirection direction) {
	auto maybeFs = _vfs->findFs(_cachedNode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "File::seekWrite");
	}

	return (*maybeFs)->seek(_fid, _cachedNode, offset, direction)
			.then([this](File::size_type pos) {
				_writeOffset = pos;
				return _writeOffset;
			});
}
