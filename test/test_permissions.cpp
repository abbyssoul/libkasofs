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
 *	@file test/test_permissions.cpp
 *	@brief		Test suit for KasoFS::Credentials
 ******************************************************************************/
#include "kasofs/credentials.hpp"    // Class being tested.

#include <gtest/gtest.h>


using namespace kasofs;
using namespace Solace;


TEST(TestVfsFileMode, modeEquality) {
	// -rwx------ / 0700
	EXPECT_TRUE(0700 == FileMode{0700});

	EXPECT_TRUE(FileMode(FileTypeMask::Dir, 0700).isDirectory());
	EXPECT_TRUE(FileMode(FileTypeMask::File, 0700).isFile());
}


TEST(TestVfsPermissions, rwx_000_000) {
    // -rwx------ / 0700
    EXPECT_TRUE(FilePermissions{0700}.user().isReadable());
    EXPECT_TRUE(FilePermissions{0700}.user().isWritable());
    EXPECT_TRUE(FilePermissions{0700}.user().isExecutable());

    EXPECT_FALSE(FilePermissions{0700}.group().isReadable());
    EXPECT_FALSE(FilePermissions{0700}.group().isWritable());
    EXPECT_FALSE(FilePermissions{0700}.group().isExecutable());

    EXPECT_FALSE(FilePermissions{0700}.others().isReadable());
    EXPECT_FALSE(FilePermissions{0700}.others().isWritable());
    EXPECT_FALSE(FilePermissions{0700}.others().isExecutable());
}

TEST(TestVfsPermissions, rwx_rwx_000) {
    // -rwxrwx--- / 0770
    EXPECT_TRUE(FilePermissions{0770}.user().isReadable());
    EXPECT_TRUE(FilePermissions{0770}.user().isWritable());
    EXPECT_TRUE(FilePermissions{0770}.user().isExecutable());

    EXPECT_TRUE(FilePermissions{0770}.group().isReadable());
    EXPECT_TRUE(FilePermissions{0770}.group().isWritable());
    EXPECT_TRUE(FilePermissions{0770}.group().isExecutable());

    EXPECT_FALSE(FilePermissions{0770}.others().isReadable());
    EXPECT_FALSE(FilePermissions{0770}.others().isWritable());
    EXPECT_FALSE(FilePermissions{0770}.others().isExecutable());
}


TEST(TestVfsPermissions, r_x_r_x_r_x) {
    // -r-xr-xr-x / 0555
    EXPECT_TRUE(FilePermissions{0555}.user().isReadable());
    EXPECT_FALSE(FilePermissions{0555}.user().isWritable());
    EXPECT_TRUE(FilePermissions{0555}.user().isExecutable());

    EXPECT_TRUE(FilePermissions{0555}.group().isReadable());
    EXPECT_FALSE(FilePermissions{0555}.group().isWritable());
    EXPECT_TRUE(FilePermissions{0555}.group().isExecutable());

    EXPECT_TRUE(FilePermissions{0555}.others().isReadable());
    EXPECT_FALSE(FilePermissions{0555}.others().isWritable());
    EXPECT_TRUE(FilePermissions{0555}.others().isExecutable());
}


TEST(TestVfsPermissions, can) {
    EXPECT_TRUE(FilePermissions{0555}.group().can(Permissions::READ | Permissions::EXEC));
    EXPECT_FALSE(FilePermissions{0555}.group().can(Permissions::READ | Permissions::WRITE));
}
