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


namespace  {

struct MockFs : public Filesystem {
	static uint32 dataType() noexcept {
		return 312;
	}

	MockFs(const char* str)
		: buffer{str}
	{}

	FilePermissions defaultFilePermissions(NodeType) const noexcept override {
		return FilePermissions{0777};
	}

	kasofs::Result<INode>
	createNode(NodeType type, User owner, FilePermissions perms) override {
		INode node{type, owner, perms};
		node.dataSize = buffer.size();

		_nCreated += 1;
		return Ok(std::move(node));
	}

	kasofs::Result<void> destroyNode(INode&) override {
		_nDestroyed += 1;
		return Ok();
	}


	auto open(INode&, Permissions) -> kasofs::Result<OpenFID> override {
		_nOpened += 1;
		return Ok<OpenFID>(0);
	}

	kasofs::Result<size_type> read(OpenFID, INode&, size_type offset, MutableMemoryView dest) override {
		if (offset >= buffer.size())
			return makeError(BasicError::Overflow, "MockFs::read");

		auto data = wrapMemory(buffer.data() + offset, buffer.size() - offset).slice(0, dest.size());
		auto isOk = dest.write(data);
		if (!isOk) {
			return isOk.moveError();
		}

		return Ok(data.size());
	}

	kasofs::Result<size_type> write(OpenFID, INode& node, size_type offset, MemoryView src) override {
		auto const newSize = offset + src.size();
		buffer.reserve(newSize);
		if (buffer.size() < newSize) {
			buffer.resize(newSize);
		}

		auto writeResult = wrapMemory(buffer.data(), buffer.size())
				.write(src, offset);

		if (!writeResult)
			return writeResult.moveError();

		node.dataSize = buffer.length();

		return Ok(src.size());
	}

	auto seek(OpenFID, INode&, size_type, SeekDirection) -> kasofs::Result<size_type> override {
		return makeError(GenericError::ISDIR, "MockFs::seek");
	}

	kasofs::Result<void> close(OpenFID, INode&) override {
		_nClosed += 1;
		return Ok();
	}

	auto filesOpen() const noexcept { return _nOpened; }
	auto filesClosed() const noexcept { return _nClosed; }
	auto nodesCreated() const noexcept { return _nCreated; }
	auto nodesDestroyed() const noexcept { return _nCreated; }

private:
	std::string buffer;

	uint32 _nCreated{0};
	uint32 _nDestroyed{0};

	uint32 _nOpened{0};
	uint32 _nClosed{0};
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

}  // namespace



TEST(TestVfs, testContructor) {
	auto vfs = Vfs{User{0, 0}, FilePermissions{0666}};

	auto root = vfs.nodeById(vfs.rootId());
	ASSERT_TRUE(root.isSome());
	EXPECT_EQ(0U, (*root).fsTypeId);
	EXPECT_EQ(1U, vfs.size());
}


struct MockFsTest : public ::testing::Test {

	void SetUp() override {
		auto maybeFsId = vfs.registerFilesystem<MockFs>("hello");
		ASSERT_TRUE(maybeFsId.isOk());
		fsId = *maybeFsId;

		auto fs = vfs.findFs(fsId);
		ASSERT_TRUE(fs.isSome());

		_mockFs = static_cast<MockFs*>(*fs);
	}

	void TearDown() override {
		EXPECT_EQ(_mockFs->filesOpen(), _mockFs->filesClosed());
		EXPECT_EQ(_mockFs->nodesCreated(), _mockFs->nodesDestroyed());
	}

protected:
	User	owner{0, 0};
	Vfs		vfs{owner, FilePermissions{0640}};
	VfsId	fsId{0};

	MockFs*	_mockFs;
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
	EXPECT_EQ(2U, vfs.size());

	auto enumerator = vfs.enumerateDirectory(owner, vfs.rootId());
	ASSERT_TRUE(enumerator.isOk());

	uint32 count = 0;
	for (auto e : *enumerator) {
		EXPECT_EQ(*maybeId, e.nodeId);
		EXPECT_EQ("id", e.name.substring(0, 2));
		count += 1;
	}
	EXPECT_EQ(3U, count);

