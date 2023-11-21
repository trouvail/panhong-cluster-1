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

extern int remote_version;

typedef unsigned short tag;

#define TABLESIZE (1 << 16) // hash表大小为2^16
#define NULL_TAG ((tag)-1)

// 误报；目标命中；匹配成功；数据传输量
static int false_alarms;
static int tag_hits;
static int matches;
static int data_transfer;

static int total_false_alarms;
static int total_tag_hits;
static int total_matches;
static int64 total_data_transfer;

struct target
{
	tag t;
	int i;
};

static struct target *targets; // 包括一个tag和一个int的链表，个数根据传过来的块数来决定

static tag *tag_table; // tag的链表，据tag中的位置判断校验和从而找到是否存在此16位弱校验和并找到在targets中的位置，个数一定为2^16个

// 获取16位弱校验和
#define gettag2(s1, s2) (((s1) + (s2)) & 0xFFFF)
#define gettag(sum) gettag2((sum) & 0xFFFF, (sum) >> 16)

// 比较两个target的16位弱校验和
static int compare_targets(struct target *t1, struct target *t2)
{
	return ((int)t1->t - (int)t2->t);
}

// 构建hash表
static void build_hash_table(struct sum_struct *s)
{
	int i;

	if (!tag_table)
		tag_table = (tag *)malloc(sizeof(tag) * TABLESIZE); // 2^16个16位校验和

	targets = (struct target *)malloc(sizeof(targets[0]) * s->count); // 根据块数构建hash表
	if (!tag_table || !targets)										  // 超过内存则报错
		out_of_memory("build_hash_table");

	for (i = 0; i < s->count; i++) // i值为序号，t值为16位弱校验和
	{
		targets[i].i = i;
		targets[i].t = gettag(s->sums[i].sum1);
	}

	qsort(targets, s->count, sizeof(targets[0]), (int (*)())compare_targets); // 在targets中按照16位校验和大小排序

	for (i = 0; i < TABLESIZE; i++) // 初值全部为NULL_TAG
		tag_table[i] = NULL_TAG;

	for (i = s->count - 1; i >= 0; i--) // 根据tag中的位置判断校验和从而找到是否存在此16位弱校验和并找到在targets中的位置
	{
		tag_table[targets[i].t] = i;
	}
}

static off_t last_match; // 上一次匹配

static void matched(int f, struct sum_struct *s, struct map_struct *buf,
					off_t offset, int i)
{
	off_t n = offset - last_match; // 本次匹配与上次匹配之间的距离
	int j;

	if (verbose > 2 && i >= 0) // 根据verbose的多少（是通过在命令行的v的个数来决定的）来决定输出的日志信息
		fprintf(FINFO, "match at %d last_match=%d j=%d len=%d n=%d\n",
				(int)offset, (int)last_match, i, (int)s->sums[i].len, (int)n);

	send_token(f, i, buf, last_match, n, i == -1 ? 0 : s->sums[i].len); // 将n数据发送
	data_transfer += n;

	if (n > 0)
		write_flush(f); // 无效函数，啥也没干

	if (i >= 0) // 如果i是正数，那么则代表块索引
		n += s->sums[i].len;

	for (j = 0; j < n; j += CHUNK_SIZE) // 32 * 1024 z
	{
		int n1 = MIN(CHUNK_SIZE, n - j); // 防止最后一块
		sum_update(map_ptr(buf, last_match + j, n1), n1);
	}

	if (i >= 0) // 更新last_match为本次的偏移
		last_match = offset + s->sums[i].len;
	else
		last_match = offset;
}

// in 检验报错

static void hash_search(int f, struct sum_struct *s,
						struct map_struct *buf, off_t len)
{
	off_t offset; // 为long int类型
	int j, k;
	int end;			   // 迭代和辅助计算
	char sum2[SUM_LENGTH]; // 128位hash值
	uint32 s1, s2, sum;	   // 存储散列值中间结果
	signed char *map;	   // 指向存储映射数据的地址

	if (verbose > 2)
		fprintf(FINFO, "hash search b=%d len=%d\n", s->n, (int)len);

	k = MIN(len, s->n); // 总长度和块长度中的较小值，防止只有一块

	map = (signed char *)map_ptr(buf, 0, k); // 为本地的

	sum = get_checksum1((char *)map, k); // 弱校验和
	s1 = sum & 0xFFFF;					 // 存储低16位
	s2 = sum >> 16;						 // 存储高16位
	if (verbose > 3)
		fprintf(FINFO, "sum=%.8x k=%d\n", sum, k);

