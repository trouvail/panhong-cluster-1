/*
   Copyright (C) Andrew Tridgell 1996
   Copyright (C) Paul Mackerras 1996

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "rsync.h"

extern int csum_length;

extern int verbose;
extern int am_server;
extern int always_checksum;
extern time_t starttime;

extern int remote_version;

extern char *backup_suffix;
extern char *tmpdir;

extern int whole_file;
extern int block_size;
extern int update_only;
extern int make_backups;
extern int preserve_links;
extern int preserve_hard_links;
extern int preserve_perms;
extern int preserve_devices;
extern int preserve_uid;
extern int preserve_gid;
extern int preserve_times;
extern int dry_run;
extern int ignore_times;
extern int recurse;
extern int delete_mode;
extern int cvs_exclude;
extern int am_root;
extern int relative_paths;
extern int io_timeout;
extern int io_error;

/*
  free a sums struct
  */
static void free_sums(struct sum_struct *s)
{
  if (s->sums)
    free(s->sums);
  free(s);
}

/*
 * delete a file or directory. If force_delet is set then delete
 * recursively
 */
static int delete_file(char *fname)
{
  DIR *d;
  struct dirent *di;
  char buf[MAXPATHLEN];
  extern int force_delete;
  struct stat st;
  int ret;

  if (do_unlink(fname) == 0 || errno == ENOENT)
    return 0;

#if SUPPORT_LINKS
  ret = lstat(fname, &st);
#else
  ret = stat(fname, &st);
#endif
  if (ret)
  {
    fprintf(FERROR, "stat(%s) : %s\n", fname, strerror(errno));
    return -1;
  }

  if (!S_ISDIR(st.st_mode))
  {
    fprintf(FERROR, "unlink(%s) : %s\n", fname, strerror(errno));
    return -1;
  }

  if (do_rmdir(fname) == 0 || errno == ENOENT)
    return 0;
  if (!force_delete || (errno != ENOTEMPTY && errno != EEXIST))
  {
    fprintf(FERROR, "rmdir(%s) : %s\n", fname, strerror(errno));
    return -1;
  }

  /* now we do a recsursive delete on the directory ... */
  d = opendir(fname);
  if (!d)
  {
    fprintf(FERROR, "opendir(%s): %s\n",
            fname, strerror(errno));
    return -1;
  }

  for (di = readdir(d); di; di = readdir(d))
  {
    if (strcmp(di->d_name, ".") == 0 ||
        strcmp(di->d_name, "..") == 0)
      continue;
    strncpy(buf, fname, (MAXPATHLEN - strlen(di->d_name)) - 2);
    strcat(buf, "/");
    strcat(buf, di->d_name);
    buf[MAXPATHLEN - 1] = 0;
    if (verbose > 0)
      fprintf(FINFO, "deleting %s\n", buf);
    if (delete_file(buf) != 0)
    {
      closedir(d);
      return -1;
    }
  }

  closedir(d);

  if (do_rmdir(fname) != 0)
  {
    fprintf(FERROR, "rmdir(%s) : %s\n", fname, strerror(errno));
    return -1;
  }

  return 0;
}

/*
  send a sums struct down a fd
  */
static void send_sums(struct sum_struct *s, int f_out)
{
  int i;

  // 将发送的块信息写入：多少块、块大小、最后一个块的大小、以及所有的块信息
  /* tell the other guy how many we are going to be doing and how many
     bytes there are in the last chunk */
  write_int(f_out, s ? s->count : 0);
  write_int(f_out, s ? s->n : block_size);
  write_int(f_out, s ? s->remainder : 0);
  if (s)
    for (i = 0; i < s->count; i++)
    {
      write_int(f_out, s->sums[i].sum1);
      write_buf(f_out, s->sums[i].sum2, csum_length); // int csum_length = 2
    }
  write_flush(f_out);
}

/*
  generate a stream of signatures/checksums that describe a buffer

  generate approximately one checksum every n bytes
  */
static struct sum_struct *generate_sums(struct map_struct *buf, off_t len, int n)
{
  int i;
  struct sum_struct *s;
  int count;                         // 块数
  int block_len = n;                 // 每个块大小
  int remainder = (len % block_len); // 最后剩余块
  off_t offset = 0;                  // 文件偏移

  count = (len + (block_len - 1)) / block_len;

  s = (struct sum_struct *)malloc(sizeof(*s)); // 分配大小
  if (!s)
    out_of_memory("generate_sums");

