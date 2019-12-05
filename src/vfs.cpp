/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
#include "kasofs/vfs.hpp"

#include <solace/posixErrorDomain.hpp>


using namespace kasofs;
using namespace Solace;


StringLiteral const Vfs::kThisDir{"."};
StringLiteral const Vfs::kParentDir("..");


VfsId const Vfs::kVfsTypeDirectory{0};
VfsNodeType const Vfs::kVfsDirectoryNodeType{0};


Filesystem::~Filesystem() = default;


struct DirFs : public Filesystem {

	Result<INode::VfsData, Error> createNode(NodeType type) override {
		if (Vfs::kVfsDirectoryNodeType != type) {
			return makeError(SystemErrors::MEDIUMTYPE, "DirFs::createNode");
		}

		auto const dataIndex = adjacencyList.size();
		adjacencyList.emplace_back();

		return Ok(dataIndex);
	}

	FilePermissions defaultFilePermissions(NodeType) const noexcept override {
		return FilePermissions{0666};
	}

	auto open(INode, Permissions op) -> Result<OpenFID, Error> override {
		if (op.can(Permissions::READ) || op.can(Permissions::WRITE))
			return Ok<OpenFID>(0);

		return makeError(GenericError::PERM, "DirFS::open");
	}

	auto read(INode, size_type, MutableMemoryView) -> Result<size_type, Error> override {
		return makeError(GenericError::ISDIR, "DirFs::write");
	}

	auto write(INode, size_type, MemoryView) -> Result<size_type, Error> override {
		return makeError(GenericError::ISDIR, "DirFs::write");
	}

	auto close(OpenFID, INode) -> Result<void, Error> override {
		return Ok();
	}

	Result<void, Error> addEntry(INode::VfsData id, Entry entry) {
		if (id >= adjacencyList.size())
			return makeError(GenericError::NOENT, "DirFs::addEntry");

		adjacencyList[id].emplace_back(std::move(entry));
		return Ok();
	}


	Result<void, Error> removeEntry(INode::VfsData id, StringView name) {
		if (id >= adjacencyList.size())
			return makeError(GenericError::NOENT, "DirFs::removeEntry");

		auto& e = adjacencyList[id];
		e.erase(std::remove_if(e.begin(), e.end(), [name](Entry const& entry) { return (name == entry.name); }),
								  e.end());

		return Ok();
	}

	Optional<Entry> lookup(INode::VfsData id, StringView name) const {
		if (id >= adjacencyList.size())
			return none;

		auto const& enties = adjacencyList[id];
		auto it = std::find_if(enties.begin(), enties.end(), [name](Entry const& e) { return e.name == name; });

		return (it == enties.end())
				? none
				: Optional{*it};
	}


	EntriesIterator enumerateEntries(INode::VfsData id) const {
		if (id >= adjacencyList.size())
			return {nullptr, nullptr};

		auto& entries = adjacencyList[id];
		return {entries.data(), entries.data() + entries.size()};
	}

private:
	/// Directory entries - Named Graph edges
	std::vector<std::vector<Entry>> adjacencyList;

};



Vfs::Vfs(User owner, FilePermissions rootPerms)
	: _index{{INode{0, kVfsTypeDirectory, kVfsDirectoryNodeType, 0, owner, rootPerms}}}
	, _vfs{}
{
	_vfs.emplace(kVfsTypeDirectory, std::make_unique<DirFs>());
	_nextId = 1;

	auto& root = _index[0];  // Adjust root node to point to first data block
	auto dirFs = static_cast<DirFs*>(*findFs(kVfsTypeDirectory));
	root.vfsData = dirFs->createNode(kVfsTypeDirectory).unwrap();
}


//INode const& Vfs::root() const { return inodes.at(0); }
//INode& Vfs::root() { return inodes.at(0); }


//Result<VfsId, Error>
//Vfs::registerFileSystem(VfsOps&& fs) {
//    auto const fsId = vfs.size();
//    vfs.emplace_back(std::move(fs));

//	return Ok(static_cast<VfsId>(fsId));
//}

