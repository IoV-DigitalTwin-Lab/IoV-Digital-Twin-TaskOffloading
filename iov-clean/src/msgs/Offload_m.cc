//
// Generated file, do not edit! Created by opp_msgtool 6.0 from src/msgs/Offload.msg.
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
#include "Offload_m.h"

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

namespace iovmini {

Register_Class(OffloadRequest)

OffloadRequest::OffloadRequest(const char *name, short kind) : ::omnetpp::cPacket(name, kind)
{
}

OffloadRequest::OffloadRequest(const OffloadRequest& other) : ::omnetpp::cPacket(other)
{
    copy(other);
}

OffloadRequest::~OffloadRequest()
{
}

OffloadRequest& OffloadRequest::operator=(const OffloadRequest& other)
{
    if (this == &other) return *this;
    ::omnetpp::cPacket::operator=(other);
    copy(other);
    return *this;
}

void OffloadRequest::copy(const OffloadRequest& other)
{
    this->srcModuleId = other.srcModuleId;
    this->cycles = other.cycles;
    this->bytes = other.bytes;
    this->rsuIndex = other.rsuIndex;
    this->created = other.created;
}

void OffloadRequest::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::omnetpp::cPacket::parsimPack(b);
    doParsimPacking(b,this->srcModuleId);
    doParsimPacking(b,this->cycles);
    doParsimPacking(b,this->bytes);
    doParsimPacking(b,this->rsuIndex);
    doParsimPacking(b,this->created);
}

void OffloadRequest::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::omnetpp::cPacket::parsimUnpack(b);
    doParsimUnpacking(b,this->srcModuleId);
    doParsimUnpacking(b,this->cycles);
    doParsimUnpacking(b,this->bytes);
    doParsimUnpacking(b,this->rsuIndex);
    doParsimUnpacking(b,this->created);
}

int OffloadRequest::getSrcModuleId() const
{
    return this->srcModuleId;
}

void OffloadRequest::setSrcModuleId(int srcModuleId)
{
    this->srcModuleId = srcModuleId;
}

double OffloadRequest::getCycles() const
{
    return this->cycles;
}

void OffloadRequest::setCycles(double cycles)
{
    this->cycles = cycles;
}

int OffloadRequest::getBytes() const
{
    return this->bytes;
}

void OffloadRequest::setBytes(int bytes)
{
    this->bytes = bytes;
}

int OffloadRequest::getRsuIndex() const
{
    return this->rsuIndex;
}

void OffloadRequest::setRsuIndex(int rsuIndex)
{
    this->rsuIndex = rsuIndex;
}

::omnetpp::simtime_t OffloadRequest::getCreated() const
{
    return this->created;
}

void OffloadRequest::setCreated(::omnetpp::simtime_t created)
{
    this->created = created;
}

class OffloadRequestDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_srcModuleId,
        FIELD_cycles,
        FIELD_bytes,
        FIELD_rsuIndex,
        FIELD_created,
    };
  public:
    OffloadRequestDescriptor();
    virtual ~OffloadRequestDescriptor();

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

Register_ClassDescriptor(OffloadRequestDescriptor)

OffloadRequestDescriptor::OffloadRequestDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(iovmini::OffloadRequest)), "omnetpp::cPacket")
{
    propertyNames = nullptr;
}

OffloadRequestDescriptor::~OffloadRequestDescriptor()
{
    delete[] propertyNames;
}

bool OffloadRequestDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<OffloadRequest *>(obj)!=nullptr;
}

const char **OffloadRequestDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *OffloadRequestDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int OffloadRequestDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 5+base->getFieldCount() : 5;
}

unsigned int OffloadRequestDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_srcModuleId
        FD_ISEDITABLE,    // FIELD_cycles
        FD_ISEDITABLE,    // FIELD_bytes
        FD_ISEDITABLE,    // FIELD_rsuIndex
        FD_ISEDITABLE,    // FIELD_created
    };
    return (field >= 0 && field < 5) ? fieldTypeFlags[field] : 0;
}

const char *OffloadRequestDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "srcModuleId",
        "cycles",
        "bytes",
        "rsuIndex",
        "created",
    };
    return (field >= 0 && field < 5) ? fieldNames[field] : nullptr;
}

int OffloadRequestDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "srcModuleId") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "cycles") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "bytes") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "rsuIndex") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "created") == 0) return baseIndex + 4;
    return base ? base->findField(fieldName) : -1;
}

const char *OffloadRequestDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "int",    // FIELD_srcModuleId
        "double",    // FIELD_cycles
        "int",    // FIELD_bytes
        "int",    // FIELD_rsuIndex
        "omnetpp::simtime_t",    // FIELD_created
    };
    return (field >= 0 && field < 5) ? fieldTypeStrings[field] : nullptr;
}

const char **OffloadRequestDescriptor::getFieldPropertyNames(int field) const
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

const char *OffloadRequestDescriptor::getFieldProperty(int field, const char *propertyName) const
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

int OffloadRequestDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    OffloadRequest *pp = omnetpp::fromAnyPtr<OffloadRequest>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void OffloadRequestDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadRequest *pp = omnetpp::fromAnyPtr<OffloadRequest>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'OffloadRequest'", field);
    }
}

const char *OffloadRequestDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadRequest *pp = omnetpp::fromAnyPtr<OffloadRequest>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string OffloadRequestDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadRequest *pp = omnetpp::fromAnyPtr<OffloadRequest>(object); (void)pp;
    switch (field) {
        case FIELD_srcModuleId: return long2string(pp->getSrcModuleId());
        case FIELD_cycles: return double2string(pp->getCycles());
        case FIELD_bytes: return long2string(pp->getBytes());
        case FIELD_rsuIndex: return long2string(pp->getRsuIndex());
        case FIELD_created: return simtime2string(pp->getCreated());
        default: return "";
    }
}

void OffloadRequestDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadRequest *pp = omnetpp::fromAnyPtr<OffloadRequest>(object); (void)pp;
    switch (field) {
        case FIELD_srcModuleId: pp->setSrcModuleId(string2long(value)); break;
        case FIELD_cycles: pp->setCycles(string2double(value)); break;
        case FIELD_bytes: pp->setBytes(string2long(value)); break;
        case FIELD_rsuIndex: pp->setRsuIndex(string2long(value)); break;
        case FIELD_created: pp->setCreated(string2simtime(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadRequest'", field);
    }
}

omnetpp::cValue OffloadRequestDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadRequest *pp = omnetpp::fromAnyPtr<OffloadRequest>(object); (void)pp;
    switch (field) {
        case FIELD_srcModuleId: return pp->getSrcModuleId();
        case FIELD_cycles: return pp->getCycles();
        case FIELD_bytes: return pp->getBytes();
        case FIELD_rsuIndex: return pp->getRsuIndex();
        case FIELD_created: return pp->getCreated().dbl();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'OffloadRequest' as cValue -- field index out of range?", field);
    }
}

void OffloadRequestDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadRequest *pp = omnetpp::fromAnyPtr<OffloadRequest>(object); (void)pp;
    switch (field) {
        case FIELD_srcModuleId: pp->setSrcModuleId(omnetpp::checked_int_cast<int>(value.intValue())); break;
        case FIELD_cycles: pp->setCycles(value.doubleValue()); break;
        case FIELD_bytes: pp->setBytes(omnetpp::checked_int_cast<int>(value.intValue())); break;
        case FIELD_rsuIndex: pp->setRsuIndex(omnetpp::checked_int_cast<int>(value.intValue())); break;
        case FIELD_created: pp->setCreated(value.doubleValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadRequest'", field);
    }
}

const char *OffloadRequestDescriptor::getFieldStructName(int field) const
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

omnetpp::any_ptr OffloadRequestDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    OffloadRequest *pp = omnetpp::fromAnyPtr<OffloadRequest>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void OffloadRequestDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadRequest *pp = omnetpp::fromAnyPtr<OffloadRequest>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadRequest'", field);
    }
}

Register_Class(OffloadResult)

OffloadResult::OffloadResult(const char *name, short kind) : ::omnetpp::cPacket(name, kind)
{
}

OffloadResult::OffloadResult(const OffloadResult& other) : ::omnetpp::cPacket(other)
{
    copy(other);
}

OffloadResult::~OffloadResult()
{
}

OffloadResult& OffloadResult::operator=(const OffloadResult& other)
{
    if (this == &other) return *this;
    ::omnetpp::cPacket::operator=(other);
    copy(other);
    return *this;
}

void OffloadResult::copy(const OffloadResult& other)
{
    this->srcModuleId = other.srcModuleId;
    this->rsuIndex = other.rsuIndex;
    this->created = other.created;
    this->finishedAt = other.finishedAt;
}

void OffloadResult::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::omnetpp::cPacket::parsimPack(b);
    doParsimPacking(b,this->srcModuleId);
    doParsimPacking(b,this->rsuIndex);
    doParsimPacking(b,this->created);
    doParsimPacking(b,this->finishedAt);
}

void OffloadResult::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::omnetpp::cPacket::parsimUnpack(b);
    doParsimUnpacking(b,this->srcModuleId);
    doParsimUnpacking(b,this->rsuIndex);
    doParsimUnpacking(b,this->created);
    doParsimUnpacking(b,this->finishedAt);
}

int OffloadResult::getSrcModuleId() const
{
    return this->srcModuleId;
}

void OffloadResult::setSrcModuleId(int srcModuleId)
{
    this->srcModuleId = srcModuleId;
}

int OffloadResult::getRsuIndex() const
{
    return this->rsuIndex;
}

void OffloadResult::setRsuIndex(int rsuIndex)
{
    this->rsuIndex = rsuIndex;
}

::omnetpp::simtime_t OffloadResult::getCreated() const
{
    return this->created;
}

void OffloadResult::setCreated(::omnetpp::simtime_t created)
{
    this->created = created;
}

::omnetpp::simtime_t OffloadResult::getFinishedAt() const
{
    return this->finishedAt;
}

