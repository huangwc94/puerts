/*
 * Tencent is pleased to support the open source community by making Puerts available.
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.  All rights reserved.
 * Puerts is licensed under the BSD 3-Clause License, except for the third-party components listed in the file 'LICENSE' which may
 * be subject to their corresponding license terms. This file is subject to the terms and conditions defined in file 'LICENSE',
 * which is part of this source code package.
 */

#pragma once

#include <sstream>
#include <tuple>
#include <type_traits>
#if BUILDING_PES_EXTENSION
#include "pesapi.h"
#else
#include "JSClassRegister.h"
#endif
#include "TypeInfo.hpp"

namespace puerts
{
template <typename T, typename = void>
struct ArgumentBufferType
{
    using type = typename std::decay<T>::type*;
    static constexpr bool is_custom = false;
};
}    // namespace puerts

#include "Converter.hpp"

#define MakeConstructor(T, ...) ::puerts::template ConstructorWrapper<T, ##__VA_ARGS__>
#define MakeGetter(M) &(::puerts::PropertyWrapper<decltype(M), M>::getter)
#define MakeSetter(M) &(::puerts::PropertyWrapper<decltype(M), M>::setter)
#define MakeProperty(M)                                                                                         \
    &(::puerts::PropertyWrapper<decltype(M), M>::getter), &(::puerts::PropertyWrapper<decltype(M), M>::setter), \
        ::puerts::PropertyWrapper<decltype(M), M>::info()
#define MakeReadonlyProperty(M) \
    &(::puerts::PropertyWrapper<decltype(M), M>::getter), nullptr, ::puerts::PropertyWrapper<decltype(M), M>::info()
#define MakeVariable(M) MakeProperty(M)
#define MakeReadonlyVariable(M) MakeReadonlyProperty(M)
#define MakeFunction(M) &(::puerts::FuncCallWrapper<decltype(M), M>::call), ::puerts::FuncCallWrapper<decltype(M), M>::info()
#define SelectFunction(SIGNATURE, M) \
    &(::puerts::FuncCallWrapper<SIGNATURE, M>::call), ::puerts::FuncCallWrapper<SIGNATURE, M>::info()
#define SelectFunction_PtrRet(SIGNATURE, M) \
    &(::puerts::FuncCallWrapper<SIGNATURE, M, true>::call), ::puerts::FuncCallWrapper<SIGNATURE, M, true>::info()
#define MakeCheckFunction(M) \
    &(::puerts::FuncCallWrapper<decltype(M), M>::checkedCall), ::puerts::FuncCallWrapper<decltype(M), M>::info()
#define MakeOverload(SIGNATURE, M) puerts::FuncCallWrapper<SIGNATURE, M>
#define CombineOverloads(...)                                                                          \
    &::puerts::OverloadsCombiner<__VA_ARGS__>::call, ::puerts::OverloadsCombiner<__VA_ARGS__>::length, \
        ::puerts::OverloadsCombiner<__VA_ARGS__>::infos()
#define CombineConstructors(...)                                                                             \
    &::puerts::ConstructorsCombiner<__VA_ARGS__>::call, ::puerts::ConstructorsCombiner<__VA_ARGS__>::length, \
        ::puerts::ConstructorsCombiner<__VA_ARGS__>::infos()

#define UsingCppType(CLS) __DefScriptTTypeName(CLS, CLS) __DefObjectType(CLS) __DefCDataPointerConverter(CLS)

namespace puerts
{
template <typename T>
struct ArgumentBufferType<T*, typename std::enable_if<is_script_type<T>::value && !std::is_const<T>::value>::type>
{
    using type = typename std::decay<T>::type;
    static constexpr bool is_custom = false;
};

template <typename T>
struct ArgumentBufferType<T,
    typename std::enable_if<std::is_lvalue_reference<T>::value && !std::is_const<typename std::remove_reference<T>::type>::value &&
                            std::is_constructible<typename std::decay<T>::type>::value &&
                            std::is_copy_constructible<typename std::decay<T>::type>::value &&
                            std::is_destructible<typename std::decay<T>::type>::value>::type>
{
    using type = typename std::decay<T>::type;
    static constexpr bool is_custom = false;
};

namespace internal
{
template <typename T, typename = void>
struct ConverterDecay
{
    using type = typename std::decay<T>::type;
};

template <typename T>
struct ConverterDecay<T, typename std::enable_if<std::is_lvalue_reference<T>::value &&
                                                 !std::is_const<typename std::remove_reference<T>::type>::value>::type>
{
    using type = std::reference_wrapper<typename std::decay<T>::type>;
};

template <typename T>
using TypeConverter = puerts::converter::Converter<typename ConverterDecay<T>::type>;

template <typename T, typename = void>
struct IsConvertibleHelper : std::false_type
{
};

template <class...>
using Void_t = void;

template <typename T>
struct IsConvertibleHelper<T,
    // test if it has a function toScript
    Void_t<decltype(&TypeConverter<T>::toScript)>> : std::true_type
{
};

}    // namespace internal

namespace converter
{
template <typename T>
constexpr bool isConvertible = internal::IsConvertibleHelper<T>::value;

}
}    // namespace puerts

