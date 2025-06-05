/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "rm_scan.h"
#include "rm_file_handle.h"

/**
 * @brief 初始化file_handle和rid
 * @param file_handle
 */
RmScan::RmScan(const RmFileHandle *file_handle) : file_handle_(file_handle) {
    // 初始化file_handle和rid（指向第一个存放了记录的位置）
    // 初始化rid，使其指向第一个可能存在记录的位置之前
    // 第一个数据页是 RM_FIRST_RECORD_PAGE
    if (file_handle_ == nullptr) {
        rid_ = {RM_NO_PAGE, -1};
        return;
    }

    rid_.page_no = RM_FIRST_RECORD_PAGE; // 从第一个数据页开始
    rid_.slot_no = -1;                  // 从第一个槽位之前开始搜索

    // 调用next()来定位到第一个实际的记录（如果存在）
    next();
    // 构造函数执行完毕后，rid_ 要么指向第一个记录，要么 is_end() 会是 true
}

/**
 * @brief 找到文件中下一个存放了记录的位置
 */
void RmScan::next() {
    // 找到文件中下一个存放了记录的非空闲位置，用rid_来指向这个位置
    if (is_end()) { // 如果已经结束，则不进行任何操作
        return;
    }

    RmFileHdr hdr = file_handle_->get_file_hdr();

    while (rid_.page_no < hdr.num_pages && rid_.page_no >= RM_FIRST_RECORD_PAGE) {
        RmPageHandle ph = file_handle_->fetch_page_handle(rid_.page_no); // Page is pinned
        // 在当前页的 bitmap 中查找下一个为 'true' (已设置) 的位
        int next_slot = Bitmap::next_bit(true, ph.bitmap, hdr.num_records_per_page, rid_.slot_no);

        if (next_slot < hdr.num_records_per_page) {
            // 在当前页面找到了下一个记录
            rid_.slot_no = next_slot;
            file_handle_->buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false); // Unpin
            return; // 找到记录，结束 next()
        }

        // 当前页面没有更多记录了，unpin 页面并尝试下一页
        file_handle_->buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false); // Unpin
        rid_.page_no++;
        rid_.slot_no = -1; // 为下一页的扫描重置 slot_no
    }

    // 如果循环结束（没有在任何页面找到记录，或者 page_no 超出范围）
    // 表示到达文件末尾或者没有更多记录
    rid_.page_no = RM_NO_PAGE; // 使用一个特殊值标记结束
    rid_.slot_no = -1;

}

/**
 * @brief ​ 判断是否到达文件末尾
 */
bool RmScan::is_end() const {
    // 如果 page_no 是 RM_NO_PAGE，明确表示结束
    if (rid_.page_no == RM_NO_PAGE) {
        return true;
    }
    // 或者，如果 file_handle_ 无效，也认为是结束
    if (file_handle_ == nullptr) {
        return true;
    }
    // 否则，根据当前 page_no 是否超出文件范围判断
    // RmFileHdr hdr = file_handle_->get_file_hdr(); // 获取最新的文件头
    // return rid_.page_no >= hdr.num_pages;
    // 上述判断可能不完全准确，因为 next() 已经将 page_no 设置为 RM_NO_PAGE
    // 所以，最简单的判断就是 rid_.page_no == RM_NO_PAGE
    return rid_.page_no == RM_NO_PAGE;
}

/**
 * @brief RmScan内部存放的rid
 */
Rid RmScan::rid() const {
    return rid_;
}