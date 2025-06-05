/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "lru_replacer.h"

LRUReplacer::LRUReplacer(size_t num_pages) { max_size_ = num_pages; }

LRUReplacer::~LRUReplacer() = default;  

/**
 * @description: 使用LRU策略删除一个victim frame，并返回该frame的id
 * @param {frame_id_t*} frame_id 被移除的frame的id，如果没有frame被移除返回nullptr
 * @return {bool} 如果成功淘汰了一个页面则返回true，否则返回false
 */
bool LRUReplacer::victim(frame_id_t* frame_id) {
    // C++17 std::scoped_lock
    // 它能够避免死锁发生，其构造函数能够自动进行上锁操作，析构函数会对互斥量进行解锁操作，保证线程安全。
    std::scoped_lock lock{latch_};

    // 检查LRUlist_是否为空
    if (LRUlist_.empty()) {
        if (frame_id != nullptr) { // 确保 frame_id 不是空指针
            // *frame_id = INVALID_FRAME_ID; // 或者不修改，取决于接口规范
        }
        return false; // 没有可淘汰的 frame
    }

    // 选择LRUlist_的尾部元素作为牺牲者 (因为约定首部是MRU)
    *frame_id = LRUlist_.back();

    // 从LRUhash_中移除
    LRUhash_.erase(*frame_id);

    // 从LRUlist_中移除
    LRUlist_.pop_back();

    return true;
}

/**
 * @description: 固定指定的frame，即该页面无法被淘汰
 * @param {frame_id_t} 需要固定的frame的id
 */
void LRUReplacer::pin(frame_id_t frame_id) {
    std::scoped_lock lock{latch_};

    // 查找 frame_id 是否在 LRUhash_ (即是否在 unpinned 列表中)
    auto it = LRUhash_.find(frame_id);
    if (it != LRUhash_.end()) {
        // 如果在 unpinned 列表中，则将其移除
        LRUlist_.erase(it->second); // it->second 是指向 LRUlist_ 中元素的迭代器
        LRUhash_.erase(it);         // 从哈希表中移除
    }
    // 如果不在 LRUhash_ 中，说明它已经被 pin 了或者从未 unpin 过，无需操作
}
/**
 * @description: 取消固定一个frame，代表该页面可以被淘汰
 * @param {frame_id_t} frame_id 取消固定的frame的id
 */
void LRUReplacer::unpin(frame_id_t frame_id) {
    std::scoped_lock lock{latch_}; // 支持并发锁

    // 检查 frame_id 是否已在 unpinned 列表中
    if (LRUhash_.count(frame_id)) {
        return;
    }

    // 如果 LRUlist_ 已满 (达到了缓冲池中所有页面都 unpinned 的情况)
    // 这种检查通常不是 LRUReplacer 的责任，而是 BufferPoolManager
    // 在 unpin 之前，如果需要空间，BufferPoolManager 会先调用 victim。
    // 但如果 LRUReplacer 设计为有固定容量限制（max_size_），
    // 并且 unpin 可能导致超出这个限制，则需要处理。
    // 假设 max_size_ 是缓冲池的总大小，LRUlist_ 不应该超过它。
    // if (LRUlist_.size() >= max_size_) {
    //   // 这种情况理论上不应由unpin直接处理淘汰，除非特定设计。
    //   // 通常是BufferPoolManager的职责。
    //   // 如果非要在这里处理，可能需要先victim一个，但这改变了unpin的单纯语义。
    //   // 或者抛出异常，表示 LRU 列表已达到其最大容量。
    //   // 对于一个标准的LRU，它只负责追踪unpinned的帧，不强制执行池的大小限制。
    //   return; // 或者抛出错误，或者移除最旧的，取决于具体要求。
    //             // 简单起见，这里先不处理，假设BufferPoolManager会正确管理。
    // }


    // 将 frame_id 加入到 LRUlist_ 的首部 (MRU 端)
    LRUlist_.push_front(frame_id);
    // 在 LRUhash_ 中记录其位置
    LRUhash_[frame_id] = LRUlist_.begin();
}

/**
 * @description: 获取当前replacer中可以被淘汰的页面数量
 */
size_t LRUReplacer::Size() { return LRUlist_.size(); }
