/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
#include "kasofs/directoryDriver.hpp"

#include <solace/posixErrorDomain.hpp>

#include <functional>  // find_if


using namespace kasofs;
using namespace Solace;

VfsId const DirFs::kTypeId{0};
VfsNodeType const DirFs::kNodeType{0};


namespace kasofs {  // to make sure ADL works

inline
bool operator== (std::string const& s, StringView const& sview) noexcept {
	return sview.equals(StringView(s.data(), s.size()));
}

inline
StringView as_string_view(std::string const& str) noexcept {
	return StringView(str.data(), str.size());
}

}  // namespace kasofs


kasofs::Result<INode>
DirFs::createNode(NodeType type, User owner, FilePermissions perms) {
	if (kNodeType != type) {
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

kasofs::Result<void>
DirFs::destroyNode(INode& node) {
	if (!isDirectoryNode(node)) {
		return makeError(GenericError::NOTDIR, "DirFs::destroyNode");
	}

	_adjacencyList.erase(node.vfsData);
	return Ok();
}


kasofs::Result<Filesystem::OpenFID>
DirFs::open(INode& dirNode, Permissions op) {
	if (!isDirectoryNode(dirNode)) {
		return makeError(GenericError::NOTDIR, "DirFs::open");
	}

	if (op.can(Permissions::READ) || op.can(Permissions::WRITE))
		return Ok<OpenFID>(0);

	return makeError(GenericError::PERM, "DirFS::open");
}


kasofs::Result<DirFs::size_type>
DirFs::read(OpenFID, INode& dirNode, size_type, MutableMemoryView) {
	if (!isDirectoryNode(dirNode)) {
		return makeError(GenericError::NOTDIR, "DirFs::read");
	}

	return makeError(GenericError::ISDIR, "DirFs::read");
}


kasofs::Result<DirFs::size_type>
DirFs::write(OpenFID, INode& dirNode, size_type, MemoryView) {
	if (!isDirectoryNode(dirNode)) {
		return makeError(GenericError::NOTDIR, "DirFs::write");
	}

	return makeError(GenericError::ISDIR, "DirFs::write");
}


kasofs::Result<DirFs::size_type>
DirFs::seek(OpenFID, INode& dirNode, size_type, SeekDirection) {
	if (!isDirectoryNode(dirNode)) {
		return makeError(GenericError::NOTDIR, "DirFs::seek");
	}

	return makeError(GenericError::ISDIR, "DirFs::seek");
}


kasofs::Result<void>
DirFs::close(OpenFID, INode& dirNode) {
	if (!isDirectoryNode(dirNode)) {
		return makeError(GenericError::NOTDIR, "DirFs::close");
	}

	return Ok();
}


kasofs::Result<void>
DirFs::addEntry(INode& dirNode, Entry entry) {
	if (!isDirectoryNode(dirNode)) {
		return makeError(GenericError::NOTDIR , "DirFs::addEntry");
	}

	auto it = _adjacencyList.find(dirNode.vfsData);
	if (it == _adjacencyList.end())
		return makeError(GenericError::NOENT, "DirFs::addEntry");

	auto maybeEmplaced = it->second.try_emplace(std::string{entry.name.data(), entry.name.size()}, entry.nodeId);
	if (maybeEmplaced.second == false) {
		return makeError(GenericError::EXIST, "DirFS::addEntry");
	}

	return Ok();
}


kasofs::Result<Optional<INode::Id>>
DirFs::removeEntry(INode& dirNode, StringView name) {
	if (!isDirectoryNode(dirNode)) {
		return makeError(GenericError::NOTDIR, "DirFs::removeEntry");
	}

	auto it = _adjacencyList.find(dirNode.vfsData);
	if (it == _adjacencyList.end())
		return makeError(GenericError::NOENT, "DirFs::removeEntry");

	Optional<INode::Id> removedId;
	auto& entries = it->second;
	auto tmp = std::string{name.data(), name.size()};
	auto entry = entries.find(tmp);
	if (entry != entries.end()) {
		removedId = entry->second;
		entries.erase(entry);
	}

	return removedId;
}


Optional<Entry>
DirFs::lookup(INode const& dirNode, StringView name) const noexcept {
	if (!isDirectoryNode(dirNode)) {
		return none;
	}

	auto it = _adjacencyList.find(dirNode.vfsData);
	if (it == _adjacencyList.end())
		return none;

	auto const& entries = it->second;
	auto entryIt = std::find_if(entries.begin(), entries.end(),
								[name](Entries::value_type const& e) { return e.first == name; });

	return (entryIt == entries.end())
			? none
			: Optional<Entry>{in_place, as_string_view(entryIt->first), entryIt->second};
}


DirFs::size_type
DirFs::countEntries(INode const& dirNode) const noexcept {
	if (!isDirectoryNode(dirNode)) {
		return 0;
	}

	auto it = _adjacencyList.find(dirNode.vfsData);
	return (it != _adjacencyList.end())
			? it->second.size()
			: 0;
}


kasofs::Result<EntriesEnumerator>
DirFs::enumerateEntries(Vfs& vfs, INode::Id dirNodeId, INode const& dirNode) const noexcept {
	if (!isDirectoryNode(dirNode)) {
		return makeError(GenericError::NOTDIR, "DirFs::enumerateEntries");
	}

	auto it = _adjacencyList.find(dirNode.vfsData);
	if (it == _adjacencyList.end())
		return makeError(GenericError::NOENT, "DirFs::enumerateEntries");

	auto& entries = it->second;
	return kasofs::Result<EntriesEnumerator>{types::okTag, in_place, vfs, dirNodeId, entries};
}
