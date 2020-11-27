#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <cosec/log.h>

#include "conf.h"
#include "mem/kheap.h"
#include "mem/pmem.h"
#include "fs/ramfs.h"

typedef void (*btree_leaf_free_f)(void *);

/* this structure is a "hierarchical" lookup table from any large index to some pointer */
struct btree_node {
    int     bt_level;       /* if 0, bt_children are leaves */
    size_t  bt_fanout;      /* how many children for a node */
    size_t  bt_used;        /* how many non-NULL children are there */
    void *  bt_children[0]; /* child BTree nodes or leaves */
};

/* mallocs and initializes a btree_node with bt_level = 0 */
static struct btree_node * btree_new(size_t fanout);

/* frees a bnode and all its children */
static void btree_free(struct btree_node *bnode, btree_leaf_free_f free_leaf);

/* look up a value by index */
static void * btree_get_index(struct btree_node *bnode, size_t index);

/* sets an existing index to `idata` */
static inode_t btree_set_leaf(struct btree_node *bnode, struct inode *idata);


static struct btree_node * btree_new(size_t fanout) {
    size_t bchildren_len = sizeof(void *) * fanout;
    struct btree_node *bnode = kmalloc(sizeof(struct btree_node) + bchildren_len);
    if (!bnode) return NULL;

    bnode->bt_level = 0;
    bnode->bt_used = 0;
    bnode->bt_fanout = fanout;
    memset(&(bnode->bt_children), 0, bchildren_len);

    logmsgdf("btree_new(%d) -> *%x\n", fanout, (uint)bnode);
    return bnode;
}

static void btree_free(struct btree_node *bnode, btree_leaf_free_f free_leaf) {
    size_t i;
    for (i = 0; i < bnode->bt_fanout; ++i) {
        if (bnode->bt_level == 0) {
            struct inode *idata = bnode->bt_children[i];
            if (idata && (idata != &theInvalidInode))
                if (free_leaf) free_leaf(idata);
        } else {
            struct btree_node *bchild = bnode->bt_children[i];
            if (bchild)
                btree_free(bchild, free_leaf);
        }
    }
}

/* get leaf or NULL for index */
static void * btree_get_index(struct btree_node *bnode, size_t index) {
    int i;
    int btree_lvl = bnode->bt_level;
    size_t fanout = bnode->bt_fanout;
    size_t btsize = fanout;

    for (i = 0; i < btree_lvl; ++i)
        btsize *= fanout;

    if (btsize <= index)
        return NULL;

    /* get btree item */
    while (btree_lvl-- > 0) {
        btsize /= fanout;
        size_t this_node_index = index / btsize;
        index %= btsize;

        struct btree_node *subnode = bnode->bt_children[ this_node_index ];
        if (!subnode) return NULL;
        bnode = subnode;
    }
    return bnode->bt_children[ index ];
}

/*
 *      Searches B-tree for a free leaf,
 *      returns 0 if no free leaves found.
 *      0 index must be taken by an "invalid" entry.
 */
static inode_t btree_set_leaf(struct btree_node *bnode, struct inode *idata) {
    const char *funcname = "btree_set_leaf";

    size_t i;
    size_t fanout = bnode->bt_fanout;
    if (bnode->bt_level == 0) {
        if (bnode->bt_used < fanout) {
            for (i = 0; i < fanout; ++i)
                if (bnode->bt_children[i] == NULL) {
                    ++bnode->bt_used;
                    bnode->bt_children[i] = idata;

                    logmsgdf("%s(%d) to *%x, bnode=*%x, bt_used=%d\n",
                             funcname, i, (uint)idata, (uint)bnode, bnode->bt_used);
                    return i;
                }
        } else {
            /* no free leaves here */
            logmsgdf("no free leaves here\n");
            return 0;
        }
    } else {
        /* search in subnodes */
        for (i = 0; i < fanout; ++i) {
            struct btree_node *bchild = bnode->bt_children[i];
            if (!bchild) continue;

            int res = btree_set_leaf(bchild, idata);
            if (res > 0) {
                int j = bnode->bt_level;
                size_t index_offset = 1;
                while (j-- > 0)
                    index_offset *= fanout;
                res += i * index_offset;
                return res;
            }
        }
    }
    return 0;
}

/*
 *      Searches for a place for a new leaf with btree_set_leaf()
 *      if no free leaves:
 *          grows up a node level,
 *          old root becomes new_root[0],
 *          grows down to level 0 from new_root[1] and inserts idata there.
 *          *btree_root redirected to new_root.
 *
 *      Do not use with an empty btree, since index 0 must be invalid!
 */