Result<void, Error>
Vfs::unregisterFileSystem(VfsId fsId) {
	if (fsId == kVfsTypeDirectory) {
		return makeError(GenericError::BADF, "unregisterFileSystem");
	}

	// FIXME: Check if fs is busy / in use
	auto nRemoved = _vfs.erase(fsId);
	if (!nRemoved) {
		return makeError(GenericError::BADF, "unregisterFileSystem");
    }

    return Ok();
}

/*
Result<void, Error>
Vfs::mount(User user, INode::Id mountingPoint, VfsId fs) {
    auto maybeMountingPoint = nodeById(mountingPoint);
    if (!maybeMountingPoint) {
		return makeError(GenericError::BADF, "mount");
    }

	auto& dir = *maybeMountingPoint;
    if (!dir.userCan(user, Permissions::WRITE)) {
		return makeError(GenericError::PERM, "mount");
    }

    mounts.emplace_back(fs, mountingPoint);

    return Ok();
}


Result<void, Error>
Vfs::umount(User user, INode::Id mountingPoint) {
    auto maybeMountingPoint = nodeById(mountingPoint);
    if (!maybeMountingPoint) {
		return makeError(GenericError::BADF, "mount");
    }

	auto& dir = *maybeMountingPoint;
    if (!dir.userCan(user, Permissions::WRITE)) {
		return makeError(GenericError::PERM, "mount");
    }

    auto mntEntriesId = std::remove_if(mounts.begin(), mounts.end(), [mountingPoint](auto const& n) { return (mountingPoint == n.mountingPoint); });
    for (auto i = mntEntriesId; i != mounts.end(); ++i) {
        vfs.erase(vfs.end());
    }
    mounts.erase(mntEntriesId, mounts.end());

    return Ok();
}
*/


constexpr
bool isDir(INode const& vnode) noexcept {
	return (vnode.fsTypeId == Vfs::kVfsTypeDirectory && vnode.nodeTypeId == Vfs::kVfsDirectoryNodeType);

}

Result<void, Error>
Vfs::link(User user, StringView linkName, INode::Id from, INode::Id to) {
    if (from == to) {
		return makeError(GenericError::BADF, "link:from::to");
    }

    auto maybeNode = nodeById(from);
    if (!maybeNode) {
		return makeError(GenericError::NOENT, "link:from");
    }

	auto& dirNode = *maybeNode;
	if (!isDir(dirNode)) {
		return makeError(GenericError::NOTDIR, "link");
    }

	if (!dirNode.userCan(user, Permissions::WRITE)) {
		return makeError(GenericError::PERM, "link");
    }

    if (nodeById(to).isNone()) {
		return makeError(GenericError::NOENT, "link:to");
    }

    // Add new entry:
    // TODO: Check dataIndex exist?
	Entry entry{linkName, to};
	auto dirFs = static_cast<DirFs*>(*findFs(kVfsTypeDirectory));
	return dirFs->addEntry(dirNode.vfsData, entry);
//	adjacencyList[dirNode.dataIndex].emplace_back(linkName, to);
}


Result<void, Error>
Vfs::unlink(User user, StringView name, INode::Id from) {
    auto maybeNode = nodeById(from);
    if (!maybeNode) {
		return makeError(GenericError::BADF, "unlink");
    }

	auto& node = *maybeNode;
	if (!isDir(node)) {
		return makeError(GenericError::NOTDIR, "unlink");
    }

    if (!node.userCan(user, Permissions::WRITE)) {
		return makeError(GenericError::PERM, "unlink");
    }

	// lookup named entry:
	auto dirFs = static_cast<DirFs*>(*findFs(kVfsTypeDirectory));
	return dirFs->removeEntry(node.vfsData, name);
}

Optional<Entry>
Vfs::lookup(INode::Id dirNodeId, StringView name) const {
    auto const maybeNode = nodeById(dirNodeId);
    if (!maybeNode) {
        return none;
    }

	auto const& dirNode = *maybeNode;
	if (!isDir(dirNode)) {
        return none;
    }

	auto dirFs = static_cast<DirFs*>(*findFs(kVfsTypeDirectory));
	return dirFs->lookup(dirNode.vfsData, name);
}


Optional<INode>
Vfs::nodeById(INode::Id id) const noexcept {
	if (id >= _index.size()) {
        return none;
    }

	return _index.at(id);
}

