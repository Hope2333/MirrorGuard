#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <openssl/evp.h>
#include <sys/stat.h>
#include "data_structs.h"

int compute_sha256(const char *file_path, char *hash_str);
FileStatus verify_file(const char *mirror_dir, const char *rel_path, const char *expected_hash);

#endif // FILE_UTILS_H