  s->count = count;
  s->remainder = remainder;
  s->n = n;
  s->flength = len; // file length 文件大小

  if (count == 0)
  {
    s->sums = NULL; // 没有块信息
    return s;
  }

  if (verbose > 3)
    fprintf(FINFO, "count=%d rem=%d n=%d flength=%d\n",
            s->count, s->remainder, s->n, (int)s->flength);

  s->sums = (struct sum_buf *)malloc(sizeof(s->sums[0]) * s->count);
  if (!s->sums)
    out_of_memory("generate_sums");

  for (i = 0; i < count; i++)
  {                                       // 赋予每一块的块信息
    int n1 = MIN(len, n);                 // 防止本次循环的块不到 700 bytes，即最后一个块
    char *map = map_ptr(buf, offset, n1); // 从offset开始的n1长度的字符串

    s->sums[i].sum1 = get_checksum1(map, n1);
    get_checksum2(map, n1, s->sums[i].sum2);

    // 每一块的块信息
    s->sums[i].offset = offset;
    s->sums[i].len = n1;
    s->sums[i].i = i;

    if (verbose > 3)
      fprintf(FINFO, "chunk[%d] offset=%d len=%d sum1=%08x\n",
              i, (int)s->sums[i].offset, s->sums[i].len, s->sums[i].sum1);

    len -= n1;
    offset += n1;
  }

  return s;
}

/*
  receive the checksums for a buffer
  */
static struct sum_struct *receive_sums(int f)
{
  struct sum_struct *s;
  int i;
  off_t offset = 0;

  s = (struct sum_struct *)malloc(sizeof(*s)); // 分配空间
  if (!s)
    out_of_memory("receive_sums");

  // 读出先导信息
  s->count = read_int(f);
  s->n = read_int(f);
  s->remainder = read_int(f);
  s->sums = NULL;

  if (verbose > 3)
    fprintf(FINFO, "count=%d n=%d rem=%d\n",
            s->count, s->n, s->remainder);

  if (s->count == 0) // 没有块信息
    return (s);

  s->sums = (struct sum_buf *)malloc(sizeof(s->sums[0]) * s->count); // 开始读每一个块的信息
  if (!s->sums)
    out_of_memory("receive_sums");

  for (i = 0; i < s->count; i++) // 对每一个块都进行赋值
  {
    s->sums[i].sum1 = read_int(f);
    read_buf(f, s->sums[i].sum2, csum_length);

    s->sums[i].offset = offset;
    s->sums[i].i = i;

    if (i == s->count - 1 && s->remainder != 0) // 最后一个块
    {
      s->sums[i].len = s->remainder;
    }
    else
    {
      s->sums[i].len = s->n;
    }
    offset += s->sums[i].len;

    if (verbose > 3)
      fprintf(FINFO, "chunk[%d] len=%d offset=%d sum1=%08x\n",
              i, s->sums[i].len, (int)s->sums[i].offset, s->sums[i].sum1);
  }

  s->flength = offset; // 总文件长度

  return s;
}

static int set_perms(char *fname, struct file_struct *file, struct stat *st,
                     int report)
{
  int updated = 0;
  struct stat st2;

  if (dry_run)
    return 0;

  if (!st)
  {
    if (link_stat(fname, &st2) != 0)
    {
      fprintf(FERROR, "stat %s : %s\n", fname, strerror(errno));
      return 0;
    }
    st = &st2;
  }

  if (preserve_times && !S_ISLNK(st->st_mode) &&
      st->st_mtime != file->modtime)
  {
    updated = 1;
    if (set_modtime(fname, file->modtime) != 0)
    {
      fprintf(FERROR, "failed to set times on %s : %s\n",
              fname, strerror(errno));
      return 0;
    }
  }

#ifdef HAVE_CHMOD
  if (preserve_perms && !S_ISLNK(st->st_mode) &&
      st->st_mode != file->mode)
  {
    updated = 1;
    if (do_chmod(fname, file->mode) != 0)
    {
      fprintf(FERROR, "failed to set permissions on %s : %s\n",
              fname, strerror(errno));
      return 0;
    }
  }
#endif

  if ((am_root && preserve_uid && st->st_uid != file->uid) ||
      (preserve_gid && st->st_gid != file->gid))
  {
    if (do_lchown(fname,
                  (am_root && preserve_uid) ? file->uid : -1,
                  preserve_gid ? file->gid : -1) != 0)
    {
      if (preserve_uid && st->st_uid != file->uid)
        updated = 1;
      if (verbose > 1 || preserve_uid)
        fprintf(FERROR, "chown %s : %s\n",
                fname, strerror(errno));
      return updated;
    }
    updated = 1;
  }

  if (verbose > 1 && report)
  {
    if (updated)
      fprintf(FINFO, "%s\n", fname);
    else
      fprintf(FINFO, "%s is uptodate\n", fname);
  }
  return updated;
}