	auto maybeNode = vfs.nodeById(*maybeId);
	ASSERT_TRUE(maybeNode.isSome());

	// Make sure there are 3 in-bound links to this node
	EXPECT_EQ(3U, (*maybeNode).nLinks);
}


TEST_F(MockFsTest, linkingNodesToNodesIsNotOk) {
	auto maybeId1 = vfs.mknode(vfs.rootId(), "node-1", fsId, MockFs::dataType(), owner);
	auto maybeId2 = vfs.mknode(vfs.rootId(), "node-2", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId1.isOk());
	ASSERT_TRUE(maybeId2.isOk());
	ASSERT_EQ(3U, vfs.size());

	EXPECT_TRUE(vfs.link(owner, "something-else", *maybeId1, *maybeId2).isError());
	EXPECT_TRUE(vfs.link(owner, "id-other", *maybeId1, *maybeId2).isError());

	// Linking to self is never a good idea
	EXPECT_TRUE(vfs.link(owner, "id-other", *maybeId1, *maybeId1).isError());
}

TEST_F(MockFsTest, linkingRequiresWritePermissions) {
	auto maybeDirId = vfs.createDirectory(vfs.rootId(), "dir", owner, 0600);
	ASSERT_TRUE(maybeDirId.isOk());

	auto maybeId1 = vfs.mknode(vfs.rootId(), "node-1", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId1.isOk());
	ASSERT_EQ(3U, vfs.size());

	EXPECT_TRUE(vfs.link(User{1, 2}, "something-else", *maybeDirId, *maybeId1).isError());
}

TEST_F(MockFsTest, linkingToSelfIsNotOk) {
	EXPECT_TRUE(vfs.link(owner, "something-else", vfs.rootId(), vfs.rootId()).isError());
}


TEST_F(MockFsTest, doubleLinkingIsNotOk) {
	auto maybeId1 = vfs.mknode(vfs.rootId(), "node-1", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId1.isOk());

	EXPECT_TRUE(vfs.link(owner, "link-1", vfs.rootId(), *maybeId1).isOk());
	EXPECT_TRUE(vfs.link(owner, "link-1", vfs.rootId(), *maybeId1).isError());
}


TEST_F(MockFsTest, linkingNonExisingNodesFails) {
    // Self Linking: exising node to itself
	EXPECT_TRUE(vfs.link(owner, "idx", {0, 0}, {0, 0}).isError());

    // Self Linking: non exising node to itself
	EXPECT_TRUE(vfs.link(owner, "id", {747, 0}, {747, 0}).isError());

    // Linking non-existing node to a different non-existing one
	EXPECT_TRUE(vfs.link(owner, "id", {87, 21}, {12, 87}).isError());

    // Linking existing node to to a non-exingsting target
	EXPECT_TRUE(vfs.link(owner, "id", vfs.rootId(), {17, 321}).isError());

    // Linking non-existing node to an existing one
	EXPECT_TRUE(vfs.link(owner, "id", {21, 0}, vfs.rootId()).isError());
}


TEST_F(MockFsTest, unlinkingNodeRemovesNodeWithNoRefTo) {
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId.isOk());

	EXPECT_EQ(2U, vfs.size());
	EXPECT_TRUE(vfs.unlink(owner, vfs.rootId(), "id").isOk());
	EXPECT_EQ(1U, vfs.size());

	auto enumerator = vfs.enumerateDirectory(owner, vfs.rootId());
	ASSERT_TRUE(enumerator.isOk());

	uint32 count = 0;
	for (auto e : *enumerator) {
		EXPECT_EQ(*maybeId, e.nodeId);
		EXPECT_EQ("id", e.name.substring(0, 2));
		count += 1;
	}
	EXPECT_EQ(0U, count);
}


TEST_F(MockFsTest, unlinkingNonExistingNameIsNoop) {
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId.isOk());

	EXPECT_TRUE(vfs.unlink(owner, vfs.rootId(), "id-some").isOk());

	auto enumerator = vfs.enumerateDirectory(owner, vfs.rootId());
	ASSERT_TRUE(enumerator.isOk());
	uint32 count = 0;
	for (auto e : *enumerator) {
		EXPECT_EQ(*maybeId, e.nodeId);
		EXPECT_EQ("id", e.name.substring(0, 2));
		count += 1;
	}
	EXPECT_EQ(1U, count);
}


