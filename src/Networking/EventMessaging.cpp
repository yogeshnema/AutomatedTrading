#include "Common/EventMessaging.h"

#include <stdexcept>
#include <zmq.hpp>

class ZeroMqEventPublisher::Implementation
{
public:
    Implementation(const std::string& endpoint, int highWaterMark)
        : context(1), socket(context, zmq::socket_type::pub)
    {
        socket.set(zmq::sockopt::sndhwm, highWaterMark);
        socket.bind(endpoint);
    }

    zmq::context_t context;
    zmq::socket_t socket;
};

ZeroMqEventPublisher::ZeroMqEventPublisher(
    const std::string& endpoint,
    int highWaterMark)
    : implementation_(std::make_unique<Implementation>(endpoint, highWaterMark))
{
}

ZeroMqEventPublisher::~ZeroMqEventPublisher() = default;

void ZeroMqEventPublisher::publish(
    const std::string& topic,
    const std::string& payload)
{
    const auto topicSent = implementation_->socket.send(
        zmq::buffer(topic), zmq::send_flags::sndmore);
    const auto payloadSent = implementation_->socket.send(
        zmq::buffer(payload), zmq::send_flags::none);

    if (!topicSent || !payloadSent) {
        throw std::runtime_error("ZeroMQ failed to publish a market data event.");
    }
}

class ZeroMqEventSubscriber::Implementation
{
public:
    Implementation(
        const std::string& endpoint,
        const std::string& topicFilter,
        int highWaterMark)
        : context(1), socket(context, zmq::socket_type::sub)
    {
        socket.set(zmq::sockopt::rcvhwm, highWaterMark);
        socket.set(zmq::sockopt::subscribe, topicFilter);
        socket.connect(endpoint);
    }

    zmq::context_t context;
    zmq::socket_t socket;
};

ZeroMqEventSubscriber::ZeroMqEventSubscriber(
    const std::string& endpoint,
    const std::string& topicFilter,
    int highWaterMark)
    : implementation_(std::make_unique<Implementation>(
        endpoint, topicFilter, highWaterMark))
{
}

ZeroMqEventSubscriber::~ZeroMqEventSubscriber() = default;

std::pair<std::string, std::string> ZeroMqEventSubscriber::receive()
{
    zmq::message_t topic;
    zmq::message_t payload;

    if (!implementation_->socket.recv(topic, zmq::recv_flags::none) ||
        !implementation_->socket.recv(payload, zmq::recv_flags::none)) {
        throw std::runtime_error("ZeroMQ failed to receive a market data event.");
    }

    return {
        topic.to_string(),
        payload.to_string()
    };
}
