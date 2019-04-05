/*
*  Copyright (C) Ivan Ryabov - All Rights Reserved
*
*  Unauthorized copying of this file, via any medium is strictly prohibited.
*  Proprietary and confidential.
*
*  Written by Ivan Ryabov <abbyssoul@gmail.com>
*/
/*******************************************************************************
 * KasoFS Unit Test Suit
 *	@file test/test_vinode.cpp
 *	@brief		Test suit for KasoFS::INode
 ******************************************************************************/
#include "vinode.hpp"    // Class being tested.

#include <gtest/gtest.h>

using namespace kasofs;
using namespace Solace;


TEST(TestVfsINode, testContructor_Dir) {
    auto node = INode{INode::TypeTag<INode::Type::Directory>{}, User{0,0}, 0, FilePermissions{0666}};

    EXPECT_EQ(INode::Type::Directory, node.type());
}

TEST(TestVfsINode, testContructor_Data) {
    auto node = INode{INode::TypeTag<INode::Type::Data>{}, User{0,0}, 0, FilePermissions{0666}};

    EXPECT_EQ(INode::Type::Data, node.type());
}


TEST(TestVfsINode, testDirectoryMeta) {
    auto node = INode{INode::TypeTag<INode::Type::Directory>{}, User{0,0}, 0, FilePermissions{0764}};

    EXPECT_EQ(0x80000000 | 0764, node.meta().mode.mode);
    EXPECT_EQ(4096, node.length());
}

TEST(TestVfsINode, testFileMeta) {
    auto node = INode{INode::TypeTag<INode::Type::Data>{}, User{0,0}, 0, FilePermissions{0666}};

    EXPECT_EQ(0666, node.meta().mode.mode);
    EXPECT_EQ(0, node.length());

    char msg[] = "hello";
    node.writer(node.meta().owner).then([msg](auto&& writer) { writer.write(wrapMemory(msg)); });
    EXPECT_EQ(5, node.length());
}


TEST(TestVfsINode, testSynthAccess) {
    bool isRead = false;
    bool isWritten = false;
    auto node = INode{User{0,0}, 0, FilePermissions{0666},
            [&isRead]()     { isRead = true; return Solace::ByteReader{}; },
            [&isWritten]()  { isWritten = true; return Solace::ByteWriter{}; }};

    auto const owner = node.meta().owner;
    node.reader(owner).then([](auto&& reader) { byte c; reader.read(&c); });
    EXPECT_TRUE(isRead);

    node.writer(owner).then([](auto&& w) { w.write(100); });
    EXPECT_TRUE(isWritten);
}


TEST(TestVfsINode, testDirectoryAddEntries) {
    auto node = INode{INode::TypeTag<INode::Type::Directory>{}, User{0,0}, 0, FilePermissions{0666}};

    EXPECT_EQ(0, node.entriesCount());
    EXPECT_TRUE(node.findEntry("some").isNone());
    EXPECT_TRUE(node.findEntry("knowhere").isNone());

    node.addDirEntry("knowhere", 32);
    node.addDirEntry("other", 776);
    EXPECT_EQ(2, node.entriesCount());
    EXPECT_TRUE(node.findEntry("knowhere").isSome());
}

TEST(TestVfsINode, testSynthDirListing) {
    bool isListed = false;

    auto node = INode{User{0,0}, 0, FilePermissions{0666},
            [&isListed]()  {
                isListed = true;
                static INode::Entry entries[] = {
                    {"one", 3},
                    {"two", 6},
                    {"none", 9},
                };

                return INode::EntriesIterator{entries, entries + 3};
            }};

    INode::index_type index = 3;
    for (auto const& e : node.entries()) {
        EXPECT_EQ(index, e.index);
        index += 3;
    }

    ASSERT_TRUE(isListed);
}
