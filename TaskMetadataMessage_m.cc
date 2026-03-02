//
// Generated file, do not edit! Created by opp_msgtool 6.1 from TaskMetadataMessage.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include <memory>
#include <type_traits>
#include "TaskMetadataMessage_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp

namespace veins {

Register_Class(TaskMetadataMessage)

TaskMetadataMessage::TaskMetadataMessage(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

TaskMetadataMessage::TaskMetadataMessage(const TaskMetadataMessage& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

TaskMetadataMessage::~TaskMetadataMessage()
{
}

TaskMetadataMessage& TaskMetadataMessage::operator=(const TaskMetadataMessage& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void TaskMetadataMessage::copy(const TaskMetadataMessage& other)
{
    this->task_id = other.task_id;
    this->vehicle_id = other.vehicle_id;
    this->task_size_bytes = other.task_size_bytes;
    this->cpu_cycles = other.cpu_cycles;
    this->created_time = other.created_time;
    this->deadline_seconds = other.deadline_seconds;
    this->received_time = other.received_time;
    this->qos_value = other.qos_value;
    this->is_profile_task = other.is_profile_task;
    this->task_type_name = other.task_type_name;
    this->task_type_id = other.task_type_id;
    this->input_size_bytes = other.input_size_bytes;
    this->output_size_bytes = other.output_size_bytes;
    this->is_offloadable = other.is_offloadable;
    this->is_safety_critical = other.is_safety_critical;
    this->priority_level = other.priority_level;
}

void TaskMetadataMessage::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->task_id);
    doParsimPacking(b,this->vehicle_id);
    doParsimPacking(b,this->task_size_bytes);
    doParsimPacking(b,this->cpu_cycles);
    doParsimPacking(b,this->created_time);
    doParsimPacking(b,this->deadline_seconds);
    doParsimPacking(b,this->received_time);
    doParsimPacking(b,this->qos_value);
    doParsimPacking(b,this->is_profile_task);
    doParsimPacking(b,this->task_type_name);
    doParsimPacking(b,this->task_type_id);
    doParsimPacking(b,this->input_size_bytes);
    doParsimPacking(b,this->output_size_bytes);
    doParsimPacking(b,this->is_offloadable);
    doParsimPacking(b,this->is_safety_critical);
    doParsimPacking(b,this->priority_level);
}

void TaskMetadataMessage::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->task_id);
    doParsimUnpacking(b,this->vehicle_id);
    doParsimUnpacking(b,this->task_size_bytes);
    doParsimUnpacking(b,this->cpu_cycles);
    doParsimUnpacking(b,this->created_time);
    doParsimUnpacking(b,this->deadline_seconds);
    doParsimUnpacking(b,this->received_time);
    doParsimUnpacking(b,this->qos_value);
    doParsimUnpacking(b,this->is_profile_task);
    doParsimUnpacking(b,this->task_type_name);
    doParsimUnpacking(b,this->task_type_id);
    doParsimUnpacking(b,this->input_size_bytes);
    doParsimUnpacking(b,this->output_size_bytes);
    doParsimUnpacking(b,this->is_offloadable);
    doParsimUnpacking(b,this->is_safety_critical);
    doParsimUnpacking(b,this->priority_level);
}

const char * TaskMetadataMessage::getTask_id() const
{
    return this->task_id.c_str();
}

void TaskMetadataMessage::setTask_id(const char * task_id)
{
    this->task_id = task_id;
}

const char * TaskMetadataMessage::getVehicle_id() const
{
    return this->vehicle_id.c_str();
}

void TaskMetadataMessage::setVehicle_id(const char * vehicle_id)
{
    this->vehicle_id = vehicle_id;
}

uint64_t TaskMetadataMessage::getTask_size_bytes() const
{
    return this->task_size_bytes;
}

void TaskMetadataMessage::setTask_size_bytes(uint64_t task_size_bytes)
{
    this->task_size_bytes = task_size_bytes;
}

uint64_t TaskMetadataMessage::getCpu_cycles() const
{
    return this->cpu_cycles;
}

void TaskMetadataMessage::setCpu_cycles(uint64_t cpu_cycles)
{
    this->cpu_cycles = cpu_cycles;
}

double TaskMetadataMessage::getCreated_time() const
{
    return this->created_time;
}

void TaskMetadataMessage::setCreated_time(double created_time)
{
    this->created_time = created_time;
}

double TaskMetadataMessage::getDeadline_seconds() const
{
    return this->deadline_seconds;
}

void TaskMetadataMessage::setDeadline_seconds(double deadline_seconds)
{
    this->deadline_seconds = deadline_seconds;
}

double TaskMetadataMessage::getReceived_time() const
{
    return this->received_time;
}

void TaskMetadataMessage::setReceived_time(double received_time)
{
    this->received_time = received_time;
}

double TaskMetadataMessage::getQos_value() const
{
    return this->qos_value;
}

void TaskMetadataMessage::setQos_value(double qos_value)
{
    this->qos_value = qos_value;
}

bool TaskMetadataMessage::getIs_profile_task() const
{
    return this->is_profile_task;
}

void TaskMetadataMessage::setIs_profile_task(bool is_profile_task)
{
    this->is_profile_task = is_profile_task;
}

const char * TaskMetadataMessage::getTask_type_name() const
{
    return this->task_type_name.c_str();
}

void TaskMetadataMessage::setTask_type_name(const char * task_type_name)
{
    this->task_type_name = task_type_name;
}

int TaskMetadataMessage::getTask_type_id() const
{
    return this->task_type_id;
}

void TaskMetadataMessage::setTask_type_id(int task_type_id)
{
    this->task_type_id = task_type_id;
}

uint64_t TaskMetadataMessage::getInput_size_bytes() const
{
    return this->input_size_bytes;
}

void TaskMetadataMessage::setInput_size_bytes(uint64_t input_size_bytes)
{
    this->input_size_bytes = input_size_bytes;
}

uint64_t TaskMetadataMessage::getOutput_size_bytes() const
{
    return this->output_size_bytes;
}

void TaskMetadataMessage::setOutput_size_bytes(uint64_t output_size_bytes)
{
    this->output_size_bytes = output_size_bytes;
}

bool TaskMetadataMessage::getIs_offloadable() const
{
    return this->is_offloadable;
}

void TaskMetadataMessage::setIs_offloadable(bool is_offloadable)
{
    this->is_offloadable = is_offloadable;
}

bool TaskMetadataMessage::getIs_safety_critical() const
{
    return this->is_safety_critical;
}

void TaskMetadataMessage::setIs_safety_critical(bool is_safety_critical)
{
    this->is_safety_critical = is_safety_critical;
}

int TaskMetadataMessage::getPriority_level() const
{
    return this->priority_level;
}

void TaskMetadataMessage::setPriority_level(int priority_level)
{
    this->priority_level = priority_level;
}

class TaskMetadataMessageDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_task_id,
        FIELD_vehicle_id,
        FIELD_task_size_bytes,
        FIELD_cpu_cycles,
        FIELD_created_time,
        FIELD_deadline_seconds,
        FIELD_received_time,
        FIELD_qos_value,
        FIELD_is_profile_task,
        FIELD_task_type_name,
        FIELD_task_type_id,
        FIELD_input_size_bytes,
        FIELD_output_size_bytes,
        FIELD_is_offloadable,
        FIELD_is_safety_critical,
        FIELD_priority_level,
    };
  public:
    TaskMetadataMessageDescriptor();
    virtual ~TaskMetadataMessageDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(TaskMetadataMessageDescriptor)

TaskMetadataMessageDescriptor::TaskMetadataMessageDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(veins::TaskMetadataMessage)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

TaskMetadataMessageDescriptor::~TaskMetadataMessageDescriptor()
{
    delete[] propertyNames;
}

bool TaskMetadataMessageDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<TaskMetadataMessage *>(obj)!=nullptr;
}

const char **TaskMetadataMessageDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *TaskMetadataMessageDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int TaskMetadataMessageDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 16+base->getFieldCount() : 16;
}

unsigned int TaskMetadataMessageDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_task_id
        FD_ISEDITABLE,    // FIELD_vehicle_id
        FD_ISEDITABLE,    // FIELD_task_size_bytes
        FD_ISEDITABLE,    // FIELD_cpu_cycles
        FD_ISEDITABLE,    // FIELD_created_time
        FD_ISEDITABLE,    // FIELD_deadline_seconds
        FD_ISEDITABLE,    // FIELD_received_time
        FD_ISEDITABLE,    // FIELD_qos_value
        FD_ISEDITABLE,    // FIELD_is_profile_task
        FD_ISEDITABLE,    // FIELD_task_type_name
        FD_ISEDITABLE,    // FIELD_task_type_id
        FD_ISEDITABLE,    // FIELD_input_size_bytes
        FD_ISEDITABLE,    // FIELD_output_size_bytes
        FD_ISEDITABLE,    // FIELD_is_offloadable
        FD_ISEDITABLE,    // FIELD_is_safety_critical
        FD_ISEDITABLE,    // FIELD_priority_level
    };
    return (field >= 0 && field < 16) ? fieldTypeFlags[field] : 0;
}

const char *TaskMetadataMessageDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "task_id",
        "vehicle_id",
        "task_size_bytes",
        "cpu_cycles",
        "created_time",
        "deadline_seconds",
        "received_time",
        "qos_value",
        "is_profile_task",
        "task_type_name",
        "task_type_id",
        "input_size_bytes",
        "output_size_bytes",
        "is_offloadable",
        "is_safety_critical",
        "priority_level",
    };
    return (field >= 0 && field < 16) ? fieldNames[field] : nullptr;
}

int TaskMetadataMessageDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "task_id") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "vehicle_id") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "task_size_bytes") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "cpu_cycles") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "created_time") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "deadline_seconds") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "received_time") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "qos_value") == 0) return baseIndex + 7;
    if (strcmp(fieldName, "is_profile_task") == 0) return baseIndex + 8;
    if (strcmp(fieldName, "task_type_name") == 0) return baseIndex + 9;
    if (strcmp(fieldName, "task_type_id") == 0) return baseIndex + 10;
    if (strcmp(fieldName, "input_size_bytes") == 0) return baseIndex + 11;
    if (strcmp(fieldName, "output_size_bytes") == 0) return baseIndex + 12;
    if (strcmp(fieldName, "is_offloadable") == 0) return baseIndex + 13;
    if (strcmp(fieldName, "is_safety_critical") == 0) return baseIndex + 14;
    if (strcmp(fieldName, "priority_level") == 0) return baseIndex + 15;
    return base ? base->findField(fieldName) : -1;
}

const char *TaskMetadataMessageDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "string",    // FIELD_task_id
        "string",    // FIELD_vehicle_id
        "uint64_t",    // FIELD_task_size_bytes
        "uint64_t",    // FIELD_cpu_cycles
        "double",    // FIELD_created_time
        "double",    // FIELD_deadline_seconds
        "double",    // FIELD_received_time
        "double",    // FIELD_qos_value
        "bool",    // FIELD_is_profile_task
        "string",    // FIELD_task_type_name
        "int",    // FIELD_task_type_id
        "uint64_t",    // FIELD_input_size_bytes
        "uint64_t",    // FIELD_output_size_bytes
        "bool",    // FIELD_is_offloadable
        "bool",    // FIELD_is_safety_critical
        "int",    // FIELD_priority_level
    };
    return (field >= 0 && field < 16) ? fieldTypeStrings[field] : nullptr;
}

const char **TaskMetadataMessageDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *TaskMetadataMessageDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int TaskMetadataMessageDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    TaskMetadataMessage *pp = omnetpp::fromAnyPtr<TaskMetadataMessage>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void TaskMetadataMessageDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskMetadataMessage *pp = omnetpp::fromAnyPtr<TaskMetadataMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'TaskMetadataMessage'", field);
    }
}

const char *TaskMetadataMessageDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskMetadataMessage *pp = omnetpp::fromAnyPtr<TaskMetadataMessage>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string TaskMetadataMessageDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskMetadataMessage *pp = omnetpp::fromAnyPtr<TaskMetadataMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return oppstring2string(pp->getTask_id());
        case FIELD_vehicle_id: return oppstring2string(pp->getVehicle_id());
        case FIELD_task_size_bytes: return uint642string(pp->getTask_size_bytes());
        case FIELD_cpu_cycles: return uint642string(pp->getCpu_cycles());
        case FIELD_created_time: return double2string(pp->getCreated_time());
        case FIELD_deadline_seconds: return double2string(pp->getDeadline_seconds());
        case FIELD_received_time: return double2string(pp->getReceived_time());
        case FIELD_qos_value: return double2string(pp->getQos_value());
        case FIELD_is_profile_task: return bool2string(pp->getIs_profile_task());
        case FIELD_task_type_name: return oppstring2string(pp->getTask_type_name());
        case FIELD_task_type_id: return long2string(pp->getTask_type_id());
        case FIELD_input_size_bytes: return uint642string(pp->getInput_size_bytes());
        case FIELD_output_size_bytes: return uint642string(pp->getOutput_size_bytes());
        case FIELD_is_offloadable: return bool2string(pp->getIs_offloadable());
        case FIELD_is_safety_critical: return bool2string(pp->getIs_safety_critical());
        case FIELD_priority_level: return long2string(pp->getPriority_level());
        default: return "";
    }
}

void TaskMetadataMessageDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskMetadataMessage *pp = omnetpp::fromAnyPtr<TaskMetadataMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id((value)); break;
        case FIELD_vehicle_id: pp->setVehicle_id((value)); break;
        case FIELD_task_size_bytes: pp->setTask_size_bytes(string2uint64(value)); break;
        case FIELD_cpu_cycles: pp->setCpu_cycles(string2uint64(value)); break;
        case FIELD_created_time: pp->setCreated_time(string2double(value)); break;
        case FIELD_deadline_seconds: pp->setDeadline_seconds(string2double(value)); break;
        case FIELD_received_time: pp->setReceived_time(string2double(value)); break;
        case FIELD_qos_value: pp->setQos_value(string2double(value)); break;
        case FIELD_is_profile_task: pp->setIs_profile_task(string2bool(value)); break;
        case FIELD_task_type_name: pp->setTask_type_name((value)); break;
        case FIELD_task_type_id: pp->setTask_type_id(string2long(value)); break;
        case FIELD_input_size_bytes: pp->setInput_size_bytes(string2uint64(value)); break;
        case FIELD_output_size_bytes: pp->setOutput_size_bytes(string2uint64(value)); break;
        case FIELD_is_offloadable: pp->setIs_offloadable(string2bool(value)); break;
        case FIELD_is_safety_critical: pp->setIs_safety_critical(string2bool(value)); break;
        case FIELD_priority_level: pp->setPriority_level(string2long(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskMetadataMessage'", field);
    }
}

omnetpp::cValue TaskMetadataMessageDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    TaskMetadataMessage *pp = omnetpp::fromAnyPtr<TaskMetadataMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return pp->getTask_id();
        case FIELD_vehicle_id: return pp->getVehicle_id();
        case FIELD_task_size_bytes: return (omnetpp::intval_t)(pp->getTask_size_bytes());
        case FIELD_cpu_cycles: return (omnetpp::intval_t)(pp->getCpu_cycles());
        case FIELD_created_time: return pp->getCreated_time();
        case FIELD_deadline_seconds: return pp->getDeadline_seconds();
        case FIELD_received_time: return pp->getReceived_time();
        case FIELD_qos_value: return pp->getQos_value();
        case FIELD_is_profile_task: return pp->getIs_profile_task();
        case FIELD_task_type_name: return pp->getTask_type_name();
        case FIELD_task_type_id: return pp->getTask_type_id();
        case FIELD_input_size_bytes: return (omnetpp::intval_t)(pp->getInput_size_bytes());
        case FIELD_output_size_bytes: return (omnetpp::intval_t)(pp->getOutput_size_bytes());
        case FIELD_is_offloadable: return pp->getIs_offloadable();
        case FIELD_is_safety_critical: return pp->getIs_safety_critical();
        case FIELD_priority_level: return pp->getPriority_level();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'TaskMetadataMessage' as cValue -- field index out of range?", field);
    }
}

void TaskMetadataMessageDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskMetadataMessage *pp = omnetpp::fromAnyPtr<TaskMetadataMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id(value.stringValue()); break;
        case FIELD_vehicle_id: pp->setVehicle_id(value.stringValue()); break;
        case FIELD_task_size_bytes: pp->setTask_size_bytes(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_cpu_cycles: pp->setCpu_cycles(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_created_time: pp->setCreated_time(value.doubleValue()); break;
        case FIELD_deadline_seconds: pp->setDeadline_seconds(value.doubleValue()); break;
        case FIELD_received_time: pp->setReceived_time(value.doubleValue()); break;
        case FIELD_qos_value: pp->setQos_value(value.doubleValue()); break;
        case FIELD_is_profile_task: pp->setIs_profile_task(value.boolValue()); break;
        case FIELD_task_type_name: pp->setTask_type_name(value.stringValue()); break;
        case FIELD_task_type_id: pp->setTask_type_id(omnetpp::checked_int_cast<int>(value.intValue())); break;
        case FIELD_input_size_bytes: pp->setInput_size_bytes(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_output_size_bytes: pp->setOutput_size_bytes(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_is_offloadable: pp->setIs_offloadable(value.boolValue()); break;
        case FIELD_is_safety_critical: pp->setIs_safety_critical(value.boolValue()); break;
        case FIELD_priority_level: pp->setPriority_level(omnetpp::checked_int_cast<int>(value.intValue())); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskMetadataMessage'", field);
    }
}

const char *TaskMetadataMessageDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr TaskMetadataMessageDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    TaskMetadataMessage *pp = omnetpp::fromAnyPtr<TaskMetadataMessage>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void TaskMetadataMessageDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskMetadataMessage *pp = omnetpp::fromAnyPtr<TaskMetadataMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskMetadataMessage'", field);
    }
}

Register_Class(ObjectDetectionDataMessage)

ObjectDetectionDataMessage::ObjectDetectionDataMessage(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

ObjectDetectionDataMessage::ObjectDetectionDataMessage(const ObjectDetectionDataMessage& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

ObjectDetectionDataMessage::~ObjectDetectionDataMessage()
{
}

ObjectDetectionDataMessage& ObjectDetectionDataMessage::operator=(const ObjectDetectionDataMessage& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void ObjectDetectionDataMessage::copy(const ObjectDetectionDataMessage& other)
{
    this->vehicle_id = other.vehicle_id;
    this->timestamp = other.timestamp;
    this->data_size_bytes = other.data_size_bytes;
    this->payload_tag = other.payload_tag;
}

void ObjectDetectionDataMessage::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->vehicle_id);
    doParsimPacking(b,this->timestamp);
    doParsimPacking(b,this->data_size_bytes);
    doParsimPacking(b,this->payload_tag);
}

void ObjectDetectionDataMessage::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->vehicle_id);
    doParsimUnpacking(b,this->timestamp);
    doParsimUnpacking(b,this->data_size_bytes);
    doParsimUnpacking(b,this->payload_tag);
}

const char * ObjectDetectionDataMessage::getVehicle_id() const
{
    return this->vehicle_id.c_str();
}

void ObjectDetectionDataMessage::setVehicle_id(const char * vehicle_id)
{
    this->vehicle_id = vehicle_id;
}

double ObjectDetectionDataMessage::getTimestamp() const
{
    return this->timestamp;
}

void ObjectDetectionDataMessage::setTimestamp(double timestamp)
{
    this->timestamp = timestamp;
}

uint64_t ObjectDetectionDataMessage::getData_size_bytes() const
{
    return this->data_size_bytes;
}

void ObjectDetectionDataMessage::setData_size_bytes(uint64_t data_size_bytes)
{
    this->data_size_bytes = data_size_bytes;
}

const char * ObjectDetectionDataMessage::getPayload_tag() const
{
    return this->payload_tag.c_str();
}

void ObjectDetectionDataMessage::setPayload_tag(const char * payload_tag)
{
    this->payload_tag = payload_tag;
}

class ObjectDetectionDataMessageDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_vehicle_id,
        FIELD_timestamp,
        FIELD_data_size_bytes,
        FIELD_payload_tag,
    };
  public:
    ObjectDetectionDataMessageDescriptor();
    virtual ~ObjectDetectionDataMessageDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(ObjectDetectionDataMessageDescriptor)

ObjectDetectionDataMessageDescriptor::ObjectDetectionDataMessageDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(veins::ObjectDetectionDataMessage)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

ObjectDetectionDataMessageDescriptor::~ObjectDetectionDataMessageDescriptor()
{
    delete[] propertyNames;
}

bool ObjectDetectionDataMessageDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<ObjectDetectionDataMessage *>(obj)!=nullptr;
}

const char **ObjectDetectionDataMessageDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *ObjectDetectionDataMessageDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int ObjectDetectionDataMessageDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 4+base->getFieldCount() : 4;
}

unsigned int ObjectDetectionDataMessageDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_vehicle_id
        FD_ISEDITABLE,    // FIELD_timestamp
        FD_ISEDITABLE,    // FIELD_data_size_bytes
        FD_ISEDITABLE,    // FIELD_payload_tag
    };
    return (field >= 0 && field < 4) ? fieldTypeFlags[field] : 0;
}

const char *ObjectDetectionDataMessageDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "vehicle_id",
        "timestamp",
        "data_size_bytes",
        "payload_tag",
    };
    return (field >= 0 && field < 4) ? fieldNames[field] : nullptr;
}

int ObjectDetectionDataMessageDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "vehicle_id") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "timestamp") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "data_size_bytes") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "payload_tag") == 0) return baseIndex + 3;
    return base ? base->findField(fieldName) : -1;
}

const char *ObjectDetectionDataMessageDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "string",    // FIELD_vehicle_id
        "double",    // FIELD_timestamp
        "uint64_t",    // FIELD_data_size_bytes
        "string",    // FIELD_payload_tag
    };
    return (field >= 0 && field < 4) ? fieldTypeStrings[field] : nullptr;
}

const char **ObjectDetectionDataMessageDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *ObjectDetectionDataMessageDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int ObjectDetectionDataMessageDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    ObjectDetectionDataMessage *pp = omnetpp::fromAnyPtr<ObjectDetectionDataMessage>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void ObjectDetectionDataMessageDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    ObjectDetectionDataMessage *pp = omnetpp::fromAnyPtr<ObjectDetectionDataMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'ObjectDetectionDataMessage'", field);
    }
}

const char *ObjectDetectionDataMessageDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    ObjectDetectionDataMessage *pp = omnetpp::fromAnyPtr<ObjectDetectionDataMessage>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string ObjectDetectionDataMessageDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    ObjectDetectionDataMessage *pp = omnetpp::fromAnyPtr<ObjectDetectionDataMessage>(object); (void)pp;
    switch (field) {
        case FIELD_vehicle_id: return oppstring2string(pp->getVehicle_id());
        case FIELD_timestamp: return double2string(pp->getTimestamp());
        case FIELD_data_size_bytes: return uint642string(pp->getData_size_bytes());
        case FIELD_payload_tag: return oppstring2string(pp->getPayload_tag());
        default: return "";
    }
}

void ObjectDetectionDataMessageDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    ObjectDetectionDataMessage *pp = omnetpp::fromAnyPtr<ObjectDetectionDataMessage>(object); (void)pp;
    switch (field) {
        case FIELD_vehicle_id: pp->setVehicle_id((value)); break;
        case FIELD_timestamp: pp->setTimestamp(string2double(value)); break;
        case FIELD_data_size_bytes: pp->setData_size_bytes(string2uint64(value)); break;
        case FIELD_payload_tag: pp->setPayload_tag((value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'ObjectDetectionDataMessage'", field);
    }
}

omnetpp::cValue ObjectDetectionDataMessageDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    ObjectDetectionDataMessage *pp = omnetpp::fromAnyPtr<ObjectDetectionDataMessage>(object); (void)pp;
    switch (field) {
        case FIELD_vehicle_id: return pp->getVehicle_id();
        case FIELD_timestamp: return pp->getTimestamp();
        case FIELD_data_size_bytes: return (omnetpp::intval_t)(pp->getData_size_bytes());
        case FIELD_payload_tag: return pp->getPayload_tag();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'ObjectDetectionDataMessage' as cValue -- field index out of range?", field);
    }
}

void ObjectDetectionDataMessageDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    ObjectDetectionDataMessage *pp = omnetpp::fromAnyPtr<ObjectDetectionDataMessage>(object); (void)pp;
    switch (field) {
        case FIELD_vehicle_id: pp->setVehicle_id(value.stringValue()); break;
        case FIELD_timestamp: pp->setTimestamp(value.doubleValue()); break;
        case FIELD_data_size_bytes: pp->setData_size_bytes(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_payload_tag: pp->setPayload_tag(value.stringValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'ObjectDetectionDataMessage'", field);
    }
}

const char *ObjectDetectionDataMessageDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr ObjectDetectionDataMessageDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    ObjectDetectionDataMessage *pp = omnetpp::fromAnyPtr<ObjectDetectionDataMessage>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void ObjectDetectionDataMessageDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    ObjectDetectionDataMessage *pp = omnetpp::fromAnyPtr<ObjectDetectionDataMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'ObjectDetectionDataMessage'", field);
    }
}

Register_Class(TaskCompletionMessage)

TaskCompletionMessage::TaskCompletionMessage(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

TaskCompletionMessage::TaskCompletionMessage(const TaskCompletionMessage& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

TaskCompletionMessage::~TaskCompletionMessage()
{
}

TaskCompletionMessage& TaskCompletionMessage::operator=(const TaskCompletionMessage& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void TaskCompletionMessage::copy(const TaskCompletionMessage& other)
{
    this->task_id = other.task_id;
    this->vehicle_id = other.vehicle_id;
    this->completion_time = other.completion_time;
    this->processing_time = other.processing_time;
    this->completed_on_time = other.completed_on_time;
    this->cpu_allocated = other.cpu_allocated;
}

void TaskCompletionMessage::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->task_id);
    doParsimPacking(b,this->vehicle_id);
    doParsimPacking(b,this->completion_time);
    doParsimPacking(b,this->processing_time);
    doParsimPacking(b,this->completed_on_time);
    doParsimPacking(b,this->cpu_allocated);
}

void TaskCompletionMessage::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->task_id);
    doParsimUnpacking(b,this->vehicle_id);
    doParsimUnpacking(b,this->completion_time);
    doParsimUnpacking(b,this->processing_time);
    doParsimUnpacking(b,this->completed_on_time);
    doParsimUnpacking(b,this->cpu_allocated);
}

const char * TaskCompletionMessage::getTask_id() const
{
    return this->task_id.c_str();
}

void TaskCompletionMessage::setTask_id(const char * task_id)
{
    this->task_id = task_id;
}

const char * TaskCompletionMessage::getVehicle_id() const
{
    return this->vehicle_id.c_str();
}

void TaskCompletionMessage::setVehicle_id(const char * vehicle_id)
{
    this->vehicle_id = vehicle_id;
}

double TaskCompletionMessage::getCompletion_time() const
{
    return this->completion_time;
}

void TaskCompletionMessage::setCompletion_time(double completion_time)
{
    this->completion_time = completion_time;
}

double TaskCompletionMessage::getProcessing_time() const
{
    return this->processing_time;
}

void TaskCompletionMessage::setProcessing_time(double processing_time)
{
    this->processing_time = processing_time;
}

bool TaskCompletionMessage::getCompleted_on_time() const
{
    return this->completed_on_time;
}

void TaskCompletionMessage::setCompleted_on_time(bool completed_on_time)
{
    this->completed_on_time = completed_on_time;
}

double TaskCompletionMessage::getCpu_allocated() const
{
    return this->cpu_allocated;
}

void TaskCompletionMessage::setCpu_allocated(double cpu_allocated)
{
    this->cpu_allocated = cpu_allocated;
}

class TaskCompletionMessageDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_task_id,
        FIELD_vehicle_id,
        FIELD_completion_time,
        FIELD_processing_time,
        FIELD_completed_on_time,
        FIELD_cpu_allocated,
    };
  public:
    TaskCompletionMessageDescriptor();
    virtual ~TaskCompletionMessageDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(TaskCompletionMessageDescriptor)

TaskCompletionMessageDescriptor::TaskCompletionMessageDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(veins::TaskCompletionMessage)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

TaskCompletionMessageDescriptor::~TaskCompletionMessageDescriptor()
{
    delete[] propertyNames;
}

bool TaskCompletionMessageDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<TaskCompletionMessage *>(obj)!=nullptr;
}

const char **TaskCompletionMessageDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *TaskCompletionMessageDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int TaskCompletionMessageDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 6+base->getFieldCount() : 6;
}

unsigned int TaskCompletionMessageDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_task_id
        FD_ISEDITABLE,    // FIELD_vehicle_id
        FD_ISEDITABLE,    // FIELD_completion_time
        FD_ISEDITABLE,    // FIELD_processing_time
        FD_ISEDITABLE,    // FIELD_completed_on_time
        FD_ISEDITABLE,    // FIELD_cpu_allocated
    };
    return (field >= 0 && field < 6) ? fieldTypeFlags[field] : 0;
}

const char *TaskCompletionMessageDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "task_id",
        "vehicle_id",
        "completion_time",
        "processing_time",
        "completed_on_time",
        "cpu_allocated",
    };
    return (field >= 0 && field < 6) ? fieldNames[field] : nullptr;
}

int TaskCompletionMessageDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "task_id") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "vehicle_id") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "completion_time") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "processing_time") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "completed_on_time") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "cpu_allocated") == 0) return baseIndex + 5;
    return base ? base->findField(fieldName) : -1;
}

const char *TaskCompletionMessageDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "string",    // FIELD_task_id
        "string",    // FIELD_vehicle_id
        "double",    // FIELD_completion_time
        "double",    // FIELD_processing_time
        "bool",    // FIELD_completed_on_time
        "double",    // FIELD_cpu_allocated
    };
    return (field >= 0 && field < 6) ? fieldTypeStrings[field] : nullptr;
}

const char **TaskCompletionMessageDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *TaskCompletionMessageDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int TaskCompletionMessageDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    TaskCompletionMessage *pp = omnetpp::fromAnyPtr<TaskCompletionMessage>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void TaskCompletionMessageDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskCompletionMessage *pp = omnetpp::fromAnyPtr<TaskCompletionMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'TaskCompletionMessage'", field);
    }
}

const char *TaskCompletionMessageDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskCompletionMessage *pp = omnetpp::fromAnyPtr<TaskCompletionMessage>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string TaskCompletionMessageDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskCompletionMessage *pp = omnetpp::fromAnyPtr<TaskCompletionMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return oppstring2string(pp->getTask_id());
        case FIELD_vehicle_id: return oppstring2string(pp->getVehicle_id());
        case FIELD_completion_time: return double2string(pp->getCompletion_time());
        case FIELD_processing_time: return double2string(pp->getProcessing_time());
        case FIELD_completed_on_time: return bool2string(pp->getCompleted_on_time());
        case FIELD_cpu_allocated: return double2string(pp->getCpu_allocated());
        default: return "";
    }
}

void TaskCompletionMessageDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskCompletionMessage *pp = omnetpp::fromAnyPtr<TaskCompletionMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id((value)); break;
        case FIELD_vehicle_id: pp->setVehicle_id((value)); break;
        case FIELD_completion_time: pp->setCompletion_time(string2double(value)); break;
        case FIELD_processing_time: pp->setProcessing_time(string2double(value)); break;
        case FIELD_completed_on_time: pp->setCompleted_on_time(string2bool(value)); break;
        case FIELD_cpu_allocated: pp->setCpu_allocated(string2double(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskCompletionMessage'", field);
    }
}

omnetpp::cValue TaskCompletionMessageDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    TaskCompletionMessage *pp = omnetpp::fromAnyPtr<TaskCompletionMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return pp->getTask_id();
        case FIELD_vehicle_id: return pp->getVehicle_id();
        case FIELD_completion_time: return pp->getCompletion_time();
        case FIELD_processing_time: return pp->getProcessing_time();
        case FIELD_completed_on_time: return pp->getCompleted_on_time();
        case FIELD_cpu_allocated: return pp->getCpu_allocated();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'TaskCompletionMessage' as cValue -- field index out of range?", field);
    }
}

void TaskCompletionMessageDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskCompletionMessage *pp = omnetpp::fromAnyPtr<TaskCompletionMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id(value.stringValue()); break;
        case FIELD_vehicle_id: pp->setVehicle_id(value.stringValue()); break;
        case FIELD_completion_time: pp->setCompletion_time(value.doubleValue()); break;
        case FIELD_processing_time: pp->setProcessing_time(value.doubleValue()); break;
        case FIELD_completed_on_time: pp->setCompleted_on_time(value.boolValue()); break;
        case FIELD_cpu_allocated: pp->setCpu_allocated(value.doubleValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskCompletionMessage'", field);
    }
}

const char *TaskCompletionMessageDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr TaskCompletionMessageDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    TaskCompletionMessage *pp = omnetpp::fromAnyPtr<TaskCompletionMessage>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void TaskCompletionMessageDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskCompletionMessage *pp = omnetpp::fromAnyPtr<TaskCompletionMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskCompletionMessage'", field);
    }
}

Register_Class(TaskFailureMessage)

TaskFailureMessage::TaskFailureMessage(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

TaskFailureMessage::TaskFailureMessage(const TaskFailureMessage& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

TaskFailureMessage::~TaskFailureMessage()
{
}

TaskFailureMessage& TaskFailureMessage::operator=(const TaskFailureMessage& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void TaskFailureMessage::copy(const TaskFailureMessage& other)
{
    this->task_id = other.task_id;
    this->vehicle_id = other.vehicle_id;
    this->failure_time = other.failure_time;
    this->failure_reason = other.failure_reason;
    this->wasted_time = other.wasted_time;
}

void TaskFailureMessage::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->task_id);
    doParsimPacking(b,this->vehicle_id);
    doParsimPacking(b,this->failure_time);
    doParsimPacking(b,this->failure_reason);
    doParsimPacking(b,this->wasted_time);
}

void TaskFailureMessage::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->task_id);
    doParsimUnpacking(b,this->vehicle_id);
    doParsimUnpacking(b,this->failure_time);
    doParsimUnpacking(b,this->failure_reason);
    doParsimUnpacking(b,this->wasted_time);
}

const char * TaskFailureMessage::getTask_id() const
{
    return this->task_id.c_str();
}

void TaskFailureMessage::setTask_id(const char * task_id)
{
    this->task_id = task_id;
}

const char * TaskFailureMessage::getVehicle_id() const
{
    return this->vehicle_id.c_str();
}

void TaskFailureMessage::setVehicle_id(const char * vehicle_id)
{
    this->vehicle_id = vehicle_id;
}

double TaskFailureMessage::getFailure_time() const
{
    return this->failure_time;
}

void TaskFailureMessage::setFailure_time(double failure_time)
{
    this->failure_time = failure_time;
}

const char * TaskFailureMessage::getFailure_reason() const
{
    return this->failure_reason.c_str();
}

void TaskFailureMessage::setFailure_reason(const char * failure_reason)
{
    this->failure_reason = failure_reason;
}

double TaskFailureMessage::getWasted_time() const
{
    return this->wasted_time;
}

void TaskFailureMessage::setWasted_time(double wasted_time)
{
    this->wasted_time = wasted_time;
}

class TaskFailureMessageDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_task_id,
        FIELD_vehicle_id,
        FIELD_failure_time,
        FIELD_failure_reason,
        FIELD_wasted_time,
    };
  public:
    TaskFailureMessageDescriptor();
    virtual ~TaskFailureMessageDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(TaskFailureMessageDescriptor)

TaskFailureMessageDescriptor::TaskFailureMessageDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(veins::TaskFailureMessage)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

TaskFailureMessageDescriptor::~TaskFailureMessageDescriptor()
{
    delete[] propertyNames;
}

bool TaskFailureMessageDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<TaskFailureMessage *>(obj)!=nullptr;
}

const char **TaskFailureMessageDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *TaskFailureMessageDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int TaskFailureMessageDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 5+base->getFieldCount() : 5;
}

unsigned int TaskFailureMessageDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_task_id
        FD_ISEDITABLE,    // FIELD_vehicle_id
        FD_ISEDITABLE,    // FIELD_failure_time
        FD_ISEDITABLE,    // FIELD_failure_reason
        FD_ISEDITABLE,    // FIELD_wasted_time
    };
    return (field >= 0 && field < 5) ? fieldTypeFlags[field] : 0;
}

const char *TaskFailureMessageDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "task_id",
        "vehicle_id",
        "failure_time",
        "failure_reason",
        "wasted_time",
    };
    return (field >= 0 && field < 5) ? fieldNames[field] : nullptr;
}

int TaskFailureMessageDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "task_id") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "vehicle_id") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "failure_time") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "failure_reason") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "wasted_time") == 0) return baseIndex + 4;
    return base ? base->findField(fieldName) : -1;
}

const char *TaskFailureMessageDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "string",    // FIELD_task_id
        "string",    // FIELD_vehicle_id
        "double",    // FIELD_failure_time
        "string",    // FIELD_failure_reason
        "double",    // FIELD_wasted_time
    };
    return (field >= 0 && field < 5) ? fieldTypeStrings[field] : nullptr;
}

const char **TaskFailureMessageDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *TaskFailureMessageDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int TaskFailureMessageDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    TaskFailureMessage *pp = omnetpp::fromAnyPtr<TaskFailureMessage>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void TaskFailureMessageDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskFailureMessage *pp = omnetpp::fromAnyPtr<TaskFailureMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'TaskFailureMessage'", field);
    }
}

const char *TaskFailureMessageDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskFailureMessage *pp = omnetpp::fromAnyPtr<TaskFailureMessage>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string TaskFailureMessageDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskFailureMessage *pp = omnetpp::fromAnyPtr<TaskFailureMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return oppstring2string(pp->getTask_id());
        case FIELD_vehicle_id: return oppstring2string(pp->getVehicle_id());
        case FIELD_failure_time: return double2string(pp->getFailure_time());
        case FIELD_failure_reason: return oppstring2string(pp->getFailure_reason());
        case FIELD_wasted_time: return double2string(pp->getWasted_time());
        default: return "";
    }
}

void TaskFailureMessageDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskFailureMessage *pp = omnetpp::fromAnyPtr<TaskFailureMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id((value)); break;
        case FIELD_vehicle_id: pp->setVehicle_id((value)); break;
        case FIELD_failure_time: pp->setFailure_time(string2double(value)); break;
        case FIELD_failure_reason: pp->setFailure_reason((value)); break;
        case FIELD_wasted_time: pp->setWasted_time(string2double(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskFailureMessage'", field);
    }
}

omnetpp::cValue TaskFailureMessageDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    TaskFailureMessage *pp = omnetpp::fromAnyPtr<TaskFailureMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return pp->getTask_id();
        case FIELD_vehicle_id: return pp->getVehicle_id();
        case FIELD_failure_time: return pp->getFailure_time();
        case FIELD_failure_reason: return pp->getFailure_reason();
        case FIELD_wasted_time: return pp->getWasted_time();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'TaskFailureMessage' as cValue -- field index out of range?", field);
    }
}

void TaskFailureMessageDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskFailureMessage *pp = omnetpp::fromAnyPtr<TaskFailureMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id(value.stringValue()); break;
        case FIELD_vehicle_id: pp->setVehicle_id(value.stringValue()); break;
        case FIELD_failure_time: pp->setFailure_time(value.doubleValue()); break;
        case FIELD_failure_reason: pp->setFailure_reason(value.stringValue()); break;
        case FIELD_wasted_time: pp->setWasted_time(value.doubleValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskFailureMessage'", field);
    }
}

const char *TaskFailureMessageDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr TaskFailureMessageDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    TaskFailureMessage *pp = omnetpp::fromAnyPtr<TaskFailureMessage>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void TaskFailureMessageDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskFailureMessage *pp = omnetpp::fromAnyPtr<TaskFailureMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskFailureMessage'", field);
    }
}

Register_Class(VehicleResourceStatusMessage)

VehicleResourceStatusMessage::VehicleResourceStatusMessage(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

VehicleResourceStatusMessage::VehicleResourceStatusMessage(const VehicleResourceStatusMessage& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

VehicleResourceStatusMessage::~VehicleResourceStatusMessage()
{
}

VehicleResourceStatusMessage& VehicleResourceStatusMessage::operator=(const VehicleResourceStatusMessage& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void VehicleResourceStatusMessage::copy(const VehicleResourceStatusMessage& other)
{
    this->vehicle_id = other.vehicle_id;
    this->pos_x = other.pos_x;
    this->pos_y = other.pos_y;
    this->speed = other.speed;
    this->acceleration = other.acceleration;
    this->heading = other.heading;
    this->cpu_total = other.cpu_total;
    this->cpu_allocable = other.cpu_allocable;
    this->cpu_available = other.cpu_available;
    this->cpu_utilization = other.cpu_utilization;
    this->mem_total = other.mem_total;
    this->mem_available = other.mem_available;
    this->mem_utilization = other.mem_utilization;
    this->tasks_generated = other.tasks_generated;
    this->tasks_completed_on_time = other.tasks_completed_on_time;
    this->tasks_completed_late = other.tasks_completed_late;
    this->tasks_failed = other.tasks_failed;
    this->tasks_rejected = other.tasks_rejected;
    this->current_queue_length = other.current_queue_length;
    this->current_processing_count = other.current_processing_count;
    this->avg_completion_time = other.avg_completion_time;
    this->deadline_miss_ratio = other.deadline_miss_ratio;
}

void VehicleResourceStatusMessage::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->vehicle_id);
    doParsimPacking(b,this->pos_x);
    doParsimPacking(b,this->pos_y);
    doParsimPacking(b,this->speed);
    doParsimPacking(b,this->acceleration);
    doParsimPacking(b,this->heading);
    doParsimPacking(b,this->cpu_total);
    doParsimPacking(b,this->cpu_allocable);
    doParsimPacking(b,this->cpu_available);
    doParsimPacking(b,this->cpu_utilization);
    doParsimPacking(b,this->mem_total);
    doParsimPacking(b,this->mem_available);
    doParsimPacking(b,this->mem_utilization);
    doParsimPacking(b,this->tasks_generated);
    doParsimPacking(b,this->tasks_completed_on_time);
    doParsimPacking(b,this->tasks_completed_late);
    doParsimPacking(b,this->tasks_failed);
    doParsimPacking(b,this->tasks_rejected);
    doParsimPacking(b,this->current_queue_length);
    doParsimPacking(b,this->current_processing_count);
    doParsimPacking(b,this->avg_completion_time);
    doParsimPacking(b,this->deadline_miss_ratio);
}

void VehicleResourceStatusMessage::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->vehicle_id);
    doParsimUnpacking(b,this->pos_x);
    doParsimUnpacking(b,this->pos_y);
    doParsimUnpacking(b,this->speed);
    doParsimUnpacking(b,this->acceleration);
    doParsimUnpacking(b,this->heading);
    doParsimUnpacking(b,this->cpu_total);
    doParsimUnpacking(b,this->cpu_allocable);
    doParsimUnpacking(b,this->cpu_available);
    doParsimUnpacking(b,this->cpu_utilization);
    doParsimUnpacking(b,this->mem_total);
    doParsimUnpacking(b,this->mem_available);
    doParsimUnpacking(b,this->mem_utilization);
    doParsimUnpacking(b,this->tasks_generated);
    doParsimUnpacking(b,this->tasks_completed_on_time);
    doParsimUnpacking(b,this->tasks_completed_late);
    doParsimUnpacking(b,this->tasks_failed);
    doParsimUnpacking(b,this->tasks_rejected);
    doParsimUnpacking(b,this->current_queue_length);
    doParsimUnpacking(b,this->current_processing_count);
    doParsimUnpacking(b,this->avg_completion_time);
    doParsimUnpacking(b,this->deadline_miss_ratio);
}

const char * VehicleResourceStatusMessage::getVehicle_id() const
{
    return this->vehicle_id.c_str();
}

void VehicleResourceStatusMessage::setVehicle_id(const char * vehicle_id)
{
    this->vehicle_id = vehicle_id;
}

double VehicleResourceStatusMessage::getPos_x() const
{
    return this->pos_x;
}

void VehicleResourceStatusMessage::setPos_x(double pos_x)
{
    this->pos_x = pos_x;
}

double VehicleResourceStatusMessage::getPos_y() const
{
    return this->pos_y;
}

void VehicleResourceStatusMessage::setPos_y(double pos_y)
{
    this->pos_y = pos_y;
}

double VehicleResourceStatusMessage::getSpeed() const
{
    return this->speed;
}

void VehicleResourceStatusMessage::setSpeed(double speed)
{
    this->speed = speed;
}

double VehicleResourceStatusMessage::getAcceleration() const
{
    return this->acceleration;
}

void VehicleResourceStatusMessage::setAcceleration(double acceleration)
{
    this->acceleration = acceleration;
}

double VehicleResourceStatusMessage::getHeading() const
{
    return this->heading;
}

void VehicleResourceStatusMessage::setHeading(double heading)
{
    this->heading = heading;
}

double VehicleResourceStatusMessage::getCpu_total() const
{
    return this->cpu_total;
}

void VehicleResourceStatusMessage::setCpu_total(double cpu_total)
{
    this->cpu_total = cpu_total;
}

double VehicleResourceStatusMessage::getCpu_allocable() const
{
    return this->cpu_allocable;
}

void VehicleResourceStatusMessage::setCpu_allocable(double cpu_allocable)
{
    this->cpu_allocable = cpu_allocable;
}

double VehicleResourceStatusMessage::getCpu_available() const
{
    return this->cpu_available;
}

void VehicleResourceStatusMessage::setCpu_available(double cpu_available)
{
    this->cpu_available = cpu_available;
}

double VehicleResourceStatusMessage::getCpu_utilization() const
{
    return this->cpu_utilization;
}

void VehicleResourceStatusMessage::setCpu_utilization(double cpu_utilization)
{
    this->cpu_utilization = cpu_utilization;
}

double VehicleResourceStatusMessage::getMem_total() const
{
    return this->mem_total;
}

void VehicleResourceStatusMessage::setMem_total(double mem_total)
{
    this->mem_total = mem_total;
}

double VehicleResourceStatusMessage::getMem_available() const
{
    return this->mem_available;
}

void VehicleResourceStatusMessage::setMem_available(double mem_available)
{
    this->mem_available = mem_available;
}

double VehicleResourceStatusMessage::getMem_utilization() const
{
    return this->mem_utilization;
}

void VehicleResourceStatusMessage::setMem_utilization(double mem_utilization)
{
    this->mem_utilization = mem_utilization;
}

uint32_t VehicleResourceStatusMessage::getTasks_generated() const
{
    return this->tasks_generated;
}

void VehicleResourceStatusMessage::setTasks_generated(uint32_t tasks_generated)
{
    this->tasks_generated = tasks_generated;
}