	offset = 0;

	end = len + 1 - s->sums[s->count - 1].len; // 文件最后一个块的第一个字节

	// int zhou = 1;

	// printf("%d", zhou);

	if (verbose > 3)
		fprintf(FINFO, "hash search s->n=%d len=%d count=%d\n",
				s->n, (int)len, s->count);

	do
	{
		tag t = gettag2(s1, s2); // 计算16位校验和
		int done_csum2 = 0;		 // 是否计算强校验和

		j = tag_table[t]; // 根据16位校验和获取相应的表项位置
		if (verbose > 4)
			fprintf(FINFO, "offset=%d sum=%08x\n", (int)offset, sum);

		if (j == NULL_TAG)
		{
			goto null_tag; // 匹配失败
		}

		sum = (s1 & 0xffff) | (s2 << 16); // 弱校验和
		tag_hits++;						  // 命中一次
		for (; j < s->count && targets[j].t == t; j++)
		{
			int i = targets[j].i; // hash表中的对应索引

			if (sum != s->sums[i].sum1) // 弱校验和不相等
				continue;

			if (verbose > 3)
				fprintf(FINFO, "potential match at %d target=%d %d sum=%08x\n",
						(int)offset, j, i, sum);

			if (!done_csum2)
			{ // 计算强校验和
				int l = MIN(s->n, len - offset);
				map = (signed char *)map_ptr(buf, offset, l);
				get_checksum2((char *)map, l, sum2);
				done_csum2 = 1;
			}

			if (memcmp(sum2, s->sums[i].sum2, csum_length) != 0)
			{ // 校验和不匹配
				false_alarms++;
				continue;
			}

			// 强弱校验和都匹配成功
			matched(f, s, buf, offset, i);
			offset += s->sums[i].len - 1;

			// 进行下一个块
			k = MIN((len - offset), s->n);
			map = (signed char *)map_ptr(buf, offset, k);
			sum = get_checksum1((char *)map, k);
			s1 = sum & 0xFFFF;
			s2 = sum >> 16;
			matches++;
			break;
		}

	null_tag:
		// 原始的Adler-32算法，s1代表着a(k, l)，s2代表着b(k, l)，滚动计算校验和
		/* Trim off the first byte from the checksum */
		map = (signed char *)map_ptr(buf, offset, k + 1);
		s1 -= map[0] + CHAR_OFFSET; // 0
		s2 -= k * (map[0] + CHAR_OFFSET);

		/* Add on the next byte (if there is one) to the checksum */
		if (k < (len - offset))
		{
			s1 += (map[k] + CHAR_OFFSET);
			s2 += s1;
		}
		else
		{
			--k;
		}

	} while (++offset < end); // 到达最后一个块的第一个字节了，不用再计算

	matched(f, s, buf, len, -1);
	map_ptr(buf, len - 1, 1);
}

// s是远程端的，buf是本地的，调用构建表、hash查找、匹配
void match_sums(int f, struct sum_struct *s, struct map_struct *buf, off_t len)
{
	char file_sum[MD4_SUM_LENGTH];

	last_match = 0;
	false_alarms = 0;
	tag_hits = 0;
	matches = 0;
	data_transfer = 0;

	sum_init();

	if (len > 0 && s->count > 0)
	{
		build_hash_table(s);

		if (verbose > 2)
			fprintf(FINFO, "built hash table\n");

		hash_search(f, s, buf, len);

		if (verbose > 2)
			fprintf(FINFO, "done hash search\n");
	}
	else
	{
		matched(f, s, buf, len, -1);
	}

	sum_end(file_sum);

	if (remote_version >= 14)
	{
		if (verbose > 2)
			fprintf(FINFO, "sending file_sum\n");
		write_buf(f, file_sum, MD4_SUM_LENGTH);
	}

	if (targets)
	{
		free(targets);
		targets = NULL;
	}

	if (verbose > 2)
		fprintf(FINFO, "false_alarms=%d tag_hits=%d matches=%d\n",
				false_alarms, tag_hits, matches);

	total_tag_hits += tag_hits;
	total_false_alarms += false_alarms;
	total_matches += matches;
	total_data_transfer += data_transfer;
}

// 根据版本来报告信息
void match_report(void)
{
	if (verbose <= 1)
		return;

	fprintf(FINFO,
			"total: matches=%d  tag_hits=%d  false_alarms=%d  data=%ld\n",
			total_matches, total_tag_hits,
			total_false_alarms, (long)total_data_transfer);
}