namespace puerts
{
namespace internal
{
namespace traits
{
template <typename Args>
struct TupleTrait;

template <typename HeadT, typename... TailT>
struct TupleTrait<std::tuple<HeadT, TailT...>>
{
    using Head = HeadT;
    using Tail = std::tuple<TailT...>;

    static constexpr size_t count = 1 + sizeof...(TailT);

    template <size_t i>
    using Arg = typename std::tuple_element<i, std::tuple<HeadT, TailT...>>::type;
};

template <>
struct TupleTrait<std::tuple<>>
{
    using Tail = std::tuple<>;
    static constexpr size_t count = 0;
};

template <typename Func, typename Enable = void>
struct FunctionTrait;

template <typename Ret, typename... Args>
struct FunctionTrait<Ret (*)(Args...)>
{
    using ReturnType = Ret;
    using Arguments = std::tuple<Args...>;
};

template <typename C, typename Ret, typename... Args>
struct FunctionTrait<Ret (C::*)(Args...)> : FunctionTrait<Ret (*)(C*, Args...)>
{
};

template <typename C, typename Ret, typename... Args>
struct FunctionTrait<Ret (C::*)(Args...) const> : FunctionTrait<Ret (*)(C*, Args...)>
{
};

template <typename C, typename Ret, typename... Args>
struct FunctionTrait<Ret (C::*)(Args...) volatile> : FunctionTrait<Ret (*)(C*, Args...)>
{
};

template <typename C, typename Ret, typename... Args>
struct FunctionTrait<Ret (C::*)(Args...) const volatile> : FunctionTrait<Ret (*)(C*, Args...)>
{
};

// functor and lambda
template <typename Functor>
struct FunctionTrait<Functor, Void_t<decltype(&Functor::operator())>>
{
private:
    using FunctorTrait = FunctionTrait<decltype(&Functor::operator())>;

public:
    using ReturnType = typename FunctorTrait::ReturnType;
    using Arguments = typename TupleTrait<typename FunctorTrait::Arguments>::Tail;
};

// decay: remove const, reference; function type to function pointer
template <typename Func>
struct FunctionTrait<Func, typename std::enable_if<!std::is_same<Func, typename std::decay<Func>::type>::value>::type>
    : FunctionTrait<typename std::decay<Func>::type>
{
};
}    // namespace traits

template <typename T>
using FuncTrait = traits::FunctionTrait<T>;

template <bool _First_value, class _First, class... _Rest>
struct _Conjunction
{    // handle false trait or last trait
    using type = _First;
};

template <class _True, class _Next, class... _Rest>
struct _Conjunction<true, _True, _Next, _Rest...>
{    // the first trait is true, try the next one
    using type = typename _Conjunction<_Next::value, _Next, _Rest...>::type;
};

template <class... _Traits>
struct Conjunction : std::true_type
{
};    // If _Traits is empty, true_type

template <class _First, class... _Rest>
struct Conjunction<_First, _Rest...> : _Conjunction<_First::value, _First, _Rest...>::type
{
    // the first false trait in _Traits, or the last trait if none are false
};

template <typename T, typename = void>
struct IsArgsConvertibleHelper : std::false_type
{
};

template <typename... Args>
struct IsArgsConvertibleHelper<std::tuple<Args...>, typename std::enable_if<Conjunction<IsConvertibleHelper<Args>...>::value>::type>
    : std::true_type
{
};

template <typename T>
constexpr bool isArgsConvertible = IsArgsConvertibleHelper<T>::value;

template <int, typename...>
struct ArgumentChecker
{
    static bool Check(CallbackInfoType Info, ContextType Context)
    {
        return true;
    }
};

template <int Pos, typename ArgType, typename... Rest>
struct ArgumentChecker<Pos, ArgType, Rest...>
{
    static constexpr int NextPos = Pos + 1;

    static bool Check(CallbackInfoType Info, ContextType Context)
    {
        if (!TypeConverter<ArgType>::accept(Context, GetArg(Info, Pos)))
        {
            return false;
        }
        else
        {
            return ArgumentChecker<NextPos, Rest...>::Check(Info, Context);
        }
    }
};

template <bool Enable, typename... Args>
struct ArgumentsChecker;

template <typename... Args>
struct ArgumentsChecker<true, Args...>
{
    static constexpr auto ArgsLength = sizeof...(Args);

    static bool Check(ContextType context, CallbackInfoType info)
    {
        if (GetArgsLen(info) != ArgsLength)
            return false;

        if (!ArgumentChecker<0, Args...>::Check(info, context))
            return false;

        return true;
    }
};

template <typename... Args>
struct ArgumentsChecker<false, Args...>
{
    static constexpr auto ArgsLength = sizeof...(Args);

    static bool Check(ContextType context, CallbackInfoType info)
    {
        return true;
    }
};

template <typename, bool CheckArguments, bool>
struct FuncCallHelper
{
};

template <typename Ret, typename... Args, bool CheckArguments, bool ReturnByPointer>
struct FuncCallHelper<std::pair<Ret, std::tuple<Args...>>, CheckArguments, ReturnByPointer>
{
private:
    template <typename T, typename = void>
    struct ArgumentType
    {
        using type = typename std::decay<T>::type;
    };

