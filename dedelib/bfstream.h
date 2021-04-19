#pragma once

#include <fstream>
#include <cstdint>

class bofstream {
	std::ofstream os;

	bofstream() = default;
	bofstream(const char* fileName) : os(fileName, std::ios::binary) {}
	bofstream(std::string fileName) : os(std::move(fileName), std::ios::binary) {}
	bofstream(const char* fileName, std::ios::openmode om) : os(fileName, om | std::ios::binary) {}
	bofstream(std::string fileName, std::ios::openmode om) : os(std::move(fileName), om | std::ios::binary) {}
	bofstream(std::ofstream&& file) : os(std::move(file)) {}

	void write(const uint8_t* data, std::streamsize size) {
		os.write(reinterpret_cast<const char*>(data), size);
	}
	/*
	template<std::size_t Size>
	void write(const uint8_t (&data)[Size]) {
		os.write(reinterpret_cast<const char*>(&data), Size);
	}
	*/
	inline void writeU8(uint8_t v) {
		this->write(&v, 1);
	}
	inline void writeU16(uint16_t v) {
		const uint8_t buf[2]{v >> 8 & 0xFF, v};
		this->write(buf, 2);
	}
	inline void writeU32(uint32_t v) {
		const uint8_t buf[4]{v >> 24 & 0xFF, v >> 16 & 0xFF, v >> 8 & 0xFF, v};
		this->write(buf, 4);
	}
	inline void writeU64(uint64_t v) {
		const uint8_t buf[8]{v >> 56 & 0xFF, v >> 48 & 0xFF, v >> 40 & 0xFF, v >> 32 & 0xFF, v >> 24 & 0xFF, v >> 16 & 0xFF, v >> 8 & 0xFF, v};
		this->write(buf, 8);
	}
	inline void writeI8(int8_t i) {
		uint8_t v = static_cast<uint8_t>(i);
		this->writeU8(v);
	}
	inline void writeI16(int16_t i) {
		uint16_t v = static_cast<uint16_t>(i);
		this->writeU16(v);
	}
	inline void writeI32(int32_t i) {
		uint32_t v = static_cast<uint32_t>(i);
		this->writeU32(v);
	}
	inline void writeI64(int64_t i) {
		uint64_t v = static_cast<uint64_t>(i);
		this->writeU64(v);
	}
};

class bifstream {
	std::ifstream is;

	bifstream() = default;
	bifstream(const char* fileName) : is(fileName, std::ios::binary) {}
	bifstream(std::string fileName) : is(std::move(fileName), std::ios::binary) {}
	bifstream(const char* fileName, std::ios::openmode om) : is(fileName, om | std::ios::binary) {}
	bifstream(std::string fileName, std::ios::openmode om) : is(std::move(fileName), om | std::ios::binary) {}
	bifstream(std::ifstream&& file) : is(std::move(file)) {}

	void read(uint8_t* data, std::streamsize size) {
		is.read(reinterpret_cast<char*>(data), size);
	}
	/*
	template<std::size_t Size>
	void read(uint8_t(&data)[Size]) {
		is.read(reinterpret_cast<char*>(&data), Size);
	}
	*/
	inline uint8_t readU8() {
		uint8_t result;
		this->read(&result, 1);
		return result;
	}
	inline uint16_t readU16() {
		uint8_t result[2];
		this->read(result, 2);
		return uint16_t(result[0]) << 8 | uint16_t(result[1]);
	}
	inline uint32_t readU32() {
		uint8_t result[4];
		this->read(result, 4);
		return uint32_t(result[0]) << 24 | uint32_t(result[1]) << 16 | uint32_t(result[2]) << 8 | uint32_t(result[3]);
	}
	inline uint64_t readU64() {
		uint8_t result[8];
		this->read(result, 8);
		return uint64_t(result[0]) << 56 | uint64_t(result[1]) << 48 | uint64_t(result[2]) << 40 | uint64_t(result[3]) << 32 | uint64_t(result[4]) << 24 | uint64_t(result[5]) << 16 | uint64_t(result[6]) << 8 | uint64_t(result[7]);
	}
	inline int8_t readI8() {
		return static_cast<int8_t>(this->readU8());
	}
	inline int16_t readI16() {
		return static_cast<int16_t>(this->readU16());
	}
	inline int32_t readI32() {
		return static_cast<int32_t>(this->readU32());
	}
	inline int64_t readI64() {
		return static_cast<int64_t>(this->readU64());
	}
};
