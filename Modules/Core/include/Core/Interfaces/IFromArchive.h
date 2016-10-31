// IFromArchive.h
#pragma once

#include <cassert>
#include "../Util/InterfaceUtils.h"
#include "../Reflection/Reflection.h"
#include "../IO/ArchiveReader.h"

namespace sge
{
	struct SGE_CORE_API IFromArchive
	{
		SGE_REFLECTED_INTERFACE;
		SGE_INTERFACE_1(IFromArchive, from_archive);

		/////////////////////
		///   Functions   ///
	public:

		void(*from_archive)(SelfMut self, const ArchiveReader& reader);
	};

	template <typename T>
	struct Impl< IFromArchive, T >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			self.as<T>()->from_archive(reader);
		}
	};

	template <>
	struct Impl< IFromArchive, bool >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<bool>());
		}
	};

	template <>
	struct Impl< IFromArchive, int8 >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<int8>());
		}
	};

	template <>
	struct Impl< IFromArchive, uint8 >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<uint8>());
		}
	};

	template <>
	struct Impl< IFromArchive, int16 >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<int16>());
		}
	};

	template <>
	struct Impl< IFromArchive, uint16 >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<uint16>());
		}
	};

	template <>
	struct Impl< IFromArchive, int32 >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<int32>());
		}
	};

	template <>
	struct Impl< IFromArchive, uint32 >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<uint32>());
		}
	};

	template <>
	struct Impl< IFromArchive, int64 >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<int64>());
		}
	};

	template <>
	struct Impl< IFromArchive, uint64 >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<uint64>());
		}
	};

	template <>
	struct Impl< IFromArchive, float >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<float>());
		}
	};

	template <>
	struct Impl< IFromArchive, double >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			reader.value(*self.as<double>());
		}
	};

	template <>
	struct Impl< IFromArchive, std::string >
	{
		static void from_archive(SelfMut self, const ArchiveReader& reader)
		{
			assert(!self.null());
			const char* str = nullptr;
			std::size_t len = 0;

			if (reader.value(str, len))
			{
				self.as<std::string>()->assign(str, len);
			}
		}
	};

	template <typename T>
	void from_archive(T& value, const ArchiveReader& reader)
	{
		Impl<IFromArchive, T>::from_archive(&value, reader);
	}
}