static inode_t btree_new_leaf(struct btree_node **btree_root, struct inode *idata) {
    logmsgdf("btree_new_leaf(idata=*%x)\n", (uint)idata);

    int res = btree_set_leaf(*btree_root, idata);
    if (res > 0) return res;

    logmsgdf("btree_new_leaf: adding a new level\n");
    struct btree_node *old_root = *btree_root;
    int lvl = old_root->bt_level + 1;
    size_t fanout = old_root->bt_fanout;

    struct btree_node *new_root = NULL;
    struct btree_node *new_node = NULL;

    new_root = btree_new(fanout);
    if (!new_root) return 0;
    new_root->bt_level = old_root->bt_level + 1;

    new_root->bt_children[0] = old_root;
    ++new_root->bt_used;

    old_root = NULL;
    res = 1;
    while (lvl-- > 0) {
        new_node = btree_new(fanout);
        if (!new_root) {
            /* unwind new_root[1] and free all its subnodes */
            if (new_root->bt_children[1])
                /* newly created bnode do not need free_leaf() */
                btree_free(new_root->bt_children[1], NULL);
            return 0;
        }

        if (old_root) {
            old_root->bt_children[0] = new_node;
            ++old_root->bt_used;
        } else {
            new_root->bt_children[1] = new_node;
            ++new_root->bt_used;
        }
        old_root = new_node;
        res *= fanout;
    }

    *btree_root = new_root;
    return res;
}

static int btree_free_leaf(struct btree_node *bnode, inode_t index) {
    /* find the 0 level bnode */
    /* set index to NULL */
    /* if bt_used drops to 0, delete the bnode */
    return ETODO;
}


/*
 *  ramfs
 */

static int ramfs_read_superblock(mountnode *sb);
static int ramfs_lookup_inode(mountnode *sb, inode_t *ino, const char *path, size_t pathlen);
static int ramfs_make_directory(mountnode *sb, inode_t *ino, const char *path, mode_t mode);
static int ramfs_get_direntry(mountnode *sb, inode_t dirnode, void **iter, struct dirent *dirent);
static int ramfs_make_node(mountnode *sb, inode_t *ino, mode_t mode, void *info);
static int ramfs_free_inode(mountnode *sb, inode_t ino);
static int ramfs_link_inode(mountnode *sb, inode_t ino, inode_t dirino, const char *name, size_t namelen);
static int ramfs_unlink_inode(mountnode *sb, const char *path, size_t pathlen);
static int ramfs_inode_get(mountnode *sb, inode_t ino, struct inode *idata);
static int ramfs_inode_set(mountnode *sb, inode_t ino, struct inode *idata);
static int ramfs_read_inode(mountnode *sb, inode_t ino, off_t pos,
                            char *buf, size_t buflen, size_t *written);
static int ramfs_write_inode(mountnode *sb, inode_t ino, off_t pos,
                             const char *buf, size_t buflen, size_t *written);
static int ramfs_trunc_inode(/*mountnode *sb, inode_t ino, off_t length*/);

static void ramfs_inode_free(struct inode *idata);
static void ramfs_free_inode_blocks(struct inode *idata);


struct filesystem_operations  ramfs_fsops = {
    .read_superblock    = ramfs_read_superblock,
    .lookup_inode       = ramfs_lookup_inode,
    .make_directory     = ramfs_make_directory,
    .get_direntry       = ramfs_get_direntry,
    .make_inode         = ramfs_make_node,
    .link_inode         = ramfs_link_inode,
    .unlink_inode       = ramfs_unlink_inode,
    .inode_get          = ramfs_inode_get,
    .inode_set          = ramfs_inode_set,
    .read_inode         = ramfs_read_inode,
    .write_inode        = ramfs_write_inode,
    .trunc_inode        = ramfs_trunc_inode,
};

struct filesystem_driver  ramfs_driver = {
    .name = "ramfs",
    .fs_id = RAMFS_ID,
    .ops = &ramfs_fsops,
    .lst = { 0 },
};

fsdriver * ramfs_fs_driver(void) {
    return &ramfs_driver;
}



/* this is a directory hashtable array entry */
/* it is an intrusive list */
struct ramfs_direntry {
    uint32_t  de_hash;
    char     *de_name;          /* its length should be a power of 2 */
    inode_t   de_ino;           /* the actual value, inode_t */

    struct ramfs_direntry *htnext;  /* next in hashtable collision list */
};

/* this is a container for directory hashtable */
struct ramfs_directory {
    size_t size;                /* number of values in the hashtable */
    size_t htcap;               /* hashtable capacity */
    struct ramfs_direntry **ht; /* hashtable array */
};


static int ramfs_directory_new(struct ramfs_directory **dir) {
    const char *funcname = "ramfs_directory_new";

    struct ramfs_directory *d = kmalloc(sizeof(struct ramfs_directory));
    if (!d) goto enomem_exit;

    d->size = 0;
    d->htcap = 8;
    size_t htlen = d->htcap * sizeof(void *);
    d->ht = kmalloc(htlen);
    if (!d->ht) goto enomem_exit;
    memset(d->ht, 0, htlen);

    logmsgdf("%s: new directory *%x\n", funcname, (uint)d);
    *dir = d;
    return 0;

enomem_exit:
    logmsgdf("%s: ENOMEM\n");
    if (d) kfree(d);
    return ENOMEM;
}

