#include "ConfigStorage.h"

size_t ConfigStorage::load(void* data, size_t length)
{
	uint32_t size1{0};
	uint32_t size2{0};
	auto blockSize = getBlockSize();
	partition.read(0, size1);
	partition.read(blockSize, size2);
	auto maxSize = blockSize - sizeof(Header);
	uint32_t offset;
	uint32_t size;
	if(size1 != 0 && size1 < maxSize) {
		offset = 0;
		size = size1;
	} else if(size2 != 0 && size2 < maxSize) {
		offset = blockSize;
		size = size2;
	} else {
		return 0;
	}
	length = std::min(uint32_t(length), size);
	return partition.read(offset + sizeof(Header), data, length) ? length : 0;
}

bool ConfigStorage::save(const void* data, size_t length)
{
	auto blockSize = getBlockSize();
	auto maxSize = blockSize - sizeof(Header);
	if(length > maxSize) {
		return false;
	}

	uint32_t size1{0};
	uint32_t size2{0};
	uint32_t offset{0};
	uint32_t size{0};
	partition.read(offset, size);
	if(size > 0 && size <= maxSize) {
		offset = blockSize;
		partition.read(offset, size);
	}
	if(!partition.erase_range(offset, blockSize)) {
		return false;
	}
	if(!partition.write(offset + sizeof(Header), data, length)) {
		return 0;
	}
	if(!partition.write(offset, &size, sizeof(size))) {
		return 0;
	}
	// Invalidate old block
	size = 0;
	partition.write(offset, &size, sizeof(size));
	return true;
}
