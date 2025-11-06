#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace pdk::spdm::platforms::res::crypto::hash {

void* sha256_new();
void  sha256_free(void* ptr);
bool  sha256_init(void* ptr);
bool  sha256_duplicate(const void* ptr, void* new_ptr);
bool  sha256_update(void* ptr, const void* data, size_t data_size);
bool  sha256_final(void* ptr, uint8_t* hash_value);
bool  sha256_hash_all(const void* data, size_t data_size, uint8_t* hash_value);

void* sha384_new();
void  sha384_free(void* ptr);
bool  sha384_init(void* ptr);
bool  sha384_duplicate(const void* ptr, void* new_ptr);
bool  sha384_update(void* ptr, const void* data, size_t data_size);
bool  sha384_final(void* ptr, uint8_t* hash_value);
bool  sha384_hash_all(const void* data, size_t data_size, uint8_t* hash_value);

void* sha512_new();
void  sha512_free(void* ptr);
bool  sha512_init(void* ptr);
bool  sha512_duplicate(const void* ptr, void* new_ptr);
bool  sha512_update(void* ptr, const void* data, size_t data_size);
bool  sha512_final(void* ptr, uint8_t* hash_value);
bool  sha512_hash_all(const void* data, size_t data_size, uint8_t* hash_value);

void* sha3_256_new();
void  sha3_256_free(void* ptr);
bool  sha3_256_init(void* ptr);
bool  sha3_256_duplicate(const void* ptr, void* new_ptr);
bool  sha3_256_update(void* ptr, const void* data, size_t data_size);
bool  sha3_256_final(void* ptr, uint8_t* hash_value);
bool  sha3_256_hash_all(const void* data, size_t data_size, uint8_t* hash_value);

void* sha3_384_new();
void  sha3_384_free(void* ptr);
bool  sha3_384_init(void* ptr);
bool  sha3_384_duplicate(const void* ptr, void* new_ptr);
bool  sha3_384_update(void* ptr, const void* data, size_t data_size);
bool  sha3_384_final(void* ptr, uint8_t* hash_value);
bool  sha3_384_hash_all(const void* data, size_t data_size, uint8_t* hash_value);

void* sha3_512_new();
void  sha3_512_free(void* ptr);
bool  sha3_512_init(void* ptr);
bool  sha3_512_duplicate(const void* ptr, void* new_ptr);
bool  sha3_512_update(void* ptr, const void* data, size_t data_size);
bool  sha3_512_final(void* ptr, uint8_t* hash_value);
bool  sha3_512_hash_all(const void* data, size_t data_size, uint8_t* hash_value);

void* sm3_256_new();
void  sm3_256_free(void* ptr);
bool  sm3_256_init(void* ptr);
bool  sm3_256_duplicate(const void* ptr, void* new_ptr);
bool  sm3_256_update(void* ptr, const void* data, size_t data_size);
bool  sm3_256_final(void* ptr, uint8_t* hash_value);
bool  sm3_256_hash_all(const void* data, size_t data_size, uint8_t* hash_value);

}  // namespace pdk::spdm::platforms::res::crypto::hash