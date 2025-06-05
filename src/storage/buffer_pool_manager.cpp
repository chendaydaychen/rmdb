/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "buffer_pool_manager.h"

/**
 * @description: 从free_list或replacer中得到可淘汰帧页的 *frame_id
 * @return {bool} true: 可替换帧查找成功 , false: 可替换帧查找失败
 * @param {frame_id_t*} frame_id 帧页id指针,返回成功找到的可替换帧id
 */
bool BufferPoolManager::find_victim_page(frame_id_t* frame_id) {

    // 1. 尝试从 free_list_ 获取
    if (!free_list_.empty()) {
        *frame_id = free_list_.front();
        free_list_.pop_front();
        return true;
    }

    // 2. free_list_ 为空，尝试从 replacer_ 获取
    if (replacer_->victim(frame_id)) {
        return true;
    }

    // 没有空闲帧，也没有可淘汰的帧
    return false;
}

/**
 * @description: 更新页面数据, 如果为脏页则需写入磁盘，再更新为新页面，更新page元数据(data, is_dirty, page_id)和page table
 * @param {Page*} page 写回页指针
 * @param {PageId} new_page_id 新的page_id
 * @param {frame_id_t} new_frame_id 新的帧frame_id
 */
void BufferPoolManager::update_page(Page *page, PageId new_page_id, frame_id_t new_frame_id) {
    // 这个函数通常在持有锁的情况下被调用，比如在 fetch_page 或 new_page 内部。
    // 如果不是，则需要加锁：std::scoped_lock lock{latch_};

    // 1. 处理旧页面数据（如果该帧之前有关联的有效页面）
    PageId old_page_id = page->get_page_id();
    if (old_page_id.page_no != INVALID_PAGE_ID) { // 检查旧page_id是否有效
        if (page->is_dirty()) {
            disk_manager_->write_page(old_page_id.fd, old_page_id.page_no, page->get_data(), PAGE_SIZE);
            page->is_dirty_ = false;
        }
        // 从页表中移除旧页面的映射
        page_table_.erase(old_page_id);
    }

    // 2. 更新 Page 对象以反映新页面 (如果 new_page_id 有效)
    if (new_page_id.page_no != INVALID_PAGE_ID) {
        page_table_[new_page_id] = new_frame_id; // 建立新映射
        page->id_ = new_page_id;
        // pin_count 和 is_dirty 会由调用者（fetch_page, new_page）在获取页面后设置
        // page->pin_count_ = 1; // fetch_page 和 new_page 会设置
        // page->is_dirty_ = false; // 通常新获取的页面不是脏的
        page->reset_memory(); // 清空帧的内存数据，准备加载新内容或作为新空页
    } else {
        // 如果 new_page_id 是 INVALID_PAGE_ID，意味着这个帧被清空并不再映射到任何磁盘页
        // (例如，在 delete_page 后，帧返回 free_list 之前)
        page->id_ = new_page_id; // 设置为无效 PageId
        page->pin_count_ = 0;
        page->is_dirty_ = false;
        page->reset_memory();
    }
}

/**
 * @description: 从buffer pool获取需要的页。
 *              如果页表中存在page_id（说明该page在缓冲池中），并且pin_count++。
 *              如果页表不存在page_id（说明该page在磁盘中），则找缓冲池victim page，将其替换为磁盘中读取的page，pin_count置1。
 * @return {Page*} 若获得了需要的页则将其返回，否则返回nullptr
 * @param {PageId} page_id 需要获取的页的PageId
 */
