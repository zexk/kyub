#include "world.h"
#include "logger.h"
#include "components.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#define MAX_RENDER_DISTANCE 8
#define WORLD_SAVE_DIR "saves/default"
#define CHUNK_SAVE_MAGIC "KYUBCHNK"
#define CHUNK_SAVE_MAJOR 1
#define CHUNK_SAVE_MINOR 0
#define CHUNK_VOLUME (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)

typedef struct {
    char id[64];
    BlockType type;
} SavePaletteEntry;

static void write_u16(FILE *f, uint16_t v) {
    fputc((int)(v & 0xff), f);
    fputc((int)((v >> 8) & 0xff), f);
}

static void write_u32(FILE *f, uint32_t v) {
    write_u16(f, (uint16_t)(v & 0xffff));
    write_u16(f, (uint16_t)((v >> 16) & 0xffff));
}

static void write_i32(FILE *f, int32_t v) {
    write_u32(f, (uint32_t)v);
}

static bool read_u16(FILE *f, uint16_t *out) {
    int b0 = fgetc(f);
    int b1 = fgetc(f);
    if (b0 == EOF || b1 == EOF) return false;
    *out = (uint16_t)((uint16_t)b0 | ((uint16_t)b1 << 8));
    return true;
}

static bool read_u32(FILE *f, uint32_t *out) {
    uint16_t lo, hi;
    if (!read_u16(f, &lo) || !read_u16(f, &hi)) return false;
    *out = (uint32_t)lo | ((uint32_t)hi << 16);
    return true;
}

static bool read_i32(FILE *f, int32_t *out) {
    uint32_t v;
    if (!read_u32(f, &v)) return false;
    *out = (int32_t)v;
    return true;
}

static void ensure_save_dirs(void) {
    if (mkdir("saves", 0755) != 0 && errno != EEXIST) {
        LOG_WARN(CAT_WORLD, "Failed to create saves directory: %s", strerror(errno));
    }
    if (mkdir(WORLD_SAVE_DIR, 0755) != 0 && errno != EEXIST) {
        LOG_WARN(CAT_WORLD, "Failed to create world save directory: %s", strerror(errno));
    }
}

static void chunk_save_path(char *out, size_t out_size, int x, int z) {
    snprintf(out, out_size, "%s/chunk_%d_%d.kch", WORLD_SAVE_DIR, x, z);
}

static const char *block_id_for_type(BlockType type) {
    Entity e = g_block_entities[type];
    C_BlockDef *def = e ? ecs_get(&g_ecs, e, COMP_BLOCK_DEF) : NULL;
    return (def && def->id) ? def->id : "kyub:air";
}

static BlockType block_type_for_id(const char *id) {
    for (int t = 0; t < 256; t++) {
        Entity e = g_block_entities[t];
        C_BlockDef *def = e ? ecs_get(&g_ecs, e, COMP_BLOCK_DEF) : NULL;
        if (def && def->id && strcmp(def->id, id) == 0) return (BlockType)t;
    }
    return BLOCK_AIR;
}

static uint16_t palette_index_for_id(const char **ids, uint16_t *count, const char *id) {
    for (uint16_t i = 0; i < *count; i++) {
        if (strcmp(ids[i], id) == 0) return i;
    }
    if (*count >= 256) return 0;
    ids[*count] = id;
    return (*count)++;
}

