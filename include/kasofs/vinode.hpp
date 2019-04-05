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
 *	@file		vinode.hpp
 ******************************************************************************/
#pragma once
#ifndef KASOFS_VINODE_HPP
#define KASOFS_VINODE_HPP

#include "credentials.hpp"

#include <solace/result.hpp>
#include <solace/error.hpp>

#include <solace/byteReader.hpp>
#include <solace/byteWriter.hpp>



namespace kasofs {

enum class NodeType : Solace::byte {
    DIR    = 0x80,  //!< directories
    APPEND = 0x40,  //!< append only files
    EXCL   = 0x20,  //!< exclusive use files
    MOUNT  = 0x10,  //!< mounted channel
    AUTH   = 0x08,  //!< authentication file (afid)
    TMP    = 0x04,  //!< non-backed-up file
    LINK   = 0x02,  //!< bit for symbolic link (Unix, 9P2000.u)
    FILE   = 0x00,  //!< bits for plain file
};


/**
 * Node of virtual file system
 */
struct INode {

    using index_type = Solace::uint32;

    struct Meta {
        Solace::uint64      path{0};
        Solace::uint32      version{0};
        FileMode            mode;           //!< permissions and flags

        Solace::uint32      atime{0};      //!< last read time
        Solace::uint32      mtime{0};      //!< last write time

        User                owner;

        constexpr Meta(Solace::uint64 index, User user, FileMode fmode) noexcept
            : path{index}
            , mode{fmode}
            , owner{user}
        {}
    };

    enum class Type : Solace::byte {
        Directory = 0,
        Data,
        Synthetic,
        SyntheticDir
    };


    template<Type t>
    struct TypeTag {
        constexpr Type type() const noexcept { return t; }
    };

    struct Entry {
        constexpr Entry(Solace::StringView n, index_type i) noexcept
            : name{n}
            , index{i}
        {}

        Solace::StringView  name;
        index_type          index;
    };

public:

    template<Type t>
    INode(TypeTag<t> tag, User owner, Solace::uint64 path, FilePermissions perms);

    template<typename R,
             typename W>
    INode(User owner, Solace::uint64 path, FilePermissions perms, R&& r, W&& w);

    template<typename F>
    INode(User owner, Solace::uint64 path, FilePermissions perms, F&& f);

    /// Get type of the node
    Type type() const noexcept;

    /// Get node matadata
    Meta meta() const noexcept { return _meta; }

    Solace::Result<void, Solace::Error> setPermissions(FilePermissions perms) noexcept;
    bool userCan(User user, Permissions perms) const noexcept;

    /** @return Length of the file data in bytes */
    Solace::uint64 length() const noexcept;


    //
    // Directory specific operations
    //

    /// Add new directory entry
//    void addDirEntry(Solace::StringView entName, index_type index);
//    /// Remove existing entry
//    void removeEntry(Solace::StringView entName, index_type index);
//    /// Get iterator for all the entries
//    EntriesIterator entries() const;
//    /// Count number of entries
//    index_type entriesCount() const;


    /// Find an entry by name
//    Solace::Optional<Entry> findEntry(Solace::StringView name) const;

    //
    // Data centric operations
    //
    Solace::Result<Solace::ByteWriter, Solace::Error> writer(User user) noexcept;
    Solace::Result<Solace::ByteReader, Solace::Error> reader(User user) noexcept;


protected:

    /// List of directory enties
//    struct Dir {
//        std::vector<Entry> entries;

//        index_type size() const noexcept {
//            return entries.size();
//        }

//        EntriesIterator iter() const {
//            return EntriesIterator{entries.data(), entries.data() + entries.size()};
//        }
//    };
/*
    /// Data stored with the node
    struct Data {
        Solace::uint64 length{0};
        char buffer[512];

        Data() noexcept;

        Solace::ByteReader reader() const noexcept;
        Solace::ByteWriter writer() noexcept;

    private:
        struct DataReadDisposer
                : public Solace::MemoryResource::Disposer {
            void dispose(Solace::MemoryView* view) const override;

            constexpr DataReadDisposer(Data* ref) noexcept
                : self{ref}
            {}

            Data* self;
        } disposer;
    };

    /// Synthetic storage with the node
    struct Synth {

        Solace::ByteReader reader() const noexcept {
            return _onRead();
        }

        Solace::ByteWriter writer() noexcept {
            return _onWrite();
        }

        template<typename R, typename W>
        Synth(R&& r, W&& w)
            : _onRead{std::forward<R>(r)}
            , _onWrite{std::forward<W>(w)}
        {}

    private:

        std::function<Solace::ByteReader()> _onRead;
        std::function<Solace::ByteWriter()> _onWrite;
    };

    /// Synthetic directory with the node
    struct SynthDir {

        template<typename F>
        SynthDir(F&& f)
            : _listEntries{std::forward<F>(f)}
        {}

        EntriesIterator iter() const {
            return _listEntries();
        }

        index_type size() const noexcept {
            index_type count = 0;
            for (auto const& e : iter()) {
                (void)(e);
                count += 1;
            }

            return count;
        }

    private:
        std::function<EntriesIterator()> _listEntries;
    };
*/
private:

    Meta                                        _meta;
//    std::variant<Dir, Data, Synth, SynthDir>    _nodeData;
};


//inline
//auto initMeta(INode::TypeTag<INode::Type::Directory>, User owner, Solace::uint64 path, FilePermissions perms) noexcept {
//    return INode::Meta{path, owner, {DirMode::DIR, perms}};
//}

//inline
//auto initMeta(INode::TypeTag<INode::Type::Data>, User owner, Solace::uint64 path, FilePermissions perms) noexcept {
//    return INode::Meta{path, owner, {DirMode::REGULAR, perms}};
//}

//inline
//auto initMeta(INode::TypeTag<INode::Type::Synthetic>, User owner, Solace::uint64 path, FilePermissions perms) noexcept {
//    return INode::Meta{path, owner, {DirMode::TMP, perms}};
//}

//inline
//auto initMeta(INode::TypeTag<INode::Type::SyntheticDir>, User owner, Solace::uint64 path, FilePermissions perms) noexcept {
//    return INode::Meta{path, owner, {DirMode::DIR, perms}};
//}



//template<>
//inline
//INode::INode(INode::TypeTag<INode::Type::Directory> tag, User owner, Solace::uint64 path, FilePermissions perms)
//    : _meta{initMeta(tag, owner, path, perms)}
//    , _nodeData{std::in_place_type<INode::Dir>}
//{}


//template<>
//inline
//INode::INode(INode::TypeTag<INode::Type::Data> tag, User owner, Solace::uint64 path, FilePermissions perms)
//    : _meta{initMeta(tag, owner, path, perms)}
//    , _nodeData{std::in_place_type<INode::Data>}
//{ }


//template<typename R,
//         typename W>
//inline
//INode::INode(User owner, Solace::uint64 path, FilePermissions perms, R&& r, W&& w)
//    : _meta{initMeta(INode::TypeTag<INode::Type::Synthetic>{}, owner, path, perms)}
//    , _nodeData{std::in_place_type<INode::Synth>, std::forward<R>(r), std::forward<W>(w)}
//{ }

//template<typename F>
//inline
//INode::INode(User owner, Solace::uint64 path, FilePermissions perms, F&& f)
//    : _meta{initMeta(INode::TypeTag<INode::Type::SyntheticDir>{}, owner, path, perms)}
//    , _nodeData{std::in_place_type<INode::SynthDir>, std::forward<F>(f)}
//{ }

}  // namespace kasofs
#endif  // KASOFS_VINODE_HPP
