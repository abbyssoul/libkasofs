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
#include "kasofs/vinode.hpp"    // Class being tested.

#include <gtest/gtest.h>


using namespace kasofs;
using namespace Solace;


TEST(TestINode, testContructor_Dir) {
    auto node = INode{INode::Type::Directory, User{0,0}, FilePermissions{0666}};
    EXPECT_EQ(INode::Type::Directory, node.type());
}

TEST(TestINode, testContructor_Data) {
    auto node = INode{INode::Type::Data, User{0,0}, FilePermissions{0666}};
    EXPECT_EQ(INode::Type::Data, node.type());
}


TEST(TestINode, testDirectoryMeta) {
    auto node = INode{INode::Type::Directory, User{0,0}, FilePermissions{0764}};

    EXPECT_EQ(0x80000000 | 0764, node.mode());
    EXPECT_EQ(4096, node.length());
}
