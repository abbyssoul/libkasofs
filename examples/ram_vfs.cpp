/*
*  Copyright 2020 Ivan Ryabov
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*/

/**
 * An example of command line argument parser for a single action CLI.
*/

#include <kasofs/kasofs.hpp>
#include <kasofs/extras/ramfsDriver.hpp>

#include <solace/output_utils.hpp>

#include <iostream>
#include <unistd.h>
#include <sys/types.h>


using namespace kasofs;
using namespace Solace;


static constexpr StringLiteral      kAppName = "ram_vfs";
static const Version                kAppVersion = Version(0, 0, 1, "dev");


namespace /*anonymous*/ {

kasofs::User
getSystemUser() noexcept {
	return {getuid(), getgid()};
}

}  // anonymous namespace


int main(int argc, const char **argv) {
	// Get current proc user.
	auto currentUser = getSystemUser();
	// Create VFS
	auto vfs = kasofs::Vfs{currentUser, kasofs::FilePermissions{0777}};

	// Register RamVFS driver
	auto maybeRamFsDriverId = vfs.registerFilesystem<RamFS>(4096);
	if (!maybeRamFsDriverId) {
		std::cerr << "Failed to register RAM fs driver: " << maybeRamFsDriverId.getError() << '\n';
		return EXIT_FAILURE;
	}

	// Create a directory
	auto maybeDir = vfs.createDirectory(vfs.rootId(), "ram", currentUser);
	if (!maybeDir) {
		std::cerr << "Failed to create a directory: " << maybeDir.getError() << '\n';
		return EXIT_FAILURE;
	}

	for (int i = 1; i < argc; ++i) {
		auto maybeFile = vfs.mknode(*maybeDir, argv[i], *maybeRamFsDriverId, RamFS::kNodeType, currentUser);
		if (!maybeFile) {
			std::cerr << "Failed to create a ram-file: " << maybeFile.getError() << '\n';
			return EXIT_FAILURE;
		}
	}


    return EXIT_SUCCESS;
}
