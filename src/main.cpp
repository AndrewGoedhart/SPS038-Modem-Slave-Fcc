#include <drivers/i2c/I2cDriver.hpp>
#include <drivers/serial/Usart.hpp>
#include <drivers/timers/AlarmTimer.hpp>
#include <drivers/plcNetwork/PlcNetworkTypes.hpp>
#include <drivers/plcNetwork/EUI64.hpp>
#include <drivers/plcNetwork/PlcNetworkDriver.hpp>
#include <drivers/plcNetwork/BootStrapDevice.hpp>
#include <hal/timers/CycleCounter.hpp>
#include <hal/usart/UsartTypes.hpp>
#include <hal/timers/WatchDog.hpp>

#include <SolexOs/messaging/Message.hpp>
#include <SolexOs/services/SystemContext.hpp>
#include <SolexOs/datastructures/ByteArray.hpp>
#include <tasks/configuration/Configuration.hpp>
#include <tasks/debug/DebugGateway.hpp>
#include <tasks/flash/FlashManager.hpp>
#include <tasks/Serial/FramedSerialGateway.hpp>
#include <tasks/status/Status.hpp>
#include <tasks/g3Trace/G3Trace.hpp>
#include <tasks/plcNetwork/PlcNetworkSlave.hpp>
#include <tests/TimerTest.hpp>

namespace Hal {
  class Interrupts;
} /* namespace Hal */

// PC Lint and MISRA
// -----------------

using Hal::Interrupts;

extern "C" {
  int main(int argc, char* argv[]);
}

namespace SolexOs {

  static inline void registerTasks(SystemContext &context) {
    auto configurationTask = new Configuration(context, new Drivers::I2cDriver(Hal::I2cPort::I2C_1));
    auto debugGateway = new DebugGateway(context, new Drivers::UsartDriver(Hal::UsartPort::USART_1, Hal::BaudRate::BAUD_460800));
    auto flashManager = new FlashManager(context, new Drivers::SpiDriver(Hal::SpiPort::SPI_1));
    auto serialGateway = new FramedSerialGateway(context, new Drivers::UsartDriver(Hal::UsartPort::USART_3, Hal::BaudRate::BAUD_115200), false);
    auto status = new Status(context);
    auto g3Trace = new G3Trace(context);
    auto plcDriver = new Drivers::PlcNetworkDriver(new Drivers::BootStrapDevice(), Drivers::PlcBand::IEEE_1901_2_FCC);
    auto plcNetwork = new PlcNetworkSlave(context, plcDriver);

    // hardware tasks
    context.getTaskScheduler().registerTask(debugGateway);
    context.getTaskScheduler().registerTask(serialGateway);
    context.getTaskScheduler().registerTask(configurationTask);
    context.getTaskScheduler().registerTask(flashManager);
    context.getTaskScheduler().registerTask(status);
    context.getTaskScheduler().registerTask(g3Trace);
    context.getTaskScheduler().registerTask(plcNetwork);

  }

  static inline void dispatchMessages(SystemContext context, TaskScheduler &scheduler) {
    while (true) {
      context.getRandom().addEntropy();
      Drivers::PlcNetworkDriver::runLib();
      (void) context;
      scheduler.processImmediates();
      scheduler.processNextNormalMessage();
      scheduler.sleepIfWaitingForMessages();
   //   Hal::WindowedWatchDog::kickWatchDog();
    }
  }

}

extern "C" {
  using namespace SolexOs;

  __attribute__((optimize("-Os")))
  int main(int, char*[]) {

    __enable_irq();
    Hal::CycleCounter::enable();

    auto *hardwareTimer = new Drivers::AlarmTimer(Hal::Timer::TIMER_2);
    auto *context = new SystemContext(*hardwareTimer);

    Drivers::EUI64 mac = Drivers::EUI64::getLocal();
    context->getRandom().updateSeed(mac.getLoWord());
    context->getRandom().updateSeed(mac.getHiWord());

    registerTasks(*context);

 //   Hal::WindowedWatchDog::enableWatchDogForMaximumTimeout();
    TaskScheduler &scheduler = context->getTaskScheduler();
    Message startupMessage = Message(MessageId::BASIC_SERVICES_AVAILABLE);
    scheduler.postMessageToTask(std::move(startupMessage));

    dispatchMessages(*context, scheduler);

    return 0;
  }
}

