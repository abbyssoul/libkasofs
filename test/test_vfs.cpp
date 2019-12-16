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

	Result<INode, Error> createNode(NodeType type, User owner, FilePermissions perms) override {
		INode node{type, owner, perms};
		node.dataSize = buffer.size();

		return Ok(std::move(node));
	}

	FilePermissions defaultFilePermissions(NodeType) const noexcept override {
		return FilePermissions{0777};
	}

	auto open(INode&, Permissions) -> Result<OpenFID, Error> override {
		return Ok<OpenFID>(0);
	}

	Result<size_type, Error> read(INode&, size_type offset, MutableMemoryView dest) override {
		if (offset >= buffer.size())
			return makeError(BasicError::Overflow, "MockFs::read");

		auto data = wrapMemory(buffer.data() + offset, buffer.size() - offset).slice(0, dest.size());
		auto isOk = dest.write(data);
		if (!isOk) {
			return isOk.moveError();
		}

		return Ok(data.size());
	}

	Result<size_type, Error> write(INode& node, size_type offset, MemoryView src) override {
		buffer.reserve(offset + src.size());
		buffer.assign(src.dataAs<char>(), src.size());
		node.dataSize = buffer.length();

		return Ok(src.size());
	}

	Result<void, Error> close(INode&, OpenFID) override {
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



TEST(TestVfs, testContructor) {
	auto vfs = Vfs{User{0, 0}, FilePermissions{0666}};

	auto root = vfs.nodeById(vfs.rootId());
	ASSERT_TRUE(root.isSome());
	EXPECT_EQ(0, (*root).fsTypeId);
	EXPECT_EQ(1, vfs.size());
}


struct MockFsTest : public ::testing::Test {

	void SetUp() override {
		auto maybeFsId = vfs.registerFilesystem<MockFs>("hello");
		ASSERT_TRUE(maybeFsId.isOk());
		fsId = *maybeFsId;
	}

protected:
	User	owner{0, 0};
	Vfs		vfs{owner, FilePermissions{0600}};
	VfsId	fsId{0};
};

// FIXME: Test adding and removing linked direcories does not screews dir listing


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


TEST_F(MockFsTest, linkingNodesToDirecotryIsOk) {
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId.isOk());

	EXPECT_TRUE(vfs.link(owner, "id2", vfs.rootId(), *maybeId).isOk());
	EXPECT_TRUE(vfs.link(owner, "id-other", vfs.rootId(), *maybeId).isOk());

	// There are only 2 nodes in vfs but 3 entries in the root directory pointing to the same node
	EXPECT_EQ(2, vfs.size());

	auto enumerator = vfs.enumerateDirectory(vfs.rootId(), owner);
	ASSERT_TRUE(enumerator.isOk());

	uint32 count = 0;
	for (auto const& e : *enumerator) {
		EXPECT_EQ(*maybeId, e.nodeId);
		EXPECT_EQ("id", e.name.substring(0, 2));
		count += 1;
	}
	EXPECT_EQ(3, count);

	auto maybeNode = vfs.nodeById(*maybeId);
	ASSERT_TRUE(maybeNode.isSome());

	// Make sure there are 3 in-bound links to this node
	EXPECT_EQ(3, (*maybeNode).nLinks);
}


TEST_F(MockFsTest, linkingNodesToNodesIsNotOk) {
	auto maybeId1 = vfs.mknode(vfs.rootId(), "node-1", fsId, MockFs::dataType(), owner);
	auto maybeId2 = vfs.mknode(vfs.rootId(), "node-2", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId1.isOk());
	ASSERT_TRUE(maybeId2.isOk());
	ASSERT_EQ(3, vfs.size());

	EXPECT_TRUE(vfs.link(owner, "something-else", *maybeId1, *maybeId2).isError());
	EXPECT_TRUE(vfs.link(owner, "id-other", *maybeId1, *maybeId2).isError());

	// Linking to self is never a good idea
	EXPECT_TRUE(vfs.link(owner, "id-other", *maybeId1, *maybeId1).isError());
}


TEST_F(MockFsTest, linkingToSelfIsNotOk) {
	EXPECT_TRUE(vfs.link(owner, "something-else", vfs.rootId(), vfs.rootId()).isError());
}


TEST(TestVfs, linkingNonExisingNodesFails) {
	auto user = User{0, 0};
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


TEST_F(MockFsTest, unlinkingNodeRemovesNodeWithNoRefTo) {
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId.isOk());

	EXPECT_EQ(2, vfs.size());
	EXPECT_TRUE(vfs.unlink(owner, "id", vfs.rootId()).isOk());
	EXPECT_EQ(1, vfs.size());

	auto enumerator = vfs.enumerateDirectory(vfs.rootId(), owner);
	ASSERT_TRUE(enumerator.isOk());

	uint32 count = 0;
	for (auto const& e : *enumerator) {
		EXPECT_EQ(*maybeId, e.nodeId);
		EXPECT_EQ("id", e.name.substring(0, 2));
		count += 1;
	}
	EXPECT_EQ(0, count);
}


TEST_F(MockFsTest, unlinkingNonExistingNameIsNoop) {
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId.isOk());

	EXPECT_TRUE(vfs.unlink(owner, "id-some", vfs.rootId()).isOk());

	auto enumerator = vfs.enumerateDirectory(vfs.rootId(), owner);
	ASSERT_TRUE(enumerator.isOk());
	uint32 count = 0;
	for (auto const& e : *enumerator) {
		EXPECT_EQ(*maybeId, e.nodeId);
		EXPECT_EQ("id", e.name.substring(0, 2));
		count += 1;
	}
	EXPECT_EQ(1, count);
}