uint32_t VehicleResourceStatusMessage::getTasks_completed_on_time() const
{
    return this->tasks_completed_on_time;
}

void VehicleResourceStatusMessage::setTasks_completed_on_time(uint32_t tasks_completed_on_time)
{
    this->tasks_completed_on_time = tasks_completed_on_time;
}

uint32_t VehicleResourceStatusMessage::getTasks_completed_late() const
{
    return this->tasks_completed_late;
}

void VehicleResourceStatusMessage::setTasks_completed_late(uint32_t tasks_completed_late)
{
    this->tasks_completed_late = tasks_completed_late;
}

uint32_t VehicleResourceStatusMessage::getTasks_failed() const
{
    return this->tasks_failed;
}

void VehicleResourceStatusMessage::setTasks_failed(uint32_t tasks_failed)
{
    this->tasks_failed = tasks_failed;
}

uint32_t VehicleResourceStatusMessage::getTasks_rejected() const
{
    return this->tasks_rejected;
}

void VehicleResourceStatusMessage::setTasks_rejected(uint32_t tasks_rejected)
{
    this->tasks_rejected = tasks_rejected;
}

uint32_t VehicleResourceStatusMessage::getCurrent_queue_length() const
{
    return this->current_queue_length;
}

void VehicleResourceStatusMessage::setCurrent_queue_length(uint32_t current_queue_length)
{
    this->current_queue_length = current_queue_length;
}

uint32_t VehicleResourceStatusMessage::getCurrent_processing_count() const
{
    return this->current_processing_count;
}

void VehicleResourceStatusMessage::setCurrent_processing_count(uint32_t current_processing_count)
{
    this->current_processing_count = current_processing_count;
}

double VehicleResourceStatusMessage::getAvg_completion_time() const
{
    return this->avg_completion_time;
}

void VehicleResourceStatusMessage::setAvg_completion_time(double avg_completion_time)
{
    this->avg_completion_time = avg_completion_time;
}

double VehicleResourceStatusMessage::getDeadline_miss_ratio() const
{
    return this->deadline_miss_ratio;
}

void VehicleResourceStatusMessage::setDeadline_miss_ratio(double deadline_miss_ratio)
{
    this->deadline_miss_ratio = deadline_miss_ratio;
}

class VehicleResourceStatusMessageDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_vehicle_id,
        FIELD_pos_x,
        FIELD_pos_y,
        FIELD_speed,
        FIELD_acceleration,
        FIELD_heading,
        FIELD_cpu_total,
        FIELD_cpu_allocable,
        FIELD_cpu_available,
        FIELD_cpu_utilization,
        FIELD_mem_total,
        FIELD_mem_available,
        FIELD_mem_utilization,
        FIELD_tasks_generated,
        FIELD_tasks_completed_on_time,
        FIELD_tasks_completed_late,
        FIELD_tasks_failed,
        FIELD_tasks_rejected,
        FIELD_current_queue_length,
        FIELD_current_processing_count,
        FIELD_avg_completion_time,
        FIELD_deadline_miss_ratio,
    };
  public:
    VehicleResourceStatusMessageDescriptor();
    virtual ~VehicleResourceStatusMessageDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(VehicleResourceStatusMessageDescriptor)

VehicleResourceStatusMessageDescriptor::VehicleResourceStatusMessageDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(veins::VehicleResourceStatusMessage)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

VehicleResourceStatusMessageDescriptor::~VehicleResourceStatusMessageDescriptor()
{
    delete[] propertyNames;
}

bool VehicleResourceStatusMessageDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<VehicleResourceStatusMessage *>(obj)!=nullptr;
}

const char **VehicleResourceStatusMessageDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *VehicleResourceStatusMessageDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int VehicleResourceStatusMessageDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 22+base->getFieldCount() : 22;
}

unsigned int VehicleResourceStatusMessageDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_vehicle_id
        FD_ISEDITABLE,    // FIELD_pos_x
        FD_ISEDITABLE,    // FIELD_pos_y
        FD_ISEDITABLE,    // FIELD_speed
        FD_ISEDITABLE,    // FIELD_acceleration
        FD_ISEDITABLE,    // FIELD_heading
        FD_ISEDITABLE,    // FIELD_cpu_total
        FD_ISEDITABLE,    // FIELD_cpu_allocable
        FD_ISEDITABLE,    // FIELD_cpu_available
        FD_ISEDITABLE,    // FIELD_cpu_utilization
        FD_ISEDITABLE,    // FIELD_mem_total
        FD_ISEDITABLE,    // FIELD_mem_available
        FD_ISEDITABLE,    // FIELD_mem_utilization
        FD_ISEDITABLE,    // FIELD_tasks_generated
        FD_ISEDITABLE,    // FIELD_tasks_completed_on_time
        FD_ISEDITABLE,    // FIELD_tasks_completed_late
        FD_ISEDITABLE,    // FIELD_tasks_failed
        FD_ISEDITABLE,    // FIELD_tasks_rejected
        FD_ISEDITABLE,    // FIELD_current_queue_length
        FD_ISEDITABLE,    // FIELD_current_processing_count
        FD_ISEDITABLE,    // FIELD_avg_completion_time
        FD_ISEDITABLE,    // FIELD_deadline_miss_ratio
    };
    return (field >= 0 && field < 22) ? fieldTypeFlags[field] : 0;
}

const char *VehicleResourceStatusMessageDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "vehicle_id",
        "pos_x",
        "pos_y",
        "speed",
        "acceleration",
        "heading",
        "cpu_total",
        "cpu_allocable",
        "cpu_available",
        "cpu_utilization",
        "mem_total",
        "mem_available",
        "mem_utilization",
        "tasks_generated",
        "tasks_completed_on_time",
        "tasks_completed_late",
        "tasks_failed",
        "tasks_rejected",
        "current_queue_length",
        "current_processing_count",
        "avg_completion_time",
        "deadline_miss_ratio",
    };
    return (field >= 0 && field < 22) ? fieldNames[field] : nullptr;
}

int VehicleResourceStatusMessageDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "vehicle_id") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "pos_x") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "pos_y") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "speed") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "acceleration") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "heading") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "cpu_total") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "cpu_allocable") == 0) return baseIndex + 7;
    if (strcmp(fieldName, "cpu_available") == 0) return baseIndex + 8;
    if (strcmp(fieldName, "cpu_utilization") == 0) return baseIndex + 9;
    if (strcmp(fieldName, "mem_total") == 0) return baseIndex + 10;
    if (strcmp(fieldName, "mem_available") == 0) return baseIndex + 11;
    if (strcmp(fieldName, "mem_utilization") == 0) return baseIndex + 12;
    if (strcmp(fieldName, "tasks_generated") == 0) return baseIndex + 13;
    if (strcmp(fieldName, "tasks_completed_on_time") == 0) return baseIndex + 14;
    if (strcmp(fieldName, "tasks_completed_late") == 0) return baseIndex + 15;
    if (strcmp(fieldName, "tasks_failed") == 0) return baseIndex + 16;
    if (strcmp(fieldName, "tasks_rejected") == 0) return baseIndex + 17;
    if (strcmp(fieldName, "current_queue_length") == 0) return baseIndex + 18;
    if (strcmp(fieldName, "current_processing_count") == 0) return baseIndex + 19;
    if (strcmp(fieldName, "avg_completion_time") == 0) return baseIndex + 20;
    if (strcmp(fieldName, "deadline_miss_ratio") == 0) return baseIndex + 21;
    return base ? base->findField(fieldName) : -1;
}

const char *VehicleResourceStatusMessageDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "string",    // FIELD_vehicle_id
        "double",    // FIELD_pos_x
        "double",    // FIELD_pos_y
        "double",    // FIELD_speed
        "double",    // FIELD_acceleration
        "double",    // FIELD_heading
        "double",    // FIELD_cpu_total
        "double",    // FIELD_cpu_allocable
        "double",    // FIELD_cpu_available
        "double",    // FIELD_cpu_utilization
        "double",    // FIELD_mem_total
        "double",    // FIELD_mem_available
        "double",    // FIELD_mem_utilization
        "uint32_t",    // FIELD_tasks_generated
        "uint32_t",    // FIELD_tasks_completed_on_time
        "uint32_t",    // FIELD_tasks_completed_late
        "uint32_t",    // FIELD_tasks_failed
        "uint32_t",    // FIELD_tasks_rejected
        "uint32_t",    // FIELD_current_queue_length
        "uint32_t",    // FIELD_current_processing_count
        "double",    // FIELD_avg_completion_time
        "double",    // FIELD_deadline_miss_ratio
    };
    return (field >= 0 && field < 22) ? fieldTypeStrings[field] : nullptr;
}

const char **VehicleResourceStatusMessageDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *VehicleResourceStatusMessageDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int VehicleResourceStatusMessageDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    VehicleResourceStatusMessage *pp = omnetpp::fromAnyPtr<VehicleResourceStatusMessage>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void VehicleResourceStatusMessageDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    VehicleResourceStatusMessage *pp = omnetpp::fromAnyPtr<VehicleResourceStatusMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'VehicleResourceStatusMessage'", field);
    }
}

const char *VehicleResourceStatusMessageDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    VehicleResourceStatusMessage *pp = omnetpp::fromAnyPtr<VehicleResourceStatusMessage>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string VehicleResourceStatusMessageDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    VehicleResourceStatusMessage *pp = omnetpp::fromAnyPtr<VehicleResourceStatusMessage>(object); (void)pp;
    switch (field) {
        case FIELD_vehicle_id: return oppstring2string(pp->getVehicle_id());
        case FIELD_pos_x: return double2string(pp->getPos_x());
        case FIELD_pos_y: return double2string(pp->getPos_y());
        case FIELD_speed: return double2string(pp->getSpeed());
        case FIELD_acceleration: return double2string(pp->getAcceleration());
        case FIELD_heading: return double2string(pp->getHeading());
        case FIELD_cpu_total: return double2string(pp->getCpu_total());
        case FIELD_cpu_allocable: return double2string(pp->getCpu_allocable());
        case FIELD_cpu_available: return double2string(pp->getCpu_available());
        case FIELD_cpu_utilization: return double2string(pp->getCpu_utilization());
        case FIELD_mem_total: return double2string(pp->getMem_total());
        case FIELD_mem_available: return double2string(pp->getMem_available());
        case FIELD_mem_utilization: return double2string(pp->getMem_utilization());
        case FIELD_tasks_generated: return ulong2string(pp->getTasks_generated());
        case FIELD_tasks_completed_on_time: return ulong2string(pp->getTasks_completed_on_time());
        case FIELD_tasks_completed_late: return ulong2string(pp->getTasks_completed_late());
        case FIELD_tasks_failed: return ulong2string(pp->getTasks_failed());
        case FIELD_tasks_rejected: return ulong2string(pp->getTasks_rejected());
        case FIELD_current_queue_length: return ulong2string(pp->getCurrent_queue_length());
        case FIELD_current_processing_count: return ulong2string(pp->getCurrent_processing_count());
        case FIELD_avg_completion_time: return double2string(pp->getAvg_completion_time());
        case FIELD_deadline_miss_ratio: return double2string(pp->getDeadline_miss_ratio());
        default: return "";
    }
}

void VehicleResourceStatusMessageDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    VehicleResourceStatusMessage *pp = omnetpp::fromAnyPtr<VehicleResourceStatusMessage>(object); (void)pp;
    switch (field) {
        case FIELD_vehicle_id: pp->setVehicle_id((value)); break;
        case FIELD_pos_x: pp->setPos_x(string2double(value)); break;
        case FIELD_pos_y: pp->setPos_y(string2double(value)); break;
        case FIELD_speed: pp->setSpeed(string2double(value)); break;
        case FIELD_acceleration: pp->setAcceleration(string2double(value)); break;
        case FIELD_heading: pp->setHeading(string2double(value)); break;
        case FIELD_cpu_total: pp->setCpu_total(string2double(value)); break;
        case FIELD_cpu_allocable: pp->setCpu_allocable(string2double(value)); break;
        case FIELD_cpu_available: pp->setCpu_available(string2double(value)); break;
        case FIELD_cpu_utilization: pp->setCpu_utilization(string2double(value)); break;
        case FIELD_mem_total: pp->setMem_total(string2double(value)); break;
        case FIELD_mem_available: pp->setMem_available(string2double(value)); break;
        case FIELD_mem_utilization: pp->setMem_utilization(string2double(value)); break;
        case FIELD_tasks_generated: pp->setTasks_generated(string2ulong(value)); break;
        case FIELD_tasks_completed_on_time: pp->setTasks_completed_on_time(string2ulong(value)); break;
        case FIELD_tasks_completed_late: pp->setTasks_completed_late(string2ulong(value)); break;
        case FIELD_tasks_failed: pp->setTasks_failed(string2ulong(value)); break;
        case FIELD_tasks_rejected: pp->setTasks_rejected(string2ulong(value)); break;
        case FIELD_current_queue_length: pp->setCurrent_queue_length(string2ulong(value)); break;
        case FIELD_current_processing_count: pp->setCurrent_processing_count(string2ulong(value)); break;
        case FIELD_avg_completion_time: pp->setAvg_completion_time(string2double(value)); break;
        case FIELD_deadline_miss_ratio: pp->setDeadline_miss_ratio(string2double(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'VehicleResourceStatusMessage'", field);
    }
}

omnetpp::cValue VehicleResourceStatusMessageDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    VehicleResourceStatusMessage *pp = omnetpp::fromAnyPtr<VehicleResourceStatusMessage>(object); (void)pp;
    switch (field) {
        case FIELD_vehicle_id: return pp->getVehicle_id();
        case FIELD_pos_x: return pp->getPos_x();
        case FIELD_pos_y: return pp->getPos_y();
        case FIELD_speed: return pp->getSpeed();
        case FIELD_acceleration: return pp->getAcceleration();
        case FIELD_heading: return pp->getHeading();
        case FIELD_cpu_total: return pp->getCpu_total();
        case FIELD_cpu_allocable: return pp->getCpu_allocable();
        case FIELD_cpu_available: return pp->getCpu_available();
        case FIELD_cpu_utilization: return pp->getCpu_utilization();
        case FIELD_mem_total: return pp->getMem_total();
        case FIELD_mem_available: return pp->getMem_available();
        case FIELD_mem_utilization: return pp->getMem_utilization();
        case FIELD_tasks_generated: return (omnetpp::intval_t)(pp->getTasks_generated());
        case FIELD_tasks_completed_on_time: return (omnetpp::intval_t)(pp->getTasks_completed_on_time());
        case FIELD_tasks_completed_late: return (omnetpp::intval_t)(pp->getTasks_completed_late());
        case FIELD_tasks_failed: return (omnetpp::intval_t)(pp->getTasks_failed());
        case FIELD_tasks_rejected: return (omnetpp::intval_t)(pp->getTasks_rejected());
        case FIELD_current_queue_length: return (omnetpp::intval_t)(pp->getCurrent_queue_length());
        case FIELD_current_processing_count: return (omnetpp::intval_t)(pp->getCurrent_processing_count());
        case FIELD_avg_completion_time: return pp->getAvg_completion_time();
        case FIELD_deadline_miss_ratio: return pp->getDeadline_miss_ratio();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'VehicleResourceStatusMessage' as cValue -- field index out of range?", field);
    }
}

void VehicleResourceStatusMessageDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    VehicleResourceStatusMessage *pp = omnetpp::fromAnyPtr<VehicleResourceStatusMessage>(object); (void)pp;
    switch (field) {
        case FIELD_vehicle_id: pp->setVehicle_id(value.stringValue()); break;
        case FIELD_pos_x: pp->setPos_x(value.doubleValue()); break;
        case FIELD_pos_y: pp->setPos_y(value.doubleValue()); break;
        case FIELD_speed: pp->setSpeed(value.doubleValue()); break;
        case FIELD_acceleration: pp->setAcceleration(value.doubleValue()); break;
        case FIELD_heading: pp->setHeading(value.doubleValue()); break;
        case FIELD_cpu_total: pp->setCpu_total(value.doubleValue()); break;
        case FIELD_cpu_allocable: pp->setCpu_allocable(value.doubleValue()); break;
        case FIELD_cpu_available: pp->setCpu_available(value.doubleValue()); break;
        case FIELD_cpu_utilization: pp->setCpu_utilization(value.doubleValue()); break;
        case FIELD_mem_total: pp->setMem_total(value.doubleValue()); break;
        case FIELD_mem_available: pp->setMem_available(value.doubleValue()); break;
        case FIELD_mem_utilization: pp->setMem_utilization(value.doubleValue()); break;
        case FIELD_tasks_generated: pp->setTasks_generated(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_tasks_completed_on_time: pp->setTasks_completed_on_time(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_tasks_completed_late: pp->setTasks_completed_late(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_tasks_failed: pp->setTasks_failed(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_tasks_rejected: pp->setTasks_rejected(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_current_queue_length: pp->setCurrent_queue_length(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_current_processing_count: pp->setCurrent_processing_count(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_avg_completion_time: pp->setAvg_completion_time(value.doubleValue()); break;
        case FIELD_deadline_miss_ratio: pp->setDeadline_miss_ratio(value.doubleValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'VehicleResourceStatusMessage'", field);
    }
}

const char *VehicleResourceStatusMessageDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr VehicleResourceStatusMessageDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    VehicleResourceStatusMessage *pp = omnetpp::fromAnyPtr<VehicleResourceStatusMessage>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void VehicleResourceStatusMessageDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    VehicleResourceStatusMessage *pp = omnetpp::fromAnyPtr<VehicleResourceStatusMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'VehicleResourceStatusMessage'", field);
    }
}

Register_Class(OffloadingRequestMessage)

OffloadingRequestMessage::OffloadingRequestMessage(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

OffloadingRequestMessage::OffloadingRequestMessage(const OffloadingRequestMessage& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

OffloadingRequestMessage::~OffloadingRequestMessage()
{
}

OffloadingRequestMessage& OffloadingRequestMessage::operator=(const OffloadingRequestMessage& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void OffloadingRequestMessage::copy(const OffloadingRequestMessage& other)
{
    this->senderAddress = other.senderAddress;
    this->task_id = other.task_id;
    this->vehicle_id = other.vehicle_id;
    this->request_time = other.request_time;
    this->task_size_bytes = other.task_size_bytes;
    this->cpu_cycles = other.cpu_cycles;
    this->deadline_seconds = other.deadline_seconds;
    this->qos_value = other.qos_value;
    this->local_cpu_available_ghz = other.local_cpu_available_ghz;
    this->local_cpu_utilization = other.local_cpu_utilization;
    this->local_mem_available_mb = other.local_mem_available_mb;
    this->local_queue_length = other.local_queue_length;
    this->local_processing_count = other.local_processing_count;
    this->pos_x = other.pos_x;
    this->pos_y = other.pos_y;
    this->speed = other.speed;
    this->local_decision = other.local_decision;
}

void OffloadingRequestMessage::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->senderAddress);
    doParsimPacking(b,this->task_id);
    doParsimPacking(b,this->vehicle_id);
    doParsimPacking(b,this->request_time);
    doParsimPacking(b,this->task_size_bytes);
    doParsimPacking(b,this->cpu_cycles);
    doParsimPacking(b,this->deadline_seconds);
    doParsimPacking(b,this->qos_value);
    doParsimPacking(b,this->local_cpu_available_ghz);
    doParsimPacking(b,this->local_cpu_utilization);
    doParsimPacking(b,this->local_mem_available_mb);
    doParsimPacking(b,this->local_queue_length);
    doParsimPacking(b,this->local_processing_count);
    doParsimPacking(b,this->pos_x);
    doParsimPacking(b,this->pos_y);
    doParsimPacking(b,this->speed);
    doParsimPacking(b,this->local_decision);
}

void OffloadingRequestMessage::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->senderAddress);
    doParsimUnpacking(b,this->task_id);
    doParsimUnpacking(b,this->vehicle_id);
    doParsimUnpacking(b,this->request_time);
    doParsimUnpacking(b,this->task_size_bytes);
    doParsimUnpacking(b,this->cpu_cycles);
    doParsimUnpacking(b,this->deadline_seconds);
    doParsimUnpacking(b,this->qos_value);
    doParsimUnpacking(b,this->local_cpu_available_ghz);
    doParsimUnpacking(b,this->local_cpu_utilization);
    doParsimUnpacking(b,this->local_mem_available_mb);
    doParsimUnpacking(b,this->local_queue_length);
    doParsimUnpacking(b,this->local_processing_count);
    doParsimUnpacking(b,this->pos_x);
    doParsimUnpacking(b,this->pos_y);
    doParsimUnpacking(b,this->speed);
    doParsimUnpacking(b,this->local_decision);
}

const LAddress::L2Type& OffloadingRequestMessage::getSenderAddress() const
{
    return this->senderAddress;
}

void OffloadingRequestMessage::setSenderAddress(const LAddress::L2Type& senderAddress)
{
    this->senderAddress = senderAddress;
}

const char * OffloadingRequestMessage::getTask_id() const
{
    return this->task_id.c_str();
}

void OffloadingRequestMessage::setTask_id(const char * task_id)
{
    this->task_id = task_id;
}

const char * OffloadingRequestMessage::getVehicle_id() const
{
    return this->vehicle_id.c_str();
}

void OffloadingRequestMessage::setVehicle_id(const char * vehicle_id)
{
    this->vehicle_id = vehicle_id;
}

double OffloadingRequestMessage::getRequest_time() const
{
    return this->request_time;
}

void OffloadingRequestMessage::setRequest_time(double request_time)
{
    this->request_time = request_time;
}

uint64_t OffloadingRequestMessage::getTask_size_bytes() const
{
    return this->task_size_bytes;
}

void OffloadingRequestMessage::setTask_size_bytes(uint64_t task_size_bytes)
{
    this->task_size_bytes = task_size_bytes;
}

uint64_t OffloadingRequestMessage::getCpu_cycles() const
{
    return this->cpu_cycles;
}

void OffloadingRequestMessage::setCpu_cycles(uint64_t cpu_cycles)
{
    this->cpu_cycles = cpu_cycles;
}

double OffloadingRequestMessage::getDeadline_seconds() const
{
    return this->deadline_seconds;
}

void OffloadingRequestMessage::setDeadline_seconds(double deadline_seconds)
{
    this->deadline_seconds = deadline_seconds;
}

double OffloadingRequestMessage::getQos_value() const
{
    return this->qos_value;
}

void OffloadingRequestMessage::setQos_value(double qos_value)
{
    this->qos_value = qos_value;
}

double OffloadingRequestMessage::getLocal_cpu_available_ghz() const
{
    return this->local_cpu_available_ghz;
}

void OffloadingRequestMessage::setLocal_cpu_available_ghz(double local_cpu_available_ghz)
{
    this->local_cpu_available_ghz = local_cpu_available_ghz;
}

double OffloadingRequestMessage::getLocal_cpu_utilization() const
{
    return this->local_cpu_utilization;
}

void OffloadingRequestMessage::setLocal_cpu_utilization(double local_cpu_utilization)
{
    this->local_cpu_utilization = local_cpu_utilization;
}

double OffloadingRequestMessage::getLocal_mem_available_mb() const
{
    return this->local_mem_available_mb;
}

void OffloadingRequestMessage::setLocal_mem_available_mb(double local_mem_available_mb)
{
    this->local_mem_available_mb = local_mem_available_mb;
}

uint32_t OffloadingRequestMessage::getLocal_queue_length() const
{
    return this->local_queue_length;
}

void OffloadingRequestMessage::setLocal_queue_length(uint32_t local_queue_length)
{
    this->local_queue_length = local_queue_length;
}

uint32_t OffloadingRequestMessage::getLocal_processing_count() const
{
    return this->local_processing_count;
}

void OffloadingRequestMessage::setLocal_processing_count(uint32_t local_processing_count)
{
    this->local_processing_count = local_processing_count;
}

double OffloadingRequestMessage::getPos_x() const
{
    return this->pos_x;
}

void OffloadingRequestMessage::setPos_x(double pos_x)
{
    this->pos_x = pos_x;
}

double OffloadingRequestMessage::getPos_y() const
{
    return this->pos_y;
}

void OffloadingRequestMessage::setPos_y(double pos_y)
{
    this->pos_y = pos_y;
}

double OffloadingRequestMessage::getSpeed() const
{
    return this->speed;
}

void OffloadingRequestMessage::setSpeed(double speed)
{
    this->speed = speed;
}

const char * OffloadingRequestMessage::getLocal_decision() const
{
    return this->local_decision.c_str();
}

void OffloadingRequestMessage::setLocal_decision(const char * local_decision)
{
    this->local_decision = local_decision;
}

class OffloadingRequestMessageDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_senderAddress,
        FIELD_task_id,
        FIELD_vehicle_id,
        FIELD_request_time,
        FIELD_task_size_bytes,
        FIELD_cpu_cycles,
        FIELD_deadline_seconds,
        FIELD_qos_value,
        FIELD_local_cpu_available_ghz,
        FIELD_local_cpu_utilization,
        FIELD_local_mem_available_mb,
        FIELD_local_queue_length,
        FIELD_local_processing_count,
        FIELD_pos_x,
        FIELD_pos_y,
        FIELD_speed,
        FIELD_local_decision,
    };
  public:
    OffloadingRequestMessageDescriptor();
    virtual ~OffloadingRequestMessageDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(OffloadingRequestMessageDescriptor)

OffloadingRequestMessageDescriptor::OffloadingRequestMessageDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(veins::OffloadingRequestMessage)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

OffloadingRequestMessageDescriptor::~OffloadingRequestMessageDescriptor()
{
    delete[] propertyNames;
}

bool OffloadingRequestMessageDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<OffloadingRequestMessage *>(obj)!=nullptr;
}

const char **OffloadingRequestMessageDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *OffloadingRequestMessageDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int OffloadingRequestMessageDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 17+base->getFieldCount() : 17;
}

unsigned int OffloadingRequestMessageDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        0,    // FIELD_senderAddress
        FD_ISEDITABLE,    // FIELD_task_id
        FD_ISEDITABLE,    // FIELD_vehicle_id
        FD_ISEDITABLE,    // FIELD_request_time
        FD_ISEDITABLE,    // FIELD_task_size_bytes
        FD_ISEDITABLE,    // FIELD_cpu_cycles
        FD_ISEDITABLE,    // FIELD_deadline_seconds
        FD_ISEDITABLE,    // FIELD_qos_value
        FD_ISEDITABLE,    // FIELD_local_cpu_available_ghz
        FD_ISEDITABLE,    // FIELD_local_cpu_utilization
        FD_ISEDITABLE,    // FIELD_local_mem_available_mb
        FD_ISEDITABLE,    // FIELD_local_queue_length
        FD_ISEDITABLE,    // FIELD_local_processing_count
        FD_ISEDITABLE,    // FIELD_pos_x
        FD_ISEDITABLE,    // FIELD_pos_y
        FD_ISEDITABLE,    // FIELD_speed
        FD_ISEDITABLE,    // FIELD_local_decision
    };
    return (field >= 0 && field < 17) ? fieldTypeFlags[field] : 0;
}

const char *OffloadingRequestMessageDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "senderAddress",
        "task_id",
        "vehicle_id",
        "request_time",
        "task_size_bytes",
        "cpu_cycles",
        "deadline_seconds",
        "qos_value",
        "local_cpu_available_ghz",
        "local_cpu_utilization",
        "local_mem_available_mb",
        "local_queue_length",
        "local_processing_count",
        "pos_x",
        "pos_y",
        "speed",
        "local_decision",
    };
    return (field >= 0 && field < 17) ? fieldNames[field] : nullptr;
}

int OffloadingRequestMessageDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "senderAddress") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "task_id") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "vehicle_id") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "request_time") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "task_size_bytes") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "cpu_cycles") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "deadline_seconds") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "qos_value") == 0) return baseIndex + 7;
    if (strcmp(fieldName, "local_cpu_available_ghz") == 0) return baseIndex + 8;
    if (strcmp(fieldName, "local_cpu_utilization") == 0) return baseIndex + 9;
    if (strcmp(fieldName, "local_mem_available_mb") == 0) return baseIndex + 10;
    if (strcmp(fieldName, "local_queue_length") == 0) return baseIndex + 11;
    if (strcmp(fieldName, "local_processing_count") == 0) return baseIndex + 12;
    if (strcmp(fieldName, "pos_x") == 0) return baseIndex + 13;
    if (strcmp(fieldName, "pos_y") == 0) return baseIndex + 14;
    if (strcmp(fieldName, "speed") == 0) return baseIndex + 15;
    if (strcmp(fieldName, "local_decision") == 0) return baseIndex + 16;
    return base ? base->findField(fieldName) : -1;
}

const char *OffloadingRequestMessageDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "veins::LAddress::L2Type",    // FIELD_senderAddress
        "string",    // FIELD_task_id
        "string",    // FIELD_vehicle_id
        "double",    // FIELD_request_time
        "uint64_t",    // FIELD_task_size_bytes
        "uint64_t",    // FIELD_cpu_cycles
        "double",    // FIELD_deadline_seconds
        "double",    // FIELD_qos_value
        "double",    // FIELD_local_cpu_available_ghz
        "double",    // FIELD_local_cpu_utilization
        "double",    // FIELD_local_mem_available_mb
        "uint32_t",    // FIELD_local_queue_length
        "uint32_t",    // FIELD_local_processing_count
        "double",    // FIELD_pos_x
        "double",    // FIELD_pos_y
        "double",    // FIELD_speed
        "string",    // FIELD_local_decision
    };
    return (field >= 0 && field < 17) ? fieldTypeStrings[field] : nullptr;
}

const char **OffloadingRequestMessageDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *OffloadingRequestMessageDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int OffloadingRequestMessageDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    OffloadingRequestMessage *pp = omnetpp::fromAnyPtr<OffloadingRequestMessage>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void OffloadingRequestMessageDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadingRequestMessage *pp = omnetpp::fromAnyPtr<OffloadingRequestMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'OffloadingRequestMessage'", field);
    }
}

const char *OffloadingRequestMessageDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadingRequestMessage *pp = omnetpp::fromAnyPtr<OffloadingRequestMessage>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string OffloadingRequestMessageDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadingRequestMessage *pp = omnetpp::fromAnyPtr<OffloadingRequestMessage>(object); (void)pp;
    switch (field) {
        case FIELD_senderAddress: return "";
        case FIELD_task_id: return oppstring2string(pp->getTask_id());
        case FIELD_vehicle_id: return oppstring2string(pp->getVehicle_id());
        case FIELD_request_time: return double2string(pp->getRequest_time());
        case FIELD_task_size_bytes: return uint642string(pp->getTask_size_bytes());
        case FIELD_cpu_cycles: return uint642string(pp->getCpu_cycles());
        case FIELD_deadline_seconds: return double2string(pp->getDeadline_seconds());
        case FIELD_qos_value: return double2string(pp->getQos_value());
        case FIELD_local_cpu_available_ghz: return double2string(pp->getLocal_cpu_available_ghz());
        case FIELD_local_cpu_utilization: return double2string(pp->getLocal_cpu_utilization());
        case FIELD_local_mem_available_mb: return double2string(pp->getLocal_mem_available_mb());
        case FIELD_local_queue_length: return ulong2string(pp->getLocal_queue_length());
        case FIELD_local_processing_count: return ulong2string(pp->getLocal_processing_count());
        case FIELD_pos_x: return double2string(pp->getPos_x());
        case FIELD_pos_y: return double2string(pp->getPos_y());
        case FIELD_speed: return double2string(pp->getSpeed());
        case FIELD_local_decision: return oppstring2string(pp->getLocal_decision());
        default: return "";
    }
}

void OffloadingRequestMessageDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadingRequestMessage *pp = omnetpp::fromAnyPtr<OffloadingRequestMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id((value)); break;
        case FIELD_vehicle_id: pp->setVehicle_id((value)); break;
        case FIELD_request_time: pp->setRequest_time(string2double(value)); break;
        case FIELD_task_size_bytes: pp->setTask_size_bytes(string2uint64(value)); break;
        case FIELD_cpu_cycles: pp->setCpu_cycles(string2uint64(value)); break;
        case FIELD_deadline_seconds: pp->setDeadline_seconds(string2double(value)); break;
        case FIELD_qos_value: pp->setQos_value(string2double(value)); break;
        case FIELD_local_cpu_available_ghz: pp->setLocal_cpu_available_ghz(string2double(value)); break;
        case FIELD_local_cpu_utilization: pp->setLocal_cpu_utilization(string2double(value)); break;
        case FIELD_local_mem_available_mb: pp->setLocal_mem_available_mb(string2double(value)); break;
        case FIELD_local_queue_length: pp->setLocal_queue_length(string2ulong(value)); break;
        case FIELD_local_processing_count: pp->setLocal_processing_count(string2ulong(value)); break;
        case FIELD_pos_x: pp->setPos_x(string2double(value)); break;
        case FIELD_pos_y: pp->setPos_y(string2double(value)); break;
        case FIELD_speed: pp->setSpeed(string2double(value)); break;
        case FIELD_local_decision: pp->setLocal_decision((value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadingRequestMessage'", field);
    }
}

omnetpp::cValue OffloadingRequestMessageDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadingRequestMessage *pp = omnetpp::fromAnyPtr<OffloadingRequestMessage>(object); (void)pp;
    switch (field) {
        case FIELD_senderAddress: return omnetpp::toAnyPtr(&pp->getSenderAddress()); break;
        case FIELD_task_id: return pp->getTask_id();
        case FIELD_vehicle_id: return pp->getVehicle_id();
        case FIELD_request_time: return pp->getRequest_time();
        case FIELD_task_size_bytes: return (omnetpp::intval_t)(pp->getTask_size_bytes());
        case FIELD_cpu_cycles: return (omnetpp::intval_t)(pp->getCpu_cycles());
        case FIELD_deadline_seconds: return pp->getDeadline_seconds();
        case FIELD_qos_value: return pp->getQos_value();
        case FIELD_local_cpu_available_ghz: return pp->getLocal_cpu_available_ghz();
        case FIELD_local_cpu_utilization: return pp->getLocal_cpu_utilization();
        case FIELD_local_mem_available_mb: return pp->getLocal_mem_available_mb();
        case FIELD_local_queue_length: return (omnetpp::intval_t)(pp->getLocal_queue_length());
        case FIELD_local_processing_count: return (omnetpp::intval_t)(pp->getLocal_processing_count());
        case FIELD_pos_x: return pp->getPos_x();
        case FIELD_pos_y: return pp->getPos_y();
        case FIELD_speed: return pp->getSpeed();
        case FIELD_local_decision: return pp->getLocal_decision();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'OffloadingRequestMessage' as cValue -- field index out of range?", field);
    }
}

void OffloadingRequestMessageDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadingRequestMessage *pp = omnetpp::fromAnyPtr<OffloadingRequestMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id(value.stringValue()); break;
        case FIELD_vehicle_id: pp->setVehicle_id(value.stringValue()); break;
        case FIELD_request_time: pp->setRequest_time(value.doubleValue()); break;
        case FIELD_task_size_bytes: pp->setTask_size_bytes(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_cpu_cycles: pp->setCpu_cycles(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_deadline_seconds: pp->setDeadline_seconds(value.doubleValue()); break;
        case FIELD_qos_value: pp->setQos_value(value.doubleValue()); break;
        case FIELD_local_cpu_available_ghz: pp->setLocal_cpu_available_ghz(value.doubleValue()); break;
        case FIELD_local_cpu_utilization: pp->setLocal_cpu_utilization(value.doubleValue()); break;
        case FIELD_local_mem_available_mb: pp->setLocal_mem_available_mb(value.doubleValue()); break;
        case FIELD_local_queue_length: pp->setLocal_queue_length(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_local_processing_count: pp->setLocal_processing_count(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_pos_x: pp->setPos_x(value.doubleValue()); break;
        case FIELD_pos_y: pp->setPos_y(value.doubleValue()); break;
        case FIELD_speed: pp->setSpeed(value.doubleValue()); break;
        case FIELD_local_decision: pp->setLocal_decision(value.stringValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadingRequestMessage'", field);
    }
}

const char *OffloadingRequestMessageDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr OffloadingRequestMessageDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    OffloadingRequestMessage *pp = omnetpp::fromAnyPtr<OffloadingRequestMessage>(object); (void)pp;
    switch (field) {
        case FIELD_senderAddress: return omnetpp::toAnyPtr(&pp->getSenderAddress()); break;
        default: return omnetpp::any_ptr(nullptr);
    }
}

void OffloadingRequestMessageDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadingRequestMessage *pp = omnetpp::fromAnyPtr<OffloadingRequestMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadingRequestMessage'", field);
    }
}

Register_Class(OffloadingDecisionMessage)

OffloadingDecisionMessage::OffloadingDecisionMessage(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

OffloadingDecisionMessage::OffloadingDecisionMessage(const OffloadingDecisionMessage& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

OffloadingDecisionMessage::~OffloadingDecisionMessage()
{
}

OffloadingDecisionMessage& OffloadingDecisionMessage::operator=(const OffloadingDecisionMessage& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void OffloadingDecisionMessage::copy(const OffloadingDecisionMessage& other)
{
    this->senderAddress = other.senderAddress;
    this->task_id = other.task_id;
    this->vehicle_id = other.vehicle_id;
    this->decision_time = other.decision_time;
    this->decision_type = other.decision_type;
    this->target_service_vehicle_id = other.target_service_vehicle_id;
    this->target_service_vehicle_mac = other.target_service_vehicle_mac;
    this->confidence_score = other.confidence_score;
    this->estimated_completion_time = other.estimated_completion_time;
    this->decision_reason = other.decision_reason;
}

void OffloadingDecisionMessage::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->senderAddress);
    doParsimPacking(b,this->task_id);
    doParsimPacking(b,this->vehicle_id);
    doParsimPacking(b,this->decision_time);
    doParsimPacking(b,this->decision_type);
    doParsimPacking(b,this->target_service_vehicle_id);
    doParsimPacking(b,this->target_service_vehicle_mac);
    doParsimPacking(b,this->confidence_score);
    doParsimPacking(b,this->estimated_completion_time);
    doParsimPacking(b,this->decision_reason);
}

void OffloadingDecisionMessage::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->senderAddress);
    doParsimUnpacking(b,this->task_id);
    doParsimUnpacking(b,this->vehicle_id);
    doParsimUnpacking(b,this->decision_time);
    doParsimUnpacking(b,this->decision_type);
    doParsimUnpacking(b,this->target_service_vehicle_id);
    doParsimUnpacking(b,this->target_service_vehicle_mac);
    doParsimUnpacking(b,this->confidence_score);
    doParsimUnpacking(b,this->estimated_completion_time);
    doParsimUnpacking(b,this->decision_reason);
}

const LAddress::L2Type& OffloadingDecisionMessage::getSenderAddress() const
{
    return this->senderAddress;
}

void OffloadingDecisionMessage::setSenderAddress(const LAddress::L2Type& senderAddress)
{
    this->senderAddress = senderAddress;
}

const char * OffloadingDecisionMessage::getTask_id() const
{
    return this->task_id.c_str();
}

void OffloadingDecisionMessage::setTask_id(const char * task_id)
{
    this->task_id = task_id;
}

const char * OffloadingDecisionMessage::getVehicle_id() const
{
    return this->vehicle_id.c_str();
}

void OffloadingDecisionMessage::setVehicle_id(const char * vehicle_id)
{
    this->vehicle_id = vehicle_id;
}

double OffloadingDecisionMessage::getDecision_time() const
{
    return this->decision_time;
}

void OffloadingDecisionMessage::setDecision_time(double decision_time)
{
    this->decision_time = decision_time;
}

const char * OffloadingDecisionMessage::getDecision_type() const
{
    return this->decision_type.c_str();
}

void OffloadingDecisionMessage::setDecision_type(const char * decision_type)
{
    this->decision_type = decision_type;
}

const char * OffloadingDecisionMessage::getTarget_service_vehicle_id() const
{
    return this->target_service_vehicle_id.c_str();
}

void OffloadingDecisionMessage::setTarget_service_vehicle_id(const char * target_service_vehicle_id)
{
    this->target_service_vehicle_id = target_service_vehicle_id;
}

uint64_t OffloadingDecisionMessage::getTarget_service_vehicle_mac() const
{
    return this->target_service_vehicle_mac;
}

void OffloadingDecisionMessage::setTarget_service_vehicle_mac(uint64_t target_service_vehicle_mac)
{
    this->target_service_vehicle_mac = target_service_vehicle_mac;
}

double OffloadingDecisionMessage::getConfidence_score() const
{
    return this->confidence_score;
}

void OffloadingDecisionMessage::setConfidence_score(double confidence_score)
{
    this->confidence_score = confidence_score;
}

double OffloadingDecisionMessage::getEstimated_completion_time() const
{
    return this->estimated_completion_time;
}

void OffloadingDecisionMessage::setEstimated_completion_time(double estimated_completion_time)
{
    this->estimated_completion_time = estimated_completion_time;
}

const char * OffloadingDecisionMessage::getDecision_reason() const
{
    return this->decision_reason.c_str();
}

void OffloadingDecisionMessage::setDecision_reason(const char * decision_reason)
{
    this->decision_reason = decision_reason;
}

class OffloadingDecisionMessageDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_senderAddress,
        FIELD_task_id,
        FIELD_vehicle_id,
        FIELD_decision_time,
        FIELD_decision_type,
        FIELD_target_service_vehicle_id,
        FIELD_target_service_vehicle_mac,
        FIELD_confidence_score,
        FIELD_estimated_completion_time,
        FIELD_decision_reason,
    };
  public:
    OffloadingDecisionMessageDescriptor();
    virtual ~OffloadingDecisionMessageDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(OffloadingDecisionMessageDescriptor)

OffloadingDecisionMessageDescriptor::OffloadingDecisionMessageDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(veins::OffloadingDecisionMessage)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

OffloadingDecisionMessageDescriptor::~OffloadingDecisionMessageDescriptor()
{
    delete[] propertyNames;
}

bool OffloadingDecisionMessageDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<OffloadingDecisionMessage *>(obj)!=nullptr;
}

const char **OffloadingDecisionMessageDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *OffloadingDecisionMessageDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int OffloadingDecisionMessageDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 10+base->getFieldCount() : 10;
}

unsigned int OffloadingDecisionMessageDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        0,    // FIELD_senderAddress
        FD_ISEDITABLE,    // FIELD_task_id
        FD_ISEDITABLE,    // FIELD_vehicle_id
        FD_ISEDITABLE,    // FIELD_decision_time
        FD_ISEDITABLE,    // FIELD_decision_type
        FD_ISEDITABLE,    // FIELD_target_service_vehicle_id
        FD_ISEDITABLE,    // FIELD_target_service_vehicle_mac
        FD_ISEDITABLE,    // FIELD_confidence_score
        FD_ISEDITABLE,    // FIELD_estimated_completion_time
        FD_ISEDITABLE,    // FIELD_decision_reason
    };
    return (field >= 0 && field < 10) ? fieldTypeFlags[field] : 0;
}

const char *OffloadingDecisionMessageDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "senderAddress",
        "task_id",
        "vehicle_id",
        "decision_time",
        "decision_type",
        "target_service_vehicle_id",
        "target_service_vehicle_mac",
        "confidence_score",
        "estimated_completion_time",
        "decision_reason",
    };
    return (field >= 0 && field < 10) ? fieldNames[field] : nullptr;
}

int OffloadingDecisionMessageDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "senderAddress") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "task_id") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "vehicle_id") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "decision_time") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "decision_type") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "target_service_vehicle_id") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "target_service_vehicle_mac") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "confidence_score") == 0) return baseIndex + 7;
    if (strcmp(fieldName, "estimated_completion_time") == 0) return baseIndex + 8;
    if (strcmp(fieldName, "decision_reason") == 0) return baseIndex + 9;
    return base ? base->findField(fieldName) : -1;
}

const char *OffloadingDecisionMessageDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "veins::LAddress::L2Type",    // FIELD_senderAddress
        "string",    // FIELD_task_id
        "string",    // FIELD_vehicle_id
        "double",    // FIELD_decision_time
        "string",    // FIELD_decision_type
        "string",    // FIELD_target_service_vehicle_id
        "uint64_t",    // FIELD_target_service_vehicle_mac
        "double",    // FIELD_confidence_score
        "double",    // FIELD_estimated_completion_time
        "string",    // FIELD_decision_reason
    };
    return (field >= 0 && field < 10) ? fieldTypeStrings[field] : nullptr;
}

const char **OffloadingDecisionMessageDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *OffloadingDecisionMessageDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int OffloadingDecisionMessageDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    OffloadingDecisionMessage *pp = omnetpp::fromAnyPtr<OffloadingDecisionMessage>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void OffloadingDecisionMessageDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadingDecisionMessage *pp = omnetpp::fromAnyPtr<OffloadingDecisionMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'OffloadingDecisionMessage'", field);
    }
}

const char *OffloadingDecisionMessageDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadingDecisionMessage *pp = omnetpp::fromAnyPtr<OffloadingDecisionMessage>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string OffloadingDecisionMessageDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadingDecisionMessage *pp = omnetpp::fromAnyPtr<OffloadingDecisionMessage>(object); (void)pp;
    switch (field) {
        case FIELD_senderAddress: return "";
        case FIELD_task_id: return oppstring2string(pp->getTask_id());
        case FIELD_vehicle_id: return oppstring2string(pp->getVehicle_id());
        case FIELD_decision_time: return double2string(pp->getDecision_time());
        case FIELD_decision_type: return oppstring2string(pp->getDecision_type());
        case FIELD_target_service_vehicle_id: return oppstring2string(pp->getTarget_service_vehicle_id());
        case FIELD_target_service_vehicle_mac: return uint642string(pp->getTarget_service_vehicle_mac());
        case FIELD_confidence_score: return double2string(pp->getConfidence_score());
        case FIELD_estimated_completion_time: return double2string(pp->getEstimated_completion_time());
        case FIELD_decision_reason: return oppstring2string(pp->getDecision_reason());
        default: return "";
    }
}

void OffloadingDecisionMessageDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadingDecisionMessage *pp = omnetpp::fromAnyPtr<OffloadingDecisionMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id((value)); break;
        case FIELD_vehicle_id: pp->setVehicle_id((value)); break;
        case FIELD_decision_time: pp->setDecision_time(string2double(value)); break;
        case FIELD_decision_type: pp->setDecision_type((value)); break;
        case FIELD_target_service_vehicle_id: pp->setTarget_service_vehicle_id((value)); break;
        case FIELD_target_service_vehicle_mac: pp->setTarget_service_vehicle_mac(string2uint64(value)); break;
        case FIELD_confidence_score: pp->setConfidence_score(string2double(value)); break;
        case FIELD_estimated_completion_time: pp->setEstimated_completion_time(string2double(value)); break;
        case FIELD_decision_reason: pp->setDecision_reason((value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadingDecisionMessage'", field);
    }
}

