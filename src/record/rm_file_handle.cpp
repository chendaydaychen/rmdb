/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "rm_file_handle.h"

/**
 * @description: 获取当前表中记录号为rid的记录
 * @param {Rid&} rid 记录号，指定记录的位置
 * @param {Context*} context
 * @return {unique_ptr<RmRecord>} rid对应的记录对象指针
 */
std::unique_ptr<RmRecord> RmFileHandle::get_record(const Rid& rid, Context* context) const {
    // 1. 获取指定记录所在的page handle
    // 2. 初始化一个指向RmRecord的指针（赋值其内部的data和size）

    // 1. 获取指定记录所在的page handle
    RmPageHandle ph = fetch_page_handle(rid.page_no); // Page is pinned

    // 检查 slot_no 是否有效
    if (rid.slot_no < 0 || rid.slot_no >= file_hdr_.num_records_per_page) {
        buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false);
        throw RMDBError("Invalid slot number: " + std::to_string(rid.slot_no));
    }

    // 检查记录是否存在
    if (!Bitmap::is_set(ph.bitmap, rid.slot_no)) {
        buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false);
        throw RecordNotFoundError(rid.page_no, rid.slot_no);
        // 或者 return nullptr; 根据接口设计
    }

    // 2. 初始化一个指向RmRecord的指针
    auto record = std::make_unique<RmRecord>(file_hdr_.record_size);
    char* slot_location = ph.get_slot(rid.slot_no);
    memcpy(record->data, slot_location, file_hdr_.record_size);
    // record->size 已经在构造时设置

    // Unpin the page
    buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false); // Read-only, so not dirty

    return record;
}

/**
 * @description: 在当前表中插入一条记录，不指定插入位置
 * @param {char*} buf 要插入的记录的数据
 * @param {Context*} context
 * @return {Rid} 插入的记录的记录号（位置）
 */
Rid RmFileHandle::insert_record(char* buf, Context* context) {
    // 1. 获取当前未满的page handle
    // 2. 在page handle中找到空闲slot位置
    // 3. 将buf复制到空闲slot位置
    // 4. 更新page_handle.page_hdr中的数据结构
    // 注意考虑插入一条记录后页面已满的情况，需要更新file_hdr_.first_free_page_no

    // 1. 获取当前未满的page handle
    RmPageHandle ph = create_page_handle(); // Page is pinned

    // 2. 在page handle中找到空闲slot位置
    int slot_no = Bitmap::first_bit(false, ph.bitmap, file_hdr_.num_records_per_page);

    // Sanity check: create_page_handle should give a page with free slots.
    if (slot_no == file_hdr_.num_records_per_page || ph.page_hdr->num_records >= file_hdr_.num_records_per_page) {
        // This case should ideally not happen if create_page_handle and free list management are correct.
        // If it does, unpin and throw error or try to recover.
        buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false); // No modification yet
        throw RMDBError("Failed to find a free slot in a supposedly free page.");
    }

    // 3. 将buf复制到空闲slot位置
    char* slot_location = ph.get_slot(slot_no);
    memcpy(slot_location, buf, file_hdr_.record_size);

    // 4. 更新page_handle.page_hdr中的数据结构和bitmap
    Bitmap::set(ph.bitmap, slot_no);
    ph.page_hdr->num_records++;
    buffer_pool_manager_->mark_dirty(ph.page);

    // 注意考虑插入一条记录后页面已满的情况，需要更新file_hdr_.first_free_page_no
    if (ph.page_hdr->num_records == file_hdr_.num_records_per_page) {
        // 页面已满，需要从文件空闲链表中移除（如果它在的话）
        int current_page_no = ph.page->get_page_id().page_no;
        if (file_hdr_.first_free_page_no == current_page_no) {
            file_hdr_.first_free_page_no = ph.page_hdr->next_free_page_no;
            // 将更新后的 file_hdr_ 写回磁盘
            disk_manager_->write_page(fd_, RM_FILE_HDR_PAGE, reinterpret_cast<char*>(&file_hdr_), sizeof(file_hdr_));
        }
        // 如果它不在链表头，但仍在链表中间（对于更复杂的空闲链表），也需要处理。
        // 对于简单链表，如果它不是头，它就不应该被认为是“first_free”。
        // 此页满了，其自身的next_free_page_no通常不重要了，或者可以设为RM_NO_PAGE。
        // ph.page_hdr->next_free_page_no = RM_NO_PAGE; // 可选
    }

    Rid rid = {ph.page->get_page_id().page_no, slot_no};
    buffer_pool_manager_->unpin_page(ph.page->get_page_id(), true); // Page was modified

    return rid;
}

/**
 * @description: 在当前表中的指定位置插入一条记录
 * @param {Rid&} rid 要插入记录的位置
 * @param {char*} buf 要插入记录的数据
 */