    template <typename T>
    struct ArgumentType<T, typename std::enable_if<(is_objecttype<typename std::decay<T>::type>::value ||
                                                       is_uetype<typename std::decay<T>::type>::value) &&
                                                   std::is_lvalue_reference<T>::value>::type>
    {
        using type = std::reference_wrapper<typename std::decay<T>::type>;
    };

    static constexpr auto ArgsLength = sizeof...(Args);

    template <typename T, typename Enable = void>
    struct ReturnConverter
    {
        static ValueType Convert(ContextType context, T ret)
        {
            return TypeConverter<typename std::remove_reference<T>::type>::toScript(
                context, std::forward<typename std::remove_reference<T>::type>(ret));
        }
    };

    template <typename T>
    struct ReturnConverter<T, typename std::enable_if<ReturnByPointer && std::is_reference<T>::value && !std::is_const<T>::value &&
                                                      (is_objecttype<typename std::decay<T>::type>::value ||
                                                          is_uetype<typename std::decay<T>::type>::value)>::type>
    {
        static ValueType Convert(ContextType context, T ret)
        {
            return TypeConverter<typename std::decay<T>::type*>::toScript(context, &ret);
        }
    };

    template <typename T, typename Enable = void>
    struct ArgumentHolder
    {
        typename ArgumentType<T>::type Arg;

        using ArgumentDecayType = typename std::decay<T>::type;

        ArgumentHolder(std::tuple<ContextType, ValueType> info)
            : Arg(TypeConverter<ArgumentDecayType>::toCpp(std::get<0>(info), std::get<1>(info)))
        {
        }

        typename ArgumentType<T>::type& GetArgument()
        {
            return Arg;
        }

        void SetRef(ContextType context, ValueType holder)
        {
        }
    };

    template <typename T>
    struct ArgumentHolder<T,
        typename std::enable_if<
            (is_objecttype<typename std::decay<T>::type>::value || is_uetype<typename std::decay<T>::type>::value) &&
            std::is_lvalue_reference<T>::value && !std::is_const<typename std::remove_reference<T>::type>::value &&
            std::is_constructible<typename std::decay<T>::type>::value &&
            std::is_copy_constructible<typename std::decay<T>::type>::value &&
            std::is_destructible<typename std::decay<T>::type>::value>::type>
    {
        typename ArgumentType<T>::type Arg;
        typename ArgumentBufferType<T>::type Buf;

        ArgumentHolder(std::tuple<ContextType, ValueType> info)
            : Arg(*TypeConverter<typename ArgumentType<T>::type>::toCpp(std::get<0>(info), std::get<1>(info)))
        {
            if (&(Arg.get()) == nullptr)
            {
                Arg = Buf;
            }
        }

        ArgumentHolder(const ArgumentHolder&& other) noexcept : Arg(other.Arg)
        {
            if (&(other.Buf) == &(other.Arg.get()))
            {
                Arg = Buf;
            }
        }

        typename ArgumentType<T>::type& GetArgument()
        {
            return Arg;
        }

        void SetRef(ContextType context, ValueType holder)
        {
            if (&Buf != &(Arg.get()))
            {
                return;
            }
            // new object and set
            UpdateRefValue(context, holder, converter::Converter<typename std::decay<T>::type>::toScript(context, Arg.get()));
        }
    };

    template <typename T>
    struct ArgumentHolder<T,
        typename std::enable_if<
            (is_objecttype<typename std::decay<T>::type>::value || is_uetype<typename std::decay<T>::type>::value) &&
            std::is_lvalue_reference<T>::value && !std::is_const<typename std::remove_reference<T>::type>::value &&
            (!std::is_constructible<typename std::decay<T>::type>::value ||
                !std::is_copy_constructible<typename std::decay<T>::type>::value ||
                !std::is_destructible<typename std::decay<T>::type>::value)>::type>
    {
        typename ArgumentType<T>::type Arg;

        // there may be nullptr ref
        ArgumentHolder(std::tuple<ContextType, ValueType> info)
            : Arg(*TypeConverter<typename ArgumentType<T>::type>::toCpp(std::get<0>(info), std::get<1>(info)))
        {
        }

        typename ArgumentType<T>::type& GetArgument()
        {
            return Arg;
        }

        void SetRef(ContextType context, ValueType holder)
        {
        }
    };

    template <typename T>
    struct ArgumentHolder<T,
        typename std::enable_if<
            (is_objecttype<typename std::decay<T>::type>::value || is_uetype<typename std::decay<T>::type>::value) &&
            std::is_lvalue_reference<T>::value && std::is_const<typename std::remove_reference<T>::type>::value>::type>
    {
        typename ArgumentType<T>::type Arg;

        // there may be nullptr ref
        ArgumentHolder(std::tuple<ContextType, ValueType> info)
            : Arg(*TypeConverter<typename std::decay<T>::type*>::toCpp(std::get<0>(info), std::get<1>(info)))
        {
        }

        typename ArgumentType<T>::type& GetArgument()
        {
            return Arg;
        }

        void SetRef(ContextType context, ValueType holder)
        {
        }
    };