static int ramfs_directory_insert(struct ramfs_directory *dir,
                                  struct ramfs_direntry *de)
{
    size_t ht_index = de->de_hash % dir->htcap;
    struct ramfs_direntry *bucket = dir->ht[ ht_index ];
    /* TODO: possibly rebalance */
    if (bucket) {
        while (true) {
            if (bucket->de_hash == de->de_hash) {
                if (!strcmp(bucket->de_name, de->de_name))
                    return EEXIST;
                else
                    logmsgef("hash collision detected: '%s'/'%s' both hash to 0x%x\n",
                              bucket->de_name, de->de_name, de->de_hash);
            }

            if (!bucket->htnext)
                break;
            bucket = bucket->htnext;
        }
        bucket->htnext = de;
    } else {
        dir->ht[ ht_index ] = de;
    }
    de->htnext = NULL;

    ++dir->size;
    return 0;
}

static void ramfs_direntry_free(struct ramfs_direntry *de) {
    kfree(de->de_name);
    kfree(de);
}

static int ramfs_directory_new_entry(
        mountnode *sb, struct ramfs_directory *dir,
        const char *name, struct inode *idata)
{
    UNUSED(sb);
    logmsgdf("ramfs_directory_new_entry(%s)\n", name);
    int ret;
    struct ramfs_direntry *de = kmalloc(sizeof(struct ramfs_direntry));
    if (!de) return ENOMEM;

    de->de_name = strdup(name);
    if (!de->de_name) { kfree(de); return ENOMEM; }

    de->de_hash = strhash(name, strlen(name));
    de->de_ino = idata->i_no;

    ret = ramfs_directory_insert(dir, de);
    if (ret) {
        ramfs_direntry_free(de);
        return ret;
    }

    return 0;
}

static int ramfs_directory_delete_entry(
        mountnode *sb, struct ramfs_directory *dir, const char *name, size_t namelen)
{
    UNUSED(sb);
    const char *funcname = "ramfs_directory_delete_entry";
    int ret;

    struct ramfs_direntry *de, *prev_de;

    uint32_t hash = strhash(name, strnlen(name, namelen));

    de = dir->ht[ hash % dir->htcap ];
    return_dbg_if(!de, ENOENT, "%s(%s): ENOENT\n", funcname, name);

    if ((de->de_hash == hash) && !strcmp(de->de_name, name)) {
        dir->ht[ hash % dir->htcap ] = de->htnext;
        -- dir->size;
        /* TODO : possibly rebalance */

        ramfs_direntry_free(de);
        return 0;
    }

    while (de->htnext) {
        prev_de = de;
        de = de->htnext;

        if (de->de_hash == hash && !strcmp(name, de->de_name)) {
            prev_de->htnext = de->htnext;
            -- dir->size;
            /* TODO: possibly rebalance */

            ramfs_direntry_free(de);
            return 0;
        }
    }

    return ENOENT;
}

/*
 *    Searches given `dir` for part between `basename` and `basename_end`
 */
static int ramfs_directory_search(
        struct ramfs_directory *dir, inode_t *ino, const char *basename, size_t basename_len)
{
    const char *funcname = "ramfs_directory_search";
    logmsgdf("%s(dir=*%x, basename='%s'[:%d]\n",
             funcname, (uint)dir, basename, basename_len);

    uint32_t hash = strhash(basename, strnlen(basename, basename_len));

    /* directory hashtable: get direntry */
    struct ramfs_direntry *de = dir->ht[ hash % dir->htcap ];

    while (de) {
        if ((de->de_hash == hash)
            && !strncmp(basename, de->de_name, basename_len))
        {
            if (ino) *ino = de->de_ino;
            return 0;
        }
        de = de->htnext;
    }

    if (ino) *ino = 0;
    return ENOENT;
}

static void ramfs_directory_free(struct ramfs_directory *dir) {
    const char *funcname = "ramfs_directory_free";
    logmsgdf("%s(*%x)\n", funcname, (uint)dir);
    size_t i;

    for (i = 0; i < dir->htcap; ++i) {
        struct ramfs_direntry *bucket = dir->ht[ i ];
        if (!bucket) continue;

        struct ramfs_direntry *nextbucket = NULL;
        while (bucket) {
            nextbucket = bucket->htnext;
            kfree(bucket->de_name);
            kfree(bucket);
            bucket = nextbucket;
        }
    }

    kfree(dir->ht);
    kfree(dir);
}


/* used by struct superblock as `data` pointer to store FS-specific state */
struct ramfs_data {
    struct btree_node *inodes_btree;  /* map from inode_t to struct inode */
};

static int ramfs_data_new(mountnode *sb) {
    struct ramfs_data *data = kmalloc(sizeof(struct ramfs_data));
    if (!data) return ENOMEM;

    /* a B-tree that maps inode indexes to actual inodes */
    struct btree_node *bnode = btree_new(64);
    if (!bnode) {
        kfree(data);
        return ENOMEM;
    }
    /* fill inode 0 */
    theInvalidInode.i_no = 0;
    btree_set_leaf(bnode, &theInvalidInode);

    data->inodes_btree = bnode;

    sb->sb_data = data;
    return 0;
}

