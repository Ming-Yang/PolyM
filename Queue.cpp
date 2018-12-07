#include "Queue.hpp"

#include <chrono>
#include <condition_variable>
#include <queue>
#include <map>
#include <mutex>
#include <utility>

namespace PolyM {

class Queue::Impl
{
public:
    Impl()
      : queue_(), queueMutex_(), queueCond_(), responseMap_(), responseMapMutex_()
    {
    }

    void put(Msg&& msg)
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            queue_.push(msg.move());
        }

        queueCond_.notify_one();
    }

    std::unique_ptr<Msg> get(int timeoutMillis)
    {
        std::unique_lock<std::mutex> lock(queueMutex_);

        // 如果没有规定时间，则一直等待直到队列不空
        // 如果有规定时间，超时则获得一个超时的MSG
		if (timeoutMillis <= 0)
		{
			// lambda表达式，wait变量为真的时候唤醒，函数返回queue不空的时候为真
			queueCond_.wait(lock, [this] {return !queue_.empty(); });
		}
        else
        {
            // wait_for returns false if the return is due to timeout
            auto timeoutOccured = !queueCond_.wait_for(
                lock,
                // C++11 提供的chrono时间类，需要传入这样参数
                std::chrono::milliseconds(timeoutMillis),
                [this]{return !queue_.empty();});

            if (timeoutOccured)
                queue_.emplace(new Msg(MSG_TIMEOUT));
        }

        // 依然是提供一个MSG副本的指针
        auto msg = queue_.front()->move();
        queue_.pop();
        return msg;
    }

    std::unique_ptr<Msg> request(Msg&& msg)
    {
        // 新建临时队列
        // Construct an ad hoc Queue to handle response Msg
        std::unique_lock<std::mutex> lock(responseMapMutex_);
        // emplace可以减少拷贝产生的开销。map的emplace返回一组pair,first->位置的迭代器,second->true or false
        auto it = responseMap_.emplace(
            std::make_pair(msg.getUniqueId(), std::unique_ptr<Queue>(new Queue))).first;
        lock.unlock();

        put(std::move(msg));

        auto response = it->second->get(); // Block until response is put to the response Queue

        lock.lock();
        responseMap_.erase(it); // Delete the response Queue
        lock.unlock();

        return response;
    }

    void respondTo(MsgUID reqUid, Msg&& responseMsg)
    {
        std::lock_guard<std::mutex> lock(responseMapMutex_);
        if (responseMap_.count(reqUid) > 0)
            responseMap_[reqUid]->put(std::move(responseMsg));
    }

private:
    // Queue for the Msgs
    std::queue<std::unique_ptr<Msg>> queue_;

    // Mutex to protect access to the queue
    std::mutex queueMutex_;

    // Condition variable to wait for when getting Msgs from the queue
    std::condition_variable queueCond_;

    // Map to keep track of which response handler queues are associated with which request Msgs
    std::map<MsgUID, std::unique_ptr<Queue>> responseMap_;

    // Mutex to protect access to response map
    std::mutex responseMapMutex_;
};

Queue::Queue()
  : impl_(new Impl)
{
}

Queue::~Queue()
{
}

// 为什么这样封装了一层呢
// 封装了新建对象的过程(构造函数)
void Queue::put(Msg&& msg)
{
    impl_->put(std::move(msg));
}

std::unique_ptr<Msg> Queue::get(int timeoutMillis)
{
    return impl_->get(timeoutMillis);
}

std::unique_ptr<Msg> Queue::request(Msg&& msg)
{
    return impl_->request(std::move(msg));
}

void Queue::respondTo(MsgUID reqUid, Msg&& responseMsg)
{
    impl_->respondTo(reqUid, std::move(responseMsg));
}

}