    template <typename T>
    struct ArgumentHolder<T,
        typename std::enable_if<!is_objecttype<typename std::decay<T>::type>::value &&
                                !is_uetype<typename std::decay<T>::type>::value && std::is_lvalue_reference<T>::value &&
                                !std::is_const<typename std::remove_reference<T>::type>::value>::type>
    {
        typename ArgumentType<T>::type Arg;

        using ArgumentDecayType = typename std::decay<T>::type;

        ArgumentHolder(std::tuple<ContextType, ValueType> info)
            : Arg(TypeConverter<std::reference_wrapper<ArgumentDecayType>>::toCpp(std::get<0>(info), std::get<1>(info)))
        {
        }

        typename ArgumentType<T>::type& GetArgument()
        {
            return Arg;
        }

        void SetRef(ContextType context, ValueType holder)
        {
            UpdateRefValue(context, holder, converter::Converter<typename std::decay<T>::type>::toScript(context, Arg));
        }
    };

    template <typename T>
    struct ArgumentHolder<T*, typename std::enable_if<is_script_type<T>::value && !std::is_const<T>::value>::type>
    {
        T Buf;

        ArgumentHolder(std::tuple<ContextType, ValueType> info)
            : Buf(TypeConverter<T*>::toCpp(std::get<0>(info), std::get<1>(info)))
        {
        }

        T* GetArgument()
        {
            return &Buf;
        }

        void SetRef(ContextType context, ValueType holder)
        {
            UpdateRefValue(context, holder, converter::Converter<typename std::decay<T>::type>::toScript(context, Buf));
        }
    };

    template <typename T>
    struct ArgumentHolder<T, typename std::enable_if<ArgumentBufferType<T>::is_custom>::type>
    {
        typename ArgumentBufferType<T>::type Buf;

        ArgumentHolder(std::tuple<ContextType, ValueType> info) : Buf(std::get<0>(info), std::get<1>(info))
        {
        }

        typename ArgumentType<T>::type GetArgument()
        {
            return Buf.Data();
        }

        void SetRef(ContextType context, ValueType holder)
        {
        }
    };

    using ArgumentsHolder = std::tuple<ArgumentHolder<Args>...>;

    template <int, typename...>
    struct RefValuesSync
    {
        static void Sync(ContextType context, CallbackInfoType info, ArgumentsHolder& cppArgHolders)
        {
        }
    };

    template <int Pos, typename T, typename... Rest>
    struct RefValuesSync<Pos, T, Rest...>
    {
        static void Sync(ContextType context, CallbackInfoType info, ArgumentsHolder& cppArgHolders)
        {
            std::get<Pos>(cppArgHolders).SetRef(context, GetArg(info, Pos));
            RefValuesSync<Pos + 1, Rest...>::Sync(context, info, cppArgHolders);
        }
    };

    template <typename Func, size_t... index>
    static
        typename std::enable_if<std::is_same<typename internal::traits::FunctionTrait<Func>::ReturnType, void>::value, bool>::type
        call(Func& func, CallbackInfoType info, std::index_sequence<index...>)
    {
        auto context = GetContext(info);

        if (!ArgumentsChecker<CheckArguments, Args...>::Check(context, info))
            return false;

        ArgumentsHolder cppArgHolders(std::tuple<ContextType, ValueType>{context, GetArg(info, index)}...);

        func(std::forward<Args>(std::get<index>(cppArgHolders).GetArgument())...);

        RefValuesSync<0, Args...>::Sync(context, info, cppArgHolders);

        return true;
    }

    template <typename Func, size_t... index>
    static
        typename std::enable_if<!std::is_same<typename internal::traits::FunctionTrait<Func>::ReturnType, void>::value, bool>::type
        call(Func& func, CallbackInfoType info, std::index_sequence<index...>)
    {
        auto context = GetContext(info);

        if (!ArgumentsChecker<CheckArguments, Args...>::Check(context, info))
            return false;

        ArgumentsHolder cppArgHolders(std::tuple<ContextType, ValueType>{context, GetArg(info, index)}...);

        SetReturn(info, ReturnConverter<Ret>::Convert(
                            context, std::forward<Ret>(func(std::forward<Args>(std::get<index>(cppArgHolders).GetArgument())...))));

        RefValuesSync<0, Args...>::Sync(context, info, cppArgHolders);

        return true;
    }

    template <typename Ins, typename Func, size_t... index>
    static
        typename std::enable_if<std::is_same<typename internal::traits::FunctionTrait<Func>::ReturnType, void>::value, bool>::type
        callMethod(Func& func, CallbackInfoType info, std::index_sequence<index...>)
    {
        auto context = GetContext(info);

        auto self = TypeConverter<Ins*>::toCpp(context, GetHolder(info));

        if (!self)
        {
            ThrowException(info, "access a null object");
            return true;
        }

        if (!ArgumentsChecker<CheckArguments, Args...>::Check(context, info))
            return false;

        ArgumentsHolder cppArgHolders(std::tuple<ContextType, ValueType>{context, GetArg(info, index)}...);

        (self->*func)(std::forward<Args>(std::get<index>(cppArgHolders).GetArgument())...);

        RefValuesSync<0, Args...>::Sync(context, info, cppArgHolders);

        return true;
    }