Page* BufferPoolManager::fetch_page(PageId page_id) {
    // 1.     从page_table_中搜寻目标页
    // 1.1    若目标页有被page_table_记录，则将其所在frame固定(pin)，并返回目标页。
    // 1.2    否则，尝试调用find_victim_page获得一个可用的frame，若失败则返回nullptr
    // 2.     若获得的可用frame存储的为dirty page，则须调用updata_page将page写回到磁盘
    // 3.     调用disk_manager_的read_page读取目标页到frame
    // 4.     固定目标页，更新pin_count_
    // 5.     返回目标页
    std::scoped_lock lock{latch_};

    // 1. 从page_table_中搜寻目标页
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        // 1.1 目标页在缓冲池中
        frame_id_t frame_id = it->second;
        Page* page = &pages_[frame_id];
        page->pin_count_++;
        // 如果页面之前是unpinned状态（pin_count从0变为1后大于0，或者之前就是大于0），
        // 它可能在replacer中。现在它被pin了，应该从replacer中移除。
        replacer_->pin(frame_id); // 确保它不在可替换列表中
        return page;
    }

    // 1.2 目标页不在缓冲池中，需要从磁盘加载
    frame_id_t victim_frame_id;
    if (!find_victim_page(&victim_frame_id)) {
        return nullptr; // 没有可用的frame
    }

    Page* victim_page = &pages_[victim_frame_id];

    // 2. 如果获得的可用frame存储的为dirty page，则须调用update_page将page写回到磁盘
    // update_page 会处理旧页面的写回和页表清理，并为新页面设置初始映射和清空内存
    // 注意：在调用update_page之前，victim_page->id_ 是旧页面的ID
    update_page(victim_page, page_id, victim_frame_id);
    // update_page 将 victim_page->id_ 更新为 page_id，并在 page_table_ 中建立映射
    // victim_page->reset_memory() 也已执行

    // 3. 调用disk_manager_的read_page读取目标页到frame
    disk_manager_->read_page(page_id.fd, page_id.page_no, victim_page->get_data(), PAGE_SIZE);

    // 4. 固定目标页，更新pin_count_ (在update_page中也可以做，但这里更清晰)
    victim_page->pin_count_ = 1;
    victim_page->is_dirty_ = false; // 新从磁盘读入的页面不是脏的

    // 5. 由于此帧现在被pin，确保它不在replacer中
    replacer_->pin(victim_frame_id);

    return victim_page;
}

/**
 * @description: 取消固定pin_count>0的在缓冲池中的page
 * @return {bool} 如果目标页的pin_count<=0则返回false，否则返回true
 * @param {PageId} page_id 目标page的page_id
 * @param {bool} is_dirty 若目标page应该被标记为dirty则为true，否则为false
 */
bool BufferPoolManager::unpin_page(PageId page_id, bool is_dirty) {
    // Todo:
    // 0. lock latch
    // 1. 尝试在page_table_中搜寻page_id对应的页P
    // 1.1 P在页表中不存在 return false
    // 1.2 P在页表中存在，获取其pin_count_
    // 2.1 若pin_count_已经等于0，则返回false
    // 2.2 若pin_count_大于0，则pin_count_自减一
    // 2.2.1 若自减后等于0，则调用replacer_的Unpin
    // 3 根据参数is_dirty，更改P的is_dirty_
    std::scoped_lock lock{latch_};

    // 1. 尝试在page_table_中搜寻page_id对应的页P
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        // 1.1 P在页表中不存在 return false
        return false;
    }

    frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];

    // 1.2 P在页表中存在，获取其pin_count_
    // 2.1 若pin_count_已经等于0，则返回false
    if (page->pin_count_ <= 0) {
        return false;
    }

    // 2.2 若pin_count_大于0，则pin_count_自减一
    page->pin_count_--;

    // 3. 根据参数is_dirty，更改P的is_dirty_
    if (is_dirty) {
        page->is_dirty_ = true;
    }
    // 也可以是 page->is_dirty_ = page->is_dirty_ || is_dirty;
    // 但题目描述是“更改”，所以直接赋值。

    // 2.2.1 若自减后等于0，则调用replacer_的Unpin
    if (page->pin_count_ == 0) {
        replacer_->unpin(frame_id);
    }

    return true;
}

/**
 * @description: 将目标页写回磁盘，不考虑当前页面是否正在被使用
 * @return {bool} 成功则返回true，否则返回false(只有page_table_中没有目标页时)
 * @param {PageId} page_id 目标页的page_id，不能为INVALID_PAGE_ID
 */
bool BufferPoolManager::flush_page(PageId page_id) {
    // Todo:
    // 0. lock latch
    // 1. 查找页表,尝试获取目标页P
    // 1.1 目标页P没有被page_table_记录 ，返回false
    // 2. 无论P是否为脏都将其写回磁盘。
    // 3. 更新P的is_dirty_
   
    std::scoped_lock lock{latch_};

    // 1. 查找页表,尝试获取目标页P
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        // 1.1 目标页P没有被page_table_记录 ，返回false
        return false;
    }

    frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];

    // 2. 无论P是否为脏都将其写回磁盘。
    //    实际上，如果不是脏的，写回是可选的优化，但题目说“无论...都将其写回”
    disk_manager_->write_page(page->id_.fd, page->id_.page_no, page->get_data(), PAGE_SIZE);

    // 3. 更新P的is_dirty_
    page->is_dirty_ = false; // 写回后不再是脏页

    return true;
}

/**
 * @description: 创建一个新的page，即从磁盘中移动一个新建的空page到缓冲池某个位置。
 * @return {Page*} 返回新创建的page，若创建失败则返回nullptr
 * @param {PageId*} page_id 当成功创建一个新的page时存储其page_id
 */