/* choose whether to skip a particular file */
static int skip_file(char *fname,
                     struct file_struct *file, struct stat *st)
{
  if (st->st_size != file->length)
  {
    return 0;
  }

  /* if always checksum is set then we use the checksum instead
     of the file time to determine whether to sync */
  if (always_checksum && S_ISREG(st->st_mode))
  {
    char sum[MD4_SUM_LENGTH];
    file_checksum(fname, sum, st->st_size);
    return (memcmp(sum, file->sum, csum_length) == 0);
  }

  if (ignore_times)
  {
    return 0;
  }

  return (st->st_mtime == file->modtime);
}

/* use a larger block size for really big files */
int adapt_block_size(struct file_struct *file, int bsize)
{
  int ret = file->length / (10000); /* rough heuristic 粗略启发式 */
  ret = ret & ~15;                  /* multiple of 16 16的倍数 将后四位变成4个0 ———— 妙哉妙哉 */
  if (ret < bsize)                  // #define BLOCK_SIZE 700
    ret = bsize;
  if (ret > CHUNK_SIZE / 2) // #define CHUNK_SIZE (32 * 1024)
    ret = CHUNK_SIZE / 2;
  return ret;
}

// 接收端生成校验和（generate_sums）并写出到输出端(send_sums) i是文件索引，f_out是输出到的文件
void recv_generator(char *fname, struct file_list *flist, int i, int f_out)
{
  int fd;
  struct stat st; // 文件元信息：修改时间、所属组等等
  struct map_struct *buf;
  struct sum_struct *s;
  int statret;
  struct file_struct *file = flist->files[i];

  if (verbose > 2)
    fprintf(FINFO, "recv_generator(%s,%d)\n", fname, i);

  // 把fname文件的元信息放进st这个buffer中
  statret = link_stat(fname, &st);

  // 接下来根据st中的信息来判断怎么发送文件
  if (S_ISDIR(file->mode))
  {
    if (dry_run)
      return;
    if (statret == 0 && !S_ISDIR(st.st_mode))
    {
      if (do_unlink(fname) != 0)
      {
        fprintf(FERROR, "unlink %s : %s\n", fname, strerror(errno));
        return;
      }
      statret = -1;
    }
    if (statret != 0 && do_mkdir(fname, file->mode) != 0 && errno != EEXIST)
    {
      if (!(relative_paths && errno == ENOENT &&
            create_directory_path(fname) == 0 &&
            do_mkdir(fname, file->mode) == 0))
      {
        fprintf(FERROR, "mkdir %s : %s (2)\n",
                fname, strerror(errno));
      }
    }
    if (set_perms(fname, file, NULL, 0) && verbose)
      fprintf(FINFO, "%s/\n", fname);
    return;
  }

  if (preserve_links && S_ISLNK(file->mode))
  {
#if SUPPORT_LINKS
    char lnk[MAXPATHLEN];
    int l;
    if (statret == 0)
    {
      l = readlink(fname, lnk, MAXPATHLEN - 1);
      if (l > 0)
      {
        lnk[l] = 0;
        if (strcmp(lnk, file->link) == 0)
        {
          set_perms(fname, file, &st, 1);
          return;
        }
      }
    }
    delete_file(fname);
    if (do_symlink(file->link, fname) != 0)
    {
      fprintf(FERROR, "link %s -> %s : %s\n",
              fname, file->link, strerror(errno));
    }
    else
    {
      set_perms(fname, file, NULL, 0);
      if (verbose)
        fprintf(FINFO, "%s -> %s\n",
                fname, file->link);
    }
#endif
    return;
  }

#ifdef HAVE_MKNOD
  if (am_root && preserve_devices && IS_DEVICE(file->mode))
  {
    if (statret != 0 ||
        st.st_mode != file->mode ||
        st.st_rdev != file->rdev)
    {
      delete_file(fname);
      if (verbose > 2)
        fprintf(FINFO, "mknod(%s,0%o,0x%x)\n",
                fname, (int)file->mode, (int)file->rdev);
      if (do_mknod(fname, file->mode, file->rdev) != 0)
      {
        fprintf(FERROR, "mknod %s : %s\n", fname, strerror(errno));
      }
      else
      {
        set_perms(fname, file, NULL, 0);
        if (verbose)
          fprintf(FINFO, "%s\n", fname);
      }
    }
    else
    {
      set_perms(fname, file, &st, 1);
    }
    return;
  }
#endif

  if (preserve_hard_links && check_hard_link(file))
  {
    if (verbose > 1)
      fprintf(FINFO, "%s is a hard link\n", f_name(file));
    return;
  }

  if (!S_ISREG(file->mode))
  {
    fprintf(FINFO, "skipping non-regular file %s\n", fname);
    return;
  }

  if (statret == -1)
  {
    if (errno == ENOENT)
    {
      write_int(f_out, i);
      if (!dry_run)
        send_sums(NULL, f_out);
    }
    else
    {
      if (verbose > 1)
        fprintf(FERROR, "recv_generator failed to open %s\n", fname);
    }
    return;
  }

  if (!S_ISREG(st.st_mode))
  {
    if (delete_file(fname) != 0)
    {
      return;
    }

    /* now pretend the file didn't exist */
    write_int(f_out, i);
    if (!dry_run)
      send_sums(NULL, f_out);
    return;
  }

  if (update_only && st.st_mtime > file->modtime)
  {
    if (verbose > 1)
      fprintf(FINFO, "%s is newer\n", fname);
    return;
  }

  if (skip_file(fname, file, &st))
  {
    set_perms(fname, file, &st, 1);
    return;
  }

  if (dry_run)
  {
    write_int(f_out, i);
    return;
  }

  if (whole_file)
  {
    write_int(f_out, i);
    send_sums(NULL, f_out);
    return;
  }

  // 判断怎么发送文件结束，开始准备发送文件
  /* open the file */
  // 只读模式
  fd = open(fname, O_RDONLY);

  if (fd == -1)
  {
    fprintf(FERROR, "failed to open %s : %s\n", fname, strerror(errno));
    fprintf(FERROR, "skipping %s\n", fname);
    return;
  }

  if (st.st_size > 0)
  { // 本地文件导入buf
    buf = map_file(fd, st.st_size);
  }
  else
  {
    buf = NULL;
  }

  if (verbose > 3)
    fprintf(FINFO, "gen mapped %s of size %d\n", fname, (int)st.st_size);

  s = generate_sums(buf, st.st_size, adapt_block_size(file, block_size)); // block_size为700

  if (verbose > 2)
    fprintf(FINFO, "sending sums for %d\n", i);

  write_int(f_out, i);
  // 发送数据
  send_sums(s, f_out);
  write_flush(f_out);

  close(fd);
  if (buf)
    unmap_file(buf);

  free_sums(s);
}