    template <typename Ins, typename Func, size_t... index>
    static
        typename std::enable_if<!std::is_same<typename internal::traits::FunctionTrait<Func>::ReturnType, void>::value, bool>::type
        callMethod(Func& func, CallbackInfoType info, std::index_sequence<index...>)
    {
        auto context = GetContext(info);

        auto self = TypeConverter<Ins*>::toCpp(context, GetHolder(info));

        if (!self)
        {
            ThrowException(info, "access a null object");
            return true;
        }

        if (!ArgumentsChecker<CheckArguments, Args...>::Check(context, info))
            return false;

        ArgumentsHolder cppArgHolders(std::tuple<ContextType, ValueType>{context, GetArg(info, index)}...);

        SetReturn(info, ReturnConverter<Ret>::Convert(context,
                            std::forward<Ret>((self->*func)(std::forward<Args>(std::get<index>(cppArgHolders).GetArgument())...))));

        RefValuesSync<0, Args...>::Sync(context, info, cppArgHolders);

        return true;
    }

public:
    template <typename Func>
    static bool call(Func&& func, CallbackInfoType info)
    {
        return call(func, info, std::make_index_sequence<ArgsLength>());
    }

    template <typename Ins, typename Func>
    static bool callMethod(Func&& func, CallbackInfoType info)
    {
        return callMethod<Ins>(func, info, std::make_index_sequence<ArgsLength>());
    }
};

}    // namespace internal

template <typename T, T, bool ReturnByPointer = false>
struct FuncCallWrapper;

template <typename Ret, typename... Args, Ret (*func)(Args...), bool ReturnByPointer>
struct FuncCallWrapper<Ret (*)(Args...), func, ReturnByPointer>
{
    static void call(CallbackInfoType info)
    {
        using Helper = internal::FuncCallHelper<std::pair<Ret, std::tuple<Args...>>, false, ReturnByPointer>;
        Helper::call(func, info);
    }

    static bool overloadCall(CallbackInfoType info)
    {
        using Helper = internal::FuncCallHelper<std::pair<Ret, std::tuple<Args...>>, true, ReturnByPointer>;
        return Helper::call(func, info);
    }
    static void checkedCall(CallbackInfoType info)
    {
        using Helper = internal::FuncCallHelper<std::pair<Ret, std::tuple<Args...>>, true, ReturnByPointer>;
        if (!Helper::call(func, info))
        {
            ThrowException(info, "invalid parameter!");
        }
    }
    static const CFunctionInfo* info()
    {
        return CFunctionInfoImpl<Ret, Args...>::get();
    }
};

template <typename Inc, typename Ret, typename... Args, Ret (Inc::*func)(Args...), bool ReturnByPointer>
struct FuncCallWrapper<Ret (Inc::*)(Args...), func, ReturnByPointer>
{
    static void call(CallbackInfoType info)
    {
        using Helper = internal::FuncCallHelper<std::pair<Ret, std::tuple<Args...>>, false, ReturnByPointer>;
        Helper::template callMethod<Inc>(func, info);
    }

    static bool overloadCall(CallbackInfoType info)
    {
        using Helper = internal::FuncCallHelper<std::pair<Ret, std::tuple<Args...>>, true, ReturnByPointer>;
        return Helper::template callMethod<Inc, decltype(func)>(func, info);
    }
    static void checkedCall(CallbackInfoType info)
    {
        using Helper = internal::FuncCallHelper<std::pair<Ret, std::tuple<Args...>>, true, ReturnByPointer>;
        if (!Helper::template callMethod<Inc, decltype(func)>(func, info))
        {
            ThrowException(info, "invalid parameter!");
        }
    }
    static const CFunctionInfo* info()
    {
        return CFunctionInfoImpl<Ret, Args...>::get();
    }
};

// TODO: Similar logic...
template <typename Inc, typename Ret, typename... Args, Ret (Inc::*func)(Args...) const, bool ReturnByPointer>
struct FuncCallWrapper<Ret (Inc::*)(Args...) const, func, ReturnByPointer>
{
    static void call(CallbackInfoType info)
    {
        using Helper = internal::FuncCallHelper<std::pair<Ret, std::tuple<Args...>>, false, ReturnByPointer>;
        Helper::template callMethod<Inc>(func, info);
    }

    static bool overloadCall(CallbackInfoType info)
    {
        using Helper = internal::FuncCallHelper<std::pair<Ret, std::tuple<Args...>>, true, ReturnByPointer>;
        return Helper::template callMethod<Inc, decltype(func)>(func, info);
    }
    static void checkedCall(CallbackInfoType info)
    {
        using Helper = internal::FuncCallHelper<std::pair<Ret, std::tuple<Args...>>, true, ReturnByPointer>;
        if (!Helper::template callMethod<Inc, decltype(func)>(func, info))
        {
            ThrowException(info, "invalid parameter!");
        }
    }
    static const CFunctionInfo* info()
    {
        return CFunctionInfoImpl<Ret, Args...>::get();
    }
};

template <typename T, typename... Args>
struct ConstructorWrapper
{
private:
    static constexpr auto ArgsLength = sizeof...(Args);