Result<File, Error>
Vfs::open(User user, INode::Id fid, Permissions op) {
	auto maybeNode = nodeById(fid);
	if (!maybeNode) {
		return makeError(GenericError::BADF, "open");
	}

	auto& vnode = *maybeNode;
	if (!vnode.userCan(user, op)) {
		return makeError(GenericError::PERM, "open");
	}

	auto maybeFs = findFsOf(vnode);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "open");
	}

	auto* fs = *maybeFs;
	auto maybeOpenedFiledId = fs->open(vnode, op);
	if (!maybeOpenedFiledId) {
		return maybeOpenedFiledId.moveError();
	}

	return File{this, vnode, *maybeOpenedFiledId};
}


File::~File() {
	auto maybeFs = vfs->findFs(inode.fsTypeId);
	if (maybeFs) {
		(*maybeFs)->close(fid, inode);
	}
}

Result<File::size_type, Error>
File::read(MutableMemoryView dest) {
	auto maybeFs = vfs->findFs(inode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "read");
	}

	return (*maybeFs)->read(inode, readOffset, dest);
}

Result<File::size_type, Error>
File::write(MemoryView src) {
	auto maybeFs = vfs->findFs(inode.fsTypeId);
	if (!maybeFs) {
		return makeError(GenericError::NXIO, "write");
	}

	return (*maybeFs)->write(inode, writeOffset, src);
}



Result<EntriesIterator, Error>
Vfs::enumerateDirectory(INode::Id dirNodeId, User user) const {
    auto maybeNode = nodeById(dirNodeId);
    if (!maybeNode) {
		return makeError(GenericError::BADF, "enumerateDirectory");
    }

	auto& dirNode = *maybeNode;
	if (!isDir(dirNode)) {
		return makeError(GenericError::NOTDIR, "enumerateDirectory");
    }

	if (!dirNode.userCan(user, Permissions::READ)) {
		return makeError(GenericError::PERM, "enumerateDirectory");
    }

	// lookup named entry:
	auto dirFs = static_cast<DirFs*>(*findFs(kVfsTypeDirectory));
	return dirFs->enumerateEntries(dirNode.vfsData);
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


Optional<Filesystem*>
Vfs::findFs(VfsId id) const {
	auto it = _vfs.find(id);
	if (it == _vfs.end())
		return none;

	return it->second.get();
}


Result<INode::Id, Error>
Vfs::createDirectory(INode::Id where, StringView name, User user, FilePermissions perms) {
	return mknode(where, name, kVfsTypeDirectory, kVfsDirectoryNodeType, user, perms);
}


Result<INode::Id, Error>
Vfs::mknode(INode::Id where, StringView name, VfsId type, VfsNodeType nodeType, User owner, FilePermissions requredPerms) {
	auto maybeVfs = findFs(type);
	if (!maybeVfs) {  // Unsupported VFS specified
		return makeError(SystemErrors::PROTONOSUPPORT, "mknode");
	}

	auto const maybeRoot = nodeById(where);
	if (!maybeRoot) {
		return makeError(GenericError::NOENT , "mkNode");
	}

	auto const& dir = *maybeRoot;
	if (!isDir(dir)) {
		return makeError(GenericError::NOTDIR, "mkNode");
	}

	if (!dir.userCan(owner, Permissions::WRITE)) {
		return makeError(GenericError::PERM, "mkNode");
	}

	auto& vfs = *maybeVfs;
	auto maybeData = vfs->createNode(nodeType);  // NOTE: will leak in case emplace_back throws
	if (!maybeData) {
		return maybeData.moveError();
	}

	uint32 const dirPerms = dir.permissions.value;
	uint32 const permBase = vfs->defaultFilePermissions(nodeType).value;
	auto const effectivePermissions = FilePermissions{requredPerms.value & (~permBase | (dirPerms & permBase))};
	auto const newNodeIndex = INode::Id(_index.size());
	_index.emplace_back(newNodeIndex, type, nodeType, *maybeData, owner, effectivePermissions);

	// Link
	auto dirFs = static_cast<DirFs*>(*findFs(kVfsTypeDirectory));
	auto maybeLinked = dirFs->addEntry(dir.vfsData, Entry{name, newNodeIndex});
	if (!maybeLinked) {
		return maybeLinked.moveError();
	}

    return Ok(newNodeIndex);
}