static bool save_chunk_data(const LoadedChunk *lc) {
    if (!lc || !lc->active || !lc->chunk) return false;
    ensure_save_dirs();

    const Chunk *chunk = lc->chunk;
    char path[256];
    char tmp_path[280];
    chunk_save_path(path, sizeof(path), chunk->x, chunk->z);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        LOG_WARN(CAT_WORLD, "Failed to open chunk save %s: %s", tmp_path, strerror(errno));
        return false;
    }

    const char *palette[256];
    uint16_t palette_count = 0;
    uint16_t block_indices[CHUNK_VOLUME];
    int n = 0;
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                const char *id = block_id_for_type((BlockType)chunk->blocks[x][y][z]);
                block_indices[n++] = palette_index_for_id(palette, &palette_count, id);
            }
        }
    }

    fwrite(CHUNK_SAVE_MAGIC, 1, 8, f);
    write_u16(f, CHUNK_SAVE_MAJOR);
    write_u16(f, CHUNK_SAVE_MINOR);
    write_i32(f, chunk->x);
    write_i32(f, chunk->z);
    write_u32(f, 2); /* PLTE, BLKS */

    uint32_t palette_size = 2;
    for (uint16_t i = 0; i < palette_count; i++) {
        palette_size += 2 + (uint32_t)strlen(palette[i]);
    }
    fwrite("PLTE", 1, 4, f);
    write_u32(f, palette_size);
    write_u16(f, palette_count);
    for (uint16_t i = 0; i < palette_count; i++) {
        uint16_t len = (uint16_t)strlen(palette[i]);
        write_u16(f, len);
        fwrite(palette[i], 1, len, f);
    }

    fwrite("BLKS", 1, 4, f);
    write_u32(f, 7 + CHUNK_VOLUME * 2);
    write_u16(f, CHUNK_SIZE);
    write_u16(f, CHUNK_SIZE);
    write_u16(f, CHUNK_SIZE);
    fputc(0, f); /* raw u16 palette indices */
    for (int i = 0; i < CHUNK_VOLUME; i++) write_u16(f, block_indices[i]);

    bool ok = ferror(f) == 0;
    if (fclose(f) != 0) ok = false;
    if (!ok) {
        remove(tmp_path);
        LOG_WARN(CAT_WORLD, "Failed while writing chunk save %s", tmp_path);
        return false;
    }
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        LOG_WARN(CAT_WORLD, "Failed to replace chunk save %s: %s", path, strerror(errno));
        return false;
    }
    LOG_DEBUG(CAT_WORLD, "Saved chunk %d,%d", chunk->x, chunk->z);
    return true;
}

static bool load_chunk_data(Chunk *chunk) {
    char path[256];
    chunk_save_path(path, sizeof(path), chunk->x, chunk->z);

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    char magic[8];
    uint16_t major, minor;
    int32_t file_x, file_z;
    uint32_t section_count;
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, CHUNK_SAVE_MAGIC, 8) != 0 ||
        !read_u16(f, &major) || !read_u16(f, &minor) ||
        !read_i32(f, &file_x) || !read_i32(f, &file_z) || !read_u32(f, &section_count)) {
        LOG_WARN(CAT_WORLD, "Ignoring invalid chunk save: %s", path);
        fclose(f);
        return false;
    }
    (void)minor;

    if (major > CHUNK_SAVE_MAJOR) {
        LOG_WARN(CAT_WORLD, "Ignoring newer chunk save %s version %u", path, major);
        fclose(f);
        return false;
    }
    if (file_x != chunk->x || file_z != chunk->z) {
        LOG_WARN(CAT_WORLD, "Ignoring chunk save %s with mismatched coordinates", path);
        fclose(f);
        return false;
    }

    SavePaletteEntry palette[256];
    uint16_t palette_count = 0;
    bool loaded_blocks = false;
    for (uint16_t i = 0; i < 256; i++) {
        palette[i].id[0] = '\0';
        palette[i].type = BLOCK_AIR;
    }

    for (uint32_t s = 0; s < section_count; s++) {
        char type[4];
        uint32_t size;
        long payload_start;
        if (fread(type, 1, 4, f) != 4 || !read_u32(f, &size)) break;
        payload_start = ftell(f);

        if (memcmp(type, "PLTE", 4) == 0) {
            uint16_t count;
            uint16_t loaded_count = 0;
            if (!read_u16(f, &count)) break;
            if (count > 256) count = 256;
            for (uint16_t i = 0; i < count; i++) {
                uint16_t file_len;
                uint16_t copy_len;
                if (!read_u16(f, &file_len)) break;
                copy_len = file_len;
                if (copy_len >= sizeof(palette[i].id)) copy_len = sizeof(palette[i].id) - 1;
                if (fread(palette[i].id, 1, copy_len, f) != copy_len) break;
                if (file_len > copy_len && fseek(f, (long)(file_len - copy_len), SEEK_CUR) != 0) break;
                palette[i].id[copy_len] = '\0';
                palette[i].type = block_type_for_id(palette[i].id);
                if (palette[i].type == BLOCK_AIR && strcmp(palette[i].id, "kyub:air") != 0) {
                    LOG_WARN(CAT_WORLD, "Unknown block id in save, mapping to air: %s", palette[i].id);
                }
                loaded_count++;
            }
            palette_count = loaded_count;
        } else if (memcmp(type, "BLKS", 4) == 0) {
            uint16_t sx, sy, sz;
            int encoding;
            if (!read_u16(f, &sx) || !read_u16(f, &sy) || !read_u16(f, &sz)) break;
            encoding = fgetc(f);
            if (sx == CHUNK_SIZE && sy == CHUNK_SIZE && sz == CHUNK_SIZE && encoding == 0) {
                for (int x = 0; x < CHUNK_SIZE; x++) {
                    for (int y = 0; y < CHUNK_SIZE; y++) {
                        for (int z = 0; z < CHUNK_SIZE; z++) {
                            uint16_t idx;
                            if (!read_u16(f, &idx)) goto done;
                            chunk->blocks[x][y][z] = (idx < palette_count) ? palette[idx].type : BLOCK_AIR;
                        }
                    }
                }
                loaded_blocks = true;
            }
        }

        if (fseek(f, payload_start + (long)size, SEEK_SET) != 0) break;
    }

