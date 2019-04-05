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
 *	@file test/test_vfs.cpp
 *	@brief		Test suit for KasoFS::Vfs
 ******************************************************************************/
#include "vfs.hpp"    // Class being tested.

#include <gtest/gtest.h>


using namespace kasofs;
using namespace Solace;


TEST(TestVfs, testContructor) {
    auto vfs = Vfs{User{0,0}, FilePermissions{0666}};

    EXPECT_EQ(1, vfs.size());
    EXPECT_EQ(INode::Type::Directory, vfs.root().type());
}


TEST(TestVfs, testLinkingNodes) {
    auto vfs = Vfs{User{0,0}, FilePermissions{0666}};

    auto maybeId = vfs.mknode(User{0,0}, 0, "id", FilePermissions{0666}, INode::Type::Data);
    EXPECT_TRUE(maybeId.isOk());

    EXPECT_TRUE(vfs.link(User{0,0}, "id2", 0, *maybeId).isOk());
    EXPECT_EQ(2, vfs.size());
    EXPECT_EQ(2, vfs.root().entriesCount());

}

TEST(TestVfs, testLinkingNonExisingNodes) {
    auto user = User{0,0};
    auto vfs = Vfs{user, FilePermissions{0666}};

    auto maybeId = vfs.mknode(User{0,0}, 0, "id", FilePermissions{0666}, INode::Type::Data);
    EXPECT_TRUE(maybeId.isOk());

    EXPECT_TRUE(vfs.link(user, "id", 87, 12).isError());

    // Self Linking non exising nodes
    EXPECT_TRUE(vfs.link(user, "id", 747, 747).isError());
    // Self Linking exising nodes
    EXPECT_TRUE(vfs.link(user, "idx", 0, 0).isError());

    // Linking to non-exingsting target
    EXPECT_TRUE(vfs.link(user, "id", 0, 17).isError());

    // Linking from non-exingsting root
    EXPECT_TRUE(vfs.link(user, "id", 881, 0).isError());
}


TEST(TestVfs, testMakingNodes) {
    auto vfs = Vfs{User{0,0}, FilePermissions{0666}};

    // Make a regular data node
    EXPECT_EQ(1, vfs.size());
    auto maybeId = vfs.mknode(User{0,0}, 0, "id", FilePermissions{0777}, INode::Type::Data);
    EXPECT_TRUE(maybeId.isOk());
    EXPECT_EQ(2, vfs.size());
    EXPECT_EQ(1, vfs.root().entriesCount());

    // Make a regular data node
    auto maybeGen = vfs.mknode(User{0,0}, 0, "generation", FilePermissions{0777}, INode::Type::Data);
    EXPECT_TRUE(maybeGen.isOk());
    EXPECT_EQ(3, vfs.size());
    EXPECT_EQ(2, vfs.root().entriesCount());

    // Make a regular Directory node
    auto maybeDir = vfs.mknode(User{0,0}, 0, "dir", FilePermissions{0777}, INode::Type::Directory);
    EXPECT_TRUE(maybeDir.isOk());
    EXPECT_EQ(4, vfs.size());
    EXPECT_EQ(3, vfs.root().entriesCount());

    // Make a regular Data node in a directory
    auto maybeDataInDir = vfs.mknode(User{0,0}, *maybeDir, "data", FilePermissions{0777}, INode::Type::Data);
    EXPECT_TRUE(maybeDataInDir.isOk());
    EXPECT_EQ(5, vfs.size());
    EXPECT_EQ(3, vfs.root().entriesCount());

    // Make a Synth node in a directory
    auto maybeSynthInDir = vfs.mknode(User{0,0}, *maybeDir, "synth", FilePermissions{0777},
                                        []() { return Solace::ByteReader{}; },
                                        []() { return Solace::ByteWriter{}; });
    EXPECT_TRUE(maybeSynthInDir.isOk());
    EXPECT_EQ(6, vfs.size());


    EXPECT_TRUE(vfs.root().findEntry("id").isSome());
    EXPECT_TRUE(vfs.root().findEntry("generation").isSome());
    EXPECT_TRUE(vfs.root().findEntry("dir").isSome());

    EXPECT_TRUE(vfs.root().findEntry("data").isNone());
    EXPECT_TRUE(vfs.root().findEntry("synth").isNone());

    auto dirNode = vfs.nodeById(*maybeDir);
    ASSERT_TRUE(dirNode.isSome());
    EXPECT_TRUE((*dirNode)->findEntry("data").isSome());
    EXPECT_TRUE((*dirNode)->findEntry("synth").isSome());
}


TEST(TestVfs, testMakingNodeInNonExistingDirFails) {
    auto user = User{0,0};
    auto vfs = Vfs{user, FilePermissions{0666}};

    EXPECT_EQ(1, vfs.size());
    auto maybeId = vfs.mknode(user, 321, "id", FilePermissions{0777}, INode::Type::Data);
    EXPECT_TRUE(maybeId.isError());
    EXPECT_EQ(1, vfs.size());
}