    template <size_t... index>
    static void* call(CallbackInfoType info, std::index_sequence<index...>)
    {
        auto context = GetContext(info);

        if (GetArgsLen(info) != ArgsLength)
            return nullptr;

        if (!internal::ArgumentChecker<0, Args...>::Check(info, context))
            return nullptr;

        return new T(internal::TypeConverter<Args>::toCpp(context, GetArg(info, index))...);
    }

public:
    static void* call(CallbackInfoType info)
    {
        return call(info, std::make_index_sequence<ArgsLength>());
    }
    static const CFunctionInfo* info()
    {
        return CFunctionInfoImpl<T, Args...>::get();
    }
};

typedef bool (*V8FunctionCallbackWithBoolRet)(CallbackInfoType info);

template <V8FunctionCallbackWithBoolRet Func, V8FunctionCallbackWithBoolRet... Rest>
struct OverloadsRecursion
{
    static bool _call(CallbackInfoType info)
    {
        if (Func(info))
            return true;
        else
            return OverloadsRecursion<Rest...>::_call(info);
    }

    static void call(CallbackInfoType info)
    {
        if (!_call(info))
        {
            ThrowException(info, "invalid parameter!");
        }
    }
};

template <V8FunctionCallbackWithBoolRet Func>
struct OverloadsRecursion<Func>
{
    static bool _call(CallbackInfoType info)
    {
        return Func(info);
    }
};

template <typename... OverloadWraps>
struct OverloadsCombiner
{
    static void call(CallbackInfoType info)
    {
        OverloadsRecursion<(&OverloadWraps::overloadCall)...>::call(info);
    }

    static constexpr int length = sizeof...(OverloadWraps);

    static const CFunctionInfo** infos()
    {
        static const CFunctionInfo* _infos[sizeof...(OverloadWraps)] = {OverloadWraps::info()...};
        return _infos;
    }
};

template <InitializeFuncType Func, InitializeFuncType... Rest>
struct ConstructorRecursion
{
    static void* _call(CallbackInfoType info)
    {
        auto Ret = Func(info);
        if (Ret)
            return Ret;
        else
            return ConstructorRecursion<Rest...>::_call(info);
    }

    static void* call(CallbackInfoType info)
    {
        auto Ret = _call(info);
        if (!Ret)
        {
            ThrowException(info, "invalid parameter!");
        }
        return Ret;
    }
};

template <InitializeFuncType Func>
struct ConstructorRecursion<Func>
{
    static void* _call(CallbackInfoType info)
    {
        return Func(info);
    }
};

template <typename... OverloadWraps>
struct ConstructorsCombiner
{
    static void* call(CallbackInfoType info)
    {
        return ConstructorRecursion<(&OverloadWraps::call)...>::call(info);
    }

    static constexpr int length = sizeof...(OverloadWraps);

    static const CFunctionInfo** infos()
    {
        static const CFunctionInfo* _infos[sizeof...(OverloadWraps)] = {OverloadWraps::info()...};
        return _infos;
    }
};

template <typename T, T, typename Enable = void>
struct PropertyWrapper;

template <class Ins, class Ret, Ret Ins::*member>
struct PropertyWrapper<Ret Ins::*, member, typename std::enable_if<!is_objecttype<Ret>::value && !is_uetype<Ret>::value>::type>
{
    static void getter(CallbackInfoType info)
    {
        auto context = GetContext(info);
        auto self = internal::TypeConverter<Ins*>::toCpp(context, GetThis(info));
        if (!self)
        {
            ThrowException(info, "access a null object");
            return;
        }
        SetReturn(info, internal::TypeConverter<Ret>::toScript(context, self->*member));
    }

    static void setter(CallbackInfoType info)
    {
        auto context = GetContext(info);
        auto self = internal::TypeConverter<Ins*>::toCpp(context, GetThis(info));
        if (!self)
        {
            ThrowException(info, "access a null object");
            return;
        }
        self->*member = internal::TypeConverter<Ret>::toCpp(context, GetArg(info, 0));
    }

    static const char* info()
    {
        return ScriptTypeName<Ret>::value;
    }
};

template <class Ins, class Ret, Ret Ins::*member>
struct PropertyWrapper<Ret Ins::*, member, typename std::enable_if<is_objecttype<Ret>::value || is_uetype<Ret>::value>::type>
{
    static void getter(CallbackInfoType info)
    {
        auto context = GetContext(info);
        auto self = internal::TypeConverter<Ins*>::toCpp(context, GetThis(info));
        if (!self)
        {
            ThrowException(info, "access a null object");
            return;
        }
        auto ret = internal::TypeConverter<Ret*>::toScript(context, &(self->*member));
        LinkOuter<Ins, Ret>(context, GetThis(info), ret);
        SetReturn(info, ret);
    }

