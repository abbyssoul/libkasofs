/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
#include "vinode.hpp"

#include <solace/posixErrorDomain.hpp>

#include <ctime>


using namespace kasofs;
using namespace Solace;


INode::Type
INode::type() const noexcept {
    struct TypeVisitor {
        constexpr Type operator() (INode::Dir  const& ) const noexcept { return Type::Directory; }
        constexpr Type operator() (INode::Data const& ) const noexcept { return Type::Data; }
        constexpr Type operator() (INode::Synth const& ) const noexcept { return Type::Synthetic; }
        constexpr Type operator() (INode::SynthDir const& ) const noexcept { return Type::SyntheticDir; }
    };

    return std::visit(TypeVisitor{}, _nodeData);
}


Result<void, Error>
INode::setPermissions(FilePermissions perms) noexcept {
    _meta.mode = _meta.mode.withPermissions(perms);

    return Ok();
}

bool INode::userCan(User user, Permissions op) const noexcept {
    auto const perms = (_meta.owner.uid == user.uid)
            ? _meta.mode.permissions().user()
            : (_meta.owner.gid == user.gid)
              ? _meta.mode.permissions().group()
              : _meta.mode.permissions().others();

    return perms.can(op);
}


uint64 INode::length() const noexcept {
    struct SizingVisitor {
        uint64 operator() (INode::Data const& d) const noexcept { return d.length; }
        uint64 operator() (INode::Dir const&) const noexcept { return 4096; }
        uint64 operator() (INode::SynthDir const&) const noexcept { return 4096; }
        uint64 operator() (INode::Synth const& /*d*/) const noexcept { return 0; }
    };

    return std::visit(SizingVisitor{}, _nodeData);
}

/*
void
INode::addDirEntry(StringView entName, index_type index) {
    struct AddDirEntryVisitor {
        constexpr void operator() (INode::Data& ) const noexcept { }
        constexpr void operator() (INode::Synth& ) const noexcept { }

        void operator() (INode::Dir& d) const {
            d.entries.emplace_back(entName, index);
        }

        constexpr void operator() (INode::SynthDir& ) const noexcept { }

        StringView entName;
        uint32 index;
    };

//    _meta.mtime = std::time(nullptr);

    return std::visit(AddDirEntryVisitor{entName, index}, _nodeData);
}

Optional<INode::Entry> INode::findEntry(StringView name) const {
    struct FindDirEntryVisitor {
        Optional<Entry> operator() (INode::Synth const&) const noexcept { return none; }
        Optional<Entry> operator() (INode::Data const&) const noexcept { return none; }
        Optional<Entry> operator() (INode::Dir const& d) const noexcept {
            for (auto& e : d.entries) { if (e.name == name) { return Optional<Entry>(e); } }
            return none;
        }

        Optional<Entry> operator() (INode::SynthDir const& d) const noexcept {
            for (auto& e : d.iter()) { if (e.name == name) { return Optional<Entry>(e); } }
            return none;
        }

        StringView name;
    };

//    _meta.atime = std::time(nullptr);
    return std::visit(FindDirEntryVisitor{name}, _nodeData);
}


INode::EntriesIterator
INode::entries() const {
    struct ListDirEntryVisitor {
        EntriesIterator operator() (INode::Synth const& ) const noexcept { return {nullptr, nullptr}; }
        EntriesIterator operator() (INode::Data const& ) const noexcept { return {nullptr, nullptr}; }
        EntriesIterator operator() (INode::Dir const& d) const noexcept { return d.iter(); }
        EntriesIterator operator() (INode::SynthDir const& d) const noexcept { return d.iter(); }
    };

//    _meta.atime = std::time(nullptr);
    return std::visit(ListDirEntryVisitor{}, _nodeData);
}

INode::index_type
INode::entriesCount() const {
    struct CountDirEntryVisitor {
        index_type operator() (INode::Synth const& ) const noexcept { return 0; }
        index_type operator() (INode::Data const& ) const noexcept { return 0; }
        index_type operator() (INode::Dir const& d) const noexcept { return d.size(); }
        index_type operator() (INode::SynthDir const& d) const noexcept { return d.size(); }
    };

    return std::visit(CountDirEntryVisitor{}, _nodeData);
}
*/

Result<ByteReader, Error>
INode::reader(User user) noexcept {
    struct DataAccessorVisitor {
        ByteReader operator() (INode::Synth& d) noexcept { return d.reader(); }
        ByteReader operator() (INode::Data& d) noexcept { return d.reader(); }
        ByteReader operator() (INode::Dir& ) noexcept { return {}; }
        ByteReader operator() (INode::SynthDir& ) noexcept { return {}; }
    };

    if (!userCan(user, Permissions::READ)) {
        return Err(makeError(GenericError::PERM, "read"));
    }

    _meta.atime = std::time(nullptr);
    return Ok(std::visit(DataAccessorVisitor{}, _nodeData));
}

Result<ByteWriter, Error>
INode::writer(User user) noexcept {
    struct DataAccessorVisitor {
        ByteWriter operator() (INode::Synth& d) noexcept { return d.writer(); }
        ByteWriter operator() (INode::Data& d) noexcept { return d.writer(); }
        ByteWriter operator() (INode::Dir& ) noexcept { return {}; }
        ByteWriter operator() (INode::SynthDir& ) noexcept { return {}; }
    };

    if (!userCan(user, Permissions::WRITE)) {
        return Err(makeError(GenericError::PERM, "write"));
    }

    _meta.mtime = std::time(nullptr);
    return Ok(std::visit(DataAccessorVisitor{}, _nodeData));
}


//MemoryView
//INode::data() const noexcept {
//    struct DataAccessorVisitor {
//        constexpr MemoryView operator() (INode::Data const& d) const noexcept { return d.view(); }
//        constexpr MemoryView operator() (INode::Dir const& ) const noexcept { return {}; }
//    };

//    return std::visit(DataAccessorVisitor{}, _nodeData);
//}


//MutableMemoryView
//INode::data() noexcept {
//    struct DataAccessorVisitor {
//        constexpr MutableMemoryView operator() (INode::Data& d) const noexcept {
//            auto view  = d.view();
//            meta.length = view.size();

//            return view;
//        }
//        constexpr MutableMemoryView operator() (INode::Dir& ) const noexcept { return {}; }

//        Meta& meta;
//    };

//    _meta.atime = std::time(nullptr);

//    return std::visit(DataAccessorVisitor{_meta}, _nodeData);
//}


void INode::Data::DataReadDisposer::dispose(MemoryView* view) const {
    MemoryView::size_type count = 0;
    for (; count < view->size(); ++count) {
        if (*(view->begin() + count) == 0)
            break;
    }

    self->length = count;
}

INode::Data::Data() noexcept
    : disposer{this}
{
    wrapMemory(buffer).fill(0);
}

ByteReader INode::Data::reader() const noexcept {
    return ByteReader{wrapMemory(buffer, length)};
}

ByteWriter INode::Data::writer() noexcept {
    disposer.self = this;
    return ByteWriter{MemoryResource{wrapMemory(buffer), &disposer}};
}
