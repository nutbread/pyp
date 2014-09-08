#ifndef __MAP_H
#define __MAP_H



#include <stdint.h>
#include "Unicode.h"



typedef uint32_t MapHashValue;

typedef enum MapStatus_ {
	MAP_ERROR = 0x0,
	MAP_ADDED = 0x1,
	MAP_ALREADY_EXISTED = 0x2,
	MAP_FOUND = 0x3,
	MAP_NOT_FOUND = 0x4,
} MapStatus;



MapHashValue mapHelperHashString(const char* key);
int mapHelperCompareString(const char* key1, const char* key2);
int mapHelperCopyString(const char* key, char** output);
void mapHelperDeleteString(char* key);


MapHashValue mapHelperHashUnicode(const unicode_char* key);
int mapHelperCompareUnicode(const unicode_char* key1, const unicode_char* key2);
int mapHelperCopyUnicode(const unicode_char* key, unicode_char** output);
void mapHelperDeleteUnicode(unicode_char* key);



#define MAP_FUNCTION_HEADERS(key_type, data_type, mapName, MapType) \
	MAP_FUNCTION_HEADERS_EXT(, key_type, data_type, mapName, MapType)

#define MAP_FUNCTION_HEADERS_STATIC(key_type, data_type, mapName, MapType) \
	MAP_FUNCTION_HEADERS_EXT(static, key_type, data_type, mapName, MapType)

#define MAP_BODY_HELPER_HEADERS(key_type, data_type, mapName, MapType) \
	MAP_BODY_HELPER_HEADERS_EXT(, key_type, data_type, mapName, MapType)

#define MAP_BODY_HELPER_HEADERS_STATIC(key_type, data_type, mapName, MapType) \
	MAP_BODY_HELPER_HEADERS_EXT(static, key_type, data_type, mapName, MapType)



// No templates sure do make type safety easy
#define MAP_DATA_HEADER(MapType) \
	struct MapType##_; \
	struct MapType##MapBucket_; \
	typedef struct MapType##_ { \
		size_t bucketCount; \
		struct MapType##MapBucket_* buckets; \
	} MapType; \


#define MAP_FUNCTION_HEADERS_EXT(static_flag, key_type, data_type, mapName, MapType) \
	struct MapType##_; \
	struct MapType##MapBucket_; \
	static_flag MapType* mapName##Create(MapType* map); \
	static_flag MapType* mapName##CreateSize(MapType* map, size_t bucketCount); \
	static_flag void mapName##Clean(MapType* map); \
	static_flag void mapName##Delete(MapType* map); \
	static_flag MapStatus mapName##Add(MapType* map, const key_type key, data_type data); \
	static_flag MapStatus mapName##Remove(MapType* map, const key_type key, data_type* data); \
	static_flag MapStatus mapName##Find(const MapType* map, const key_type key, data_type* data); \


#define MAP_BODY_HELPER_HEADERS_EXT(static_flag, key_type, data_type, mapName, MapType) \
	static_flag MapHashValue mapName##KeyHashFunction(const key_type key); \
	static_flag int mapName##KeyCompareFunction(const key_type key1, const key_type key2); \
	static_flag int mapName##KeyCopyFunction(const key_type key, key_type* output); \
	static_flag void mapName##KeyDeleteFunction(key_type key); \
	static_flag void mapName##ValueDeleteFunction(data_type value); \