void RmFileHandle::insert_record(const Rid& rid, char* buf) {
    RmPageHandle ph = fetch_page_handle(rid.page_no); // Page is pinned

    if (rid.slot_no < 0 || rid.slot_no >= file_hdr_.num_records_per_page) {
        buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false);
        throw RMDBError("Invalid slot number for insert: " + std::to_string(rid.slot_no));
    }

    // 检查槽位是否已经是set状态，如果是，说明是覆盖或者需要特殊处理
    // 对于恢复或回滚，可能就是期望覆盖一个已删除的标记或写入一个之前的值
    bool was_set = Bitmap::is_set(ph.bitmap, rid.slot_no);

    char* slot_location = ph.get_slot(rid.slot_no);
    memcpy(slot_location, buf, file_hdr_.record_size);

    if (!was_set) { // 如果之前槽位是空的 (0), 现在填入记录
        Bitmap::set(ph.bitmap, rid.slot_no);
        ph.page_hdr->num_records++;
    } else {
        // 如果槽位已经是 set (1), 再次 set 没影响。num_records 不增加。
        Bitmap::set(ph.bitmap, rid.slot_no); // 确保它是set状态
    }
    buffer_pool_manager_->mark_dirty(ph.page);

    buffer_pool_manager_->unpin_page(ph.page->get_page_id(), true); // Page was modified
}

/**
 * @description: 删除记录文件中记录号为rid的记录
 * @param {Rid&} rid 要删除的记录的记录号（位置）
 * @param {Context*} context
 */
void RmFileHandle::delete_record(const Rid& rid, Context* context) {
    // 1. 获取指定记录所在的page handle
    // 2. 更新page_handle.page_hdr中的数据结构
    // 注意考虑删除一条记录后页面未满的情况，需要调用release_page_handle()
    // 1. 获取指定记录所在的page handle
    RmPageHandle ph = fetch_page_handle(rid.page_no); // Page is pinned

    if (rid.slot_no < 0 || rid.slot_no >= file_hdr_.num_records_per_page) {
        buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false);
        throw RMDBError("Invalid slot number for delete: " + std::to_string(rid.slot_no));
    }

    if (!Bitmap::is_set(ph.bitmap, rid.slot_no)) {
        buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false); // No modification
        throw RecordNotFoundError(rid.page_no, rid.slot_no);
    }

    // 2. 更新page_handle.page_hdr中的数据结构 (bitmap 和 num_records)
    bool was_full = (ph.page_hdr->num_records == file_hdr_.num_records_per_page);

    Bitmap::reset(ph.bitmap, rid.slot_no);
    ph.page_hdr->num_records--;
    buffer_pool_manager_->mark_dirty(ph.page);

    // 注意考虑删除一条记录后页面从未满变满（不可能），或从满变未满的情况
    // 如果页面之前是满的，现在由于删除而有空闲空间了，需要将其加入空闲列表
    if (was_full && ph.page_hdr->num_records < file_hdr_.num_records_per_page) {
        release_page_handle(ph); // release_page_handle会处理将其加入全局空闲列表的逻辑
                                 // 并标记file_hdr脏（如果需要立即写回）
    }

    buffer_pool_manager_->unpin_page(ph.page->get_page_id(), true); // Page was modified
}


/**
 * @description: 更新记录文件中记录号为rid的记录
 * @param {Rid&} rid 要更新的记录的记录号（位置）
 * @param {char*} buf 新记录的数据
 * @param {Context*} context
 */
void RmFileHandle::update_record(const Rid& rid, char* buf, Context* context) {
    // 1. 获取指定记录所在的page handle
    // 2. 更新记录
    // 1. 获取指定记录所在的page handle
    RmPageHandle ph = fetch_page_handle(rid.page_no); // Page is pinned

    if (rid.slot_no < 0 || rid.slot_no >= file_hdr_.num_records_per_page) {
        buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false);
        throw RMDBError("Invalid slot number for update: " + std::to_string(rid.slot_no));
    }

    if (!Bitmap::is_set(ph.bitmap, rid.slot_no)) {
        buffer_pool_manager_->unpin_page(ph.page->get_page_id(), false); // No modification
        throw RecordNotFoundError(rid.page_no, rid.slot_no);
    }

    // 2. 更新记录
    char* slot_location = ph.get_slot(rid.slot_no);
    memcpy(slot_location, buf, file_hdr_.record_size);
    buffer_pool_manager_->mark_dirty(ph.page);

    buffer_pool_manager_->unpin_page(ph.page->get_page_id(), true); // Page was modified
}

/**
 * 以下函数为辅助函数，仅提供参考，可以选择完成如下函数，也可以删除如下函数，在单元测试中不涉及如下函数接口的直接调用
*/
/**
 * @description: 获取指定页面的页面句柄
 * @param {int} page_no 页面号
 * @return {RmPageHandle} 指定页面的句柄
 */
