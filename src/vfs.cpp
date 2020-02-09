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


#include <functional>


using namespace kasofs;
using namespace Solace;


StringLiteral const Vfs::kThisDir{"."};
StringLiteral const Vfs::kParentDir("..");


VfsId const Vfs::kVfsTypeDirectory{0};
VfsNodeType const Vfs::kVfsDirectoryNodeType{0};


Filesystem::~Filesystem() = default;

bool operator== (std::string s, StringView sview) noexcept {
	return sview.equals(StringView(s.data(), s.size()));
}

StringView as_string_view(std::string const& str) noexcept {
	return StringView(str.data(), str.size());
}


struct DirFs : public Filesystem {

//	using Entries = std::vector<Entry>;
	using Entries = std::unordered_map<std::string, INode::Id>;

	kasofs::Result<INode>
	createNode(NodeType type, User owner, FilePermissions perms) override {
		if (Vfs::kVfsDirectoryNodeType != type) {
			return makeError(GenericError::NOTDIR, "DirFs::createNode");
		}

		INode node{type, owner, perms};
		node.dataSize = 4096;
		node.vfsData = nextId();
		auto emplacement = _adjacencyList.try_emplace(node.vfsData, Entries{});
		if (!emplacement.second) {
			return makeError(GenericError::NFILE, "DirFs::createNode");
		}

		return kasofs::Result<INode>{types::okTag, in_place, mv(node)};
	}

	kasofs::Result<void> destroyNode(INode& node) override {
		_adjacencyList.erase(node.vfsData);
		return Ok();
	}


	FilePermissions defaultFilePermissions(NodeType) const noexcept override {
		return FilePermissions{0666};
	}

	auto open(INode&, Permissions op) -> kasofs::Result<OpenFID> override {
		if (op.can(Permissions::READ) || op.can(Permissions::WRITE))
			return Ok<OpenFID>(0);

		return makeError(GenericError::PERM, "DirFS::open");
	}

	auto read(OpenFID, INode&, size_type, MutableMemoryView) -> kasofs::Result<size_type> override {
		return makeError(GenericError::ISDIR, "DirFs::write");
	}

	auto write(OpenFID, INode&, size_type, MemoryView) -> kasofs::Result<size_type> override {
		return makeError(GenericError::ISDIR, "DirFs::write");
	}

	auto seek(OpenFID, INode&, size_type, SeekDirection) -> kasofs::Result<size_type> override {
		return makeError(GenericError::ISDIR, "DirFs::seek");
	}

	auto close(OpenFID, INode&) -> kasofs::Result<void> override {
		return Ok();
	}

	kasofs::Result<void>
	addEntry(INode const& dirNode, Entry entry) {
		if (dirNode.nodeTypeId != Vfs::kVfsDirectoryNodeType)
			return makeError(GenericError::NOTDIR , "DirFs::addEntry");

		auto const id = dirNode.vfsData;
		auto it = _adjacencyList.find(id);
		if (it == _adjacencyList.end())
			return makeError(GenericError::NOENT, "DirFs::addEntry");

		it->second.try_emplace(std::string{entry.name.data(), entry.name.size()}, entry.nodeId);
		return Ok();
	}


	kasofs::Result<Optional<INode::Id>>
	removeEntry(INode& dirNode, StringView name) {
		if (dirNode.nodeTypeId != Vfs::kVfsDirectoryNodeType)
			return makeError(GenericError::NOTDIR , "DirFs::removeEntry");

		auto const id = dirNode.vfsData;
		auto it = _adjacencyList.find(id);
		if (it == _adjacencyList.end())
			return makeError(GenericError::NOENT, "DirFs::removeEntry");

		Optional<INode::Id> removedId;
		auto& entries = it->second;
		auto tmp = std::string{name.data(), name.size()};
		auto entry = entries.find(tmp);
		if (entry != entries.end())
			removedId = entry->second;

		entries.erase(tmp);
//		enties.erase(std::remove_if(enties.begin(), enties.end(), [name, &removedId](Entries::value_type const& entry) {
//														if (entry.first == name) {
//															removedId = entry.second;
//															return true;
//														}
//														return false;
//													}),
//				enties.end());

		return removedId;
	}