static int receive_data(int f_in, struct map_struct *buf, int fd, char *fname) // (f_in, buf, fd2, fname) 获得的数据、本地、写入的文件、本地文件
{
  int i, n, remainder, len, count;
  off_t offset = 0;
  off_t offset2;
  char *data;
  static char file_sum1[MD4_SUM_LENGTH];
  static char file_sum2[MD4_SUM_LENGTH];
  char *map = NULL;

  count = read_int(f_in);
  n = read_int(f_in);
  remainder = read_int(f_in);

  sum_init();

  for (i = recv_token(f_in, &data); i != 0; i = recv_token(f_in, &data))
  {
    if (i > 0) // 好像代表这个长度的数据
    {
      if (verbose > 3)
        fprintf(FINFO, "data recv %d at %d\n", i, (int)offset);

      sum_update(data, i);

      if (fd != -1 && write_file(fd, data, i) != i) // 写入数据
      {
        fprintf(FERROR, "write failed on %s : %s\n", fname, strerror(errno));
        exit_cleanup(1);
      }
      offset += i;
    }
    else // 好像代表着索引
    {
      i = -(i + 1);
      offset2 = i * n;
      len = n;
      if (i == count - 1 && remainder != 0)
        len = remainder;

      if (verbose > 3)
        fprintf(FINFO, "chunk[%d] of size %d at %d offset=%d\n",
                i, len, (int)offset2, (int)offset);

      map = map_ptr(buf, offset2, len);

      see_token(map, len);
      sum_update(map, len);

      if (fd != -1 && write_file(fd, map, len) != len) // 写入数据
      {
        fprintf(FERROR, "write failed on %s : %s\n", fname, strerror(errno));
        exit_cleanup(1);
      }
      offset += len;
    }
  }

  if (fd != -1 && offset > 0 && sparse_end(fd) != 0)
  {
    fprintf(FERROR, "write failed on %s : %s\n", fname, strerror(errno));
    exit_cleanup(1);
  }

  sum_end(file_sum1);

  if (remote_version >= 14)
  {
    read_buf(f_in, file_sum2, MD4_SUM_LENGTH);
    if (verbose > 2)
      fprintf(FINFO, "got file_sum\n");
    if (fd != -1 && memcmp(file_sum1, file_sum2, MD4_SUM_LENGTH) != 0)
      return 0;
  }
  return 1;
}

