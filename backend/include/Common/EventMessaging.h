#pragma once

#include <memory>
#include <string>
#include <utility>

class IEventPublisher
{
public:
    virtual ~IEventPublisher() = default;
    virtual void publish(const std::string& topic, const std::string& payload) = 0;
};

class NullEventPublisher final : public IEventPublisher
{
public:
    void publish(const std::string&, const std::string&) override {}
};

class ZeroMqEventPublisher final : public IEventPublisher
{
public:
    ZeroMqEventPublisher(const std::string& endpoint, int highWaterMark);
    ~ZeroMqEventPublisher() override;

    void publish(const std::string& topic, const std::string& payload) override;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

class IEventSubscriber
{
public:
    virtual ~IEventSubscriber() = default;
    virtual std::pair<std::string, std::string> receive() = 0;
};

class ZeroMqEventSubscriber final : public IEventSubscriber
{
public:
    ZeroMqEventSubscriber(
        const std::string& endpoint,
        const std::string& topicFilter,
        int highWaterMark);
    ~ZeroMqEventSubscriber() override;

    std::pair<std::string, std::string> receive() override;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};
