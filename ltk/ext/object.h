#ifndef EXT_OBJECT_H
#define EXT_OBJECT_H

#include <atomic>
#include <common.h>
#include <interface/intf_any.h>
#include <interface/intf_object.h>
#include <interface/intf_object_factory.h>
#include <interface/intf_registry.h>

template<class... Interfaces>
class BaseObject : public IObject, public Interfaces...
{
private:
    template<class T>
    constexpr void FindInterface(Uid uid, void **interface)
    {
        if (uid == T::UID) {
            *interface = static_cast<T *>(this);
        }
        // Note that we don't support interface hierarchies here..
    }

    template<class... B, class = std::enable_if_t<sizeof...(B) == 0>>
    constexpr void FindSiblings(Uid, void **)
    {}
    template<class A, class... B>
    constexpr void FindSiblings(Uid uid, void **interface)
    {
        FindInterface<A>(uid, interface);
        if (*interface == nullptr) {
            FindSiblings<B...>(uid, interface);
        }
    }

public:
    IInterface *GetInterface(Uid uid) override
    {
        void *interface = nullptr;
        if (uid == IInterface::UID) {
            interface = static_cast<IObject *>(this);
        } else if (uid == IObject::UID) {
            interface = static_cast<IObject *>(this);
        } else {
            FindSiblings<Interfaces...>(uid, &interface);
        }
        return static_cast<IInterface *>(interface);
    }
    const IInterface *GetInterface(Uid uid) const override
    {
        return const_cast<BaseObject *>(this)->GetInterface(uid);
    }

    template<class T>
    T *GetInterface()
    {
        return static_cast<T *>(GetInterface(T::UID));
    }

    void Ref() override { data_.refCount++; }
    void UnRef() override
    {
        if (--data_.refCount == 0) {
            delete this;
        }
    }

public:
    BaseObject() = default;
    ~BaseObject() override = default;

protected:
    struct ObjectData
    {
        alignas(4) std::atomic<int32_t> refCount{1};
        alignas(4) int32_t flags{};
    };

private:
    ObjectData data_;
};

template<class FinalClass>
class ObjectFactory : public IObjectFactory
{
public:
    ObjectFactory() = default;
    ~ObjectFactory() override = default;

    IInterface *GetInterface(Uid uid) override
    {
        void *interface = nullptr;
        if (uid == IInterface::UID) {
            interface = static_cast<IObjectFactory *>(this);
        } else if (uid == IObjectFactory::UID) {
            interface = static_cast<IObjectFactory *>(this);
        }
        return static_cast<IInterface *>(interface);
    }
    const IInterface *GetInterface(Uid uid) const override
    {
        return const_cast<ObjectFactory *>(this)->GetInterface(uid);
    }

    void Ref() override{};
    virtual void UnRef() override{};

public:
    IObject::Ptr CreateInstance() const override
    {
        // Custom deleter for factory-created shared_ptrs which just decrease refcount
        return std::shared_ptr<FinalClass>(new FinalClass, [](FinalClass *p) { p->UnRef(); });
    }
};

// Base class for objects which implement interfaces
template<class FinalClass, class... Interfaces>
class Object : public BaseObject<Interfaces..., ISharedFromObject>
{
public:
    Object() = default;
    ~Object() override = default;

public:
    static constexpr std::string_view GetClassName() noexcept { return GetName<FinalClass>(); }
    static constexpr Uid GetClassUid() noexcept { return TypeUid<FinalClass>(); }

    void SetSelf(const IObject::Ptr &self) override { self_ = self; }
    IObject::Ptr GetSelf() const override { return self_.lock(); }

    template<class T>
    typename T::Ptr GetSelf() const
    {
        return interface_pointer_cast<T>(self_.lock());
    }

public:
    static const IObjectFactory &GetFactory()
    {
        static Factory factory_;
        return factory_;
    }

private:
    IObject::WeakPtr self_{};

    class Factory : public ObjectFactory<FinalClass>
    {
        const ClassInfo &GetClassInfo() const override
        {
            static constexpr ClassInfo info{FinalClass::GetClassUid(), FinalClass::GetClassName()};
            return info;
        }
    };
};

#endif // EXT_OBJECT_H
