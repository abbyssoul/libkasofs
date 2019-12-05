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
#include "kasofs/vfs.hpp"    // Class being tested.

#include <gtest/gtest.h>
#include <solace/output_utils.hpp>


using namespace kasofs;
using namespace Solace;

struct MockFs: public Filesystem {
	static uint32 dataType() noexcept {
		return 312;
	}

	MockFs(const char* str)
		: buffer{str}
	{}

	Result<INode::VfsData, Error> createNode(NodeType /*type*/) override {
		return Ok<INode::VfsData>(0);
	}

	FilePermissions defaultFilePermissions(NodeType) const noexcept override {
		return FilePermissions{0777};
	}

	auto open(INode, Permissions) -> Result<OpenFID, Error> override {
		return Ok<OpenFID>(0);
	}

	Result<size_type, Error> read(INode, size_type offset, MutableMemoryView dest) override {
		if (offset >= buffer.size())
			return makeError(BasicError::Overflow, "MockFs::read");

		auto data = wrapMemory(buffer.data() + offset, buffer.size() - offset).slice(0, dest.size());
		auto isOk = dest.write(data);
		if (!isOk) {
			return isOk.moveError();
		}

		return Ok(data.size());
	}

	Result<size_type, Error> write(INode, size_type offset, MemoryView src) override {
		buffer.reserve(offset + src.size());
		buffer.assign(src.dataAs<char>(), src.size());

		return Ok(src.size());
	}

	Result<void, Error> close(OpenFID, INode) override {
		return Ok();
	}

private:
	std::string buffer;
};


struct SynthFs: public MockFs {

	static uint32 dataType() noexcept {
		return 91228;
	}

	SynthFs(const char* str)
		: MockFs{str}
	{}


	FilePermissions defaultFilePermissions(NodeType) const noexcept {
		return FilePermissions{0777};
	}

};


// FIXME: Test adding and removing linked direcories does not screews dir listing


TEST(TestVfs, testContructor) {
    auto vfs = Vfs{User{0,0}, FilePermissions{0666}};

    auto root = vfs.nodeById(vfs.rootId());
    ASSERT_TRUE(root.isSome());
	EXPECT_EQ(0, (*root).fsTypeId);
}

/*
TEST(TestVfs, testMountingRO_FS) {
    auto user = User{0,0};
    auto vfs = Vfs{user, FilePermissions{0666}};

//            .lookup = [](INode const& node, StringView name) -> Optional<Entry> {
//                if (dirId == 0 && name == "file")
//                    return Optional<Entry>{in_place, "file", VNodeId{dirId.fsId, 1}};
//                return none;
//            },

//            .listEntries = [](INode const& node) -> EntriesIterator {
//                static Entry rootEntries[] = {
//                    {"file", {0,0}},
//                    {"other", {0,1}},
//                    {"more", {0,2}}
//                };

//                static Entry dirEntries[] = {
//                    {"0", {0, 3}},
//                    {"1", {0, 4}},
//                    {"2", {0, 1}}
//                };

//                if (id == 0)
//                    return {std::begin(rootEntries), std::end(rootEntries)};

//                if (id == 1)
//                    return {std::begin(dirEntries), std::end(dirEntries)};

//                return {nullptr, nullptr};
//            }
//    };

	auto rego = vfs.registerFileSystem<SynthFs>("Synch");
    ASSERT_TRUE(rego);
    ASSERT_TRUE(vfs.mount(user, vfs.rootId(), *rego));

    uint32 count = 0;
    auto maybeEnum = vfs.enumerateDirectory(vfs.rootId(), user);
    ASSERT_TRUE(maybeEnum);

    static Entry expectedEntries[] = {
        {"file", 0},
        {"other", 1},
        {"more", 2}
    };

    for (auto const& e : *maybeEnum) {
        EXPECT_EQ(expectedEntries[count].nodeId, e.nodeId);
        EXPECT_EQ(expectedEntries[count].name, e.name);
        count += 1;
    }

    EXPECT_EQ(0, count);
}
*/

