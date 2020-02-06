/*
 * ObjectArray.h
 *
 *  Created on: 29 May 2018
 *      Author: mikee47
 *
 * Maintains a static array of objects, or rather any type of data
 */

#pragma once

#include <stdlib.h>

template <typename T> class ObjectArray
{
public:
	virtual ~ObjectArray()
	{
		free();
	}

	bool init(unsigned elementCount);

	void free()
	{
		count = 0;
		delete[] elements;
		elements = nullptr;
	}

	unsigned size() const
	{
		return count;
	}

	const T& operator[](unsigned i) const
	{
		if(i < count) {
			return elements[i];
		}

		abort();
		return *static_cast<T*>(nullptr);
	}

	T& operator[](unsigned i)
	{
		if(i < count) {
			return elements[i];
		}

		abort();
		return *static_cast<T*>(nullptr);
	}

protected:
	T& getElement(unsigned i);

private:
	T* elements = nullptr;
	unsigned count = 0;
};

template <typename T> bool ObjectArray<T>::init(unsigned elementCount)
{
	free();

	if(elementCount != 0) {
		elements = new T[elementCount];
		if(elements == nullptr) {
			return false;
		}

		count = elementCount;
	}

	return true;
}
