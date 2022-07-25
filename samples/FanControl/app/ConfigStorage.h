#pragma once

#include <Storage.h>

class ConfigStorage
{
public:
	ConfigStorage(const String& partitionName) : partition(Storage::findPartition(partitionName))
	{
	}

	ConfigStorage(Storage::Partition partition) : partition(partition)
	{
	}

	explicit operator bool() const
	{
		return bool(partition);
	}

	size_t load(void* data, size_t length);
	bool save(const void* data, size_t length);

	template <typename T> size_t load(T& data)
	{
		return load(&data, sizeof(T));
	}

	template <typename T> bool save(const T& data)
	{
		return save(&data, sizeof(T));
	}

private:
	struct Header {
		/**
		 * size is 0xffffffff when content newly written.
		 * Changed to actual data size when valid.
		 * Set to 0 when retired.
		 */
		uint32_t size;
		// uint8_t data[];
	};

	size_t getBlockSize() const
	{
		return std::min(partition.getBlockSize(), 4096U);
	}

	Storage::Partition partition;
};
