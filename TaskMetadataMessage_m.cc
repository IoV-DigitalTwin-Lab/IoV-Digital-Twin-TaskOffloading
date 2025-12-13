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
    return base ? 8+base->getFieldCount() : 8;
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
    };
    return (field >= 0 && field < 8) ? fieldTypeFlags[field] : 0;
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
    };
    return (field >= 0 && field < 8) ? fieldNames[field] : nullptr;
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
    };
    return (field >= 0 && field < 8) ? fieldTypeStrings[field] : nullptr;
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
    return base ? 20+base->getFieldCount() : 20;
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
    return (field >= 0 && field < 20) ? fieldTypeFlags[field] : 0;
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
    return (field >= 0 && field < 20) ? fieldNames[field] : nullptr;
}

int VehicleResourceStatusMessageDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "vehicle_id") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "pos_x") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "pos_y") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "speed") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "cpu_total") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "cpu_allocable") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "cpu_available") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "cpu_utilization") == 0) return baseIndex + 7;
    if (strcmp(fieldName, "mem_total") == 0) return baseIndex + 8;
    if (strcmp(fieldName, "mem_available") == 0) return baseIndex + 9;
    if (strcmp(fieldName, "mem_utilization") == 0) return baseIndex + 10;
    if (strcmp(fieldName, "tasks_generated") == 0) return baseIndex + 11;
    if (strcmp(fieldName, "tasks_completed_on_time") == 0) return baseIndex + 12;
    if (strcmp(fieldName, "tasks_completed_late") == 0) return baseIndex + 13;
    if (strcmp(fieldName, "tasks_failed") == 0) return baseIndex + 14;
    if (strcmp(fieldName, "tasks_rejected") == 0) return baseIndex + 15;
    if (strcmp(fieldName, "current_queue_length") == 0) return baseIndex + 16;
    if (strcmp(fieldName, "current_processing_count") == 0) return baseIndex + 17;
    if (strcmp(fieldName, "avg_completion_time") == 0) return baseIndex + 18;
    if (strcmp(fieldName, "deadline_miss_ratio") == 0) return baseIndex + 19;
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
    return (field >= 0 && field < 20) ? fieldTypeStrings[field] : nullptr;
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

}  // namespace veins

namespace omnetpp {

}  // namespace omnetpp

