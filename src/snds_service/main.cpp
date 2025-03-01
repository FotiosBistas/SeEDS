#include "snds_producer.h"
#include "snds_consumer.h"
#include "snds_daemon.h" 

int main() {
  // Create a daemon instance.
  SNDSDaemon daemon;

  // Create a producer instance.
  auto producer = std::make_unique<SNDSProducer>();
  auto consumer = std::make_unique<SNDSConsumer>();

  // Add the producer to the daemon.
  daemon.addService(std::move(producer));
  daemon.addService(std::move(consumer));

  // Initialize and start the daemon.
  daemon.start();

  // (Optionally) run a loop or wait for a key press to shut down.
  daemon.stop();
  return 0;
}