TEST_F(MockFsTest, unlinkingFromNonExistingNodeFails) {
	EXPECT_TRUE(vfs.unlink(owner, "id-some", 61253).isError());
}

// TODO(abbyssoul): Unlink directory while enumeratitg it

TEST_F(MockFsTest, unlinkingOpenFileRemovesNodeFromIndex) {
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId.isOk());

	auto maybeOpenedFile = vfs.open(owner, *maybeId, Permissions::WRITE);
	ASSERT_TRUE(maybeOpenedFile.isOk());
	EXPECT_TRUE(vfs.unlink(owner, "id", vfs.rootId()).isOk());

	char msg[] = "other-message";
	ASSERT_TRUE((*maybeOpenedFile).write(wrapMemory(msg)));
	EXPECT_EQ(sizeof(msg), (*maybeOpenedFile).size());

	EXPECT_TRUE(vfs.nodeById(*maybeId).isNone());
}


TEST_F(MockFsTest, makingMockNodes) {
    // Make a regular data node
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
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


TEST_F(MockFsTest, makingNodeInNonExistingDirFails) {
	EXPECT_TRUE(vfs.mknode(321, "id", fsId, MockFs::dataType(), owner).isError());
}


TEST_F(MockFsTest, makingNodeWithoutDirectoryWritePermissionFails) {
	EXPECT_TRUE(vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), User{31, 0}).isError());
}


TEST_F(MockFsTest, enumeratingNonDirectoryFails) {
	// Make a regular data node
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	EXPECT_TRUE(maybeId.isOk());

	ASSERT_TRUE(vfs.enumerateDirectory(*maybeId, owner).isError());
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

TEST_F(MockFsTest, testWalk) {
	auto maybeDirId0 = vfs.createDirectory(vfs.rootId(), "dir0", owner, FilePermissions{0777});
	auto maybeDirId1 = vfs.createDirectory(vfs.rootId(), "dir1", owner, FilePermissions{0700});
    ASSERT_TRUE(maybeDirId0.isOk());
    ASSERT_TRUE(maybeDirId1.isOk());

	auto maybeDir0Id1 =  vfs.createDirectory(*maybeDirId0, "dir0", owner, FilePermissions{0777});
	ASSERT_TRUE(maybeDir0Id1.isOk());

	auto maybeData0Id0 = vfs.mknode(*maybeDirId0, "data0", fsId, MockFs::dataType(), owner);
	auto maybeData0Id1 = vfs.mknode(*maybeDirId0, "data1", fsId, MockFs::dataType(), owner);
	auto maybeData1Id0 = vfs.mknode(*maybeDirId1, "data0", fsId, MockFs::dataType(), owner);
	auto maybeData1Id1 = vfs.mknode(*maybeDirId1, "data1", fsId, MockFs::dataType(), owner);

    ASSERT_TRUE(maybeData0Id0.isOk());
    ASSERT_TRUE(maybeData0Id1.isOk());
    ASSERT_TRUE(maybeData1Id0.isOk());
    ASSERT_TRUE(maybeData1Id1.isOk());
	ASSERT_EQ(8, vfs.size());

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
	EXPECT_FALSE(vfs.walk(User{9, 0}, vfs.rootId(), *makePath("dir1", "data0"), [&count](auto const&) { count += 1; }));
    EXPECT_EQ(-3, count);
}


TEST_F(MockFsTest, testPremissionsInheritence) {
	auto maybeNodeId = vfs.mknode(vfs.rootId(), "data", fsId, MockFs::dataType(), owner, FilePermissions{0777});
	auto maybeNode = vfs.nodeById(maybeNodeId);
	ASSERT_TRUE(maybeNode);

	auto const& node = *maybeNode;
	EXPECT_EQ(0600, node.permissions);
}


TEST_F(MockFsTest, testFileWriteUpdatesSize) {
	auto maybeNodeId = vfs.mknode(vfs.rootId(), "str1", fsId, MockFs::dataType(), owner);
    ASSERT_TRUE(maybeNodeId);

    {
		auto maybeNode = vfs.nodeById(maybeNodeId);
        ASSERT_TRUE(maybeNode);

		auto const& node = *maybeNode;
		EXPECT_EQ(5, node.dataSize);
		EXPECT_EQ(0600, node.permissions);
    }

	auto maybeFile = vfs.open(owner, *maybeNodeId, Permissions::READ);
	ASSERT_TRUE(maybeFile);

	char msg[] = "other-message";
	auto buf = wrapMemory(msg);
	ASSERT_TRUE((*maybeFile).write(buf));

    {
        auto maybeNode = vfs.nodeById(*maybeNodeId);
        ASSERT_TRUE(maybeNode);
		auto& node = *maybeNode;
		EXPECT_EQ(buf.size(), node.dataSize);
    }

	// TODO(abbyssoul): Write few more bytes to test writeOffset works
}



TEST_F(MockFsTest, stringFs) {
	auto maybeNodeId = vfs.mknode(vfs.rootId(), "str1", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeNodeId);

	char destBuffer[32];
	auto dest = wrapMemory(destBuffer);
	dest.fill(0);

	auto maybeFile = vfs.open(owner, *maybeNodeId, Permissions::READ);
	ASSERT_TRUE(maybeFile);
	ASSERT_TRUE((*maybeFile).read(dest));

	// TODO(abbyssoul): Read few more bytes to test readOffset works

	destBuffer[31] = 0;
	EXPECT_STREQ("hello", destBuffer);
}