	Optional<Entry>
	lookup(INode const& dirNode, StringView name) const {
		if (dirNode.nodeTypeId != Vfs::kVfsDirectoryNodeType)
			return none;

		auto const id = dirNode.vfsData;
		auto it = _adjacencyList.find(id);
		if (it == _adjacencyList.end())
			return none;

		auto const& entries = it->second;
//		auto entryIt = enties.find(name)
		auto entryIt = std::find_if(entries.begin(), entries.end(), [name](Entries::value_type const& e) { return e.first == name; });

		return (entryIt == entries.end())
				? none
				: Optional<Entry>{in_place, as_string_view(entryIt->first), entryIt->second};
	}


	kasofs::Result<EntriesEnumerator>
	enumerateEntries(INode const& dirNode) const {
		if (dirNode.nodeTypeId != Vfs::kVfsDirectoryNodeType)
			return makeError(GenericError::NOTDIR, "DirFs::enumerateEntries");

		auto const id = dirNode.vfsData;
		auto it = _adjacencyList.find(id);
		if (it == _adjacencyList.end())
			return makeError(GenericError::NOENT, "DirFs::enumerateEntries");

		// FIXME: It is possbile to unlink a dirctory while it is enumerated and get UB!
		auto& entries = it->second;
		return kasofs::Result<EntriesEnumerator>{types::okTag, in_place, std::begin(entries), std::end(entries)};
	}

private:
	using DataId = INode::VfsData;
	DataId nextId() noexcept { return _idBase++; }

	DataId _idBase{0};

	/// Directory entries - Named Graph edges
	std::unordered_map<DataId, Entries> _adjacencyList;
};



Vfs::Vfs(User owner, FilePermissions rootPerms)
	: _index{}
	, _vfs{}
{
	_vfs.emplace(kVfsTypeDirectory, std::make_unique<DirFs>());
	_nextId = 1;

	createUnlinkedNode(kVfsTypeDirectory, kVfsDirectoryNodeType, owner, rootPerms, FilePermissions{0666});
}


kasofs::Result<void>
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
kasofs::Result<void>
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


kasofs::Result<void>
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



kasofs::Result<void>
Vfs::link(User user, StringView linkName, INode::Id from, INode::Id to) {
    if (from == to) {
		return makeError(GenericError::BADF, "link:from::to");
    }

    auto maybeNode = nodeById(from);
    if (!maybeNode) {
		return makeError(GenericError::NOENT, "link:from");
    }

	auto& dirNode = *maybeNode;
	if (!isDirectory(dirNode)) {
		return makeError(GenericError::NOTDIR, "link");
    }

	if (!dirNode.userCan(user, Permissions::WRITE)) {
		return makeError(GenericError::PERM, "link");
    }

	auto maybeTargetNode = nodeById(to);
	if (!maybeTargetNode) {
		return makeError(GenericError::NOENT, "link:to");
    }

    // Add new entry:
	Entry entry{linkName, to};
	auto dirFs = static_cast<DirFs*>(*findFs(kVfsTypeDirectory));
	auto result = dirFs->addEntry(dirNode, entry);
	if (result) {  // TODO(abbyssoul): this is a race condition as node could have been changed
		_index[to].nLinks += 1;  // (*maybeTargetNode).nLinks += 1;
	}

	return result;
}


kasofs::Result<void>
Vfs::unlink(User user, StringView name, INode::Id fromDir) {
	auto maybeDirNode = nodeById(fromDir);
	if (!maybeDirNode) {
		return makeError(GenericError::BADF, "unlink");
    }

	auto& dirNode = *maybeDirNode;
	if (!isDirectory(dirNode)) {
		return makeError(GenericError::NOTDIR, "unlink");
    }

	if (!dirNode.userCan(user, Permissions::WRITE)) {
		return makeError(GenericError::PERM, "unlink");
    }

	auto dirFs = static_cast<DirFs*>(*findFs(kVfsTypeDirectory));
	auto maybeUnlinked = dirFs->removeEntry(dirNode, name);
	if (!maybeUnlinked) {
		return maybeUnlinked.moveError();
	}

	auto const& maybeNodeId = *maybeUnlinked;
	if (!maybeNodeId)
		return Ok();

	auto maybeRemovedNode = nodeById(*maybeNodeId);
	if (!maybeRemovedNode)
		return Ok();

	auto node = *maybeRemovedNode;
	node.nLinks -= 1;
	if (!node.nLinks) {
		_index.erase(_index.begin() + *maybeNodeId);
	}

	return Ok();
}