static void delete_one(struct file_struct *f)
{
  if (!S_ISDIR(f->mode))
  {
    if (do_unlink(f_name(f)) != 0)
    {
      fprintf(FERROR, "unlink %s : %s\n", f_name(f), strerror(errno));
    }
    else if (verbose)
    {
      fprintf(FINFO, "deleting %s\n", f_name(f));
    }
  }
  else
  {
    if (do_rmdir(f_name(f)) != 0)
    {
      if (errno != ENOTEMPTY && errno != EEXIST)
        fprintf(FERROR, "rmdir %s : %s\n", f_name(f), strerror(errno));
    }
    else if (verbose)
    {
      fprintf(FINFO, "deleting directory %s\n", f_name(f));
    }
  }
}

static struct delete_list
{
  dev_t dev;
  ino_t inode;
} *delete_list;
static int dlist_len, dlist_alloc_len;

static void add_delete_entry(struct file_struct *file)
{
  if (dlist_len == dlist_alloc_len)
  {
    dlist_alloc_len += 1024;
    if (!delete_list)
    {
      delete_list = (struct delete_list *)malloc(sizeof(delete_list[0]) * dlist_alloc_len);
    }
    else
    {
      delete_list = (struct delete_list *)realloc(delete_list, sizeof(delete_list[0]) * dlist_alloc_len);
    }
    if (!delete_list)
      out_of_memory("add_delete_entry");
  }

  delete_list[dlist_len].dev = file->dev;
  delete_list[dlist_len].inode = file->inode;
  dlist_len++;

  if (verbose > 3)
    fprintf(FINFO, "added %s to delete list\n", f_name(file));
}

/* yuck! This function wouldn't have been necessary if I had the sorting
   algorithm right. Unfortunately fixing the sorting algorithm would introduce
   a backward incompatibility as file list indexes are sent over the link.
*/
static int delete_already_done(struct file_list *flist, int j)
{
  int i;
  struct stat st;

  if (link_stat(f_name(flist->files[j]), &st))
    return 1;

  for (i = 0; i < dlist_len; i++)
  {
    if (st.st_ino == delete_list[i].inode &&
        st.st_dev == delete_list[i].dev)
      return 1;
  }

  return 0;
}

/* this deletes any files on the receiving side that are not present
   on the sending side. For version 1.6.4 I have changed the behaviour
   to match more closely what most people seem to expect of this option */
static void delete_files(struct file_list *flist)
{
  struct file_list *local_file_list;
  int i, j;
  char *name;

  if (cvs_exclude)
    add_cvs_excludes();

  if (io_error)
  {
    fprintf(FINFO, "IO error encountered - skipping file deletion\n");
    return;
  }

  for (j = 0; j < flist->count; j++)
  {
    if (!S_ISDIR(flist->files[j]->mode) ||
        !(flist->files[j]->flags & FLAG_DELETE))
      continue;

    if (delete_already_done(flist, j))
      continue;

    name = strdup(f_name(flist->files[j]));

    if (!(local_file_list = send_file_list(-1, 1, &name)))
    {
      free(name);
      continue;
    }

    if (verbose > 1)
      fprintf(FINFO, "deleting in %s\n", name);

    for (i = local_file_list->count - 1; i >= 0; i--)
    {
      if (!local_file_list->files[i]->basename)
        continue;
      if (S_ISDIR(local_file_list->files[i]->mode))
        add_delete_entry(local_file_list->files[i]);
      if (-1 == flist_find(flist, local_file_list->files[i]))
      {
        delete_one(local_file_list->files[i]);
      }
    }
    flist_free(local_file_list);
    free(name);
  }
}

static char *cleanup_fname;