RmPageHandle RmFileHandle::fetch_page_handle(int page_no) const {
    // 检查page_no的有效性
    // 文件头页是 RM_FILE_HDR_PAGE (通常是0)。数据页从1开始。
    if (page_no <= RM_FILE_HDR_PAGE || page_no >= file_hdr_.num_pages) {
        throw PageNotExistError(disk_manager_->get_file_name(fd_), page_no);
    }

    // 使用缓冲池获取指定页面
    Page* page = buffer_pool_manager_->fetch_page({fd_, page_no});
    if (page == nullptr) {
        // fetch_page 失败，可能是缓冲池满了且无法替换，或者其他错误
        // 这种情况通常会由 buffer_pool_manager 内部处理或抛出异常
        // 如果它返回 nullptr，我们需要决定如何处理。这里抛出一个通用错误。
        throw RMDBError("Failed to fetch page " + std::to_string(page_no) + " from buffer pool for file fd " + std::to_string(fd_));
    }

    // 生成page_handle返回给上层
    return RmPageHandle(&file_hdr_, page);}

/**
 * @description: 创建一个新的page handle
 * @return {RmPageHandle} 新的PageHandle
 */
RmPageHandle RmFileHandle::create_new_page_handle() {
    // 1. 使用缓冲池来创建一个新page
    PageId new_pid_out;
    new_pid_out.fd = fd_; // 指定文件描述符
    // new_pid_out.page_no 会被 buffer_pool_manager_->new_page() 填充

    Page* new_page_ptr = buffer_pool_manager_->new_page(&new_pid_out);
    if (new_page_ptr == nullptr) {
        throw RMDBError("Failed to create a new page in buffer pool for fd " + std::to_string(fd_));
    }

    // 2. 更新page handle中的相关信息 (通过构造RmPageHandle并初始化其元数据)
    RmPageHandle new_page_h(&file_hdr_, new_page_ptr);

    // 初始化新页面的 RmPageHdr
    new_page_h.page_hdr->num_records = 0;
    // 新创建的页面，通常不立即加入空闲列表，除非它有空间。
    // 它如何加入空闲列表取决于插入逻辑，或者它是否已经是第一个有空间的页。
    // 暂时将其 next_free_page_no 设为 RM_NO_PAGE。
    new_page_h.page_hdr->next_free_page_no = RM_NO_PAGE;

    // 初始化位图 (所有槽位都为空)
    Bitmap::init(new_page_h.bitmap, file_hdr_.num_records_per_page); // 假设 Bitmap::init 将所有位清零
    // 3.更新file_hdr_
    if (new_pid_out.page_no >= file_hdr_.num_pages) {
        file_hdr_.num_pages = new_pid_out.page_no + 1;
    }
    // file_hdr_ 的持久化由 RmManager::close_file 处理

    // 标记页面为脏页，因为我们修改了它的头部和位图
    buffer_pool_manager_->mark_dirty(new_page_ptr);

    return new_page_h;
}

/**
 * @brief 创建或获取一个空闲的page handle
 *
 * @return RmPageHandle 返回生成的空闲page handle
 * @note pin the page, remember to unpin it outside!
 */
RmPageHandle RmFileHandle::create_page_handle() {
    // 1. 判断file_hdr_中是否还有空闲页
    if (file_hdr_.first_free_page_no != RM_NO_PAGE) {
        // 1.2 有空闲页：直接获取第一个空闲页
        // 需要确保这个页面确实有空闲槽位，或者其 page_hdr->num_records < file_hdr_.num_records_per_page
        RmPageHandle free_page_h = fetch_page_handle(file_hdr_.first_free_page_no);
        // 通常，在空闲链表中的页面应该是有空闲空间的。
        return free_page_h;
    } else {
        // 1.1 没有空闲页：使用缓冲池来创建一个新page；可直接调用create_new_page_handle()
        return create_new_page_handle();
    }
}

/**
 * @description: 当一个页面从没有空闲空间的状态变为有空闲空间状态时，更新文件头和页头中空闲页面相关的元数据
 */
void RmFileHandle::release_page_handle(RmPageHandle&page_handle) {
    int current_page_no = page_handle.page->get_page_id().page_no;
    // 1. page_handle.page_hdr->next_free_page_no 指向旧的链表头
    page_handle.page_hdr->next_free_page_no = file_hdr_.first_free_page_no;

    // 2. file_hdr_.first_free_page_no 更新为当前页面
    file_hdr_.first_free_page_no = current_page_no;

    // 标记页面为脏页，因为修改了它的 page_hdr
    buffer_pool_manager_->mark_dirty(page_handle.page);
    // file_hdr_ 的持久化由 RmManager::close_file 负责
}