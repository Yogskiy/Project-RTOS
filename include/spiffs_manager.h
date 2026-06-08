#ifndef SPIFFS_H
#define SPIFFS_H

#include <stdint.h>
#include "data_structures.h"

// ============ SPIFFS INITIALIZATION ============

/**
 * @brief Initialize SPIFFS file system
 * 
 * @return 0 if success, -1 if failed
 */
int initSPIFFS();

/**
 * @brief Check if SPIFFS is mounted
 * 
 * @return 1 if mounted, 0 if not
 */
int isSPIFFSMounted();

// ============ UID DATABASE FILE OPERATIONS ============

/**
 * @brief Load UID database from SPIFFS JSON file
 * 
 * @param db Pointer to UIDDatabase structure to fill
 * @param filepath Path to JSON file (e.g., "/spiffs/uids.json")
 * @return 0 if success, -1 if file not found or parse error
 */
int loadUIDsFromFile(UIDDatabase *db, const char *filepath);

/**
 * @brief Save UID database to SPIFFS JSON file
 * 
 * @param db Pointer to UIDDatabase structure
 * @param filepath Path to JSON file
 * @return 0 if success, -1 if write error
 */
int saveUIDsToFile(UIDDatabase *db, const char *filepath);

/**
 * @brief Add new UID entry to SPIFFS database
 * Loads existing entries, adds new one, saves back
 * 
 * @param entry Pointer to UIDEntry to add
 * @param filepath Path to JSON file
 * @return 0 if success, -1 if error
 */
int addUIDToFile(UIDEntry *entry, const char *filepath);

/**
 * @brief Delete UID entry from SPIFFS database
 * 
 * @param uid_hex UID as hex string (e.g., "12345678")
 * @param filepath Path to JSON file
 * @return 0 if success, -1 if UID not found
 */
int deleteUIDFromFile(const char *uid_hex, const char *filepath);

/**
 * @brief Check if UID exists in file
 * 
 * @param uid_hex UID as hex string
 * @param filepath Path to JSON file
 * @return 1 if exists, 0 if not found
 */
int uidExistsInFile(const char *uid_hex, const char *filepath);

// ============ SPIFFS FILE UTILITIES ============

/**
 * @brief Check if file exists
 * 
 * @param filepath File path
 * @return 1 if exists, 0 if not
 */
int fileExists(const char *filepath);

/**
 * @brief Delete file from SPIFFS
 * 
 * @param filepath File path
 * @return 0 if success, -1 if error
 */
int deleteFile(const char *filepath);

/**
 * @brief Get file size
 * 
 * @param filepath File path
 * @return File size in bytes, -1 if error
 */
int getFileSize(const char *filepath);

/**
 * @brief List all files in SPIFFS
 * Prints to Serial
 */
void listSPIFFSFiles();

/**
 * @brief Get SPIFFS total and used space
 * 
 * @param total_bytes Output: total SPIFFS size
 * @param used_bytes Output: used space
 * @return 0 if success
 */
int getSPIFFSInfo(uint32_t *total_bytes, uint32_t *used_bytes);

#endif // SPIFFS_H