void exit_cleanup(int code)
{
  if (cleanup_fname)
    do_unlink(cleanup_fname);
  signal(SIGUSR1, SIG_IGN);
  if (code)
  {
    kill_all(SIGUSR1);
  }
  exit(code);
}

void sig_int(void)
{
  exit_cleanup(1);
}

// f_in是收到的文件，f_gen是即将生成的文件
int recv_files(int f_in, struct file_list *flist, char *local_name, int f_gen)
{
  int fd1, fd2;
  struct stat st; // 文件元信息：修改时间、所属组等等
  char *fname;
  char fnametmp[MAXPATHLEN]; // 文件全名包括路径
  struct map_struct *buf;
  int i;
  struct file_struct *file;
  int phase = 0;
  int recv_ok;

  if (verbose > 2)
  {
    fprintf(FINFO, "recv_files(%d) starting\n", flist->count);
  }

  if (recurse && delete_mode && !local_name && flist->count > 0) // 递归下的删除模式？
  {
    delete_files(flist);
  }

  while (1)
  {
    // 跟send_files接到i = -1一样
    i = read_int(f_in);
    if (i == -1)
    {
      if (phase == 0 && remote_version >= 13)
      {
        phase++;
        csum_length = SUM_LENGTH;
        if (verbose > 2)
          fprintf(FINFO, "recv_files phase=%d\n", phase);
        write_int(f_gen, -1);
        write_flush(f_gen);
        continue;
      }
      break;
    }

    file = flist->files[i];
    fname = f_name(file);

    if (local_name)
      fname = local_name;

    if (dry_run) // dry_run?试运行
    {
      if (!am_server && verbose)
        printf("%s\n", fname);
      continue;
    }

    if (verbose > 2)
      fprintf(FINFO, "recv_files(%s)\n", fname);

    /* open the file */
    fd1 = open(fname, O_RDONLY); // fd1是本地文件

    if (fd1 != -1 && fstat(fd1, &st) != 0)
    {
      fprintf(FERROR, "fstat %s : %s\n", fname, strerror(errno));
      receive_data(f_in, NULL, -1, NULL);
      close(fd1);
      continue;
    }

    if (fd1 != -1 && !S_ISREG(st.st_mode)) // 模式为一般/常规文件
    {
      fprintf(FERROR, "%s : not a regular file (recv_files)\n", fname);
      receive_data(f_in, NULL, -1, NULL);
      close(fd1);
      continue;
    }

    if (fd1 != -1 && st.st_size > 0)
    {
      buf = map_file(fd1, st.st_size); // 本地文件结构
      if (verbose > 2)
        fprintf(FINFO, "recv mapped %s of size %d\n", fname, (int)st.st_size);
    }
    else
    {
      buf = NULL;
    }

    /* open tmp file */
    if (strlen(fname) > (MAXPATHLEN - 8)) // 文件名字太长 差不多1024了
    {
      fprintf(FERROR, "filename too long\n");
      if (buf)
        unmap_file(buf);
      close(fd1);
      continue;
    }
    if (tmpdir)
    {
      char *f;
      f = strrchr(fname, '/'); // 在参数 str 所指向的字符串中搜索最后一次出现字符 c（一个无符号字符）的位置，本处即为获取文件名，去除目录
      if (f == NULL)
        f = fname;
      else
        f++;
      sprintf(fnametmp, "%s/%s.XXXXXX", tmpdir, f); // 写入文件路径
    }
    else
    {
      sprintf(fnametmp, "%s.XXXXXX", fname);
    }
    if (NULL == do_mktemp(fnametmp)) // 创建本地目录失败
    {
      fprintf(FERROR, "mktemp %s failed\n", fnametmp);
      receive_data(f_in, buf, -1, NULL);
      if (buf)
        unmap_file(buf);
      close(fd1);
      continue;
    }
    fd2 = do_open(fnametmp, O_WRONLY | O_CREAT | O_EXCL, file->mode); // 可写可创建可执行
    if (fd2 == -1 && relative_paths && errno == ENOENT &&
        create_directory_path(fnametmp) == 0)
    {
      fd2 = do_open(fnametmp, O_WRONLY | O_CREAT | O_EXCL, file->mode);
    }
    if (fd2 == -1)
    {
      fprintf(FERROR, "open %s : %s\n", fnametmp, strerror(errno));
      receive_data(f_in, buf, -1, NULL);
      if (buf)
        unmap_file(buf);
      close(fd1);
      continue;
    }

    cleanup_fname = fnametmp;

    if (!am_server && verbose)
      printf("%s\n", fname);

    /* recv file data */
    recv_ok = receive_data(f_in, buf, fd2, fname); // 处理获得的数据

    // 释放资源
    if (buf)
      unmap_file(buf);
    if (fd1 != -1)
    {
      close(fd1);
    }
    close(fd2);

    if (verbose > 2)
      fprintf(FINFO, "renaming %s to %s\n", fnametmp, fname);

    if (make_backups) // 进行备份
    {
      char fnamebak[MAXPATHLEN];
      if (strlen(fname) + strlen(backup_suffix) > (MAXPATHLEN - 1)) // #define BACKUP_SUFFIX "~"
      {
        fprintf(FERROR, "backup filename too long\n");
        continue;
      }
      sprintf(fnamebak, "%s%s", fname, backup_suffix); // 根文件吗？
      if (do_rename(fname, fnamebak) != 0 && errno != ENOENT) // 重命名
      {
        fprintf(FERROR, "rename %s %s : %s\n", fname, fnamebak, strerror(errno));
        continue;
      }
    }

    /* move tmp file over real file */
    if (do_rename(fnametmp, fname) != 0)
    {
      if (errno == EXDEV)
      {
        /* rename failed on cross-filesystem link. 跨文件系统命名失败
     Copy the file instead. */
        if (copy_file(fnametmp, fname, file->mode))
        {
          fprintf(FERROR, "copy %s -> %s : %s\n",
                  fnametmp, fname, strerror(errno));
        }
        else
        {
          set_perms(fname, file, NULL, 0);
        }
        do_unlink(fnametmp);
      }
      else
      {
        fprintf(FERROR, "rename %s -> %s : %s\n",
                fnametmp, fname, strerror(errno));
        do_unlink(fnametmp);
      }
    }
    else
    {
      set_perms(fname, file, NULL, 0);
    }

    cleanup_fname = NULL;

    if (!recv_ok)
    {
      if (csum_length == SUM_LENGTH)
      {
        fprintf(FERROR, "ERROR: file corruption in %s. File changed during transfer?\n",
                fname);
      }
      else
      {
        if (verbose > 1)
          fprintf(FINFO, "redoing %s(%d)\n", fname, i);
        write_int(f_gen, i);
      }
    }
  }

  if (preserve_hard_links) // 保留硬链接
    do_hard_links(flist);

  /* now we need to fix any directory permissions that were
     modified during the transfer
     现在，我们需要修复在传输过程中修改的任意目录权限 */
  for (i = 0; i < flist->count; i++)
  {
    struct file_struct *file = flist->files[i];
    if (!file->basename || !S_ISDIR(file->mode)) // 不是文件基名或者不是一个文件
      continue;
    recv_generator(f_name(file), flist, i, -1); // -1的意思？感觉一般不会进到这里，所以应该不用管
  }

  if (verbose > 2)
    fprintf(FINFO, "recv_files finished\n");

  return 0;
}