omnetpp::cValue OffloadingDecisionMessageDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadingDecisionMessage *pp = omnetpp::fromAnyPtr<OffloadingDecisionMessage>(object); (void)pp;
    switch (field) {
        case FIELD_senderAddress: return omnetpp::toAnyPtr(&pp->getSenderAddress()); break;
        case FIELD_task_id: return pp->getTask_id();
        case FIELD_vehicle_id: return pp->getVehicle_id();
        case FIELD_decision_time: return pp->getDecision_time();
        case FIELD_decision_type: return pp->getDecision_type();
        case FIELD_target_service_vehicle_id: return pp->getTarget_service_vehicle_id();
        case FIELD_target_service_vehicle_mac: return (omnetpp::intval_t)(pp->getTarget_service_vehicle_mac());
        case FIELD_confidence_score: return pp->getConfidence_score();
        case FIELD_estimated_completion_time: return pp->getEstimated_completion_time();
        case FIELD_decision_reason: return pp->getDecision_reason();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'OffloadingDecisionMessage' as cValue -- field index out of range?", field);
    }
}

void OffloadingDecisionMessageDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadingDecisionMessage *pp = omnetpp::fromAnyPtr<OffloadingDecisionMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id(value.stringValue()); break;
        case FIELD_vehicle_id: pp->setVehicle_id(value.stringValue()); break;
        case FIELD_decision_time: pp->setDecision_time(value.doubleValue()); break;
        case FIELD_decision_type: pp->setDecision_type(value.stringValue()); break;
        case FIELD_target_service_vehicle_id: pp->setTarget_service_vehicle_id(value.stringValue()); break;
        case FIELD_target_service_vehicle_mac: pp->setTarget_service_vehicle_mac(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_confidence_score: pp->setConfidence_score(value.doubleValue()); break;
        case FIELD_estimated_completion_time: pp->setEstimated_completion_time(value.doubleValue()); break;
        case FIELD_decision_reason: pp->setDecision_reason(value.stringValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadingDecisionMessage'", field);
    }
}

const char *OffloadingDecisionMessageDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr OffloadingDecisionMessageDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    OffloadingDecisionMessage *pp = omnetpp::fromAnyPtr<OffloadingDecisionMessage>(object); (void)pp;
    switch (field) {
        case FIELD_senderAddress: return omnetpp::toAnyPtr(&pp->getSenderAddress()); break;
        default: return omnetpp::any_ptr(nullptr);
    }
}

void OffloadingDecisionMessageDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadingDecisionMessage *pp = omnetpp::fromAnyPtr<OffloadingDecisionMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadingDecisionMessage'", field);
    }
}

Register_Class(TaskOffloadPacket)

TaskOffloadPacket::TaskOffloadPacket(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

TaskOffloadPacket::TaskOffloadPacket(const TaskOffloadPacket& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

TaskOffloadPacket::~TaskOffloadPacket()
{
}

TaskOffloadPacket& TaskOffloadPacket::operator=(const TaskOffloadPacket& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void TaskOffloadPacket::copy(const TaskOffloadPacket& other)
{
    this->task_id = other.task_id;
    this->origin_vehicle_id = other.origin_vehicle_id;
    this->origin_vehicle_mac = other.origin_vehicle_mac;
    this->offload_time = other.offload_time;
    this->task_size_bytes = other.task_size_bytes;
    this->cpu_cycles = other.cpu_cycles;
    this->deadline_seconds = other.deadline_seconds;
    this->qos_value = other.qos_value;
    this->task_input_data = other.task_input_data;
}

void TaskOffloadPacket::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->task_id);
    doParsimPacking(b,this->origin_vehicle_id);
    doParsimPacking(b,this->origin_vehicle_mac);
    doParsimPacking(b,this->offload_time);
    doParsimPacking(b,this->task_size_bytes);
    doParsimPacking(b,this->cpu_cycles);
    doParsimPacking(b,this->deadline_seconds);
    doParsimPacking(b,this->qos_value);
    doParsimPacking(b,this->task_input_data);
}

void TaskOffloadPacket::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->task_id);
    doParsimUnpacking(b,this->origin_vehicle_id);
    doParsimUnpacking(b,this->origin_vehicle_mac);
    doParsimUnpacking(b,this->offload_time);
    doParsimUnpacking(b,this->task_size_bytes);
    doParsimUnpacking(b,this->cpu_cycles);
    doParsimUnpacking(b,this->deadline_seconds);
    doParsimUnpacking(b,this->qos_value);
    doParsimUnpacking(b,this->task_input_data);
}

const char * TaskOffloadPacket::getTask_id() const
{
    return this->task_id.c_str();
}

void TaskOffloadPacket::setTask_id(const char * task_id)
{
    this->task_id = task_id;
}

const char * TaskOffloadPacket::getOrigin_vehicle_id() const
{
    return this->origin_vehicle_id.c_str();
}

void TaskOffloadPacket::setOrigin_vehicle_id(const char * origin_vehicle_id)
{
    this->origin_vehicle_id = origin_vehicle_id;
}

uint64_t TaskOffloadPacket::getOrigin_vehicle_mac() const
{
    return this->origin_vehicle_mac;
}

void TaskOffloadPacket::setOrigin_vehicle_mac(uint64_t origin_vehicle_mac)
{
    this->origin_vehicle_mac = origin_vehicle_mac;
}

double TaskOffloadPacket::getOffload_time() const
{
    return this->offload_time;
}

void TaskOffloadPacket::setOffload_time(double offload_time)
{
    this->offload_time = offload_time;
}

uint64_t TaskOffloadPacket::getTask_size_bytes() const
{
    return this->task_size_bytes;
}

void TaskOffloadPacket::setTask_size_bytes(uint64_t task_size_bytes)
{
    this->task_size_bytes = task_size_bytes;
}

uint64_t TaskOffloadPacket::getCpu_cycles() const
{
    return this->cpu_cycles;
}

void TaskOffloadPacket::setCpu_cycles(uint64_t cpu_cycles)
{
    this->cpu_cycles = cpu_cycles;
}

double TaskOffloadPacket::getDeadline_seconds() const
{
    return this->deadline_seconds;
}

void TaskOffloadPacket::setDeadline_seconds(double deadline_seconds)
{
    this->deadline_seconds = deadline_seconds;
}

double TaskOffloadPacket::getQos_value() const
{
    return this->qos_value;
}

void TaskOffloadPacket::setQos_value(double qos_value)
{
    this->qos_value = qos_value;
}

const char * TaskOffloadPacket::getTask_input_data() const
{
    return this->task_input_data.c_str();
}

void TaskOffloadPacket::setTask_input_data(const char * task_input_data)
{
    this->task_input_data = task_input_data;
}

class TaskOffloadPacketDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_task_id,
        FIELD_origin_vehicle_id,
        FIELD_origin_vehicle_mac,
        FIELD_offload_time,
        FIELD_task_size_bytes,
        FIELD_cpu_cycles,
        FIELD_deadline_seconds,
        FIELD_qos_value,
        FIELD_task_input_data,
    };
  public:
    TaskOffloadPacketDescriptor();
    virtual ~TaskOffloadPacketDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(TaskOffloadPacketDescriptor)

TaskOffloadPacketDescriptor::TaskOffloadPacketDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(veins::TaskOffloadPacket)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

TaskOffloadPacketDescriptor::~TaskOffloadPacketDescriptor()
{
    delete[] propertyNames;
}

bool TaskOffloadPacketDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<TaskOffloadPacket *>(obj)!=nullptr;
}

const char **TaskOffloadPacketDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *TaskOffloadPacketDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int TaskOffloadPacketDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 9+base->getFieldCount() : 9;
}

unsigned int TaskOffloadPacketDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_task_id
        FD_ISEDITABLE,    // FIELD_origin_vehicle_id
        FD_ISEDITABLE,    // FIELD_origin_vehicle_mac
        FD_ISEDITABLE,    // FIELD_offload_time
        FD_ISEDITABLE,    // FIELD_task_size_bytes
        FD_ISEDITABLE,    // FIELD_cpu_cycles
        FD_ISEDITABLE,    // FIELD_deadline_seconds
        FD_ISEDITABLE,    // FIELD_qos_value
        FD_ISEDITABLE,    // FIELD_task_input_data
    };
    return (field >= 0 && field < 9) ? fieldTypeFlags[field] : 0;
}

const char *TaskOffloadPacketDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "task_id",
        "origin_vehicle_id",
        "origin_vehicle_mac",
        "offload_time",
        "task_size_bytes",
        "cpu_cycles",
        "deadline_seconds",
        "qos_value",
        "task_input_data",
    };
    return (field >= 0 && field < 9) ? fieldNames[field] : nullptr;
}

int TaskOffloadPacketDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "task_id") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "origin_vehicle_id") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "origin_vehicle_mac") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "offload_time") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "task_size_bytes") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "cpu_cycles") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "deadline_seconds") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "qos_value") == 0) return baseIndex + 7;
    if (strcmp(fieldName, "task_input_data") == 0) return baseIndex + 8;
    return base ? base->findField(fieldName) : -1;
}

const char *TaskOffloadPacketDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "string",    // FIELD_task_id
        "string",    // FIELD_origin_vehicle_id
        "uint64_t",    // FIELD_origin_vehicle_mac
        "double",    // FIELD_offload_time
        "uint64_t",    // FIELD_task_size_bytes
        "uint64_t",    // FIELD_cpu_cycles
        "double",    // FIELD_deadline_seconds
        "double",    // FIELD_qos_value
        "string",    // FIELD_task_input_data
    };
    return (field >= 0 && field < 9) ? fieldTypeStrings[field] : nullptr;
}

const char **TaskOffloadPacketDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *TaskOffloadPacketDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int TaskOffloadPacketDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    TaskOffloadPacket *pp = omnetpp::fromAnyPtr<TaskOffloadPacket>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void TaskOffloadPacketDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskOffloadPacket *pp = omnetpp::fromAnyPtr<TaskOffloadPacket>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'TaskOffloadPacket'", field);
    }
}

const char *TaskOffloadPacketDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskOffloadPacket *pp = omnetpp::fromAnyPtr<TaskOffloadPacket>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string TaskOffloadPacketDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskOffloadPacket *pp = omnetpp::fromAnyPtr<TaskOffloadPacket>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return oppstring2string(pp->getTask_id());
        case FIELD_origin_vehicle_id: return oppstring2string(pp->getOrigin_vehicle_id());
        case FIELD_origin_vehicle_mac: return uint642string(pp->getOrigin_vehicle_mac());
        case FIELD_offload_time: return double2string(pp->getOffload_time());
        case FIELD_task_size_bytes: return uint642string(pp->getTask_size_bytes());
        case FIELD_cpu_cycles: return uint642string(pp->getCpu_cycles());
        case FIELD_deadline_seconds: return double2string(pp->getDeadline_seconds());
        case FIELD_qos_value: return double2string(pp->getQos_value());
        case FIELD_task_input_data: return oppstring2string(pp->getTask_input_data());
        default: return "";
    }
}

void TaskOffloadPacketDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskOffloadPacket *pp = omnetpp::fromAnyPtr<TaskOffloadPacket>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id((value)); break;
        case FIELD_origin_vehicle_id: pp->setOrigin_vehicle_id((value)); break;
        case FIELD_origin_vehicle_mac: pp->setOrigin_vehicle_mac(string2uint64(value)); break;
        case FIELD_offload_time: pp->setOffload_time(string2double(value)); break;
        case FIELD_task_size_bytes: pp->setTask_size_bytes(string2uint64(value)); break;
        case FIELD_cpu_cycles: pp->setCpu_cycles(string2uint64(value)); break;
        case FIELD_deadline_seconds: pp->setDeadline_seconds(string2double(value)); break;
        case FIELD_qos_value: pp->setQos_value(string2double(value)); break;
        case FIELD_task_input_data: pp->setTask_input_data((value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskOffloadPacket'", field);
    }
}

omnetpp::cValue TaskOffloadPacketDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    TaskOffloadPacket *pp = omnetpp::fromAnyPtr<TaskOffloadPacket>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return pp->getTask_id();
        case FIELD_origin_vehicle_id: return pp->getOrigin_vehicle_id();
        case FIELD_origin_vehicle_mac: return (omnetpp::intval_t)(pp->getOrigin_vehicle_mac());
        case FIELD_offload_time: return pp->getOffload_time();
        case FIELD_task_size_bytes: return (omnetpp::intval_t)(pp->getTask_size_bytes());
        case FIELD_cpu_cycles: return (omnetpp::intval_t)(pp->getCpu_cycles());
        case FIELD_deadline_seconds: return pp->getDeadline_seconds();
        case FIELD_qos_value: return pp->getQos_value();
        case FIELD_task_input_data: return pp->getTask_input_data();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'TaskOffloadPacket' as cValue -- field index out of range?", field);
    }
}

void TaskOffloadPacketDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskOffloadPacket *pp = omnetpp::fromAnyPtr<TaskOffloadPacket>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id(value.stringValue()); break;
        case FIELD_origin_vehicle_id: pp->setOrigin_vehicle_id(value.stringValue()); break;
        case FIELD_origin_vehicle_mac: pp->setOrigin_vehicle_mac(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_offload_time: pp->setOffload_time(value.doubleValue()); break;
        case FIELD_task_size_bytes: pp->setTask_size_bytes(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_cpu_cycles: pp->setCpu_cycles(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_deadline_seconds: pp->setDeadline_seconds(value.doubleValue()); break;
        case FIELD_qos_value: pp->setQos_value(value.doubleValue()); break;
        case FIELD_task_input_data: pp->setTask_input_data(value.stringValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskOffloadPacket'", field);
    }
}

const char *TaskOffloadPacketDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr TaskOffloadPacketDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    TaskOffloadPacket *pp = omnetpp::fromAnyPtr<TaskOffloadPacket>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void TaskOffloadPacketDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskOffloadPacket *pp = omnetpp::fromAnyPtr<TaskOffloadPacket>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskOffloadPacket'", field);
    }
}

Register_Class(TaskResultMessage)

TaskResultMessage::TaskResultMessage(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

TaskResultMessage::TaskResultMessage(const TaskResultMessage& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

TaskResultMessage::~TaskResultMessage()
{
}

TaskResultMessage& TaskResultMessage::operator=(const TaskResultMessage& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void TaskResultMessage::copy(const TaskResultMessage& other)
{
    this->task_id = other.task_id;
    this->origin_vehicle_id = other.origin_vehicle_id;
    this->processor_id = other.processor_id;
    this->completion_time = other.completion_time;
    this->processing_time = other.processing_time;
    this->success = other.success;
    this->task_output_data = other.task_output_data;
    this->failure_reason = other.failure_reason;
}

void TaskResultMessage::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->task_id);
    doParsimPacking(b,this->origin_vehicle_id);
    doParsimPacking(b,this->processor_id);
    doParsimPacking(b,this->completion_time);
    doParsimPacking(b,this->processing_time);
    doParsimPacking(b,this->success);
    doParsimPacking(b,this->task_output_data);
    doParsimPacking(b,this->failure_reason);
}

void TaskResultMessage::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->task_id);
    doParsimUnpacking(b,this->origin_vehicle_id);
    doParsimUnpacking(b,this->processor_id);
    doParsimUnpacking(b,this->completion_time);
    doParsimUnpacking(b,this->processing_time);
    doParsimUnpacking(b,this->success);
    doParsimUnpacking(b,this->task_output_data);
    doParsimUnpacking(b,this->failure_reason);
}

const char * TaskResultMessage::getTask_id() const
{
    return this->task_id.c_str();
}

