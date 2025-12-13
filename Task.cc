#include "Task.h"
#include <sstream>
#include <iomanip>

namespace complex_network {

Task::Task(const std::string& vid, uint64_t seq_num, uint64_t size, uint64_t cycles, 
           double deadline_sec, double qos) 
    : vehicle_id(vid),
      task_size_bytes(size),
      cpu_cycles(cycles),
      relative_deadline(deadline_sec),
      qos_value(qos),
      state(CREATED),
      cpu_cycles_executed(0),
      cpu_allocated(0.0),
      completion_event(nullptr),
      deadline_event(nullptr)
{
    // Generate unique task ID
    std::ostringstream oss;
    oss << "V" << vid << "_T" << seq_num << "_" << std::fixed << std::setprecision(6) 
        << simTime().dbl();
    task_id = oss.str();
    
    // Initialize timing
    created_time = simTime();
    deadline = created_time + SimTime(deadline_sec);
    received_time = SimTime(0);
    processing_start_time = SimTime(0);
    completion_time = SimTime(0);
    
    // Log task creation with detailed info
    EV_INFO << "╔════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                    TASK CREATED                                    ║" << endl;
    EV_INFO << "╠════════════════════════════════════════════════════════════════════╣" << endl;
    EV_INFO << "║ Task ID:          " << std::left << std::setw(48) << task_id << "║" << endl;
    EV_INFO << "║ Vehicle ID:       " << std::setw(48) << vehicle_id << "║" << endl;
    EV_INFO << "║ Task Size:        " << std::setw(38) << (size / 1024.0) << " KB    ║" << endl;
    EV_INFO << "║ CPU Cycles:       " << std::setw(38) << (cycles / 1e9) << " G      ║" << endl;
    EV_INFO << "║ Deadline:         " << std::setw(38) << deadline_sec << " sec    ║" << endl;
    EV_INFO << "║ QoS Value:        " << std::setw(38) << qos_value << "        ║" << endl;
    EV_INFO << "║ Created at:       " << std::setw(38) << created_time.dbl() << " sec    ║" << endl;
    EV_INFO << "║ Absolute Deadline:" << std::setw(38) << deadline.dbl() << " sec    ║" << endl;
    EV_INFO << "║ State:            " << std::setw(48) << "CREATED" << "║" << endl;
    EV_INFO << "╚════════════════════════════════════════════════════════════════════╝" << endl;
}

Task::~Task() {
    // Log task destruction
    EV_INFO << "╔════════════════════════════════════════════════════════════════════╗" << endl;
    EV_INFO << "║                    TASK DESTROYED                                  ║" << endl;
    EV_INFO << "╠════════════════════════════════════════════════════════════════════╣" << endl;
    EV_INFO << "║ Task ID:          " << std::left << std::setw(48) << task_id << "║" << endl;
    EV_INFO << "║ Final State:      " << std::setw(48) << getStateString() << "║" << endl;
    EV_INFO << "║ Cycles Executed:  " << std::setw(38) << (cpu_cycles_executed / 1e9) << " G      ║" << endl;
    EV_INFO << "║ Cycles Required:  " << std::setw(38) << (cpu_cycles / 1e9) << " G      ║" << endl;
    if (completion_time > 0) {
        double total_time = (completion_time - created_time).dbl();
        EV_INFO << "║ Total Time:       " << std::setw(38) << total_time << " sec    ║" << endl;
    }
    EV_INFO << "╚════════════════════════════════════════════════════════════════════╝" << endl;
    
    // Note: Events should be cancelled by the module before deleting the task
    // Do not delete scheduled events here as they are owned by the simulation
}

std::string Task::getStateString() const {
    switch(state) {
        case CREATED: return "CREATED";
        case METADATA_SENT: return "METADATA_SENT";
        case REJECTED: return "REJECTED";
        case QUEUED: return "QUEUED";
        case PROCESSING: return "PROCESSING";
        case COMPLETED_ON_TIME: return "COMPLETED_ON_TIME";
        case COMPLETED_LATE: return "COMPLETED_LATE";
        case FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

double Task::getRemainingCycles() const {
    if (cpu_cycles_executed >= cpu_cycles)
        return 0.0;
    return static_cast<double>(cpu_cycles - cpu_cycles_executed);
}

double Task::getRemainingDeadline(simtime_t current_time) const {
    double remaining = (deadline - current_time).dbl();
    return std::max(0.0, remaining);
}

bool Task::isDeadlineMissed(simtime_t current_time) const {
    return current_time >= deadline;
}

void Task::logTaskInfo(const std::string& prefix) const {
    EV_INFO << "┌────────────────────────────────────────────────────────────────────┐" << endl;
    EV_INFO << "│ " << std::left << std::setw(66) << prefix << " │" << endl;
    EV_INFO << "├────────────────────────────────────────────────────────────────────┤" << endl;
    EV_INFO << "│ Task ID:          " << std::setw(48) << task_id << "│" << endl;
    EV_INFO << "│ State:            " << std::setw(48) << getStateString() << "│" << endl;
    EV_INFO << "│ Task Size:        " << std::setw(38) << (task_size_bytes / 1024.0) << " KB    │" << endl;
    EV_INFO << "│ CPU Cycles:       " << std::setw(38) << (cpu_cycles / 1e9) << " G      │" << endl;
    EV_INFO << "│ Cycles Executed:  " << std::setw(38) << (cpu_cycles_executed / 1e9) << " G      │" << endl;
    EV_INFO << "│ Cycles Remaining: " << std::setw(38) << (getRemainingCycles() / 1e9) << " G      │" << endl;
    EV_INFO << "│ CPU Allocated:    " << std::setw(38) << (cpu_allocated / 1e9) << " GHz    │" << endl;
    EV_INFO << "│ QoS Value:        " << std::setw(48) << qos_value << "│" << endl;
    EV_INFO << "│ Created at:       " << std::setw(38) << created_time.dbl() << " sec    │" << endl;
    EV_INFO << "│ Deadline at:      " << std::setw(38) << deadline.dbl() << " sec    │" << endl;
    EV_INFO << "│ Remaining Time:   " << std::setw(38) << getRemainingDeadline(simTime()) << " sec    │" << endl;
    if (processing_start_time > 0) {
        EV_INFO << "│ Processing Since: " << std::setw(38) << processing_start_time.dbl() << " sec    │" << endl;
        double processing_time = (simTime() - processing_start_time).dbl();
        EV_INFO << "│ Processing Time:  " << std::setw(38) << processing_time << " sec    │" << endl;
    }
    if (completion_time > 0) {
        double total_time = (completion_time - created_time).dbl();
        EV_INFO << "│ Completed at:     " << std::setw(38) << completion_time.dbl() << " sec    │" << endl;
        EV_INFO << "│ Total Time:       " << std::setw(38) << total_time << " sec    │" << endl;
    }
    EV_INFO << "└────────────────────────────────────────────────────────────────────┘" << endl;
}

} // namespace complex_network