// f_in都是相对本地接收的文件，f_out都是相对本地将要写道输出端的文件
void send_files(struct file_list *flist, int f_out, int f_in)
{
  int fd;
  struct sum_struct *s;
  struct map_struct *buf; // 本地的文件信息
  struct stat st;         // 文件元信息：修改时间、所属组等等
  char fname[MAXPATHLEN]; // #define MAXPATHLEN 1024
  int i;
  struct file_struct *file;
  int phase = 0;
  int offset = 0;

  if (verbose > 2)
    fprintf(FINFO, "send_files starting\n");

  setup_nonblocking(f_in, f_out); // 设置非阻塞态

  // 逐一的匹配文件
  while (1)
  {
    i = read_int(f_in);
    if (i == -1)
    {
      if (phase == 0 && remote_version >= 13)
      {
        phase++;
        csum_length = SUM_LENGTH; // #define SUM_LENGTH 16
        write_int(f_out, -1);
        write_flush(f_out);
        if (verbose > 2)
          fprintf(FINFO, "send_files phase=%d\n", phase);
        continue;
      }
      break;
    }

    file = flist->files[i]; // 第i个文件 flist是输入的

    // 获取文件名字 -- start
    fname[0] = 0; // 文件路径
    if (file->basedir)
    {
      strncpy(fname, file->basedir, MAXPATHLEN - 1); // 最多复制n
      fname[MAXPATHLEN - 1] = 0;
      if (strlen(fname) == MAXPATHLEN - 1)
      {
        io_error = 1;
        fprintf(FERROR, "send_files failed on long-named directory %s\n",
                fname);
        return;
      }
      strcat(fname, "/"); // 追加字符串
      offset = strlen(file->basedir) + 1;
    }
    strncat(fname, f_name(file), MAXPATHLEN - strlen(fname));
    // 获取文件名字 -- end

    if (verbose > 2)
      fprintf(FINFO, "send_files(%d,%s)\n", i, fname);

    if (dry_run)
    {
      if (!am_server && verbose)
        printf("%s\n", fname);
      write_int(f_out, i);
      continue;
    }

    s = receive_sums(f_in); // 接收缓冲区的校验和
    if (!s)                 // 接收校验和失败
    {
      io_error = 1;
      fprintf(FERROR, "receive_sums failed\n");
      return;
    }

    fd = open(fname, O_RDONLY);
    if (fd == -1) // 打开本地文件失败
    {
      io_error = 1;
      fprintf(FERROR, "send_files failed to open %s: %s\n",
              fname, strerror(errno));
      free_sums(s);
      continue;
    }

    /* map the local file */
    if (fstat(fd, &st) != 0) // 赋予状态失败
    {
      io_error = 1;
      fprintf(FERROR, "fstat failed : %s\n", strerror(errno));
      free_sums(s);
      close(fd);
      return;
    }

    if (st.st_size > 0)
    {
      buf = map_file(fd, st.st_size); // 导入到本地的文件结构buf（map_struct）中
    }
    else
    {
      buf = NULL;
    }

    if (verbose > 2)
      fprintf(FINFO, "send_files mapped %s of size %d\n",
              fname, (int)st.st_size);

    // 将接收到的块信息写入到输出文件中
    write_int(f_out, i);

    write_int(f_out, s->count);
    write_int(f_out, s->n);
    write_int(f_out, s->remainder);

    if (verbose > 2)
      fprintf(FINFO, "calling match_sums %s\n", fname);

    if (!am_server && verbose)
      printf("%s\n", fname + offset); // 输出文件名字不带路径

    // 进行hash查找匹配
    match_sums(f_out, s, buf, st.st_size);
    write_flush(f_out);

    // 善后处理 -- start
    if (buf)
      unmap_file(buf);
    close(fd);

    free_sums(s);
    // 善后处理 -- end

    if (verbose > 2)
      fprintf(FINFO, "sender finished %s\n", fname);
  }

  if (verbose > 2)
    fprintf(FINFO, "send files finished\n");

  match_report();

  write_int(f_out, -1);
  write_flush(f_out);
}