static void ramfs_data_free(mountnode *sb) {
    struct ramfs_data *data = sb->sb_data;

    btree_free(data->inodes_btree, (btree_leaf_free_f)ramfs_inode_free);

    kfree(sb->sb_data);
    sb->sb_data = NULL;
}



static int ramfs_inode_new(mountnode *sb, struct inode **iref, mode_t mode) {
    int ret = 0;
    struct ramfs_data *data = sb->sb_data;
    struct inode *idata = kmalloc(sizeof(struct inode));
    if (!idata) {
        ret = ENOMEM;
        goto error_exit;
    }
    memset(idata, 0, sizeof(struct inode));

    idata->i_no = btree_new_leaf(&data->inodes_btree, idata);
    idata->i_mode = mode;

    if (iref) *iref = idata;
    return 0;

error_exit:
    if (iref) *iref = NULL;
    return ret;
}

static void ramfs_inode_free(struct inode *idata) {
    const char *funcname = "ramfs_inode_free";
    logmsgdf("%s(idata=*%x, ino=%d)\n", funcname, idata, idata->i_no);

    /* some type-specific actions here */
    switch (S_IFMT & idata->i_mode) {
      case S_IFDIR:
        if (idata->i_data)
            ramfs_directory_free(idata->i_data);
        break;
      case S_IFREG:
        ramfs_free_inode_blocks(idata);
        break;
    }
    kfree(idata);
}

static inode * ramfs_idata_by_inode(mountnode *sb, inode_t ino) {
    struct ramfs_data *data = sb->sb_data;

    return btree_get_index(data->inodes_btree, ino);
    int i;

    struct btree_node *btnode = data->inodes_btree;
    int btree_lvl = btnode->bt_level;
    size_t btree_blksz = btnode->bt_fanout;

    size_t last_ind = btree_blksz;
    for (i = 0; i < btree_lvl; ++i) {
        last_ind *= btree_blksz;
    }

    if (last_ind - 1 < ino)
        return NULL; // ENOENT

    /* get btree item */
    while (btree_lvl > 0) {
        size_t this_node_index = ino / last_ind;
        ino = ino % last_ind;
        struct btree_node *subnode = btnode->bt_children[ this_node_index ];
        if (!subnode) return NULL;
        btnode = subnode;
    }
    return btnode->bt_children[ ino ];
}

static int ramfs_inode_get(mountnode *sb, inode_t ino, struct inode *inobuf) {
    int ret;

    struct inode *idata;
    idata = ramfs_idata_by_inode(sb, ino);
    if (!idata) return ENOENT;

    memcpy(inobuf, idata, sizeof(struct inode));
    return 0;
}

static int ramfs_inode_set(mountnode *sb, inode_t ino, struct inode *inobuf) {
    int ret;

    struct inode *idata;
    idata = ramfs_idata_by_inode(sb, ino);
    if (!idata) return ENOENT;

    memcpy(idata, inobuf, sizeof(struct inode));

    if ((inobuf->i_nlinks == 0) && (inobuf->i_nfds == 0))
        return ramfs_free_inode(sb, ino);

    return 0;
}


static int ramfs_lookup_inode(mountnode *sb, inode_t *result, const char *path, size_t pathlen)
{
    const char *funcname = "ramfs_lookup_inode";
    logmsgdf("%s(path='%s', pathlen=%d)\n", funcname, path, pathlen);
    struct ramfs_data *data = sb->sb_data;

    //     "some/longdirectoryname/to/examplefilename"
    //           ^                ^
    //       basename        basename_end
    const char *basename = path;
    const char *basename_end = path;
    inode_t ino;
    int ret = 0;

    if ((basename[0] == 0) || (pathlen == 0)) {
        if (result) *result = sb->sb_root_ino;
        return 0;
    }

    struct inode *root_idata = ramfs_idata_by_inode(sb, sb->sb_root_ino);
    return_err_if(!root_idata, EKERN,
            "%s: no idata for root_ino=%d\n", funcname, sb->sb_root_ino);
    return_err_if(! S_ISDIR(root_idata->i_mode), EKERN,
            "%s: root_ino is not a directory", funcname);

    struct ramfs_directory *dir = root_idata->i_data;

    for (;;) {
        while (true) {
            if ((pathlen <= (size_t)(basename_end - path))
                || (basename_end[0] == '\0'))
            {
                /* basename is "examplefilename" now */
                //logmsgdf("%s: basename found\n", basename);
                ret = ramfs_directory_search(dir, &ino, basename, basename_end - basename);
                if (result) *result = (ret ? 0 : ino);
                return ret;
            }
            if (basename_end[0] == FS_SEP)
                break;

            ++basename_end;
        }

        ret = ramfs_directory_search(dir, &ino, basename, basename_end - basename);
        if (ret) {
            if (result) *result = 0;
            return ret;
        }
        if (basename_end[1] == '\0') {
            logmsgdf("%s: trailing /, assuming a directory");
            if (result) *result = ino;
            return 0;
        }

        inode *idata = ramfs_idata_by_inode(sb, ino);
        return_err_if(!idata, EKERN,
                "%s: No inode for inode index %d on mountpoint *%p", funcname, ino, (uint)sb);
        return_dbg_if(! S_ISDIR(idata->i_mode), ENOTDIR, "%s: ENOTDIR\n", funcname);

        dir = idata->i_data;

        while (*++basename_end == FS_SEP);
        basename = basename_end;
    }
}