void TaskResultMessage::setTask_id(const char * task_id)
{
    this->task_id = task_id;
}

const char * TaskResultMessage::getOrigin_vehicle_id() const
{
    return this->origin_vehicle_id.c_str();
}

void TaskResultMessage::setOrigin_vehicle_id(const char * origin_vehicle_id)
{
    this->origin_vehicle_id = origin_vehicle_id;
}

const char * TaskResultMessage::getProcessor_id() const
{
    return this->processor_id.c_str();
}

void TaskResultMessage::setProcessor_id(const char * processor_id)
{
    this->processor_id = processor_id;
}

double TaskResultMessage::getCompletion_time() const
{
    return this->completion_time;
}

void TaskResultMessage::setCompletion_time(double completion_time)
{
    this->completion_time = completion_time;
}

double TaskResultMessage::getProcessing_time() const
{
    return this->processing_time;
}

void TaskResultMessage::setProcessing_time(double processing_time)
{
    this->processing_time = processing_time;
}

bool TaskResultMessage::getSuccess() const
{
    return this->success;
}

void TaskResultMessage::setSuccess(bool success)
{
    this->success = success;
}

const char * TaskResultMessage::getTask_output_data() const
{
    return this->task_output_data.c_str();
}

void TaskResultMessage::setTask_output_data(const char * task_output_data)
{
    this->task_output_data = task_output_data;
}

const char * TaskResultMessage::getFailure_reason() const
{
    return this->failure_reason.c_str();
}

void TaskResultMessage::setFailure_reason(const char * failure_reason)
{
    this->failure_reason = failure_reason;
}

class TaskResultMessageDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_task_id,
        FIELD_origin_vehicle_id,
        FIELD_processor_id,
        FIELD_completion_time,
        FIELD_processing_time,
        FIELD_success,
        FIELD_task_output_data,
        FIELD_failure_reason,
    };
  public:
    TaskResultMessageDescriptor();
    virtual ~TaskResultMessageDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(TaskResultMessageDescriptor)

TaskResultMessageDescriptor::TaskResultMessageDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(veins::TaskResultMessage)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

TaskResultMessageDescriptor::~TaskResultMessageDescriptor()
{
    delete[] propertyNames;
}

bool TaskResultMessageDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<TaskResultMessage *>(obj)!=nullptr;
}

const char **TaskResultMessageDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *TaskResultMessageDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int TaskResultMessageDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 8+base->getFieldCount() : 8;
}

unsigned int TaskResultMessageDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_task_id
        FD_ISEDITABLE,    // FIELD_origin_vehicle_id
        FD_ISEDITABLE,    // FIELD_processor_id
        FD_ISEDITABLE,    // FIELD_completion_time
        FD_ISEDITABLE,    // FIELD_processing_time
        FD_ISEDITABLE,    // FIELD_success
        FD_ISEDITABLE,    // FIELD_task_output_data
        FD_ISEDITABLE,    // FIELD_failure_reason
    };
    return (field >= 0 && field < 8) ? fieldTypeFlags[field] : 0;
}

const char *TaskResultMessageDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "task_id",
        "origin_vehicle_id",
        "processor_id",
        "completion_time",
        "processing_time",
        "success",
        "task_output_data",
        "failure_reason",
    };
    return (field >= 0 && field < 8) ? fieldNames[field] : nullptr;
}

int TaskResultMessageDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "task_id") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "origin_vehicle_id") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "processor_id") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "completion_time") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "processing_time") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "success") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "task_output_data") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "failure_reason") == 0) return baseIndex + 7;
    return base ? base->findField(fieldName) : -1;
}

const char *TaskResultMessageDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "string",    // FIELD_task_id
        "string",    // FIELD_origin_vehicle_id
        "string",    // FIELD_processor_id
        "double",    // FIELD_completion_time
        "double",    // FIELD_processing_time
        "bool",    // FIELD_success
        "string",    // FIELD_task_output_data
        "string",    // FIELD_failure_reason
    };
    return (field >= 0 && field < 8) ? fieldTypeStrings[field] : nullptr;
}

const char **TaskResultMessageDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *TaskResultMessageDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int TaskResultMessageDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    TaskResultMessage *pp = omnetpp::fromAnyPtr<TaskResultMessage>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void TaskResultMessageDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskResultMessage *pp = omnetpp::fromAnyPtr<TaskResultMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'TaskResultMessage'", field);
    }
}

const char *TaskResultMessageDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskResultMessage *pp = omnetpp::fromAnyPtr<TaskResultMessage>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string TaskResultMessageDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskResultMessage *pp = omnetpp::fromAnyPtr<TaskResultMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return oppstring2string(pp->getTask_id());
        case FIELD_origin_vehicle_id: return oppstring2string(pp->getOrigin_vehicle_id());
        case FIELD_processor_id: return oppstring2string(pp->getProcessor_id());
        case FIELD_completion_time: return double2string(pp->getCompletion_time());
        case FIELD_processing_time: return double2string(pp->getProcessing_time());
        case FIELD_success: return bool2string(pp->getSuccess());
        case FIELD_task_output_data: return oppstring2string(pp->getTask_output_data());
        case FIELD_failure_reason: return oppstring2string(pp->getFailure_reason());
        default: return "";
    }
}

void TaskResultMessageDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskResultMessage *pp = omnetpp::fromAnyPtr<TaskResultMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id((value)); break;
        case FIELD_origin_vehicle_id: pp->setOrigin_vehicle_id((value)); break;
        case FIELD_processor_id: pp->setProcessor_id((value)); break;
        case FIELD_completion_time: pp->setCompletion_time(string2double(value)); break;
        case FIELD_processing_time: pp->setProcessing_time(string2double(value)); break;
        case FIELD_success: pp->setSuccess(string2bool(value)); break;
        case FIELD_task_output_data: pp->setTask_output_data((value)); break;
        case FIELD_failure_reason: pp->setFailure_reason((value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskResultMessage'", field);
    }
}

omnetpp::cValue TaskResultMessageDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    TaskResultMessage *pp = omnetpp::fromAnyPtr<TaskResultMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return pp->getTask_id();
        case FIELD_origin_vehicle_id: return pp->getOrigin_vehicle_id();
        case FIELD_processor_id: return pp->getProcessor_id();
        case FIELD_completion_time: return pp->getCompletion_time();
        case FIELD_processing_time: return pp->getProcessing_time();
        case FIELD_success: return pp->getSuccess();
        case FIELD_task_output_data: return pp->getTask_output_data();
        case FIELD_failure_reason: return pp->getFailure_reason();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'TaskResultMessage' as cValue -- field index out of range?", field);
    }
}

void TaskResultMessageDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskResultMessage *pp = omnetpp::fromAnyPtr<TaskResultMessage>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id(value.stringValue()); break;
        case FIELD_origin_vehicle_id: pp->setOrigin_vehicle_id(value.stringValue()); break;
        case FIELD_processor_id: pp->setProcessor_id(value.stringValue()); break;
        case FIELD_completion_time: pp->setCompletion_time(value.doubleValue()); break;
        case FIELD_processing_time: pp->setProcessing_time(value.doubleValue()); break;
        case FIELD_success: pp->setSuccess(value.boolValue()); break;
        case FIELD_task_output_data: pp->setTask_output_data(value.stringValue()); break;
        case FIELD_failure_reason: pp->setFailure_reason(value.stringValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskResultMessage'", field);
    }
}

const char *TaskResultMessageDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr TaskResultMessageDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    TaskResultMessage *pp = omnetpp::fromAnyPtr<TaskResultMessage>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void TaskResultMessageDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskResultMessage *pp = omnetpp::fromAnyPtr<TaskResultMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskResultMessage'", field);
    }
}

Register_Class(TaskOffloadingEvent)

TaskOffloadingEvent::TaskOffloadingEvent(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

TaskOffloadingEvent::TaskOffloadingEvent(const TaskOffloadingEvent& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

TaskOffloadingEvent::~TaskOffloadingEvent()
{
}

TaskOffloadingEvent& TaskOffloadingEvent::operator=(const TaskOffloadingEvent& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void TaskOffloadingEvent::copy(const TaskOffloadingEvent& other)
{
    this->task_id = other.task_id;
    this->event_type = other.event_type;
    this->source_entity_id = other.source_entity_id;
    this->target_entity_id = other.target_entity_id;
    this->event_time = other.event_time;
    this->event_details = other.event_details;
}

void TaskOffloadingEvent::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->task_id);
    doParsimPacking(b,this->event_type);
    doParsimPacking(b,this->source_entity_id);
    doParsimPacking(b,this->target_entity_id);
    doParsimPacking(b,this->event_time);
    doParsimPacking(b,this->event_details);
}

void TaskOffloadingEvent::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->task_id);
    doParsimUnpacking(b,this->event_type);
    doParsimUnpacking(b,this->source_entity_id);
    doParsimUnpacking(b,this->target_entity_id);
    doParsimUnpacking(b,this->event_time);
    doParsimUnpacking(b,this->event_details);
}

const char * TaskOffloadingEvent::getTask_id() const
{
    return this->task_id.c_str();
}

void TaskOffloadingEvent::setTask_id(const char * task_id)
{
    this->task_id = task_id;
}

const char * TaskOffloadingEvent::getEvent_type() const
{
    return this->event_type.c_str();
}

void TaskOffloadingEvent::setEvent_type(const char * event_type)
{
    this->event_type = event_type;
}

const char * TaskOffloadingEvent::getSource_entity_id() const
{
    return this->source_entity_id.c_str();
}

void TaskOffloadingEvent::setSource_entity_id(const char * source_entity_id)
{
    this->source_entity_id = source_entity_id;
}

const char * TaskOffloadingEvent::getTarget_entity_id() const
{
    return this->target_entity_id.c_str();
}

void TaskOffloadingEvent::setTarget_entity_id(const char * target_entity_id)
{
    this->target_entity_id = target_entity_id;
}

double TaskOffloadingEvent::getEvent_time() const
{
    return this->event_time;
}

void TaskOffloadingEvent::setEvent_time(double event_time)
{
    this->event_time = event_time;
}

const char * TaskOffloadingEvent::getEvent_details() const
{
    return this->event_details.c_str();
}

void TaskOffloadingEvent::setEvent_details(const char * event_details)
{
    this->event_details = event_details;
}

class TaskOffloadingEventDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_task_id,
        FIELD_event_type,
        FIELD_source_entity_id,
        FIELD_target_entity_id,
        FIELD_event_time,
        FIELD_event_details,
    };
  public:
    TaskOffloadingEventDescriptor();
    virtual ~TaskOffloadingEventDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(TaskOffloadingEventDescriptor)

TaskOffloadingEventDescriptor::TaskOffloadingEventDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(veins::TaskOffloadingEvent)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

TaskOffloadingEventDescriptor::~TaskOffloadingEventDescriptor()
{
    delete[] propertyNames;
}

bool TaskOffloadingEventDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<TaskOffloadingEvent *>(obj)!=nullptr;
}

const char **TaskOffloadingEventDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *TaskOffloadingEventDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int TaskOffloadingEventDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 6+base->getFieldCount() : 6;
}

unsigned int TaskOffloadingEventDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_task_id
        FD_ISEDITABLE,    // FIELD_event_type
        FD_ISEDITABLE,    // FIELD_source_entity_id
        FD_ISEDITABLE,    // FIELD_target_entity_id
        FD_ISEDITABLE,    // FIELD_event_time
        FD_ISEDITABLE,    // FIELD_event_details
    };
    return (field >= 0 && field < 6) ? fieldTypeFlags[field] : 0;
}

const char *TaskOffloadingEventDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "task_id",
        "event_type",
        "source_entity_id",
        "target_entity_id",
        "event_time",
        "event_details",
    };
    return (field >= 0 && field < 6) ? fieldNames[field] : nullptr;
}

int TaskOffloadingEventDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "task_id") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "event_type") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "source_entity_id") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "target_entity_id") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "event_time") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "event_details") == 0) return baseIndex + 5;
    return base ? base->findField(fieldName) : -1;
}

const char *TaskOffloadingEventDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "string",    // FIELD_task_id
        "string",    // FIELD_event_type
        "string",    // FIELD_source_entity_id
        "string",    // FIELD_target_entity_id
        "double",    // FIELD_event_time
        "string",    // FIELD_event_details
    };
    return (field >= 0 && field < 6) ? fieldTypeStrings[field] : nullptr;
}

const char **TaskOffloadingEventDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *TaskOffloadingEventDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int TaskOffloadingEventDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    TaskOffloadingEvent *pp = omnetpp::fromAnyPtr<TaskOffloadingEvent>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void TaskOffloadingEventDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskOffloadingEvent *pp = omnetpp::fromAnyPtr<TaskOffloadingEvent>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'TaskOffloadingEvent'", field);
    }
}

const char *TaskOffloadingEventDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskOffloadingEvent *pp = omnetpp::fromAnyPtr<TaskOffloadingEvent>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string TaskOffloadingEventDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    TaskOffloadingEvent *pp = omnetpp::fromAnyPtr<TaskOffloadingEvent>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return oppstring2string(pp->getTask_id());
        case FIELD_event_type: return oppstring2string(pp->getEvent_type());
        case FIELD_source_entity_id: return oppstring2string(pp->getSource_entity_id());
        case FIELD_target_entity_id: return oppstring2string(pp->getTarget_entity_id());
        case FIELD_event_time: return double2string(pp->getEvent_time());
        case FIELD_event_details: return oppstring2string(pp->getEvent_details());
        default: return "";
    }
}

void TaskOffloadingEventDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskOffloadingEvent *pp = omnetpp::fromAnyPtr<TaskOffloadingEvent>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id((value)); break;
        case FIELD_event_type: pp->setEvent_type((value)); break;
        case FIELD_source_entity_id: pp->setSource_entity_id((value)); break;
        case FIELD_target_entity_id: pp->setTarget_entity_id((value)); break;
        case FIELD_event_time: pp->setEvent_time(string2double(value)); break;
        case FIELD_event_details: pp->setEvent_details((value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskOffloadingEvent'", field);
    }
}

omnetpp::cValue TaskOffloadingEventDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    TaskOffloadingEvent *pp = omnetpp::fromAnyPtr<TaskOffloadingEvent>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: return pp->getTask_id();
        case FIELD_event_type: return pp->getEvent_type();
        case FIELD_source_entity_id: return pp->getSource_entity_id();
        case FIELD_target_entity_id: return pp->getTarget_entity_id();
        case FIELD_event_time: return pp->getEvent_time();
        case FIELD_event_details: return pp->getEvent_details();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'TaskOffloadingEvent' as cValue -- field index out of range?", field);
    }
}

void TaskOffloadingEventDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskOffloadingEvent *pp = omnetpp::fromAnyPtr<TaskOffloadingEvent>(object); (void)pp;
    switch (field) {
        case FIELD_task_id: pp->setTask_id(value.stringValue()); break;
        case FIELD_event_type: pp->setEvent_type(value.stringValue()); break;
        case FIELD_source_entity_id: pp->setSource_entity_id(value.stringValue()); break;
        case FIELD_target_entity_id: pp->setTarget_entity_id(value.stringValue()); break;
        case FIELD_event_time: pp->setEvent_time(value.doubleValue()); break;
        case FIELD_event_details: pp->setEvent_details(value.stringValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskOffloadingEvent'", field);
    }
}

const char *TaskOffloadingEventDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr TaskOffloadingEventDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    TaskOffloadingEvent *pp = omnetpp::fromAnyPtr<TaskOffloadingEvent>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void TaskOffloadingEventDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    TaskOffloadingEvent *pp = omnetpp::fromAnyPtr<TaskOffloadingEvent>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'TaskOffloadingEvent'", field);
    }
}

}  // namespace veins

namespace omnetpp {

}  // namespace omnetpp