#define MAP_BODY(key_type, data_type, mapName, MapType) \
	struct MapType##Entry_; \
	struct MapType##Entry_ { \
		key_type key; \
		data_type data; \
		struct MapType##Entry_* nextSibling; \
	}; \
	struct MapType##MapBucket_ { \
		struct MapType##Entry_* firstChild; \
		struct MapType##Entry_** ptrToNext; \
	}; \
	\
	MapType* \
	mapName##Create(MapType* map) { \
		/* Create */ \
		return mapName##CreateSize(map, 256); \
	} \
	\
	MapType* \
	mapName##CreateSize(MapType* map, size_t bucketCount) { \
		/* Vars */ \
		size_t i; \
		\
		/* Assertions */ \
		assert(bucketCount > 0); \
		\
		/* Create */ \
		if (map == NULL) { \
			map = memAlloc(MapType); \
			if (map == NULL) return NULL; /* error */ \
		} \
		\
		/* Setup */ \
		map->bucketCount = bucketCount; \
		map->buckets = memAllocArray(struct MapType##MapBucket_, map->bucketCount); \
		if (map->buckets == NULL) { \
			/* Cleanup */ \
			memFree(map); \
			return NULL; \
		} \
		\
		/* Setup buckets */ \
		for (i = 0; i < map->bucketCount; ++i) { \
			map->buckets[i].firstChild = NULL; \
			map->buckets[i].ptrToNext = &map->buckets[i].firstChild; \
		} \
		\
		/* Done */ \
		return map; \
	} \
	\
	void \
	mapName##Clean(MapType* map) { \
		/* Vars */ \
		struct MapType##Entry_* entry; \
		struct MapType##Entry_* next; \
		size_t i; \
		\
		/* Assertions */ \
		assert(map != NULL); \
		\
		/* Delete bucket lists */ \
		for (i = 0; i < map->bucketCount; ++i) { \
			for (entry = map->buckets[i].firstChild; entry != NULL; entry = next) { \
				/* Delete */ \
				mapName##KeyDeleteFunction(entry->key); \
				mapName##ValueDeleteFunction(entry->data); \
				next = entry->nextSibling; \
				memFree(entry); \
			} \
		} \
		\
		/* Delete map */ \
		memFree(map->buckets); \
		map->buckets = NULL; \
	} \
	\
	void \
	mapName##Delete(MapType* map) { \
		assert(map != NULL); \
		\
		/* Clean and free */ \
		mapName##Clean(map); \
		memFree(map); \
	} \
	\
	MapStatus \
	mapName##Add(MapType* map, const key_type key, data_type data) { \
		/* Vars */ \
		MapHashValue bucket; \
		struct MapType##Entry_* entry; \
		\
		/* Assertions */ \
		assert(map != NULL); \
		assert(key != NULL); \
		assert(mapName##Find(map, key, NULL) == MAP_NOT_FOUND); /* Cannot already exist */ \
		assert(mapName##KeyCompareFunction(key, key)); \
		\
		/* Find hash/bucket */ \
		bucket = mapName##KeyHashFunction(key) % map->bucketCount; \
		\
		/* Create new */ \
		entry = memAlloc(struct MapType##Entry_); \
		if (entry == NULL) return MAP_ERROR; /* error */ \
		\
		if (mapName##KeyCopyFunction(key, &entry->key) != 0) { \
			/* Cleanup */ \
			memFree(entry); \
			return MAP_ERROR; \
		} \
		entry->data = data; \
		entry->nextSibling = NULL; \
		\
		assert(entry->key != key); \
		assert(mapName##KeyCompareFunction(entry->key, key)); \
		\
		/* Add */ \
		*(map->buckets[bucket].ptrToNext) = entry; \
		map->buckets[bucket].ptrToNext = &entry->nextSibling; \
		\
		/* Done */ \
		return MAP_ADDED; \
	} \
	\
	MapStatus \
	mapName##Remove(MapType* map, const key_type key, data_type* data) { \
		/* Vars */ \
		MapHashValue bucket; \
		struct MapType##Entry_** ptrEntry; \
		struct MapType##Entry_* deletionEntry; \
		\
		/* Assertions */ \
		assert(map != NULL); \
		assert(key != NULL); \
		assert(mapName##KeyCompareFunction(key, key)); \
		\
		/* Find hash/bucket */ \
		bucket = mapName##KeyHashFunction(key) % map->bucketCount; \
		\
		/* Find */ \
		for (ptrEntry = &map->buckets[bucket].firstChild; *ptrEntry != NULL; ptrEntry = &(*ptrEntry)->nextSibling) { \
			/* Check */ \
			if (mapName##KeyCompareFunction((*ptrEntry)->key, key)) { \
				/* Get values */ \
				deletionEntry = (*ptrEntry); \
				if (data != NULL) *data = (*ptrEntry)->data; \
				\
				/* Change linking */ \
				if (map->buckets[bucket].ptrToNext == &(*ptrEntry)->nextSibling) { \
					map->buckets[bucket].ptrToNext = ptrEntry; \
				} \
				(*ptrEntry) = (*ptrEntry)->nextSibling; \
				\
				/* Delete */ \
				memFree(deletionEntry); \
				\
				/* Return value */ \
				return MAP_FOUND; \
			} \
		} \
		\
		/* Done */ \
		return MAP_NOT_FOUND; \
	} \
	\
	MapStatus \
	mapName##Find(const MapType* map, const key_type key, data_type* data) { \
		/* Vars */ \
		MapHashValue bucket; \
		struct MapType##Entry_* entry; \
		\
		/* Assertions */ \
		assert(map != NULL); \
		assert(key != NULL); \
		assert(mapName##KeyCompareFunction(key, key)); \
		\
		/* Find hash/bucket */ \
		bucket = mapName##KeyHashFunction(key) % map->bucketCount; \
		\
		/* Find */ \
		for (entry = map->buckets[bucket].firstChild; entry != NULL; entry = entry->nextSibling) { \
			/* Check */ \
			if (mapName##KeyCompareFunction(entry->key, key)) { \
				/* Get values */ \
				if (data != NULL) *data = entry->data; \
				\
				/* Return value */ \
				return MAP_FOUND; \
			} \
		} \
		\
		/* Done */ \
		return MAP_NOT_FOUND; \
	} \



#endif


