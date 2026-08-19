#include "stdint.h"
#include "inode.h"
#include "super_block.h"
#include "string.h"
#include "ide.h"
#include "thread.h"
#include "interrupt.h"
#include "stdio-kernel.h"
#include "debug.h"
#include "fs.h"
#include "file.h"

// 用于存储inode位置
struct inode_position {

    bool two_sec;           // inode是否跨区
    uint32_t sec_lba;       // inode所在的扇区号
    uint32_t off_size;      // inode所在扇区的偏移地址

};

// 获取inode所在的扇区和扇区内偏移量
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos) {

    // inode_table在硬盘上是连续的
    ASSERT(inode_no < 4096);
    uint32_t inode_table_lba = part->sb->inode_table_lba;

    uint32_t inode_size = sizeof(struct inode);
    uint32_t off_size = inode_no *  inode_size;

    // 第inode_no号I节点相对于inode_table_lba的字节偏移量
    uint32_t off_sec = off_size / 512;

    // 第inode_no号相对与inode_table_lba的扇区偏移量
    uint32_t off_size_in_sec = off_size % 512;

    // 判断此i节点是否跨越2个扇区
    uint32_t left_in_sec = 512 - off_size_in_sec;

    if (left_in_sec < inode_size) {

        // 如果剩下空间不够那肯定是两个扇区
        inode_pos->two_sec = true;

    } else {

        inode_pos->two_sec = false;

    }

    inode_pos->sec_lba = inode_table_lba + off_sec;
    inode_pos->off_size = off_size_in_sec;

}

// 将inode写入分区part
void inode_sync(struct partition* part, struct inode* inode, void* io_buf) {

    // io_buf是用于硬盘io缓冲区
    uint8_t inode_no = inode->i_no;
    struct inode_position inode_pos;

    // inode位置的信息会存入inode_pos
    inode_locate(part, inode_no, &inode_pos);
    ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

    // 硬盘中的inode成员inode_tag个i_open_cnts是不需要的 只是记录位置链表和进程共享
    struct inode pure_inode;
    memcpy(&pure_inode, inode, sizeof(struct inode));

    // 给那几个变量清零
    pure_inode.i_open_cnts = 0;
    // 置为false 已保证在硬盘中读出为可写
    pure_inode.write_deny = false;
    pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

    char* inode_buf = (char*)io_buf;
    if (inode_pos.two_sec) {

        // 如果跨了两个扇区 就要读出两个扇区再写入两个扇区
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);

        // inode再format中写入硬盘是连续写入的 所以读入2块扇区
        // 开始将待写入的inode拼入到这2个扇区中的相应位置
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));

        // 将拼接好的数据再写入磁盘
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);

    } else {

        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);

    }

}

// 根据i节点号返回对应的i节点
struct inode* inode_open(struct partition* part, uint32_t inode_no) {

    // 现在打开的inode链表中找到inode 此链表是为提速创建的缓冲区
    struct list_elem* elem = part->open_inodes.head.next;
    struct inode* inode_found;

    while (elem != &part->open_inodes.tail) {
        
        inode_found = elem2entry(struct inode, inode_tag, elem);

        if (inode_found->i_no == inode_no) {

            inode_found->i_open_cnts ++;
            return inode_found;

        }
        
        elem = elem->next;

    }

    // 由于open_inodes链表找不到 下面从硬盘上读入inode并加入到此链表
    struct inode_position inode_pos;

    // inode位置信息会存入inode_pos 包括inode所在扇区地址和扇区内的字节偏移量
    inode_locate(part, inode_no, &inode_pos);

    // 为使通过sys_malloc创建新的inode被所有任务共享 需要将inode置于内核空间 需要零时cur->pgdir == NULL
    struct task_struct* cur = running_thread();
    uint32_t* cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;

    // 以上三行代码完成后下面分配的内存将位于内核区
    inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
    
    // 恢复pddir
    cur->pgdir = cur_pagedir_bak;

    char* inode_buf;

