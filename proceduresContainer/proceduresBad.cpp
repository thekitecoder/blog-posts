#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#define LOG(X) std::cout << "[" << prefix << "]: " << X

enum class MsgId
{
    DeleteEntity,
    DeleteResource,
    DeleteResourceAck,
    DeleteResourceFinished
};

const std::unordered_map<MsgId, const char*> msgIdToStr{
    {MsgId::DeleteEntity, "DeleteEntity"},
    {MsgId::DeleteResource, "DeleteResource"},
    {MsgId::DeleteResourceAck, "DeleteResourceAck"},
    {MsgId::DeleteResourceFinished, "DeleteResourceFinished"}};

std::ostream& operator<<(std::ostream& stream, MsgId id)
{
    stream << msgIdToStr.at(id);
    return stream;
}

using Callback = std::function<void()>;

struct MessageDispatcher
{
    void registerForMsg(MsgId id, Callback callback)
    {
        msgIdToCallback[id].push_back(callback);
    }

    void queue(MsgId id)
    {

        msgs.push_front(id);
        LOG("Queue msg: " << id);
        std::cout << ". Currently on queue: ";
        for (auto const& msgId : msgs)
        {
            std::cout << msgId << ", ";
        }
        std::cout << std::endl;
    }

    void handleMsgFromQueue()
    {
        if (!msgs.empty())
        {
            auto msgId = msgs.back();
            LOG("Start handling msg: " << msgId << std::endl);
            auto callbackListIt = msgIdToCallback.find(msgId);
            if (callbackListIt != msgIdToCallback.end())
            {
                auto& [msgId, callbackList] = *callbackListIt;
                for (auto& callback : callbackList)
                {
                    callback();
                }
            }
            msgs.pop_back();
            LOG("Finished handling msg: " << msgId << std::endl);
        }
    }

    std::deque<MsgId> msgs{};
    std::unordered_map<MsgId, std::vector<Callback>> msgIdToCallback{};
    static constexpr const char* prefix = "MessageDispatcher";
};

struct RemoteResourceOwner
{
    RemoteResourceOwner(MessageDispatcher& dispatcher) : dispatcher{dispatcher}
    {
        dispatcher.registerForMsg(MsgId::DeleteResource,
                                  [this]() { handleResourceDelete(); });
    }
    void handleResourceDelete() { LOG("Handle Resource delete\n"); };

    void finishResourceDeletion()
    {
        LOG("Finished resource deletion - sending Ack\n");
        dispatcher.queue(MsgId::DeleteResourceAck);
    }

    MessageDispatcher& dispatcher;
    static constexpr const char* prefix = "RemoteResourceOwner";
};

struct ResourceDeleter
{
    ResourceDeleter(MessageDispatcher& dispatcher) : dispatcher{dispatcher} {}

    ~ResourceDeleter() { LOG("~ResourceDeleter()\n"); }
    void startDeletion(Callback callback)
    {
        LOG("Send deletion req to remote owner\n");

        dispatcher.queue(MsgId::DeleteResource);
        dispatcher.registerForMsg(MsgId::DeleteResourceAck, [this, callback]()
                                  { onResourceDeletion(callback); });
    }

    void onResourceDeletion(Callback callback)
    {
        LOG("Entering onResourceDeletion. Executing callback.\n");
        callback();
        LOG("After callback I want to access some data...\n");
        LOG("I don't know it yet, but I was deleted in the callback() - "
            "accessing mine own data will crash! "
            << *someValue << std::endl);
    }

    std::unique_ptr<int> someValue = std::make_unique<int>(12345);
    MessageDispatcher& dispatcher;
    static constexpr const char* prefix = "ResourceDeleter";
};

struct ResourceDeleterContainer
{
    ResourceDeleterContainer(MessageDispatcher& dispatcher)
        : dispatcher{dispatcher}
    {
        dispatcher.registerForMsg(MsgId::DeleteResourceFinished,
                                  [this]() { removeResourceDeleter(); });
    }
    void startDeletion(Callback CALLBACK)
    {
        auto [it, succesful] = resourceDeleters.emplace(
            std::piecewise_construct, std::forward_as_tuple(id),
            std::forward_as_tuple(dispatcher));
        if (succesful)
        {
            LOG("Resource deleter created succesfully - starting "
                "resource deletion\n");
            auto& [_, deleter] = *it;
            auto callback = [this, id = id, CALLBACK]()
            {
                LOG("Executing CALLBACK()\n");
                CALLBACK();
                LOG("After CALLBACK()\n");
                LOG("id in callback is " << id << std::endl);
                if (1 == resourceDeleters.erase(id))
                {
                    LOG("Removed resource\n");
                }
                else
                {
                    LOG("Did not remove resource\n");
                }
            };
            deleter.startDeletion(callback);
            ++id;
        }
    }

    void removeResourceDeleter()
    {
        LOG("Removing resourceDeleter\n");
        resourceDeleters.clear();
    }
    MessageDispatcher& dispatcher;
    std::unordered_map<int, ResourceDeleter> resourceDeleters;
    int id = 0;
    static constexpr const char* prefix = "ResourceDeleterContainer";
};

struct Handler
{
    Handler(MessageDispatcher& dispatcher,
            ResourceDeleterContainer& resourceDeleterContainer)
        : dispatcher{dispatcher},
          resourceDeleterContainer(resourceDeleterContainer)
    {
        dispatcher.registerForMsg(MsgId::DeleteEntity,
                                  [this]() { handleDeleteEntityMsg(); });
    }
    void handleDeleteEntityMsg()
    {
        LOG("Handle deleteEntity msg\n");
        resourceDeleterContainer.startDeletion(
            []()
            {
                LOG("Resource deletion handled, can continue entity "
                    "deletion.\n");
            });
    }
    MessageDispatcher& dispatcher;
    ResourceDeleterContainer& resourceDeleterContainer;
    static constexpr const char* prefix = "Handler";
};

int main()
{
    MessageDispatcher dispatcher{};
    RemoteResourceOwner remoteOwner{dispatcher};
    ResourceDeleterContainer resourceDeleterContainer(dispatcher);
    Handler handler{dispatcher, resourceDeleterContainer};

    dispatcher.queue(MsgId::DeleteEntity);
    dispatcher.handleMsgFromQueue();
    dispatcher.handleMsgFromQueue();
    remoteOwner.finishResourceDeletion();
    dispatcher.handleMsgFromQueue();

    dispatcher.handleMsgFromQueue();

    return 0;
}