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
#ifndef KASOFS_DIRDRIVER_HPP
#define KASOFS_DIRDRIVER_HPP

#include "vinode.hpp"
#include "fs.hpp"


#include <unordered_map>


namespace kasofs {

/**
 * Directory entry - helper struct to represent a directory entry: Key and Value
 */
struct Entry {
	constexpr Entry(Solace::StringView n, INode::Id i) noexcept
		: name{n}
		, nodeId{i}
	{}

	Solace::StringView  name;
	INode::Id			nodeId;
};

/**
 * Helper class for directory enumerator.
 * Enables easy for-each loop
 */
struct EntriesEnumerator {
	using Entries = std::unordered_map<std::string, INode::Id>;
	using Iter = Entries::const_iterator;

	struct Iterator {

		bool operator!= (Iterator const& other) const noexcept {
			return (_position != other._position);
		}

		bool operator== (Iterator const& other) const noexcept {
			return (_position == other._position);
		}

		Iterator& operator++ () {
			++_position;
			return *this;
		}

		Entry operator* () const {
			return operator ->();
		}

		Entry operator-> () const {
			return Entry{Solace::StringView(_position->first.data(), _position->first.size()), _position->second};
		}

		Iter _position;
		Iter _end;
	};

	~EntriesEnumerator();

	EntriesEnumerator(struct Vfs& vfs, INode::Id dirId, Entries const& entries) noexcept;

	auto begin() const noexcept  { return Iterator{_entries.begin(), _entries.end()}; }
	auto end() const noexcept    { return Iterator{_entries.end(), _entries.end()}; }

private:
	Vfs&			_vfs;
	INode::Id		_dirId;
	Entries const&	_entries;
};




struct DirFs final : public Filesystem {

	using Entries = std::unordered_map<std::string, INode::Id>;

	static VfsId const kTypeId;
	static VfsNodeType const kNodeType;


	Result<INode>
	createNode(NodeType type, User owner, FilePermissions perms) override;

	Result<void>
	destroyNode(INode& node) override;


	FilePermissions defaultFilePermissions(NodeType) const noexcept override {
		return FilePermissions{0666};
	}

	auto open(INode&, Permissions op) -> Result<OpenFID> override;

	auto read(OpenFID, INode&, size_type, Solace::MutableMemoryView) -> Result<size_type> override;

	auto write(OpenFID, INode&, size_type, Solace::MemoryView) -> Result<size_type> override;

	auto seek(OpenFID, INode&, size_type, SeekDirection) -> Result<size_type> override;

	auto close(OpenFID, INode&) -> Result<void> override;

	Result<void>
	addEntry(INode& dirNode, Entry entry);

	Result<Solace::Optional<INode::Id>>
	removeEntry(INode& dirNode, Solace::StringView name);

	Solace::Optional<Entry>
	lookup(INode const& dirNode, Solace::StringView name) const;

	size_type
	countEntries(INode const& dirNode) const;

	Result<EntriesEnumerator>
	enumerateEntries(Vfs& vfs, INode::Id dirNodeId, INode const& dirNode) const;


	static bool isDirectoryNode(INode const& node) noexcept {
		return (kNodeType == node.nodeTypeId);
	}

private:

	using DataId = INode::VfsData;
	DataId nextId() noexcept { return _idBase++; }

	DataId _idBase{0};

	/// Directory entries - Named Graph edges
	std::unordered_map<DataId, Entries> _adjacencyList;
};


inline constexpr
bool isDirectory(INode const& vnode) noexcept {
	return ((vnode.fsTypeId == DirFs::kTypeId) && (vnode.nodeTypeId == DirFs::kNodeType));
}


}  // namespace kasofs
#endif  // KASOFS_DIRDRIVER_HPP