TEST_F(MockFsTest, unlinkingOneOfMultimpleLinksIsOk) {
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId.isOk());

	auto const nodeId = *maybeId;
	EXPECT_TRUE(vfs.link(owner, "id-2", vfs.rootId(), nodeId).isOk());
	EXPECT_TRUE(vfs.link(owner, "id-3", vfs.rootId(), nodeId).isOk());

	EXPECT_EQ(2U, vfs.size());
	EXPECT_TRUE(vfs.unlink(owner, vfs.rootId(), "id").isOk());
	EXPECT_EQ(2U, vfs.size());

	auto enumerator = vfs.enumerateDirectory(owner, vfs.rootId());
	ASSERT_TRUE(enumerator.isOk());

	uint32 count = 0;
	for (auto e : *enumerator) {
		EXPECT_EQ(nodeId, e.nodeId);
		EXPECT_EQ("id-", e.name.substring(0, 3));
		count += 1;
	}
	EXPECT_EQ(2U, count);
}


TEST_F(MockFsTest, unlinkingFromNonExistingNodeFails) {
	EXPECT_TRUE(vfs.unlink(owner, {61253, 0}, "id-some").isError());
}


TEST_F(MockFsTest, unlinkingEmptyEnumeratedDirectoryIsOK) {
	auto maybeDirId = vfs.createDirectory(vfs.rootId(), "dir", owner);
	ASSERT_TRUE(maybeDirId.isOk());

	auto const dirId = *maybeDirId;
	{
		auto enumerator = vfs.enumerateDirectory(owner, dirId);
		ASSERT_TRUE(enumerator.isOk());

		ASSERT_TRUE(vfs.unlink(owner, vfs.rootId(), "dir").isOk());

		uint32 count = 0;
		for (auto e : *enumerator) {
			EXPECT_EQ("id-", e.name.substring(0, 3));
			count += 1;
		}
		EXPECT_EQ(0U, count);
		EXPECT_EQ(2U, vfs.size());
	}

	EXPECT_EQ(1U, vfs.size());
}



TEST_F(MockFsTest, unlinkingNoneEmptyDirectoryIsNotOK) {
	auto maybeDirId = vfs.createDirectory(vfs.rootId(), "dir", owner);
	ASSERT_TRUE(maybeDirId.isOk());

	auto const dirId = *maybeDirId;
	kasofs::Result<INode::Id> maybeId[] = {
		vfs.mknode(dirId, "id-0", fsId, MockFs::dataType(), owner),
		vfs.mknode(dirId, "id-1", fsId, MockFs::dataType(), owner),
		vfs.mknode(dirId, "id-2", fsId, MockFs::dataType(), owner)
	};

	ASSERT_TRUE(maybeId[0].isOk());
	ASSERT_TRUE(maybeId[1].isOk());
	ASSERT_TRUE(maybeId[2].isOk());

	ASSERT_TRUE(vfs.unlink(owner, vfs.rootId(), "dir").isError());
}


TEST_F(MockFsTest, unlinkingOpenFileRemovesNodeFromIndex) {
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId.isOk());

	auto maybeOpenedFile = vfs.open(owner, *maybeId, Permissions::WRITE);
	ASSERT_TRUE(maybeOpenedFile.isOk());

	// Test case - unlink file, while it is open, and make sure it is not discoverable
	EXPECT_TRUE(vfs.unlink(owner, vfs.rootId(), "id").isOk());
	EXPECT_TRUE(vfs.nodeById(maybeId).isNone());

	char msg[] = "other-message";
	auto& file = *maybeOpenedFile;
	EXPECT_TRUE(file.write(wrapMemory(msg)));
	EXPECT_EQ(sizeof(msg), file.size());
}