    static void setter(CallbackInfoType info)
    {
        auto context = GetContext(info);
        auto self = internal::TypeConverter<Ins*>::toCpp(context, GetThis(info));
        if (!self)
        {
            ThrowException(info, "access a null object");
            return;
        }
        self->*member = internal::TypeConverter<Ret>::toCpp(context, GetArg(info, 0));
    }

    static const char* info()
    {
        return ScriptTypeName<Ret>::value;
    }
};

template <typename Ret, Ret* Variable>
struct PropertyWrapper<Ret*, Variable>
{
    static void getter(CallbackInfoType info)
    {
        auto context = GetContext(info);
        SetReturn(info, internal::TypeConverter<Ret>::toScript(context, *Variable));
    }

    static void setter(CallbackInfoType info)
    {
        auto context = GetContext(info);
        *Variable = internal::TypeConverter<Ret>::toCpp(context, GetArg(info, 0));
    }

    static const char* info()
    {
        return ScriptTypeName<Ret>::value;
    }
};

template <typename T>
class ClassDefineBuilder
{
    template <typename...>
    using sfina = ClassDefineBuilder<T>&;

    const char* className_ = nullptr;

    const void* superTypeId_ = nullptr;

    std::vector<GeneralFunctionInfo> functions_{};

    std::vector<GeneralFunctionInfo> methods_{};

    std::vector<GeneralPropertyInfo> properties_{};

    std::vector<GeneralPropertyInfo> variables_{};

    InitializeFuncType constructor_{};

    std::vector<GeneralFunctionReflectionInfo> constructorInfos_{};
    std::vector<GeneralFunctionReflectionInfo> methodInfos_{};
    std::vector<GeneralFunctionReflectionInfo> functionInfos_{};
    std::vector<GeneralPropertyReflectionInfo> propertyInfos_{};
    std::vector<GeneralPropertyReflectionInfo> variableInfos_{};

public:
    explicit ClassDefineBuilder(const char* className) : className_(className)
    {
    }

    template <typename S>
    ClassDefineBuilder<T>& Extends()
    {
        superTypeId_ = StaticTypeId<S>::get();
        return *this;
    }

    template <typename... Args>
    std::enable_if_t<internal::isArgsConvertible<std::tuple<Args...>>, ClassDefineBuilder<T>&> Constructor()
    {
        InitializeFuncType constructor = ConstructorWrapper<T, Args...>::call;
        constructor_ = constructor;
        constructorInfos_.push_back(GeneralFunctionReflectionInfo{"constructor", ConstructorWrapper<T, Args...>::info()});
        return *this;
    }

    ClassDefineBuilder<T>& Constructor(InitializeFuncType constructor, int length, const CFunctionInfo** infos)
    {
        for (int i = 0; i < length; i++)
        {
            constructorInfos_.push_back(GeneralFunctionReflectionInfo{"constructor", infos[i]});
        }
        constructor_ = constructor;
        return *this;
    }

    ClassDefineBuilder<T>& Function(const char* name, FunctionCallbackType func, const CFunctionInfo* info)
    {
        if (info)
        {
            functionInfos_.push_back(GeneralFunctionReflectionInfo{name, info});
        }
        functions_.push_back(GeneralFunctionInfo{name, func, nullptr});
        return *this;
    }

    ClassDefineBuilder<T>& Function(const char* name, FunctionCallbackType func, int length, const CFunctionInfo** infos)
    {
        for (int i = 0; i < length; i++)
        {
            functionInfos_.push_back(GeneralFunctionReflectionInfo{name, infos[i]});
        }
        functions_.push_back(GeneralFunctionInfo{name, func, nullptr});
        return *this;
    }

    ClassDefineBuilder<T>& Method(const char* name, FunctionCallbackType func, const CFunctionInfo* info)
    {
        if (info)
        {
            methodInfos_.push_back(GeneralFunctionReflectionInfo{name, info});
        }
        methods_.push_back(GeneralFunctionInfo{name, func, nullptr});
        return *this;
    }

    ClassDefineBuilder<T>& Method(const char* name, FunctionCallbackType func, int length, const CFunctionInfo** infos)
    {
        for (int i = 0; i < length; i++)
        {
            methodInfos_.push_back(GeneralFunctionReflectionInfo{name, infos[i]});
        }
        methods_.push_back(GeneralFunctionInfo{name, func, nullptr});
        return *this;
    }

    ClassDefineBuilder<T>& Property(
        const char* name, FunctionCallbackType getter, FunctionCallbackType setter = nullptr, const char* type = nullptr)
    {
        if (type)
        {
            propertyInfos_.push_back(GeneralPropertyReflectionInfo{name, type});
        }
        properties_.push_back(GeneralPropertyInfo{name, getter, setter, nullptr});
        return *this;
    }

    ClassDefineBuilder<T>& Variable(
        const char* name, FunctionCallbackType getter, FunctionCallbackType setter = nullptr, const char* type = nullptr)
    {
        if (type)
        {
            variableInfos_.push_back(GeneralPropertyReflectionInfo{name, type});
        }
        variables_.push_back(GeneralPropertyInfo{name, getter, setter, nullptr});
        return *this;
    }