static int ramfs_link_inode(
        mountnode *sb, inode_t ino, inode_t dirino, const char *name, size_t namelen)
{
    const char *funcname = "ramfs_link_inode";
    char buf[UCHAR_MAX];
    strncpy(buf, name, namelen);

    int ret;
    struct inode *dir_idata = ramfs_idata_by_inode(sb, dirino);
    return_err_if(!dir_idata, EKERN, "%s: no idata for inode %d", funcname, dirino);
    return_dbg_if(!S_ISDIR(dir_idata->i_mode), ENOTDIR,
                  "%s: dirino is not a directory", funcname);

    struct ramfs_directory *dir = dir_idata->i_data;
    return_err_if(!dir, EKERN, "%s: no directory data in dirino %d\n", dirino);

    struct inode *idata = ramfs_idata_by_inode(sb, ino);
    return_dbg_if(!idata, ENOENT, "%s: no idata for ino %d\n", funcname, ino);

    ret = ramfs_directory_new_entry(sb, dir, buf, idata);
    return_dbg_if(ret, ret, "%s: ramfs_directory_new_entry(ino=%d in dirino=%d) failed\n",
                  funcname, ino, dirino);

    ++idata->i_nlinks;
    return 0;
}

static int ramfs_unlink_inode(mountnode *sb, const char *path, size_t pathlen) {
    const char *funcname = "ramfS_unlink_inode";
    int ret;

    int dlen = vfs_path_dirname_len(path, pathlen);
    return_dbg_if(dlen < 0, EINVAL, "%s: dirlen=%d\n", funcname, dlen);
    size_t dirlen = (size_t)dlen;

    inode_t ino, parino;
    struct inode *idata, *dir_idata;

    ret = ramfs_lookup_inode(sb, &parino, path, dirlen);
    return_dbg_if(ret, ret, "%s: lookup_inode(%s[:%d]) failed(%d)\n", funcname, dirlen, ret);

    dir_idata = ramfs_idata_by_inode(sb, parino);
    return_dbg_if(!dir_idata, EKERN,
            "%s: no inode for ino=%d\n", funcname, parino);
    return_dbg_if(!S_ISDIR(dir_idata->i_mode), ENOTDIR,
            "%s: %s[:%d] is not a directory", funcname, path, dirlen);

    struct ramfs_directory *dir = dir_idata->i_data;
    return_dbg_if(!dir, EKERN, "%s: no %s[:%d]->i_data\n", funcname, path, dirlen);

    const char *basename = path + dirlen;
    while (basename[0] == FS_SEP) {
        return_dbg_if(dirlen >= pathlen, EINVAL, "%s: no basename\n");
        ++basename;
        ++dirlen;
    }

    ret = ramfs_directory_search(dir, &ino, basename, pathlen - dirlen);
    return_dbg_if(ret, ret,
            "%s; directory_search(%s) failed(%d)\n", funcname, basename, ret);

    idata = ramfs_idata_by_inode(sb, ino);
    return_dbg_if(!idata, EKERN, "%s: no idata for ino=%d\n", funcname, ino);
    return_dbg_if(S_ISDIR(idata->i_mode), EISDIR, "%s(%s): EISDIR\n", funcname, basename);

    ret = ramfs_directory_delete_entry(sb, dir, basename, pathlen - dirlen);
    return_dbg_if(ret, ret, "%s: delete_entry(%s) failed(%d)\n", funcname, basename, ret);

    -- idata->i_nlinks;
    logmsgdf("%s(%s): success, nlinks=%d\n", funcname, path, idata->i_nlinks);
    if ((idata->i_nlinks > 0) || (idata->i_nfds > 0))
        return 0;

    /* delete the inode */
    logmsgdf("%s: deleting ino=%d\n", funcname, ino);
    return ramfs_free_inode(sb, ino);
}


