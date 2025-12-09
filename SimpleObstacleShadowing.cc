
#include "veins/modules/analogueModel/SimpleObstacleShadowing.h"
#include "veins/base/utils/FWMath.h"

using namespace veins;

using veins::AirFrame;

SimpleObstacleShadowing::SimpleObstacleShadowing(cComponent* owner, ObstacleControl& obstacleControl, bool useTorus, const Coord& playgroundSize)
    : AnalogueModel(owner)
    , obstacleControl(obstacleControl)
    , useTorus(useTorus)
    , playgroundSize(playgroundSize)
{
    EV_INFO << "=== SimpleObstacleShadowing Constructor ===" << endl;
    EV_INFO << "Owner: " << owner->getFullPath() << endl;
    EV_INFO << "UseTorus: " << useTorus << endl;
    EV_INFO << "Playground Size: x=" << playgroundSize.x << ", y=" << playgroundSize.y << ", z=" << playgroundSize.z << endl;
    
    if (useTorus) {
        EV_ERROR << "SimpleObstacleShadowing: ERROR - Torus-shaped playgrounds are not supported!" << endl;
        throw cRuntimeError("SimpleObstacleShadowing does not work on torus-shaped playgrounds");
    }
    
    EV_INFO << "SimpleObstacleShadowing initialized successfully" << endl;
    EV_INFO << "========================================" << endl;
}

void SimpleObstacleShadowing::filterSignal(Signal* signal)
{
    EV_TRACE << "======= SimpleObstacleShadowing::filterSignal START =======" << endl;
    
    // Get sender and receiver positions
    auto senderPos = signal->getSenderPoa().pos.getPositionAt();
    auto receiverPos = signal->getReceiverPoa().pos.getPositionAt();

    EV_INFO << "Signal Filtering:" << endl;
    EV_INFO << "  Sender Position: (" << senderPos.x << ", " << senderPos.y << ", " << senderPos.z << ")" << endl;
    EV_INFO << "  Receiver Position: (" << receiverPos.x << ", " << receiverPos.y << ", " << receiverPos.z << ")" << endl;
    
    // Calculate distance between sender and receiver
    double distance = senderPos.distance(receiverPos);
    EV_INFO << "  Distance between sender and receiver: " << distance << " m" << endl;
    
    // Get original signal power before attenuation (in milliwatts)
    double originalPower_mW = signal->getMax();
    double originalPower_dBm = FWMath::mW2dBm(originalPower_mW);
    EV_DEBUG << "  Original signal power: " << originalPower_mW << " mW (" << originalPower_dBm << " dBm)" << endl;
    
    // Calculate attenuation factor through obstacles
    EV_TRACE << "  Calling obstacleControl.calculateAttenuation()..." << endl;
    double factor = obstacleControl.calculateAttenuation(senderPos, receiverPos);

    EV_INFO << "  Attenuation factor from obstacles: " << factor << endl;
    EV_INFO << "  Attenuation in dB: " << 10 * log10(factor) << " dB" << endl;
    
    if (factor < 1.0) {
        EV_WARN << "  Signal is ATTENUATED (factor < 1.0) - obstacles detected in path" << endl;
    } else if (factor == 1.0) {
        EV_DEBUG << "  NO attenuation (factor = 1.0) - clear line of sight" << endl;
    } else {
        EV_WARN << "  UNEXPECTED: factor > 1.0 (amplification?) - value: " << factor << endl;
    }

    // Apply attenuation
    *signal *= factor;
    
    // Get new signal power after attenuation (in milliwatts)
    double newPower_mW = signal->getMax();
    double newPower_dBm = FWMath::mW2dBm(newPower_mW);
    EV_INFO << "  New signal power after attenuation: " << newPower_mW << " mW (" << newPower_dBm << " dBm)" << endl;
    EV_INFO << "  Total power loss: " << (originalPower_mW - newPower_mW) << " mW" << endl;
    EV_INFO << "  Power loss percentage: " << ((1.0 - factor) * 100) << "%" << endl;
    
    EV_TRACE << "======= SimpleObstacleShadowing::filterSignal END =======" << endl;
}