TEST(TestVfs, testLinkingNonExisingNodes) {
    auto user = User{0,0};
    auto vfs = Vfs{user, FilePermissions{0666}};

    // Self Linking: exising node to itself
    EXPECT_TRUE(vfs.link(user, "idx", 0, 0).isError());

    // Self Linking: non exising node to itself
    EXPECT_TRUE(vfs.link(user, "id", 747, 747).isError());

    // Linking non-existing node to a different non-existing one
    EXPECT_TRUE(vfs.link(user, "id", 87, 12).isError());

    // Linking existing node to to a non-exingsting target
    EXPECT_TRUE(vfs.link(user, "id", 0, 17).isError());

    // Linking non-existing node to an existing one
    EXPECT_TRUE(vfs.link(user, "id", 21, 0).isError());
}


TEST(TestVfs, testLinkingNodes) {
    auto owner = User{0, 0};
    auto vfs = Vfs{owner, FilePermissions{0666}};
	auto fsId = vfs.registerFileSystem<MockFs>("hello");
	ASSERT_TRUE(fsId);

	auto maybeId = vfs.mknode(vfs.rootId(), "id", *fsId, MockFs::dataType(), owner, FilePermissions{0666});
	if (!maybeId) {
		FAIL() << maybeId.getError();
	}
	ASSERT_TRUE(maybeId.isOk());
//    EXPECT_EQ(2, vfs.size());

    EXPECT_TRUE(vfs.link(owner, "id2", vfs.rootId(), *maybeId).isOk());
    EXPECT_TRUE(vfs.link(owner, "id-other", vfs.rootId(), *maybeId).isOk());
//    EXPECT_EQ(2, vfs.size());

    auto enumerator = vfs.enumerateDirectory(vfs.rootId(), owner);
    ASSERT_TRUE(enumerator.isOk());

    uint32 count = 0;
    for (auto const& e : *enumerator) {
        EXPECT_EQ(*maybeId, e.nodeId);
        EXPECT_EQ("id", e.name.substring(0, 2));
        count += 1;
    }
    EXPECT_EQ(3, count);
}


TEST(TestVfs, testMakingNodes) {
    auto owner = User{0, 0};
    auto vfs = Vfs{owner, FilePermissions{0666}};
	auto fsId = vfs.registerFileSystem<MockFs>("hello");
	ASSERT_TRUE(fsId);

    // Make a regular data node
//    EXPECT_EQ(1, vfs.size());
	auto maybeId = vfs.mknode(vfs.rootId(), "id", *fsId, MockFs::dataType(), owner, FilePermissions{0777});
	EXPECT_TRUE(maybeId.isOk());
//    EXPECT_EQ(2, vfs.size());
//    EXPECT_EQ(1, *vfs.countEntries(User{0,0}, vfs.rootId()));

    // Make a regular data node
//    auto maybeGen = vfs.mknode(INode::Type::Data, owner, FilePermissions{0777}, vfs.rootId(), "generation");
//    EXPECT_TRUE(maybeGen.isOk());
//    EXPECT_EQ(3, vfs.size());
//    EXPECT_EQ(2, *vfs.countEntries(User{0,0}, vfs.rootId()));

    // Make a regular Directory node
//    auto maybeDir = vfs.mknode(INode::Type::Directory, owner, FilePermissions{0777}, vfs.rootId(), "dir");
//    EXPECT_TRUE(maybeDir.isOk());
//    EXPECT_EQ(4, vfs.size());
//    EXPECT_EQ(3, *vfs.countEntries(User{0,0}, vfs.rootId()));

    // Make a regular Data node in a directory
//    auto maybeDataInDir = vfs.mknode(INode::Type::Data, owner, FilePermissions{0777}, *maybeDir, "data");
//    EXPECT_TRUE(maybeDataInDir.isOk());
//    EXPECT_EQ(5, vfs.size());
//    EXPECT_EQ(3, *vfs.countEntries(User{0,0}, vfs.rootId()));

    // Make a Synth node in a directory
//    auto maybeSynthInDir = vfs.mknode(User{0,0}, *maybeDir, "synth", FilePermissions{0777},
//                                        []() { return ByteReader{}; },
//                                        []() { return ByteWriter{}; });
//    EXPECT_TRUE(maybeSynthInDir.isOk());
//    EXPECT_EQ(6, vfs.size());


//    EXPECT_TRUE(vfs.findEntry(vfs.rootId(), "id").isSome());
//    EXPECT_TRUE(vfs.findEntry(vfs.rootId(), "generation").isSome());
//    EXPECT_TRUE(vfs.findEntry(vfs.rootId(), "dir").isSome());

//    EXPECT_TRUE(vfs.findEntry(vfs.rootId(), "data").isNone());
//    EXPECT_TRUE(vfs.findEntry(vfs.rootId(), "synth").isNone());

//    auto dirNode = vfs.nodeById(*maybeDir);
//    ASSERT_TRUE(dirNode.isSome());
//    EXPECT_TRUE(vfs.findEntry(*maybeDir, "data").isSome());
//    EXPECT_TRUE(vfs.findEntry(*maybeDir, "synth").isSome());
}