static int ramfs_make_directory(mountnode *sb, inode_t *ino, const char *path, mode_t mode) {
    const char *funcname = "ramfs_make_directory";

    int ret;
    const char *basename = strrchr(path, FS_SEP);
    struct inode *idata = NULL;
    struct ramfs_directory *dir = NULL;
    struct ramfs_directory *parent_dir = NULL;
    logmsgdf("%s: path = *%x, basename = *%x\n", funcname, (uint)path, (uint)basename);
    return_err_if(!path, EINVAL, "ramfs_make_directory(NULL)");

    ret = ramfs_inode_new(sb, &idata, S_IFDIR | mode);
    logmsgdf("%s: ramfs_inode_new(), ino=%d, ret=%d\n", funcname, idata->i_no, ret);
    if (ret) return ret;

    ret = ramfs_directory_new(&dir);
    if (ret) goto error_exit;

    if (path[0] == '\0') {
        if (sb->sb_parent == NULL) {
            /* it's a root mountpoint, '..' points to '.' */
            ret = ramfs_directory_new_entry(sb, dir, "..", idata);
            if (ret) goto error_exit;
            ++idata->i_nlinks;
        } else {
            /* look up the parent directory inode */
            logmsge("%s: TODO: lookup parent inode", funcname);
        }
    } else {
        /* it's a subdirectory of a directory on the same device */
        if (!basename) /* it's a subdirectory of top directory */
            basename = path;

        inode_t parino = 0;
        struct inode *par_idata = NULL;

        ret = ramfs_lookup_inode(sb, &parino, path, basename - path);
        if (ret) {
            logmsgef("%s: ramfs_lookup_inode(%s) failed", funcname, path);
            goto error_exit;
        }
        logmsgdf("%s: parino=%d\n", funcname, parino);

        par_idata = ramfs_idata_by_inode(sb, parino);
        if (!par_idata) {
            logmsgef("%s: ramfs_idata_by_inode(%d) failed", funcname, parino);
            goto error_exit;
        }

        if (! S_ISDIR(par_idata->i_mode)) {
            ret = ENOTDIR;
            goto error_exit;
        }

        while (*basename == FS_SEP) ++basename;

        parent_dir = par_idata->i_data;
        ret = ramfs_directory_new_entry(sb, parent_dir, basename, idata);
        ++idata->i_nlinks;
    }

    ret = ramfs_directory_new_entry(sb, dir, ".", idata);
    if (ret) goto error_exit;

    idata->i_data = dir;
    ++idata->i_nlinks;

    if (ino) *ino = idata->i_no;
    return 0;

error_exit:
    if (dir)    ramfs_directory_free(dir);
    if (idata)  ramfs_inode_free(idata);

    if (ino) *ino = 0;
    return ret;
}


static int ramfs_make_node(mountnode *sb, inode_t *ino, mode_t mode, void *info) {
    const char *funcname = "ramfs_make_node";
    int ret;

    //return_dbg_if(S_ISDIR(mode) || S_ISLNK(mode), EINVAL, "%s(DIR or LNK)\n", funcname);

    struct inode *idata;
    ret = ramfs_inode_new(sb, &idata, mode);
    return_dbg_if(ret, ret, "%s: ramfs_inode_new() failed", funcname);

    if (S_ISCHR(mode) || S_ISBLK(mode)) {
        dev_t dev = (dev_t)(size_t)info;
        idata->as.dev.maj = gnu_dev_major(dev);
        idata->as.dev.min = gnu_dev_minor(dev);
    }

    if (ino) *ino = idata->i_no;
    return 0;
}

static int ramfs_free_inode(mountnode *sb, inode_t ino) {
    int ret;
    struct ramfs_data *fsdata = sb->sb_data;

    struct inode *idata;
    idata = ramfs_idata_by_inode(sb, ino);
    if (!idata) return ENOENT;

    ramfs_inode_free(idata);

    /* remove it from B-tree */
    btree_free_leaf(fsdata->inodes_btree, ino);
    return 0;
}


static int ramfs_read_superblock(mountnode *sb) {
    const char *funcname = "ramfs_read_superblock";
    logmsgdf("%s()\n", funcname);
    int ret;

    sb->sb_blksz = PAGE_BYTES;
    sb->sb_fs = &ramfs_driver;

    ret = ramfs_data_new(sb);
    if (ret) return ret;

    inode_t root_ino;
    struct ramfs_data *data = sb->sb_data;

    ret = ramfs_make_directory(sb, &root_ino, "", S_IFDIR | 0755);
    if (ret) {
        logmsgef("%s: ramfs_make_directory() failed", funcname);
        goto enomem_exit;
    }
    sb->sb_root_ino = root_ino;
    logmsgdf("%s: sb->root_ino = %d\n", funcname, root_ino);

    return 0;

enomem_exit:
    ramfs_data_free(sb);
    return ret;
}


