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
		_vfs->findFs(_inode.fsTypeId)
				.flatMap([this](Filesystem* fs) -> Optional<Unit>{
					fs->close(_fid, _inode);
					return none;
				});
	}
}


kasofs::Result<File::size_type>
File::read(MutableMemoryView dest) {
	auto maybeFs = _vfs->findFs(_inode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "File::read");
	}

	auto readResult = (*maybeFs)->read(_fid, _inode, _readOffset, dest);
	if (!readResult) {
		return readResult.moveError();
	}

	_readOffset += *readResult;

	return readResult;
}


kasofs::Result<File::size_type>
File::write(MemoryView src) {
	auto maybeFs = _vfs->findFs(_inode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "File::write");
	}

	auto writeResult = (*maybeFs)->write(_fid, _inode, _writeOffset, src);
	if (!writeResult) {
		return writeResult.moveError();
	}

	_writeOffset += writeResult.unwrap();

	_vfs->updateNode(_nodeId, _inode);

	return writeResult;
}


kasofs::Result<File::size_type>
File::seekRead(size_type offset, Filesystem::SeekDirection direction) {
	auto maybeFs = _vfs->findFs(_inode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "File::seekWrite");
	}

	auto seekResult = (*maybeFs)->seek(_fid, _inode, offset, direction);
	if (!seekResult) {
		return seekResult.moveError();
	}

	_readOffset = *seekResult;

	_vfs->updateNode(_nodeId, _inode);

	return Result<size_type>{types::okTag, _readOffset};
}


kasofs::Result<File::size_type>
File::seekWrite(size_type offset, Filesystem::SeekDirection direction) {
	auto maybeFs = _vfs->findFs(_inode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "File::seekWrite");
	}

	auto seekResult = (*maybeFs)->seek(_fid, _inode, offset, direction);
	if (!seekResult) {
		return seekResult.moveError();
	}

	_writeOffset = *seekResult;

	_vfs->updateNode(_nodeId, _inode);

	return Result<size_type>{types::okTag, _writeOffset};
}