void generate_files(int f, struct file_list *flist, char *local_name, int f_recv)
{
  int i;
  int phase = 0;

  if (verbose > 2)
    fprintf(FINFO, "generator starting pid=%d count=%d\n",
            (int)getpid(), flist->count);

  for (i = 0; i < flist->count; i++)
  {
    struct file_struct *file = flist->files[i];
    mode_t saved_mode = file->mode;
    if (!file->basename)
      continue;

    /* we need to ensure that any directories we create have writeable
       permissions initially so that we can create the files within
       them. This is then fixed after the files are transferred */
    if (!am_root && S_ISDIR(file->mode))
    {
      file->mode |= S_IWUSR; /* user write */
    }

    recv_generator(local_name ? local_name : f_name(file),
                   flist, i, f);

    file->mode = saved_mode;
  }

  phase++;
  csum_length = SUM_LENGTH;
  ignore_times = 1;

  if (verbose > 2)
    fprintf(FINFO, "generate_files phase=%d\n", phase);

  write_int(f, -1);
  write_flush(f);

  /* we expect to just sit around now, so don't exit on a timeout. If we
     really get a timeout then the other process should exit */
  io_timeout = 0;

  if (remote_version >= 13)
  {
    /* in newer versions of the protocol the files can cycle through
       the system more than once to catch initial checksum errors */
    for (i = read_int(f_recv); i != -1; i = read_int(f_recv))
    {
      struct file_struct *file = flist->files[i];
      recv_generator(local_name ? local_name : f_name(file),
                     flist, i, f);
    }

    phase++;
    if (verbose > 2)
      fprintf(FINFO, "generate_files phase=%d\n", phase);

    write_int(f, -1);
    write_flush(f);
  }

  if (verbose > 2)
    fprintf(FINFO, "generator wrote %ld\n", (long)write_total());
}