TEST_F(MockFsTest, linkingOwnsName) {
	char linkName[16];
	strncpy(linkName, "name1", sizeof(linkName));

	auto maybeId = vfs.mknode(vfs.rootId(), linkName, fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeId.isOk());

	strncpy(linkName, "eman3", sizeof(linkName));

	EXPECT_EQ(2U, vfs.size());

	auto enumerator = vfs.enumerateDirectory(owner, vfs.rootId());
	ASSERT_TRUE(enumerator.isOk());
	uint32 count = 0;
	for (auto e : *enumerator) {
		EXPECT_EQ(*maybeId, e.nodeId);
		EXPECT_EQ("name1", e.name);
		count += 1;
	}
	EXPECT_EQ(1U, count);
}


TEST_F(MockFsTest, makingMockNodes) {
    // Make a regular data node
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	EXPECT_TRUE(maybeId.isOk());

//    EXPECT_EQ(2U, vfs.size());
//    EXPECT_EQ(1, *vfs.countEntries(User{0,0}, vfs.rootId()));

    // Make a regular data node
//    auto maybeGen = vfs.mknode(INode::Type::Data, owner, FilePermissions{0777}, vfs.rootId(), "generation");
//    EXPECT_TRUE(maybeGen.isOk());
//    EXPECT_EQ(3U, vfs.size());
//    EXPECT_EQ(2, *vfs.countEntries(User{0,0}, vfs.rootId()));

    // Make a regular Directory node
//    auto maybeDir = vfs.mknode(INode::Type::Directory, owner, FilePermissions{0777}, vfs.rootId(), "dir");
//    EXPECT_TRUE(maybeDir.isOk());
//    EXPECT_EQ(4U, vfs.size());
//    EXPECT_EQ(3, *vfs.countEntries(User{0,0}, vfs.rootId()));

    // Make a regular Data node in a directory
//    auto maybeDataInDir = vfs.mknode(INode::Type::Data, owner, FilePermissions{0777}, *maybeDir, "data");
//    EXPECT_TRUE(maybeDataInDir.isOk());
//    EXPECT_EQ(5U, vfs.size());
//    EXPECT_EQ(3, *vfs.countEntries(User{0,0}, vfs.rootId()));

    // Make a Synth node in a directory
//    auto maybeSynthInDir = vfs.mknode(User{0,0}, *maybeDir, "synth", FilePermissions{0777},
//                                        []() { return ByteReader{}; },
//                                        []() { return ByteWriter{}; });
//    EXPECT_TRUE(maybeSynthInDir.isOk());
//    EXPECT_EQ(6U, vfs.size());


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
	EXPECT_TRUE(vfs.mknode({321, 0}, "id", fsId, MockFs::dataType(), owner).isError());
}


TEST_F(MockFsTest, makingNodeWithoutDirectoryWritePermissionFails) {
	EXPECT_TRUE(vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), User{31, 0}).isError());
}


TEST_F(MockFsTest, enumeratingNonDirectoryFails) {
	// Make a regular data node
	auto maybeId = vfs.mknode(vfs.rootId(), "id", fsId, MockFs::dataType(), owner);
	EXPECT_TRUE(maybeId.isOk());

	ASSERT_TRUE(vfs.enumerateDirectory(owner, *maybeId).isError());
}