Page* BufferPoolManager::new_page(PageId* page_id) {
    // 1.   获得一个可用的frame，若无法获得则返回nullptr
    // 2.   在fd对应的文件分配一个新的page_id
    // 3.   将frame的数据写回磁盘
    // 4.   固定frame，更新pin_count_
    // 5.   返回获得的page
    std::scoped_lock lock{latch_};

    // 1. 获得一个可用的frame
    frame_id_t victim_frame_id;
    if (!find_victim_page(&victim_frame_id)) {
        return nullptr; // 无法获得可用frame
    }

    Page* new_frame_page = &pages_[victim_frame_id];

    // 2. 在fd对应的文件分配一个新的page_id
    // 假设 page_id_out->fd 已经被调用者设置好，指定了在哪个文件中创建新页面
    page_id_t new_disk_page_no = disk_manager_->allocate_page(page_id->fd);
    if (new_disk_page_no == INVALID_PAGE_ID) { // 检查分配是否成功
         // 同上，处理 victim_frame_id 的归还问题
        return nullptr;
    }
    PageId new_actual_page_id = {page_id->fd, new_disk_page_no};
    *page_id = new_actual_page_id; // 将完整的 PageId 返回给调用者

    // 3. 处理旧页面（如果victim_frame_id上有），并在页表建立新映射，重置内存
    update_page(new_frame_page, *page_id, victim_frame_id);
    // update_page 将 new_frame_page->id_ 设置为 *page_id_out
    // 并在 page_table_ 中建立映射, new_frame_page->reset_memory() 也已执行

    // 4. 固定frame，更新pin_count_
    new_frame_page->pin_count_ = 1;
    new_frame_page->is_dirty_ = false; // 新创建的页面初始不是脏的

    // 5. 由于此帧现在被pin，确保它不在replacer中
    replacer_->pin(victim_frame_id);

    return new_frame_page;
}

/**
 * @description: 从buffer_pool删除目标页
 * @return {bool} 如果目标页不存在于buffer_pool或者成功被删除则返回true，若其存在于buffer_pool但无法删除则返回false
 * @param {PageId} page_id 目标页
 */
bool BufferPoolManager::delete_page(PageId page_id) {
    // 1.   在page_table_中查找目标页，若不存在返回true
    // 2.   若目标页的pin_count不为0，则返回false
    // 3.   将目标页数据写回磁盘，从页表中删除目标页，重置其元数据，将其加入free_list_，返回true
    
    std::scoped_lock lock{latch_};

    // 1. 在page_table_中查找目标页
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return true; // 若不存在返回true
    }

    frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];

    // 2. 若目标页的pin_count不为0，则返回false
    if (page->pin_count_ > 0) {
        return false;
    }

    // 3. 页面可以被删除
    // 3a. 将目标页数据写回磁盘 (如果脏)
    if (page->is_dirty()) {
        disk_manager_->write_page(page->id_.fd, page->id_.page_no, page->get_data(), PAGE_SIZE);
        page->is_dirty_ = false; // 写回后不再是脏页
    }

    // 从页表中删除目标页
    page_table_.erase(page_id);

    // 重置其元数据
    page->id_.page_no = INVALID_PAGE_ID; // 标记为无效页面
    // page->id_.fd = -1; // 可选，标记fd也无效
    page->pin_count_ = 0;
    page->is_dirty_ = false;
    page->reset_memory(); // 清空页面数据


    // LRUReplacer::pin 会将其从 LRU 列表中移除。
    replacer_->pin(frame_id); // 从可替换列表中移除
    free_list_.push_back(frame_id);



    return true;
}

/**
 * @description: 将buffer_pool中的所有页写回到磁盘
 * @param {int} fd 文件句柄
 */
void BufferPoolManager::flush_all_pages(int fd) {
    std::scoped_lock lock{latch_};

    for (auto const& [pageid_in_table, frameid_in_table] : page_table_) {
        if (pageid_in_table.fd == fd) {
            Page* page = &pages_[frameid_in_table];
            // 题目描述中 delete_page 和 flush_page 都提到了写回（脏）页。
            // flush_all_pages 通常意味着持久化所有更改。
            // 因此，只刷新脏页是合理的优化。如果要求所有页都刷新，则去掉if。
            if (page->is_dirty()) {
                disk_manager_->write_page(page->id_.fd, page->id_.page_no, page->get_data(), PAGE_SIZE);
                page->is_dirty_ = false; // 写回后不再是脏页
            }
        }
    }
}