void OffloadResult::setFinishedAt(::omnetpp::simtime_t finishedAt)
{
    this->finishedAt = finishedAt;
}

class OffloadResultDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_srcModuleId,
        FIELD_rsuIndex,
        FIELD_created,
        FIELD_finishedAt,
    };
  public:
    OffloadResultDescriptor();
    virtual ~OffloadResultDescriptor();

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

Register_ClassDescriptor(OffloadResultDescriptor)

OffloadResultDescriptor::OffloadResultDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(iovmini::OffloadResult)), "omnetpp::cPacket")
{
    propertyNames = nullptr;
}

OffloadResultDescriptor::~OffloadResultDescriptor()
{
    delete[] propertyNames;
}

bool OffloadResultDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<OffloadResult *>(obj)!=nullptr;
}

const char **OffloadResultDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *OffloadResultDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int OffloadResultDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 4+base->getFieldCount() : 4;
}

unsigned int OffloadResultDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_srcModuleId
        FD_ISEDITABLE,    // FIELD_rsuIndex
        FD_ISEDITABLE,    // FIELD_created
        FD_ISEDITABLE,    // FIELD_finishedAt
    };
    return (field >= 0 && field < 4) ? fieldTypeFlags[field] : 0;
}

const char *OffloadResultDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "srcModuleId",
        "rsuIndex",
        "created",
        "finishedAt",
    };
    return (field >= 0 && field < 4) ? fieldNames[field] : nullptr;
}

int OffloadResultDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "srcModuleId") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "rsuIndex") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "created") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "finishedAt") == 0) return baseIndex + 3;
    return base ? base->findField(fieldName) : -1;
}

const char *OffloadResultDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "int",    // FIELD_srcModuleId
        "int",    // FIELD_rsuIndex
        "omnetpp::simtime_t",    // FIELD_created
        "omnetpp::simtime_t",    // FIELD_finishedAt
    };
    return (field >= 0 && field < 4) ? fieldTypeStrings[field] : nullptr;
}

const char **OffloadResultDescriptor::getFieldPropertyNames(int field) const
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

const char *OffloadResultDescriptor::getFieldProperty(int field, const char *propertyName) const
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

int OffloadResultDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    OffloadResult *pp = omnetpp::fromAnyPtr<OffloadResult>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void OffloadResultDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadResult *pp = omnetpp::fromAnyPtr<OffloadResult>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'OffloadResult'", field);
    }
}

const char *OffloadResultDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadResult *pp = omnetpp::fromAnyPtr<OffloadResult>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string OffloadResultDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadResult *pp = omnetpp::fromAnyPtr<OffloadResult>(object); (void)pp;
    switch (field) {
        case FIELD_srcModuleId: return long2string(pp->getSrcModuleId());
        case FIELD_rsuIndex: return long2string(pp->getRsuIndex());
        case FIELD_created: return simtime2string(pp->getCreated());
        case FIELD_finishedAt: return simtime2string(pp->getFinishedAt());
        default: return "";
    }
}

void OffloadResultDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadResult *pp = omnetpp::fromAnyPtr<OffloadResult>(object); (void)pp;
    switch (field) {
        case FIELD_srcModuleId: pp->setSrcModuleId(string2long(value)); break;
        case FIELD_rsuIndex: pp->setRsuIndex(string2long(value)); break;
        case FIELD_created: pp->setCreated(string2simtime(value)); break;
        case FIELD_finishedAt: pp->setFinishedAt(string2simtime(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadResult'", field);
    }
}

omnetpp::cValue OffloadResultDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    OffloadResult *pp = omnetpp::fromAnyPtr<OffloadResult>(object); (void)pp;
    switch (field) {
        case FIELD_srcModuleId: return pp->getSrcModuleId();
        case FIELD_rsuIndex: return pp->getRsuIndex();
        case FIELD_created: return pp->getCreated().dbl();
        case FIELD_finishedAt: return pp->getFinishedAt().dbl();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'OffloadResult' as cValue -- field index out of range?", field);
    }
}

void OffloadResultDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadResult *pp = omnetpp::fromAnyPtr<OffloadResult>(object); (void)pp;
    switch (field) {
        case FIELD_srcModuleId: pp->setSrcModuleId(omnetpp::checked_int_cast<int>(value.intValue())); break;
        case FIELD_rsuIndex: pp->setRsuIndex(omnetpp::checked_int_cast<int>(value.intValue())); break;
        case FIELD_created: pp->setCreated(value.doubleValue()); break;
        case FIELD_finishedAt: pp->setFinishedAt(value.doubleValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadResult'", field);
    }
}

const char *OffloadResultDescriptor::getFieldStructName(int field) const
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

omnetpp::any_ptr OffloadResultDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    OffloadResult *pp = omnetpp::fromAnyPtr<OffloadResult>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void OffloadResultDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    OffloadResult *pp = omnetpp::fromAnyPtr<OffloadResult>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OffloadResult'", field);
    }
}

}  // namespace iovmini

namespace omnetpp {

}  // namespace omnetpp

