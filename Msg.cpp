#include "Msg.hpp"

#include <atomic>

namespace PolyM {

namespace {

MsgUID generateUniqueId()
{
    // 生成UID的标识符，从0开始计数，线程安全
    static std::atomic<MsgUID> i(0);
    return ++i;
}

}

// 根据类型构造一个message
Msg::Msg(int msgId)
  : msgId_(msgId), uniqueId_(generateUniqueId())
{
}

std::unique_ptr<Msg> Msg::move()
{
    // 返回一个MSG类型的指针，使用了移动构造函数，新建一个临时的副本，move用来减少拷贝构造的事件损失
    return std::unique_ptr<Msg>(new Msg(std::move(*this)));
}

int Msg::getMsgId() const
{
    return msgId_;
}

MsgUID Msg::getUniqueId() const
{
    return uniqueId_;
}

}