static int ramfs_get_direntry(mountnode *sb, inode_t dirnode, void **iter, struct dirent *dirent) {
    const char *funcname = "ramfs_get_direntry";
    logmsgdf("%s: inode=%d, iter=0x%x\n", funcname, dirnode, (uint)*iter);

    int ret = 0;
    struct inode *dir_idata;
    dir_idata = ramfs_idata_by_inode(sb, dirnode);
    return_err_if(!dir_idata, EKERN,
                  "%s: ramfs_idata_by_inode(%d) failed", funcname, dirnode);
    return_log_if(!S_ISDIR(dir_idata->i_mode), ENOTDIR,
                  "%s: node %d is not a directory\n", funcname, dirnode);

    struct ramfs_directory *dir = dir_idata->i_data;

    uint32_t hash = (uint32_t)*iter;
    size_t htindex = hash % dir->htcap;
    struct ramfs_direntry *de = dir->ht[ htindex ];

    if (hash) { /* search through bucket for the current hash */
        while (de->de_hash != hash) {
            return_err_if(!de->htnext, EKERN, "%s: no hash 0x%x in its bucket", funcname, hash);
            de = de->htnext;
        }
    } else { /* start enumerating dirhashtable entries */
        htindex = 0;
        /* the directory must have at least . and .. entries,
         * so the hashtable may not be empty: */
        while (!(de = dir->ht[ htindex ]))
            ++htindex;
    }

    /* fill in the dirent */
    dirent->d_namlen = strlen(de->de_name);
    strncpy(dirent->d_name, de->de_name, UCHAR_MAX);

    dirent->d_ino = de->de_ino;
    dirent->d_reclen = sizeof(struct dirent) - UCHAR_MAX + dirent->d_namlen + 1;

    struct inode *idata = ramfs_idata_by_inode(sb, dirent->d_ino);
    if (idata) {
        switch (idata->i_mode & S_IFMT) {
            case S_IFREG: dirent->d_type = DT_REG; break;
            case S_IFDIR: dirent->d_type = DT_DIR; break;
            case S_IFLNK: dirent->d_type = DT_LNK; break;
            case S_IFCHR: dirent->d_type = DT_CHR; break;
            case S_IFBLK: dirent->d_type = DT_BLK; break;
            case S_IFSOCK: dirent->d_type = DT_SOCK; break;
            case S_IFIFO: dirent->d_type = DT_FIFO; break;
            default:
                logmsgef("%s: unknown inode mode 0x%x for inode %d\n",
                              funcname, idata->i_mode, dirent->d_ino);
                dirent->d_type = DT_UNKNOWN;
        }
    } else logmsgef("%s: no idata for inode %d", funcname, dirent->d_ino);

    /* search next */
    de = de->htnext;
    if (de) {
        *iter = (void *)de->de_hash;
        return 0;
    }

    /* search for the next non-empty bucket */
    while (++htindex < dir->htcap) {
        de = dir->ht[ htindex ];
        if (de) {
            *iter = (void *)de->de_hash;
            return 0;
        }
    }

    *iter = NULL; /* signal the end of the directory list */
    return 0;
}


/*
 *  ramfs block management
 */
inline static char * ramfs_new_block() {
    return kmem_alloc(1);
}

static char * ramfs_block_by_index(struct inode *idata, off_t index) {
    const char *funcname = __FUNCTION__;

    if (index < N_DIRECT_BLOCKS) {
        return (char *)idata->as.reg.directblock[index];
    }

    const char *ind1blk = (const char *)idata->as.reg.indir1st_block;
    if (!ind1blk)
        return NULL;

    if ((size_t)(index - N_DIRECT_BLOCKS) < PAGE_BYTES/sizeof(off_t)) {
        return (char *)(size_t)ind1blk[index - N_DIRECT_BLOCKS];
    }

    logmsgef("%s: index > N_DIRECT_BLOCKS+PAGE_BYTES/sizeof(off_t)", funcname);
    return NULL;
}

static char * ramfs_block_by_index_or_new(struct inode *idata, off_t index) {
    const char *funcname = __FUNCTION__;
    char *blkdata;
    off_t n_indblocks = PAGE_BYTES/sizeof(off_t);

    if (index < N_DIRECT_BLOCKS) {
        blkdata = (char *)idata->as.reg.directblock[index];
        if (!blkdata) {
            blkdata = ramfs_new_block();
            idata->as.reg.directblock[index] = (off_t)blkdata;
            ++idata->as.reg.block_count;

            logmsgdf("%s: ino=%d, block %d set to *%x\n",
                    funcname, idata->i_no, index, (uint)blkdata);
        }
        return blkdata;
    }

    index -= N_DIRECT_BLOCKS;
    if (index < n_indblocks) {
        off_t *ind1blk = (off_t *)idata->as.reg.indir1st_block;
        if (!ind1blk) {
            ind1blk = (off_t *)ramfs_new_block();
            idata->as.reg.indir1st_block = (off_t)ind1blk;
            logmsgdf("%s: ino=%d, ind1st block set to *%x\n",
                    funcname, idata->i_no, (uint)ind1blk);
        }

        blkdata = (char *)ind1blk[ index ];
        if (!blkdata) {
            blkdata = ramfs_new_block();
            ind1blk[ index ] = (off_t)blkdata;

            ++idata->as.reg.block_count;
            logmsgdf("%s: ino=%d, block %d set to *%x\n", funcname,
                    idata->i_no, N_DIRECT_BLOCKS + index, (uint)blkdata);
        }
        return blkdata;
    }

    logmsgef("%s: ETODO: who on earth needs the second level of indirection?", funcname);
    return NULL;
}

static void ramfs_free_blocks_in_list(off_t *blklst) {
    if (!blklst) return;

    size_t i;
    for (i = 0; i < PAGE_BYTES / sizeof(off_t); ++i) {
        char *blkdata = (char *)(size_t)blklst[i];
        if (!blkdata) continue;

        pmem_free((uint)blkdata, 1);
    }
}