done:
    fclose(f);
    if (loaded_blocks) {
        LOG_DEBUG(CAT_WORLD, "Loaded saved chunk %d,%d", chunk->x, chunk->z);
    }
    return loaded_blocks;
}

void world_init(World *world, int render_distance) {
    if (render_distance < 1) render_distance = 1;
    if (render_distance > MAX_RENDER_DISTANCE) render_distance = MAX_RENDER_DISTANCE;
    world->render_distance = render_distance;
    world->capacity = (2 * MAX_RENDER_DISTANCE + 1) * (2 * MAX_RENDER_DISTANCE + 1);
    world->chunks = calloc(world->capacity, sizeof(LoadedChunk));
    world->count = 0;
    ensure_save_dirs();
#ifdef ENABLE_COMPUTE
    world->mesh_compute_program = renderer_create_compute("build/shaders/mesh");
#endif
}

static int world_chunk_coord(int x) {
    if (x >= 0) return x / CHUNK_SIZE;
    return -((-x + CHUNK_SIZE - 1) / CHUNK_SIZE);
}

static bool chunk_is_loaded(const World *world, int x, int z) {
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active && world->chunks[i].chunk->x == x && world->chunks[i].chunk->z == z)
            return true;
    }
    return false;
}

static void load_chunk(World *world, int x, int z) {
    for (int i = 0; i < world->capacity; i++) {
        if (!world->chunks[i].active) {
            world->chunks[i].chunk = malloc(sizeof(Chunk));
            chunk_init(world->chunks[i].chunk, x, z);
            load_chunk_data(world->chunks[i].chunk);
            world->chunks[i].mesh = malloc(sizeof(Mesh));
            mesh_init(world->chunks[i].mesh);
            world->chunks[i].voxel_tex = R_INVALID_HANDLE;
            voxel_upload_texture(&world->chunks[i].voxel_tex, world->chunks[i].chunk);
            mesh_generate_greedy(world->chunks[i].mesh, world->chunks[i].chunk);
            mesh_upload(world->chunks[i].mesh);
            world->chunks[i].active = true;
            world->chunks[i].dirty = false;
            world->chunks[i].save_dirty = false;
            world->count++;
            LOG_DEBUG(CAT_WORLD, "Loaded chunk %d,%d (slot %d)", x, z, i);
            return;
        }
    }
    LOG_WARN(CAT_WORLD, "load_chunk FAILED: no free slot for chunk %d,%d (active=%d, capacity=%d)", x, z, world->count, world->capacity);
}