Optional<Entry>
Vfs::lookup(INode::Id dirNodeId, StringView name) const {
    auto const maybeNode = nodeById(dirNodeId);
    if (!maybeNode) {
        return none;
    }

	auto const& dirNode = *maybeNode;
	if (!isDirectory(dirNode)) {
        return none;
    }

	auto dirFs = static_cast<DirFs*>(*findFs(kVfsTypeDirectory));
	return dirFs->lookup(dirNode, name);
}


Optional<INode>
Vfs::nodeById(INode::Id id) const noexcept {
	if (id >= _index.size()) {
        return none;
    }

	return _index[id];
}


void Vfs::updateNode(INode::Id id, INode inode) {
	if (id >= _index.size()) {
		return;
	}

	_index[id].swap(inode);
}


kasofs::Result<File>
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

	return kasofs::Result<File>{types::okTag, in_place, this, fid, *maybeOpenedFiledId};
//	return File{this, fid, *maybeOpenedFiledId};
}



kasofs::Result<EntriesEnumerator>
Vfs::enumerateDirectory(INode::Id dirNodeId, User user) const {
    auto maybeNode = nodeById(dirNodeId);
    if (!maybeNode) {
		return makeError(GenericError::BADF, "enumerateDirectory");
    }

	auto& dirNode = *maybeNode;
	if (!isDirectory(dirNode)) {
		return makeError(GenericError::NOTDIR, "enumerateDirectory");
    }

	if (!dirNode.userCan(user, Permissions::READ)) {
		return makeError(GenericError::PERM, "enumerateDirectory");
    }

	// lookup named entry:
	auto dirFs = static_cast<DirFs*>(*findFs(kVfsTypeDirectory));
	return dirFs->enumerateEntries(dirNode);
}


Optional<Filesystem*>
Vfs::findFs(VfsId id) const {
	auto it = _vfs.find(id);
	if (it == _vfs.end())
		return none;

	return it->second.get();
}


kasofs::Result<INode::Id>
Vfs::createDirectory(INode::Id where, StringView name, User user, FilePermissions perms) {
	return mknode(where, name, kVfsTypeDirectory, kVfsDirectoryNodeType, user, perms);
}


kasofs::Result<INode::Id>
Vfs::mknode(INode::Id where, StringView name, VfsId type, VfsNodeType nodeType, User owner, FilePermissions perms) {
	auto const maybeRoot = nodeById(where);
	if (!maybeRoot) {
		return makeError(GenericError::NOENT , "mkNode");
	}

	auto const& dir = *maybeRoot;
	if (!isDirectory(dir)) {
		return makeError(GenericError::NOTDIR, "mkNode");
	}

	if (!dir.userCan(owner, Permissions::WRITE)) {
		return makeError(GenericError::PERM, "mkNode");
	}

	auto maybeNewNodeID = createUnlinkedNode(type, nodeType, owner, perms, dir.permissions);
	if (!maybeNewNodeID) {  // FIXME: new node leakage in case of linking error
		return maybeNewNodeID.moveError();
	}

	// Link
	auto linkResult = link(owner, name, where, *maybeNewNodeID);
	if (!linkResult) {  // FIXME: failed to link a new node - node must be removed.
		return linkResult.moveError();
	}

	return maybeNewNodeID;
}


kasofs::Result<INode::Id>
Vfs::createUnlinkedNode(VfsId type, VfsNodeType nodeType, User owner, FilePermissions perms, FilePermissions baseP) {
	auto maybeVfs = findFs(type);
	if (!maybeVfs) {  // Unsupported VFS specified
		return makeError(SystemErrors::PROTONOSUPPORT, "mknode");
	}

	auto& vfs = *maybeVfs;
	uint32 const dirPerms = baseP.value;
	uint32 const permBase = vfs->defaultFilePermissions(nodeType).value;
	auto const effectivePermissions = FilePermissions{perms.value & (~permBase | (dirPerms & permBase))};

	auto maybeNewNode = vfs->createNode(nodeType, owner, effectivePermissions);
	if (!maybeNewNode) {  // FIXME: will leak in case emplace_back throws
		return maybeNewNode.moveError();
	}

	auto& newNode = *maybeNewNode;
	newNode.fsTypeId = type;

	auto const newNodeIndex = INode::Id(_index.size());
	_index.emplace_back(maybeNewNode.moveResult());

	return Ok(newNodeIndex);
}