static void ramfs_free_blocks_2ndlvl(off_t *ind2lst) {
    if (!ind2lst) return;
    size_t i;

    for (i = 0; i < PAGE_BYTES / sizeof(off_t); ++i) {
        off_t *ind1lst = (off_t *)(size_t)ind2lst[i];
        ramfs_free_blocks_in_list(ind1lst);
    }
}

static void ramfs_free_inode_blocks(struct inode *idata) {
    const char *funcname = __FUNCTION__;
    int i;

    returnv_log_if(idata->as.reg.indir3rd_block, "%s: TODO: indir3rd_block\n");
    ramfs_free_blocks_2ndlvl((off_t *)(size_t)idata->as.reg.indir2nd_block);
    ramfs_free_blocks_in_list((off_t *)(size_t)idata->as.reg.indir1st_block);

    for (i = 0; i < N_DIRECT_BLOCKS; ++i) {
        uint blkind = (uint)idata->as.reg.directblock[i];
        if (!blkind) continue;

        pmem_free(blkind, 1);
    }
}


static int ramfs_read_inode(
        mountnode *sb, inode_t ino, off_t pos,
        char *buf, size_t buflen, size_t *written)
{
    const char *funcname = __FUNCTION__;
    int ret = 0;

    struct inode *idata = ramfs_idata_by_inode(sb, ino);
    return_dbg_if(!idata, ENOENT, "%s(ino = %d): ENOENT\n", funcname, ino);

    off_t blkindex = pos / PAGE_BYTES;
    size_t nread = 0;

    if (pos >= idata->i_size)
        goto fun_exit;

    size_t offset = pos % PAGE_BYTES;
    if (offset) {
        /* copy initial partial block */
        nread = PAGE_BYTES - offset;
        if (nread > buflen)
            nread = buflen;
        if ((int)nread > idata->i_size)
            nread = idata->i_size;

        char *blkdata = ramfs_block_by_index(idata, blkindex);
        if (blkdata) {
            memcpy(buf, blkdata + offset, nread);
        } else {
            memset(buf, 0, nread);
        }

        ++blkindex;
    }

    while (((nread + PAGE_BYTES) <= buflen)
           && ((int)(pos + nread + PAGE_BYTES) <= idata->i_size))
    {
        /* copy full blocks while possible */
        char *blkdata = ramfs_block_by_index(idata, blkindex);
        if (blkdata) {
            memcpy(buf + nread, blkdata, PAGE_BYTES);
        } else {
            memset(buf + nread, 0, PAGE_BYTES);
        }
        nread += PAGE_BYTES;
        ++blkindex;
    }

    if ((nread < buflen) && ((int)(pos + nread) < idata->i_size)) {
        /* copy the partial tail */
        size_t tocopy = buflen - nread;
        if ((int)(pos + nread + tocopy) > idata->i_size)
            tocopy = idata->i_size - (pos + nread);

        char *blkdata = ramfs_block_by_index(idata, blkindex);
        if (blkdata) {
            memcpy(buf + nread, blkdata, tocopy);
        } else {
            memset(buf + nread, 0, tocopy);
        }
        nread += tocopy;
    }

fun_exit:
    if (written) *written = nread;
    return ret;
}

static int ramfs_write_inode(
        mountnode *sb, inode_t ino, off_t pos,
        const char *buf, size_t buflen, size_t *written)
{
    const char *funcname = __FUNCTION__;
    int ret = 0;

    struct inode *idata = ramfs_idata_by_inode(sb, ino);
    return_dbg_if(!idata, ENOENT, "%s(ino = %d): ENOENT\n", funcname, ino);

    off_t blkindex = pos / PAGE_BYTES;
    size_t nwrite = 0;

    size_t offset = pos % PAGE_BYTES;
    if (offset) {
        /* copy initial partial block */
        char *blkdata = ramfs_block_by_index_or_new(idata, blkindex);
        if (!blkdata) { ret = EIO; goto fun_exit; }

        nwrite = PAGE_BYTES - offset;
        if (nwrite > buflen)
            nwrite = buflen;
        memcpy(blkdata + offset, buf, nwrite);

        ++blkindex;
    }

    while ((nwrite + PAGE_BYTES) <= buflen) {
        /* copy full blocks while possible */
        char *blkdata = ramfs_block_by_index_or_new(idata, blkindex);
        if (!blkdata) { ret = EIO; goto fun_exit; }

        memcpy(blkdata, buf + nwrite, PAGE_BYTES);
        nwrite += PAGE_BYTES;
        ++blkindex;
    }

    if (nwrite < buflen) {
        /* copy the partial tail */
        char *blkdata = ramfs_block_by_index_or_new(idata, blkindex);
        if (!blkdata) { ret = EIO; goto fun_exit; }

        memcpy(blkdata, buf + nwrite, buflen - nwrite);
        nwrite = buflen;
    }

fun_exit:
    if ((int)(pos + buflen) > idata->i_size) {
        idata->i_size = pos + buflen;
        logmsgdf("%s: i_size=%d\n", funcname, idata->i_size);
    }
    if (written) *written = nwrite;
    return ret;
}


static int ramfs_trunc_inode(/*mountnode *sb, inode_t ino, off_t length*/) {
    return ETODO;
}
