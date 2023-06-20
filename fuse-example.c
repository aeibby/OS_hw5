#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <json-c/json.h>
#include <pthread.h>

#define MAX_FILES 128
#define MAX_ENTRIES 16
#define MAX_FILE_SIZE 4098

typedef struct {
    int inode;
    char type[4];
    char data[MAX_FILE_SIZE];
} File;

typedef struct {
    int inode;
    char name[256];
    int parent_inode;
} DirectoryEntry;

typedef struct {
    int inode;
    char type[4];
    DirectoryEntry entries[MAX_ENTRIES];
    int num_entries;
} Directory;

typedef struct {
    File files[MAX_FILES];
    int num_files;
    pthread_mutex_t lock;
} FileSystem;

static FileSystem fileSystem;

static void initializeFileSystem(struct json_object *fs_json) {
    fileSystem.num_files = 0;
    pthread_mutex_init(&fileSystem.lock, NULL);

    int n = json_object_array_length(fs_json);
    for (int i = 0; i < n; i++) {
        struct json_object *obj = json_object_array_get_idx(fs_json, i);

        json_object *inode_obj;
        if (json_object_object_get_ex(obj, "inode", &inode_obj)) {
            int inode = json_object_get_int(inode_obj);

            json_object *type_obj;
            if (json_object_object_get_ex(obj, "type", &type_obj)) {
                const char *type = json_object_get_string(type_obj);

                if (strcmp(type, "reg") == 0) {
                    File file;
                    file.inode = inode;
                    strcpy(file.type, type);

                    json_object *data_obj;
                    if (json_object_object_get_ex(obj, "data", &data_obj)) {
                        const char *data = json_object_get_string(data_obj);
                        strcpy(file.data, data);
                    }

                    pthread_mutex_lock(&fileSystem.lock);
                    fileSystem.files[fileSystem.num_files++] = file;
                    pthread_mutex_unlock(&fileSystem.lock);
                } else if (strcmp(type, "dir") == 0) {
                    Directory directory;
                    directory.inode = inode;
                    strcpy(directory.type, type);
                    directory.num_entries = 0;

                    json_object *entries_obj;
                    if (json_object_object_get_ex(obj, "entries", &entries_obj)) {
                        int num_entries = json_object_array_length(entries_obj);
                        for (int j = 0; j < num_entries; j++) {
                            json_object *entry_obj = json_object_array_get_idx(entries_obj, j);

                            json_object *name_obj = json_object_object_get(entry_obj, "name");
                            const char *name = json_object_get_string(name_obj);

                            json_object *entry_inode_obj = json_object_object_get(entry_obj, "inode");
                            int entry_inode = json_object_get_int(entry_inode_obj);

                            DirectoryEntry entry;
                            entry.inode = entry_inode;
                            strcpy(entry.name, name);
                            entry.parent_inode = inode;

                            pthread_mutex_lock(&fileSystem.lock);
                            directory.entries[directory.num_entries++] = entry;
                            pthread_mutex_unlock(&fileSystem.lock);
                        }
                    }

                    pthread_mutex_lock(&fileSystem.lock);
                    fileSystem.files[fileSystem.num_files++] = *(File *) &directory;
                    pthread_mutex_unlock(&fileSystem.lock);
                }
            }
        }
    }
}

static int getattr_callback(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    pthread_mutex_lock(&fileSystem.lock);
    for (int i = 0; i < fileSystem.num_files; i++) {
        if (strcmp(path + 1, fileSystem.files[i].data) == 0) {
            if (strcmp(fileSystem.files[i].type, "reg") == 0) {
                stbuf->st_mode = S_IFREG | 0777;
                stbuf->st_nlink = 1;
                stbuf->st_size = strlen(fileSystem.files[i].data);
                pthread_mutex_unlock(&fileSystem.lock);
                return 0;
            } else if (strcmp(fileSystem.files[i].type, "dir") == 0) {
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
                pthread_mutex_unlock(&fileSystem.lock);
                return 0;
            }
        }
    }
    pthread_mutex_unlock(&fileSystem.lock);

    return -ENOENT;
}

static int readdir_callback(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi) {

    if (strcmp(path, "/") == 0) {
        pthread_mutex_lock(&fileSystem.lock);
        for (int i = 0; i < fileSystem.num_files; i++) {
            if (fileSystem.files[i].inode != 0 && fileSystem.files[i].inode != 1) {
                filler(buf, fileSystem.files[i].data, NULL, 0);
            }
        }
        pthread_mutex_unlock(&fileSystem.lock);
        return 0;
    }

    return -ENOENT;
}

static int open_callback(const char *path, struct fuse_file_info *fi) {
    return 0;
}