    if (inode_pos.two_sec) {

        // 考虑跨扇区情况
        inode_buf = (char*)sys_malloc(1024);

        // 因为i节点是被partition_format函数连续写入扇区的 所以下面可以连续读出来
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);

    } else {

        // 否则所查找的inode扇区未跨扇区 一个扇区大小的缓冲区足够
        inode_buf = (char*)sys_malloc(512);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);

    }

    memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));
    list_push(&part->open_inodes, &inode_found->inode_tag);
    inode_found->i_open_cnts = 1;

    sys_free(inode_buf);

    return inode_found;

}

// 关闭inode或减少inode的打开数
void inode_close(struct inode* inode) {

    // 若没有进程打开此文件 将此inode去掉并释放空间
    enum intr_status old_status = intr_disable();

    if (-- inode->i_open_cnts == 0) {

        // 将i节点从part->open_inodes中去掉
        list_remove(&inode->inode_tag);

        // inode_open时为实现inode被所有进程共享 
        // 已在sys_malloc为inode分配了内核空间
        // 释放inode时也要确保释放的是内核内存池
        struct task_struct* cur = running_thread();
        uint32_t* cur_pagedir_bak = cur->pgdir;
        cur->pgdir = NULL;
        sys_free(inode);
        cur->pgdir = cur_pagedir_bak;

    }

    intr_set_status(old_status);

}

// 将硬盘part上的inode清空
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf) {

    ASSERT(inode_no < 4096);
    struct inode_position inode_pos;

    // inode位置信息会存入inode_pos
    inode_locate(part, inode_no, &inode_pos);
    
    char* inode_buf = (char*)io_buf;
    if (inode_pos.two_sec) {

        // 如果跨区 读入两个扇区
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);

        // 将inode_buf清零
        memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
        
        // 写入磁盘
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);

    }  else {

        // 未跨区 只写入一个磁盘
                // 如果跨区 读入两个扇区
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);

        // 将inode_buf清零
        memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
        
        // 写入磁盘
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);

    }

}

// 回收inode的数据块和inode本身
void inode_release(struct partition* part, uint32_t inode_no) {

    struct inode* inode_to_del = inode_open(part, inode_no);
    ASSERT(inode_to_del->i_no == inode_no);

    // 1 回收inode占用的所有块
    uint8_t block_idx = 0, block_cnt = 12;
    uint32_t block_bitmap_idx;
    uint32_t all_blocks[140] = {0};

    // a 先将12个直接快存入all_blocks
    while (block_idx < 12) {

        all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
        block_idx ++;

    }
    // b 如果一级间接块存在 将其128个间接块读到all_block[12~] 并释放一级间接块所占的扇区
    if (inode_to_del->i_sectors[12] != 0) {

        ide_read(part->my_disk, inode_to_del->i_sectors[12], all_blocks + 12, 1);
        block_cnt = 140;

        // 回收一级间接块所占的扇区
        block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
        ASSERT(block_bitmap_idx > 0);
        bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

    }
    // c inode所有的块地址已经收集到all_blocks中 下面逐个回收
    block_idx = 0;
    while (block_idx < block_cnt) {

        if (all_blocks[block_idx] != 0) {

            block_bitmap_idx = 0;
            block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
            ASSERT(block_bitmap_idx > 0 );
            bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

        }
        block_idx ++;

    }
    // 2回收改inode所占用的inode
    bitmap_set(&part->inode_bitmap, inode_no, 0);
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    // debug
    // 将inode_table中将此inode清零
    void* io_buf = sys_malloc(1024);
    inode_delete(part, inode_no, io_buf);
    sys_free(io_buf);

    inode_close(inode_to_del);

 }

// 初始化new_inode
void inode_init(uint32_t inode_no, struct inode* new_inode) {

    new_inode->i_no = inode_no;
    new_inode->i_size = 0;
    new_inode->i_open_cnts = 0;
    new_inode->write_deny = false;

    // 初始化块索引数组i_sector
    uint8_t sec_idx = 0;
    while (sec_idx < 13) {

        // i_sectors[12] 为一级简介地址
        new_inode->i_sectors[sec_idx] = 0;
        sec_idx ++;

    }

}