TEST(TestVfs, testMakingNodeWithoutDirectoryWritePermissionFails) {
    auto owner = User{0, 0};
    auto vfs = Vfs{owner, FilePermissions{0600}};

    EXPECT_EQ(1, vfs.size());
    auto maybeId = vfs.mknode(User{1, 0}, 0, "id", FilePermissions{0777}, INode::Type::Data);
    EXPECT_TRUE(maybeId.isError());
    EXPECT_EQ(1, vfs.size());
}


TEST(TestVfs, testDataNodesRelocatable) {
    auto vfs = Vfs{User{0,0}, FilePermissions{0666}};
    ASSERT_EQ(1, vfs.size());

    auto maybeId = vfs.mknode(User{0,0}, 0, "data0", FilePermissions{0777}, INode::Type::Data);
    ASSERT_TRUE(maybeId.isOk());
    ASSERT_EQ(2, vfs.size());

    {
        auto maybeNode = vfs.nodeById(*maybeId);
        ASSERT_TRUE(maybeNode);

        auto* node = *maybeNode;
        EXPECT_EQ(0, node->length());
        char msg[] = "hello";
        node->writer(node->meta().owner).then([msg](auto&& writer) { writer.write(wrapMemory(msg).slice(0, strlen(msg))); });

        EXPECT_EQ(strlen(msg), node->length());
    }

    auto maybeId2 = vfs.mknode(User{0,0}, 0, "data1", FilePermissions{0777}, INode::Type::Data);
    ASSERT_TRUE(maybeId2.isOk());
    ASSERT_EQ(3, vfs.size());

    {
        auto maybeNode = vfs.nodeById(*maybeId2);
        ASSERT_TRUE(maybeNode);

        auto* node = *maybeNode;
        EXPECT_EQ(0, node->length());
        char msg[] = "some other message";
        node->writer(node->meta().owner).then([msg](auto&& writer) { writer.write(wrapMemory(msg)); });
        EXPECT_EQ(strlen(msg), node->length());
    }

    auto maybeNode0 = vfs.nodeById(*maybeId);
    ASSERT_TRUE(maybeNode0);

    auto* node0 = *maybeNode0;
    EXPECT_EQ(5, node0->length());

    char buffer[512];
    auto bufferView = wrapMemory(buffer);
    node0->reader(node0->meta().owner).then([bufferView, node0](auto&& reader) { reader.read(bufferView, node0->length()); });

    buffer[node0->length()] = 0;
    EXPECT_STREQ("hello", buffer);

    strcpy(buffer, "Completely different");
    node0->writer(node0->meta().owner).then([buffer](auto&& writer) { writer.write(wrapMemory(buffer)); });

    EXPECT_EQ(strlen(buffer), node0->length());

    char msg2[] = "some other message";
    auto maybeNode2 = vfs.nodeById(*maybeId2);
    ASSERT_TRUE(maybeNode2);
    auto* node2 = *maybeNode2;
//    node2->writer().write(wrapMemory(msg2));
    EXPECT_EQ(strlen(msg2), node2->length());
}


TEST(TestVfs, testWalk) {
    auto owner = User{0, 0};
    auto vfs = Vfs{owner, FilePermissions{0600}};
    auto maybeDirId0 = vfs.mknode(owner, 0, "dir0", FilePermissions{0777}, INode::Type::Directory);
    auto maybeDirId1 = vfs.mknode(owner, 0, "dir1", FilePermissions{0700}, INode::Type::Directory);
    ASSERT_TRUE(maybeDirId0.isOk());
    ASSERT_TRUE(maybeDirId1.isOk());

    auto maybeDir0Id1 = vfs.mknode(owner, *maybeDirId0, "dir0", FilePermissions{0777}, INode::Type::Directory);
    auto maybeData0Id0 = vfs.mknode(owner, *maybeDirId0, "data0", FilePermissions{0777}, INode::Type::Data);
    auto maybeData0Id1 = vfs.mknode(owner, *maybeDirId0, "data1", FilePermissions{0777}, INode::Type::Data);
    auto maybeData1Id0 = vfs.mknode(owner, *maybeDirId1, "data0", FilePermissions{0777}, INode::Type::Data);
    auto maybeData1Id1 = vfs.mknode(owner, *maybeDirId1, "data1", FilePermissions{0777}, INode::Type::Data);

    ASSERT_TRUE(maybeDir0Id1.isOk());
    ASSERT_TRUE(maybeData0Id0.isOk());
    ASSERT_TRUE(maybeData0Id1.isOk());
    ASSERT_TRUE(maybeData1Id0.isOk());
    ASSERT_TRUE(maybeData1Id1.isOk());
    ASSERT_EQ(8, vfs.size());

    int count = 0;
    EXPECT_TRUE(vfs.walk(owner, 0, makePath("dir0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(1, count);

    count = 0;
    EXPECT_TRUE(vfs.walk(owner, 0, makePath("dir0", "data0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(2, count);

    count = 0;
    EXPECT_TRUE(vfs.walk(owner, 0, makePath("dir0", "dir0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(2, count);

    count = 0;
    EXPECT_TRUE(vfs.walk(owner, 0, makePath("dir1", "data0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(2, count);

    count = -3;
    EXPECT_FALSE(vfs.walk(User{32,0}, 0, makePath("dir1", "data0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(-3, count);
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