TEST(TestVfs, testMakingNodeInNonExistingDirFails) {
    auto user = User{0,0};
    auto vfs = Vfs{user, FilePermissions{0666}};
	auto fsId = vfs.registerFileSystem<MockFs>("hello");
	ASSERT_TRUE(fsId);

//    EXPECT_EQ(1, vfs.size());
	auto maybeId = vfs.mknode(321, "id", *fsId, MockFs::dataType(), User{1, 0}, FilePermissions{0777});
    EXPECT_TRUE(maybeId.isError());
//    EXPECT_EQ(1, vfs.size());
}


TEST(TestVfs, testMakingNodeWithoutDirectoryWritePermissionFails) {
    auto owner = User{0, 0};
    auto vfs = Vfs{owner, FilePermissions{0600}};
	auto fsId = vfs.registerFileSystem<MockFs>("hello");
	ASSERT_TRUE(fsId);

	auto maybeId = vfs.mknode(vfs.rootId(), "id", *fsId, MockFs::dataType(), User{1, 0}, FilePermissions{0777});
    EXPECT_TRUE(maybeId.isError());
//    EXPECT_EQ(1, vfs.size());
}

/*
TEST(TestVfs, testDataNodesRelocatable) {
    auto owner = User{0,0};
    auto vfs = Vfs{owner, FilePermissions{0666}};
//    ASSERT_EQ(1, vfs.size());

    auto maybeId = vfs.mknode(INode::Type::Data, owner, FilePermissions{0777}, vfs.rootId(), "data0");
    ASSERT_TRUE(maybeId.isOk());
//    ASSERT_EQ(2, vfs.size());

    {
        auto maybeNode = vfs.nodeById(*maybeId);
        ASSERT_TRUE(maybeNode);

        auto* node = *maybeNode;
        EXPECT_EQ(0, node->length());
        char msg[] = "hello";
        node->writer(owner).then([msg](auto&& writer) { writer.write(wrapMemory(msg).slice(0, strlen(msg))); });

        EXPECT_EQ(strlen(msg), node->length());
    }

    auto maybeId2 = vfs.mknode(INode::Type::Data, owner, FilePermissions{0777}, vfs.rootId(), "data1");
    ASSERT_TRUE(maybeId2.isOk());
//    ASSERT_EQ(3, vfs.size());

    {
        auto maybeNode = vfs.nodeById(*maybeId2);
        ASSERT_TRUE(maybeNode);

        auto* node = *maybeNode;
        EXPECT_EQ(0, node->length());
        char msg[] = "some other message";
        node->writer(owner).then([msg](auto&& writer) { writer.write(wrapMemory(msg)); });
        EXPECT_EQ(strlen(msg), node->length());
    }

    auto maybeNode0 = vfs.nodeById(*maybeId);
    ASSERT_TRUE(maybeNode0);

    auto* node0 = *maybeNode0;
    EXPECT_EQ(5, node0->length());

    char buffer[512];
    auto bufferView = wrapMemory(buffer);
    node0->reader(owner).then([bufferView, node0](auto&& reader) { reader.read(bufferView, node0->length()); });

    buffer[node0->length()] = 0;
    EXPECT_STREQ("hello", buffer);

    strcpy(buffer, "Completely different");
    node0->writer(owner).then([buffer](auto&& writer) { writer.write(wrapMemory(buffer)); });

    EXPECT_EQ(strlen(buffer), node0->length());

    char msg2[] = "some other message";
    auto maybeNode2 = vfs.nodeById(*maybeId2);
    ASSERT_TRUE(maybeNode2);
    auto* node2 = *maybeNode2;
//    node2->writer().write(wrapMemory(msg2));
    EXPECT_EQ(strlen(msg2), node2->length());
}
*/

