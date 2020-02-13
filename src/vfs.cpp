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



Filesystem::~Filesystem() = default;


EntriesEnumerator::~EntriesEnumerator() {
	_vfs.releaseNode(_dirId);
}


EntriesEnumerator::EntriesEnumerator(Vfs& vfs, INode::Id dirId, Entries const& entries) noexcept
	: _vfs{vfs}
	, _dirId{dirId}
	, _entries{entries}
{
	_vfs.addNodeLink(_dirId);
}




Vfs::Vfs(User owner, FilePermissions rootPerms)
	: _index{}
	, _vfs{}
{
	_vfs.emplace(DirFs::kTypeId, std::make_unique<DirFs>());
	_nextId = 1;

	createUnlinkedNode(DirFs::kTypeId, DirFs::kNodeType, owner, rootPerms, FilePermissions{0666})
			.then([this](INode::Id rootId) {addNodeLink(rootId); });
}


kasofs::Result<void>
Vfs::unregisterFileSystem(VfsId fsId) {
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
	auto result = _directories.addEntry(dirNode, entry);
	if (result) {  // TODO(abbyssoul): this is a race condition as node could have been changed
		addNodeLink(to);  // (*maybeTargetNode).nLinks += 1;
	}

	return result;
}


kasofs::Result<void>
Vfs::unlink(User user, INode::Id fromDir, StringView name) {
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

	auto maybeEntry = _directories.lookup(dirNode, name);
	if (!maybeEntry)  // No entry - no-op.
		return Ok();

	auto& entry = *maybeEntry;
	auto maybeTargetNode = nodeById(entry.nodeId);
	if (maybeTargetNode) {
		auto& targetNode = *maybeTargetNode;
		if (isDirectory(targetNode) && _directories.countEntries(targetNode) > 0) {
			return makeError(SystemErrors::NOTEMPTY, "unlink");
		}
	}

	auto maybeUnlinked = _directories.removeEntry(dirNode, name);
	if (!maybeUnlinked) {
		return maybeUnlinked.moveError();
	}

	auto const& maybeNodeId = *maybeUnlinked;
	if (!maybeNodeId)
		return Ok();

	releaseNode(*maybeNodeId);
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

	return _directories.lookup(dirNode, name);
}


Optional<INode>
Vfs::nodeById(INode::Id id) const noexcept {
	if (id.index >= _index.size()) {
        return none;
    }

	auto& node = _index[id.index];
	if (id.gen != node.gen)
		return none;

	return node.inode;
}


kasofs::Result<void>
Vfs::updateNode(INode::Id id, INode inode) {
	if (id.index >= _index.size()) {
		return makeError(GenericError::BADF, "updateNode");
	}

	auto& existingNode = _index[id.index];
	if (id.gen != existingNode.gen)
		return makeError(GenericError::BADF, "updateNode");

	if (existingNode.inode.fsTypeId != inode.fsTypeId || existingNode.inode.nodeTypeId != inode.nodeTypeId)
		return makeError(GenericError::BADF, "updateNode");

	existingNode.inode.swap(inode);
	return Ok();
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

	return kasofs::Result<File>{types::okTag, in_place, this, fid, vnode, *maybeOpenedFiledId};
}



kasofs::Result<EntriesEnumerator>
Vfs::enumerateDirectory(User user, INode::Id dirNodeId) {
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

	// Enumerate content of a directory node
	return _directories.enumerateEntries(*this, dirNodeId, dirNode);
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
	return mknode(where, name, DirFs::kTypeId, DirFs::kNodeType, user, perms);
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
	auto maybeVfs = type == DirFs::kTypeId
			? Optional<Filesystem*>{&_directories}
			: findFs(type);

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

	auto const newNodeIndex = INode::Id(_index.size(), _genCount++);
	_index.emplace_back(newNodeIndex.gen, maybeNewNode.moveResult());

	return Ok(newNodeIndex);
}


void
Vfs::addNodeLink(INode::Id id) noexcept {
	if (id.index >= _index.size()) {
		return;
	}

	auto& node = _index[id.index];
	if (id.gen != node.gen)
		return;

	node.inode.nLinks += 1;
}


void
Vfs::releaseNode(INode::Id id) noexcept {
	if (id.index >= _index.size()) {
		return;
	}

	auto& node = _index[id.index];
	if (id.gen != node.gen)
		return;

	if (node.inode.nLinks > 0)
		node.inode.nLinks -= 1;

	if (node.inode.nLinks <= 0) {
		_index.erase(_index.begin() + id.index);
	}
}