/*
TEST(TestVfs, testDataNodesRelocatable) {
    auto owner = User{0,0};
    auto vfs = Vfs{owner, FilePermissions{0666}};
//    ASSERT_EQ(1U, vfs.size());

    auto maybeId = vfs.mknode(INode::Type::Data, owner, FilePermissions{0777}, vfs.rootId(), "data0");
    ASSERT_TRUE(maybeId.isOk());
//    ASSERT_EQ(2U, vfs.size());

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
//    ASSERT_EQ(3U, vfs.size());

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

TEST_F(MockFsTest, testWalkEmptyDir) {
	int count = 0;
	Entry resultEntry{"", {0, 0}};
	INode resultNode = *vfs.nodeById(vfs.rootId());

	EXPECT_TRUE(vfs.walk(owner, vfs.rootId(), Path{},
						 [&](Entry e, INode node) { resultEntry = e; resultNode = node; count += 1; }));
	EXPECT_EQ(0, count);
	EXPECT_EQ(vfs.rootId(), resultEntry.nodeId);
	EXPECT_EQ("", resultEntry.name);
}


TEST_F(MockFsTest, testWalkNonExistentEntry) {
	int count = 0;
	Entry resultEntry{"", {0, 0}};
	INode resultNode = *vfs.nodeById(vfs.rootId());

	EXPECT_FALSE(vfs.walk(owner, vfs.rootId(), *makePath("dir0"),
						 [&](Entry e, INode node) { resultEntry = e; resultNode = node; count += 1; }));
	EXPECT_EQ(0, count);
	EXPECT_EQ(vfs.rootId(), resultEntry.nodeId);
	EXPECT_EQ("", resultEntry.name);
}

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
	ASSERT_EQ(8U, vfs.size());

	{
		Entry resultEntry{".", vfs.rootId()};
		int count = 0;
		EXPECT_TRUE(vfs.walk(owner, vfs.rootId(), *makePath("dir0"),
							 [&count, &resultEntry](Entry e, auto const&) { resultEntry = e; count += 1; }));
		EXPECT_EQ(1, count);
		EXPECT_EQ(*maybeDirId0, resultEntry.nodeId);
	}

	{
		Entry resultEntry{".", vfs.rootId()};
		int count = 0;
		EXPECT_TRUE(vfs.walk(owner, vfs.rootId(), *makePath("dir0", "data0"),
							 [&count, &resultEntry](Entry e, auto const&) { resultEntry = e; count += 1; }));
		EXPECT_EQ(2, count);
		EXPECT_EQ(*maybeData0Id0, resultEntry.nodeId);
	}

	{
		Entry resultEntry{".", vfs.rootId()};
		int count = 0;
		EXPECT_TRUE(vfs.walk(owner, vfs.rootId(), *makePath("dir0", "dir0"),
							 [&count, &resultEntry](Entry e, auto const&) { resultEntry = e; count += 1; }));
		EXPECT_EQ(2, count);
		EXPECT_EQ(*maybeDir0Id1, resultEntry.nodeId);
	}

	{
		Entry resultEntry{".", vfs.rootId()};
		int count = 0;
		EXPECT_TRUE(vfs.walk(owner, vfs.rootId(), *makePath("dir1", "data0"),
							 [&count, &resultEntry](Entry e, auto const&) { resultEntry = e; count += 1; }));
		EXPECT_EQ(2, count);
		EXPECT_EQ(*maybeData1Id0, resultEntry.nodeId);
	}

	{
		Entry resultEntry{".", vfs.rootId()};
		int count = 0;
		EXPECT_TRUE(vfs.walk(owner, vfs.rootId(), *makePath("dir1", "data0"),
							 [&count, &resultEntry](Entry e, auto const&) { resultEntry = e; count += 1; }));
		EXPECT_EQ(2, count);
		EXPECT_EQ(*maybeData1Id0, resultEntry.nodeId);
	}

	{
		Entry resultEntry{".", vfs.rootId()};
		int count = 0;
		EXPECT_FALSE(vfs.walk(owner, vfs.rootId(), *makePath("dir1", "data7"),
							 [&count, &resultEntry](Entry e, auto const&) { resultEntry = e; count += 1; }));
		EXPECT_EQ(1, count);
		EXPECT_EQ(*maybeDirId1, resultEntry.nodeId);
	}

	{
		Entry resultEntry{".", vfs.rootId()};
		int count = -3;
		EXPECT_FALSE(vfs.walk(User{9, 1}, vfs.rootId(), *makePath("dir1", "data0"),
							  [&count, &resultEntry](Entry e, auto const&) { resultEntry = e; count += 1; }));
		EXPECT_EQ(-3, count);
		EXPECT_EQ(vfs.rootId(), resultEntry.nodeId);
	}
}


TEST_F(MockFsTest, testPremissionsInheritence) {
	auto maybeNodeId = vfs.mknode(vfs.rootId(), "data", fsId, MockFs::dataType(), owner, FilePermissions{0777});
	auto maybeNode = vfs.nodeById(maybeNodeId);
	ASSERT_TRUE(maybeNode);

	auto const& node = *maybeNode;
	EXPECT_EQ(0640, node.permissions);
}


TEST_F(MockFsTest, testFileWriteUpdatesSize) {
	auto maybeNodeId = vfs.mknode(vfs.rootId(), "str1", fsId, MockFs::dataType(), owner);
    ASSERT_TRUE(maybeNodeId);

	auto const nodeId = *maybeNodeId;
    {
		auto maybeNode = vfs.nodeById(maybeNodeId);
        ASSERT_TRUE(maybeNode);

		auto const& node = *maybeNode;
		EXPECT_EQ(5U, node.dataSize);
		EXPECT_EQ(0640, node.permissions);
    }

	char msg[] = "other-message";
	auto buf = wrapMemory(msg);
	{
		auto maybeFile = vfs.open(owner, nodeId, Permissions::READ);
		ASSERT_TRUE(maybeFile.isOk());
		ASSERT_TRUE((*maybeFile).write(buf).isOk());

		// Note: file is closed after this point
	}

    {
		auto maybeNode = vfs.nodeById(nodeId);
        ASSERT_TRUE(maybeNode);
		auto& node = *maybeNode;
		EXPECT_EQ(buf.size(), node.dataSize);

		auto maybeReadFile = vfs.open(owner, nodeId, Permissions::READ);
		ASSERT_TRUE(maybeReadFile.isOk());

		char readBuf[32];
		auto wrappedReadBuf = wrapMemory(readBuf);
		wrappedReadBuf.fill(0);
		ASSERT_TRUE((*maybeReadFile).read(wrappedReadBuf).isOk());
		EXPECT_STREQ(msg, readBuf);
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


TEST_F(MockFsTest, movingOpenFilesOk) {
	auto maybeNodeId = vfs.mknode(vfs.rootId(), "str1", fsId, MockFs::dataType(), owner);
	ASSERT_TRUE(maybeNodeId);

	{
		EXPECT_EQ(_mockFs->filesOpen(), 0U);
		std::vector<File> openFiles;

		auto const nodeId = *maybeNodeId;
		{
			auto maybeFile = vfs.open(owner, nodeId, Permissions::READ);
			ASSERT_TRUE(maybeFile.isOk());
			EXPECT_EQ(_mockFs->filesOpen(), 1U);
			EXPECT_EQ(_mockFs->filesClosed(), 0U);

			openFiles.emplace_back(maybeFile.moveResult());
			EXPECT_EQ(_mockFs->filesOpen(), 1U);
			EXPECT_EQ(_mockFs->filesClosed(), 0U);
		}

		EXPECT_EQ(_mockFs->filesOpen(), 1U);
		EXPECT_EQ(_mockFs->filesClosed(), 0U);
	}

	EXPECT_EQ(_mockFs->filesOpen(), 1U);
	EXPECT_EQ(_mockFs->filesClosed(), 1U);
}



TEST_F(MockFsTest, creatingDirectoryIsOK) {
	auto maybeDirId = vfs.createDirectory(vfs.rootId(), "dir", owner);
	ASSERT_TRUE(maybeDirId.isOk());
	EXPECT_EQ(2U, vfs.size());

	auto const dirId = *maybeDirId;
	kasofs::Result<INode::Id> maybeId[] = {
		vfs.mknode(dirId, "id-0", fsId, MockFs::dataType(), owner),
		vfs.mknode(dirId, "id-1", fsId, MockFs::dataType(), owner),
		vfs.mknode(dirId, "id-2", fsId, MockFs::dataType(), owner)
	};

	ASSERT_TRUE(maybeId[0].isOk());
	ASSERT_TRUE(maybeId[1].isOk());
	ASSERT_TRUE(maybeId[2].isOk());
	EXPECT_EQ(5U, vfs.size());

	auto enumerator = vfs.enumerateDirectory(owner, dirId);
	ASSERT_TRUE(enumerator.isOk());

	uint32 count = 0;
	for (auto e : *enumerator) {
		EXPECT_EQ("id-", e.name.substring(0, 3));
		count += 1;
	}
	EXPECT_EQ(3U, count);
}