TEST(TestVfs, testWalk) {
    auto owner = User{0, 0};
    auto vfs = Vfs{owner, FilePermissions{0600}};
	auto fsId = vfs.registerFileSystem<MockFs>("hello");
	ASSERT_TRUE(fsId);


	auto maybeDirId0 = vfs.createDirectory(vfs.rootId(), "dir0", owner, FilePermissions{0777});
	auto maybeDirId1 = vfs.createDirectory(vfs.rootId(), "dir1", owner, FilePermissions{0700});
    ASSERT_TRUE(maybeDirId0.isOk());
    ASSERT_TRUE(maybeDirId1.isOk());

	auto maybeDir0Id1 =  vfs.createDirectory(*maybeDirId0, "dir0", owner, FilePermissions{0777});

	auto maybeNodeId = vfs.mknode(vfs.rootId(), "data", *fsId, MockFs::dataType(), owner, FilePermissions{0777});

	auto maybeData0Id0 = vfs.mknode(*maybeDirId0, "data0", *fsId, MockFs::dataType(), owner, FilePermissions{0777});
	auto maybeData0Id1 = vfs.mknode(*maybeDirId0, "data1", *fsId, MockFs::dataType(), owner, FilePermissions{0777});
	auto maybeData1Id0 = vfs.mknode(*maybeDirId1, "data0", *fsId, MockFs::dataType(), owner, FilePermissions{0777});
	auto maybeData1Id1 = vfs.mknode(*maybeDirId1, "data1", *fsId, MockFs::dataType(), owner, FilePermissions{0777});

    ASSERT_TRUE(maybeDir0Id1.isOk());
    ASSERT_TRUE(maybeData0Id0.isOk());
    ASSERT_TRUE(maybeData0Id1.isOk());
    ASSERT_TRUE(maybeData1Id0.isOk());
    ASSERT_TRUE(maybeData1Id1.isOk());
//    ASSERT_EQ(8, vfs.size());

    int count = 0;
	EXPECT_TRUE(vfs.walk(owner, vfs.rootId(), *makePath("dir0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(1, count);

    count = 0;
	EXPECT_TRUE(vfs.walk(owner, vfs.rootId(), *makePath("dir0", "data0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(2, count);

    count = 0;
	EXPECT_TRUE(vfs.walk(owner, vfs.rootId(), *makePath("dir0", "dir0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(2, count);

    count = 0;
	EXPECT_TRUE(vfs.walk(owner, vfs.rootId(), *makePath("dir1", "data0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(2, count);

    count = -3;
	EXPECT_FALSE(vfs.walk(User{32,0}, vfs.rootId(), *makePath("dir1", "data0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(-3, count);
}



//TEST(TestVfsINode, testDirectoryAddEntries) {
//    auto node = INode{INode::TypeTag<INode::Type::Directory>{}, User{0,0}, 0, FilePermissions{0666}};

//    EXPECT_EQ(0, node.entriesCount());
//    EXPECT_TRUE(node.findEntry("some").isNone());
//    EXPECT_TRUE(node.findEntry("knowhere").isNone());

//    node.addDirEntry("knowhere", 32);
//    node.addDirEntry("other", 776);
//    EXPECT_EQ(2, node.entriesCount());
//    EXPECT_TRUE(node.findEntry("knowhere").isSome());
//}


//TEST(TestVfsINode, testSynthDirListing) {
//    bool isListed = false;

//    auto node = INode{User{0,0}, 0, FilePermissions{0666},
//            [&isListed]()  {
//                isListed = true;
//                static INode::Entry entries[] = {
//                    {"one", 3},
//                    {"two", 6},
//                    {"none", 9},
//                };

//                return INode::EntriesIterator{entries, entries + 3};
//            }};

//    INode::index_type index = 3;
//    for (auto const& e : node.entries()) {
//        EXPECT_EQ(index, e.index);
//        index += 3;
//    }

//    ASSERT_TRUE(isListed);
//}


TEST(TestVfs, testPremissionsInheritence) {
	auto owner = User{0, 0};
	auto vfs = Vfs{owner, FilePermissions{0600}};
	auto fsId = vfs.registerFileSystem<MockFs>("hello");
	ASSERT_TRUE(fsId);

	auto maybeNodeId = vfs.mknode(vfs.rootId(), "data", *fsId, MockFs::dataType(), owner, FilePermissions{0777});
	auto maybeNode = vfs.nodeById(maybeNodeId);
	ASSERT_TRUE(maybeNode);

	auto const& node = *maybeNode;
	EXPECT_EQ(0600, node.permissions);
	EXPECT_EQ(0, node.dataSize);
}


TEST(TestVfs, testFileWriteUpdatesSize) {
	auto user = User{0, 0};
	auto vfs = Vfs{user, FilePermissions{0600}};
	auto fsId = vfs.registerFileSystem<MockFs>("hello");
	ASSERT_TRUE(fsId);

	auto maybeNodeId = vfs.mknode(vfs.rootId(), "str1", *fsId, MockFs::dataType(), user);
    ASSERT_TRUE(maybeNodeId);

    {
		auto maybeNode = vfs.nodeById(maybeNodeId);
        ASSERT_TRUE(maybeNode);

		auto const& node = *maybeNode;
		EXPECT_EQ(0, node.dataSize);
		EXPECT_EQ(0600, node.permissions);
    }

    char msg[] = "hello";

	auto maybeFile = vfs.open(user, *maybeNodeId, Permissions::READ);
	ASSERT_TRUE(maybeFile);
	ASSERT_TRUE((*maybeFile).write(wrapMemory(msg)));

    {
        auto maybeNode = vfs.nodeById(*maybeNodeId);
        ASSERT_TRUE(maybeNode);
		auto& node = *maybeNode;
		EXPECT_EQ(5, node.dataSize);
    }
}



TEST(TestVfs, stringFs) {
	auto user = User{0,0};
	auto vfs = Vfs{user, FilePermissions{0666}};

	auto fsId = vfs.registerFileSystem<MockFs>("hello");
	ASSERT_TRUE(fsId);

	auto maybeNodeId = vfs.mknode(vfs.rootId(), "str1", *fsId, MockFs::dataType(), user);
	ASSERT_TRUE(maybeNodeId);

	char destBuffer[32];
	auto dest = wrapMemory(destBuffer);
	dest.fill(0);

	auto maybeFile = vfs.open(user, *maybeNodeId, Permissions::READ);
	ASSERT_TRUE(maybeFile);

	ASSERT_TRUE((*maybeFile).read(dest));

	destBuffer[31] = 0;
	EXPECT_STREQ("hello", destBuffer);
}

