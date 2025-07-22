#include "snds_service_base.h"

// Define and initialize static message queues.
// These will be created once and shared across all instances of ServiceBase.
std::unique_ptr<MessageQueue> ServiceBase::v_to_proxy = std::make_unique<MessageQueue>();
std::unique_ptr<MessageQueue> ServiceBase::v_to_dt = std::make_unique<MessageQueue>();
std::unique_ptr<MessageQueue> ServiceBase::v_to_producer = std::make_unique<MessageQueue>();
std::unique_ptr<MessageQueue> ServiceBase::v_to_consumer = std::make_unique<MessageQueue>();
std::unique_ptr<MessageQueue> ServiceBase::v_to_service = std::make_unique<MessageQueue>();
std::unique_ptr<MessageQueue> ServiceBase::v_to_worker = std::make_unique<MessageQueue>();