static void unload_chunk(World *world, int index) {
    if (!world->chunks[index].active) return;
    int cx = world->chunks[index].chunk->x;
    int cz = world->chunks[index].chunk->z;
    if (world->chunks[index].save_dirty && save_chunk_data(&world->chunks[index])) {
        world->chunks[index].save_dirty = false;
    }
    mesh_free(world->chunks[index].mesh);
    free(world->chunks[index].mesh);
    free(world->chunks[index].chunk);
    if (world->chunks[index].voxel_tex != R_INVALID_HANDLE) renderer_destroy_texture(world->chunks[index].voxel_tex);
    world->chunks[index].active = false;
    world->count--;
    LOG_DEBUG(CAT_WORLD, "Unloaded chunk %d,%d", cx, cz);
}

void world_flush_saves(World *world) {
    if (!world) return;
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active && world->chunks[i].save_dirty) {
            if (save_chunk_data(&world->chunks[i])) {
                world->chunks[i].save_dirty = false;
            }
        }
    }
}

void world_update(World *world, vec3 camera_pos) {
    int cx = world_chunk_coord((int)floorf(camera_pos.x));
    int cz = world_chunk_coord((int)floorf(camera_pos.z));

    // Load new chunks (+1 buffer for interaction range at chunk boundaries)
    int load_dist = world->render_distance + 1;
    for (int x = cx - load_dist; x <= cx + load_dist; x++) {
        for (int z = cz - load_dist; z <= cz + load_dist; z++) {
            if (!chunk_is_loaded(world, x, z)) {
                load_chunk(world, x, z);
            }
        }
    }

    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active) {
            int dx = abs(world->chunks[i].chunk->x - cx);
            int dz = abs(world->chunks[i].chunk->z - cz);
            if (dx > world->render_distance + 2 || dz > world->render_distance + 2) {
                unload_chunk(world, i);
            }
        }
    }

    // Rebuild dirty chunks (deferred mesh update)
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active && world->chunks[i].dirty) {
            mesh_generate_greedy(world->chunks[i].mesh, world->chunks[i].chunk);
            mesh_upload(world->chunks[i].mesh);
            world->chunks[i].dirty = false;
        }
    }
}

void world_free(World *world) {
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active) unload_chunk(world, i);
    }
    free(world->chunks);
}

static LoadedChunk* find_chunk(const World *world, int cx, int cz) {
    for (int i = 0; i < world->capacity; i++) {
        if (world->chunks[i].active && world->chunks[i].chunk->x == cx && world->chunks[i].chunk->z == cz)
            return &world->chunks[i];
    }
    return NULL;
}

BlockType world_get_block(const World *world, int x, int y, int z) {
    if (y < 0 || y >= CHUNK_SIZE) return BLOCK_AIR;
    int cx = world_chunk_coord(x);
    int cz = world_chunk_coord(z);
    LoadedChunk *lc = find_chunk(world, cx, cz);
    if (!lc) {
        LOG_WARN(CAT_WORLD, "get_block: chunk %d,%d not loaded for block %d,%d,%d", cx, cz, x, y, z);
        return BLOCK_AIR;
    }
    int lx = x - cx * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;
    return lc->chunk->blocks[lx][y][lz];
}

bool world_is_solid(const World *world, int x, int y, int z) {
    return world_get_block(world, x, y, z) != BLOCK_AIR;
}

void world_set_block(World *world, int x, int y, int z, BlockType type) {
    if (y < 0 || y >= CHUNK_SIZE) return;
    int cx = world_chunk_coord(x);
    int cz = world_chunk_coord(z);
    LoadedChunk *lc = find_chunk(world, cx, cz);
    if (!lc) {
        if (world->count < world->capacity) {
            load_chunk(world, cx, cz);
            lc = find_chunk(world, cx, cz);
        }
    }
    if (!lc) {
        LOG_WARN(CAT_WORLD, "set_block: chunk %d,%d not loaded for block %d,%d,%d", cx, cz, x, y, z);
        return;
    }
    int lx = x - cx * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;
    lc->chunk->blocks[lx][y][lz] = type;
    lc->dirty = true;
    lc->save_dirty = true;
}