    typedef void (*FinalizeFuncType)(void* Ptr);

    template <class FC, typename Enable = void>
    struct FinalizeBuilder
    {
        static FinalizeFuncType Build()
        {
            return FinalizeFuncType{};
        }
    };

    template <class FC>
    struct FinalizeBuilder<FC, typename std::enable_if<std::is_destructible<FC>::value>::type>
    {
        static FinalizeFuncType Build()
        {
            return [](void* Ptr) { delete static_cast<FC*>(Ptr); };
        }
    };

#if !BUILDING_PES_EXTENSION
    void Register()
    {
        const bool isUEType = puerts::is_uetype<T>::value;
        static std::vector<JSFunctionInfo> s_functions_{};
        static std::vector<JSFunctionInfo> s_methods_{};
        static std::vector<JSPropertyInfo> s_properties_{};
        static std::vector<JSPropertyInfo> s_variables_{};

        static std::vector<NamedFunctionInfo> s_constructorInfos_{};
        static std::vector<NamedFunctionInfo> s_methodInfos_{};
        static std::vector<NamedFunctionInfo> s_functionInfos_{};
        static std::vector<NamedPropertyInfo> s_propertyInfos_{};
        static std::vector<NamedPropertyInfo> s_variableInfos_{};

        puerts::JSClassDefinition ClassDef = JSClassEmptyDefinition;

        if (isUEType)
        {
            ClassDef.UETypeName = className_;
        }
        else
        {
            ClassDef.ScriptName = className_;
            ClassDef.TypeId = StaticTypeId<T>::get();
            ClassDef.SuperTypeId = superTypeId_;
        }

        ClassDef.Initialize = constructor_;
        ClassDef.Finalize = FinalizeBuilder<T>::Build();

        s_functions_ = std::move(functions_);
        s_functions_.push_back({nullptr, nullptr, nullptr});
        ClassDef.Functions = s_functions_.data();

        s_methods_ = std::move(methods_);
        s_methods_.push_back({nullptr, nullptr, nullptr});
        ClassDef.Methods = s_methods_.data();

        s_properties_ = std::move(properties_);
        s_properties_.push_back(JSPropertyInfo{nullptr, nullptr, nullptr, nullptr});
        ClassDef.Properties = s_properties_.data();

        s_variables_ = std::move(variables_);
        s_variables_.push_back(JSPropertyInfo{nullptr, nullptr, nullptr, nullptr});
        ClassDef.Variables = s_variables_.data();

        s_constructorInfos_ = std::move(constructorInfos_);
        s_constructorInfos_.push_back(NamedFunctionInfo{nullptr, nullptr});
        ClassDef.ConstructorInfos = s_constructorInfos_.data();

        s_methodInfos_ = std::move(methodInfos_);
        s_methodInfos_.push_back(NamedFunctionInfo{nullptr, nullptr});
        ClassDef.MethodInfos = s_methodInfos_.data();

        s_functionInfos_ = std::move(functionInfos_);
        s_functionInfos_.push_back(NamedFunctionInfo{nullptr, nullptr});
        ClassDef.FunctionInfos = s_functionInfos_.data();

        s_propertyInfos_ = std::move(propertyInfos_);
        s_propertyInfos_.push_back(NamedPropertyInfo{nullptr, nullptr});
        ClassDef.PropertyInfos = s_propertyInfos_.data();

        s_variableInfos_ = std::move(variableInfos_);
        s_variableInfos_.push_back(NamedPropertyInfo{nullptr, nullptr});
        ClassDef.VariableInfos = s_variableInfos_.data();

        puerts::RegisterJSClass(ClassDef);
    }
#else
    void Register()
    {
        size_t properties_count = functions_.size() + methods_.size() + properties_.size();
        auto properties = pesapi_alloc_property_descriptors(properties_count);
        size_t pos = 0;
        for (const auto& func : functions_)
        {
            pesapi_set_method_info(properties, pos++, func.Name, true, func.Callback, nullptr, nullptr);
        }

        for (const auto& method : methods_)
        {
            pesapi_set_method_info(properties, pos++, method.Name, false, method.Callback, nullptr, nullptr);
        }

        for (const auto& prop : properties_)
        {
            pesapi_set_property_info(properties, pos++, prop.Name, false, prop.Getter, prop.Setter, nullptr, nullptr);
        }

        for (const auto& prop : variables_)
        {
            pesapi_set_property_info(properties, pos++, prop.Name, true, prop.Getter, prop.Setter, nullptr, nullptr);
        }

        pesapi_finalize finalize = FinalizeBuilder<T>::Build();
        pesapi_define_class(StaticTypeId<T>::get(), superTypeId_, className_, constructor_, finalize, properties_count, properties);
    }
#endif
};

template <typename T>
inline ClassDefineBuilder<T> DefineClass()
{
    return ClassDefineBuilder<T>(ScriptTypeName<T>::value);
}

}    // namespace puerts
