#ifndef VERIFICATION_H
#define VERIFICATION_H

#include "data_structs.h"

int generate_manifest_multi(const char *manifest_path);
int verify_mirror(const char *mirror_dir, const char *manifest_path);

#endif // VERIFICATION_H