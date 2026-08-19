#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"


// 将位图初始化
void bitmap_init(struct bitmap* btmp) {

    memset(btmp->bits, 0, btmp->btmp_bytes_len);

}

// 判断bit_idx位是否为1 若为1 则返回true 否则返回false
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx) {

    uint32_t byte_idx = bit_idx / 8;        // 向下取整用于索引数组下标
    uint32_t bit_odd  = bit_idx % 8;        // 取余得到索引数组里面的位

    return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd));

}

// 在位图中连续申请cnt个位 成功则返回起始位下表 失败则返回-1
int bitmap_scan(struct bitmap* btmp, uint32_t cnt) {

    uint32_t idx_byte = 0;                  // 用于记录空闲位所在字节

    // 逐字比较
    while ((0xff == btmp->bits[idx_byte] && (idx_byte < btmp->btmp_bytes_len))) {

        idx_byte ++;

    }

    ASSERT(idx_byte < btmp->btmp_bytes_len);

    // 若内存池找不到可用空间
    if (idx_byte == btmp->btmp_bytes_len) {
        
        return -1;

    }

    // 如果在位图数组内的某字节找到了空闲位
    // 逐字比较 返回空闲位索引
    int idx_bit = 0;
    // 和btmp->bits[idx_type]这个字节逐位对比
    while ((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]) {

        idx_bit ++;

    }

    int bit_idx_start = idx_byte * 8 + idx_bit;             // 空闲位置下表
    if (cnt == 1) {

        return bit_idx_start;

    }

    uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start);
    // 记录还有多少位可以判断
    uint32_t next_bit = bit_idx_start + 1; 
    uint32_t count = 1;                                     // 用于记录找到空闲位的个数

    bit_idx_start = -1;                                     // 先设置为-1 若找不到就直接返回

    while (bit_left -- > 0) {

        // 若next_bit为0
        if (!(bitmap_scan_test(btmp, next_bit))) {

            count ++;

        } else {

            count = 0;

        }

        // 若找到连续的cnt个空位
        if (count == cnt) {

            bit_idx_start = next_bit - cnt + 1;
            break;

        }

        next_bit ++;

    }

    return bit_idx_start;
    
}

// 将位图btmp的bit_idx位设置为value
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value) {

    ASSERT((value == 0) || (value == 1));
    
    uint32_t byte_idx = bit_idx / 8;            // 向下取整用于索引数组下标
    uint32_t bit_odd  = bit_idx % 8;            // 数组内的位

    // 一般会用0x1这样的数字对字节中的位操作
    // 将1任意移动后再取反 或者先取反再移位 可用来对位置0操作
    if (value) {

         btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd); 

    } else {

         btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);

    }

}