static int read_callback(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi) {

    pthread_mutex_lock(&fileSystem.lock);
    for (int i = 0; i < fileSystem.num_files; i++) {
        if (strcmp(path + 1, fileSystem.files[i].data) == 0) {
            if (strcmp(fileSystem.files[i].type, "reg") == 0) {
                size_t len = strlen(fileSystem.files[i].data);
                if (offset >= len) {
                    pthread_mutex_unlock(&fileSystem.lock);
                    return 0;
                }

                if (offset + size > len) {
                    memcpy(buf, fileSystem.files[i].data + offset, len - offset);
                    pthread_mutex_unlock(&fileSystem.lock);
                    return len - offset;
                }

                memcpy(buf, fileSystem.files[i].data + offset, size);
                pthread_mutex_unlock(&fileSystem.lock);
                return size;
            }
        }
    }
    pthread_mutex_unlock(&fileSystem.lock);

    return -ENOENT;
}

static int write_callback(const char *path, const char *buf, size_t size, off_t offset,
                          struct fuse_file_info *fi) {

    pthread_mutex_lock(&fileSystem.lock);
    for (int i = 0; i < fileSystem.num_files; i++) {
        if (strcmp(path + 1, fileSystem.files[i].data) == 0) {
            if (strcmp(fileSystem.files[i].type, "reg") == 0) {
                size_t len = strlen(fileSystem.files[i].data);
                if (offset + size > MAX_FILE_SIZE) {
                    pthread_mutex_unlock(&fileSystem.lock);
                    return -EFBIG; // File too large
                }

                memcpy(fileSystem.files[i].data + offset, buf, size);
                if (offset + size > len) {
                    fileSystem.files[i].data[offset + size] = '\0';
                }
                pthread_mutex_unlock(&fileSystem.lock);
                return size;
            }
        }
    }
    pthread_mutex_unlock(&fileSystem.lock);

    return -ENOENT;
}

static int create_callback(const char *path, mode_t mode, struct fuse_file_info *fi) {
    pthread_mutex_lock(&fileSystem.lock);
    for (int i = 0; i < fileSystem.num_files; i++) {
        if (fileSystem.files[i].inode == 0) {
            File file;
            file.inode = fileSystem.num_files;
            strcpy(file.type, "reg");
            strcpy(file.data, path + 1);

            fileSystem.files[fileSystem.num_files++] = file;
            pthread_mutex_unlock(&fileSystem.lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&fileSystem.lock);

    return -ENOSPC; // No space left on device
}

static int mkdir_callback(const char *path, mode_t mode) {
    pthread_mutex_lock(&fileSystem.lock);
    for (int i = 0; i < fileSystem.num_files; i++) {
        if (fileSystem.files[i].inode == 0) {
            Directory directory;
            directory.inode = fileSystem.num_files;
            strcpy(directory.type, "dir");
            directory.num_entries = 0;

            DirectoryEntry entry;
            entry.inode = fileSystem.num_files + 1;
            strcpy(entry.name, path + 1);
            entry.parent_inode = 0;

            directory.entries[directory.num_entries++] = entry;

            fileSystem.files[fileSystem.num_files++] = *(File *) &directory;
            pthread_mutex_unlock(&fileSystem.lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&fileSystem.lock);

    return -ENOSPC; // No space left on device
}

static int unlink_callback(const char *path) {
    pthread_mutex_lock(&fileSystem.lock);
    for (int i = 0; i < fileSystem.num_files; i++) {
        if (strcmp(path + 1, fileSystem.files[i].data) == 0) {
            if (strcmp(fileSystem.files[i].type, "reg") == 0) {
                fileSystem.num_files--;
                for (int j = i; j < fileSystem.num_files; j++) {
                    fileSystem.files[j] = fileSystem.files[j + 1];
                }
                pthread_mutex_unlock(&fileSystem.lock);
                return 0;
            } else if (strcmp(fileSystem.files[i].type, "dir") == 0) {
                int parent_inode = fileSystem.files[i].inode;
                for (int j = 0; j < fileSystem.num_files; j++) {
                    if (strcmp(fileSystem.files[j].type, "dir") == 0) {
                        Directory *directory = (Directory *) &fileSystem.files[j];
                        for (int k = 0; k < directory->num_entries; k++) {
                            if (directory->entries[k].inode == fileSystem.files[i].inode) {
                                for (int l = k; l < directory->num_entries - 1; l++) {
                                    directory->entries[l] = directory->entries[l + 1];
                                }
                                directory->num_entries--;
                                break;
                            }
                        }
                    }
                }

                fileSystem.num_files--;
                for (int j = i; j < fileSystem.num_files; j++) {
                    fileSystem.files[j] = fileSystem.files[j + 1];
                }

                pthread_mutex_unlock(&fileSystem.lock);
                return 0;
            }
        }
    }
    pthread_mutex_unlock(&fileSystem.lock);

    return -ENOENT;
}

static struct fuse_operations operations = {
    .getattr = getattr_callback,
    .readdir = readdir_callback,
    .open = open_callback,
    .read = read_callback,
    .write = write_callback,
    .create = create_callback,
    .mkdir = mkdir_callback,
    .unlink = unlink_callback,
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mount_path> <fs_json_path>\n", argv[0]);
        return 1;
    }

    struct json_object *fs_json = json_object_from_file(argv[2]);
    if (fs_json == NULL) {
        fprintf(stderr, "Error: Failed to read the filesystem JSON file.\n");
        return 1;
    }

    initializeFileSystem(fs_json);

    return fuse_main(argc, argv, &operations, NULL);
}
